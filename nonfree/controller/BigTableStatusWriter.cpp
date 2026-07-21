/* (c) ZeroTier, Inc.
 * See LICENSE.txt in nonfree/
 */

#include "BigTableStatusWriter.hpp"

#include "../../osdep/OSUtils.hpp"
#include "ControllerConfig.hpp"
#include "CtlUtil.hpp"
#include "PubSubWriter.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <functional>
#include <google/cloud/bigtable/mutations.h>
#include <google/cloud/bigtable/row.h>
#include <google/cloud/bigtable/table.h>
#include <google/cloud/status.h>
#include <iterator>
#include <opentelemetry/trace/provider.h>
#include <thread>

namespace cbt = google::cloud::bigtable;

namespace ZeroTier {

const std::string nodeInfoColumnFamily = "node_info";
const std::string checkInColumnFamily = "check_in";

const std::string osColumn = "os";
const std::string archColumn = "arch";
const std::string versionColumn = "version";
const std::string ipv4Column = "ipv4";
const std::string ipv6Column = "ipv6";
const std::string lastSeenColumn = "last_seen";

// node_info changes rarely, so we skip rewriting an unchanged row.  As a safety
// net we still rewrite it at most this often, so the cell can never age out from
// under us if the GC policy is ever changed to union (OR) semantics.
static const int64_t kNodeInfoRefreshMs = 6LL * 24 * 60 * 60 * 1000;	// 6 days
// Drop cache entries for rows we haven't seen within this window (node assumed
// offline), so the cache tracks the active set rather than every node ever seen.
static const int64_t kCacheEntryTtlMs = 24LL * 60 * 60 * 1000;	// 24 hours
static const int64_t kEvictionIntervalMs = 60LL * 60 * 1000;	// sweep at most hourly
// Bigtable's MutateRows API rejects requests containing more than 100,000
// mutations (SetCells) total, and google-cloud-cpp sends a BulkMutation as a
// single request -- it never splits by mutation count.  Send small batches
// (500 rows x <=5 SetCells each, far below the limit) from a small pool of
// worker threads.  Each worker uses the *synchronous* BulkApply on purpose:
// the async client path makes progress only on the connection's shared
// background event threads, which stall for tens of seconds at a time in this
// environment, while a sync call is driven entirely by its own thread.
static const size_t kRowsPerBatch = 500;
static const size_t kWriterThreads = 8;

BigTableStatusWriter::BigTableStatusWriter(
	const std::string& project_id,
	const std::string& instance_id,
	const std::string& table_id)
	: _project_id(project_id)
	, _instance_id(instance_id)
	, _table_id(table_id)
	, _table(nullptr)
	, _lastEvictionMs(0)
{
	_table = new cbt::Table(cbt::MakeDataConnection(), cbt::TableResource(_project_id, _instance_id, _table_id));
	fprintf(
		stderr, "BigTableStatusWriter for project %s instance %s table %s\n", project_id.c_str(), instance_id.c_str(),
		table_id.c_str());
}

BigTableStatusWriter::~BigTableStatusWriter()
{
	writePending();

	if (_table != nullptr) {
		delete _table;
		_table = nullptr;
	}
}

void BigTableStatusWriter::updateNodeStatus(
	const std::string& network_id,
	const std::string& node_id,
	const std::string& os,
	const std::string& arch,
	const std::string& version,
	const InetAddress& address,
	int64_t last_seen,
	const std::string& frontend)
{
	auto provider = opentelemetry::trace::Provider::GetTracerProvider();
	auto tracer = provider->GetTracer("BigTableStatusWriter");
	auto span = tracer->StartSpan("BigTableStatusWriter::updateNodeStatus");
	auto scope = tracer->WithActiveSpan(span);

	std::lock_guard<std::mutex> l(_lock);
	_pending.push_back({ network_id, node_id, os, arch, version, address, last_seen, frontend });
}

size_t BigTableStatusWriter::queueLength() const
{
	std::lock_guard<std::mutex> l(_lock);
	return _pending.size();
}

void BigTableStatusWriter::writePending()
{
	auto provider = opentelemetry::trace::Provider::GetTracerProvider();
	auto tracer = provider->GetTracer("BigTableStatusWriter");
	auto span = tracer->StartSpan("BigTableStatusWriter::writePending");
	auto scope = tracer->WithActiveSpan(span);

	std::vector<PendingStatusEntry> toWrite;
	{
		std::lock_guard<std::mutex> l(_lock);
		toWrite.swap(_pending);
	}
	if (toWrite.empty()) {
		ZTC_LOG("BigTableStatusWriter::writePending: nothing to write\n");
		return;
	}

	const int64_t nowMs = OSUtils::now();
	const std::hash<std::string> hasher;

	// Build every batch on this thread first (all _lastNodeInfo reads and
	// updates stay here, lock-free), then fan the RPCs out to worker threads.
	// Workers only perform the RPC and record its outcome; the results --
	// cache invalidation, re-queueing, logging -- are processed back on this
	// thread after the workers are joined.
	struct WriteBatch {
		cbt::BulkMutation bulk;
		// Row-key hash per row, aligned with the batch's indices, so a failed
		// mutation can invalidate its cache entry
		// (FailedMutation::original_index() is relative to this batch's request).
		std::vector<uint64_t> rowHashes;
		size_t entryStart = 0;	 // index into toWrite of this batch's first entry
		size_t entryCount = 0;
		size_t mutationCount = 0;
		std::vector<cbt::FailedMutation> failures;
		bool threw = false;
		std::string error;
		int64_t durationMs = 0;
	};
	std::vector<WriteBatch> batches;
	batches.reserve((toWrite.size() + kRowsPerBatch - 1) / kRowsPerBatch);

	for (size_t batchStart = 0; batchStart < toWrite.size(); batchStart += kRowsPerBatch) {
		const size_t batchEnd = std::min(batchStart + kRowsPerBatch, toWrite.size());

		WriteBatch batch;
		batch.entryStart = batchStart;
		batch.entryCount = batchEnd - batchStart;
		batch.rowHashes.reserve(batch.entryCount);

		for (size_t i = batchStart; i < batchEnd; ++i) {
			const auto& entry = toWrite[i];

			std::string row_key = entry.network_id + "#" + entry.node_id;
			const uint64_t keyHash = hasher(row_key);

			// Use the member's last-seen time (epoch ms) as the explicit cell timestamp for
			// every cell in this row.  With a server-set timestamp each SetCell lands at a
			// fresh server clock value, so re-applying the same mutation creates a *new* cell
			// version instead of overwriting -- non-idempotent, and google-cloud-cpp won't
			// retry such writes.  A deterministic timestamp makes a re-applied update an exact
			// overwrite (idempotent + safely retriable).  Bigtable cell timestamps are
			// millisecond-granular, so last_seen (already ms) aligns exactly; it's also
			// monotonic per member, so a later check-in always reads as the newest version.
			// It also makes duplicate rows racing across parallel batches order-independent.
			const std::chrono::milliseconds cellTs(entry.last_seen);

			cbt::SingleRowMutation m(row_key);
			size_t rowMutations = 0;

			// node_info (os/arch/version) changes rarely.  Write it only when our
			// last-written value for this row differs, or hasn't been refreshed in a
			// while -- no read RPC, the controller is the sole writer of node_info.
			const uint64_t valueHash = hasher(entry.os + "|" + entry.arch + "|" + entry.version);
			auto it = _lastNodeInfo.find(keyHash);
			const bool writeNodeInfo = (it == _lastNodeInfo.end()) || (it->second.valueHash != valueHash)
				|| ((nowMs - it->second.lastWrittenMs) > kNodeInfoRefreshMs);

			if (writeNodeInfo) {
				m.emplace_back(cbt::SetCell(nodeInfoColumnFamily, osColumn, cellTs, entry.os));
				m.emplace_back(cbt::SetCell(nodeInfoColumnFamily, archColumn, cellTs, entry.arch));
				m.emplace_back(cbt::SetCell(nodeInfoColumnFamily, versionColumn, cellTs, entry.version));
				rowMutations += 3;
				_lastNodeInfo[keyHash] = NodeInfoState { valueHash, nowMs, nowMs };
			}
			else {
				it->second.lastSeenMs = nowMs;
			}

			// check_in (ip + last_seen) changes every cycle, so it's always written.
			char buf[64] = { 0 };
			std::string addressStr = entry.address.toString(buf);
			if (entry.address.ss_family == AF_INET) {
				m.emplace_back(cbt::SetCell(checkInColumnFamily, ipv4Column, cellTs, std::move(addressStr)));
				rowMutations++;
			}
			else if (entry.address.ss_family == AF_INET6) {
				m.emplace_back(cbt::SetCell(checkInColumnFamily, ipv6Column, cellTs, std::move(addressStr)));
				rowMutations++;
			}
			m.emplace_back(cbt::SetCell(checkInColumnFamily, lastSeenColumn, cellTs, entry.last_seen));
			rowMutations++;

			batch.bulk.emplace_back(m);
			batch.rowHashes.push_back(keyHash);
			batch.mutationCount += rowMutations;
		}

		batches.push_back(std::move(batch));
	}

	ZTC_LOG("Applying %zu rows to BigTable in %zu batches of up to %zu\n", toWrite.size(), batches.size(), kRowsPerBatch);

	std::atomic<size_t> nextBatch { 0 };
	std::vector<std::thread> workers;
	workers.reserve(std::min(kWriterThreads, batches.size()));
	for (size_t t = 0; t < std::min(kWriterThreads, batches.size()); ++t) {
		workers.emplace_back([this, &batches, &nextBatch]() {
			for (size_t i = nextBatch.fetch_add(1); i < batches.size(); i = nextBatch.fetch_add(1)) {
				WriteBatch& b = batches[i];
				const auto start = std::chrono::steady_clock::now();
				try {
					b.failures = _table->BulkApply(std::move(b.bulk));
				}
				catch (const std::exception& e) {
					b.threw = true;
					b.error = e.what();
				}
				b.durationMs =
					std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start)
						.count();
			}
		});
	}
	for (auto& w : workers) {
		w.join();
	}

	// Failed entries are re-queued for the next cycle rather than dropped: the
	// deterministic cell timestamps make replays idempotent and
	// requeuePendingStatus caps the backlog.  This covers transient
	// per-mutation failures (expired auth tokens, UNAVAILABLE, ...) that the
	// client does not retry internally.
	std::vector<PendingStatusEntry> toRequeue;
	size_t totalFailures = 0;
	for (WriteBatch& batch : batches) {
		if (batch.threw) {
			// The batch's outcome is unknown, so clear the rows it covered to force a
			// node_info rewrite next cycle rather than trusting an optimistic update,
			// and re-queue all of its entries.
			ZTC_LOG("Exception writing to BigTable after %lld ms: %s\n", (long long)batch.durationMs, batch.error.c_str());
			span->SetAttribute("error", batch.error);
			span->SetStatus(opentelemetry::trace::StatusCode::kError, batch.error);
			for (uint64_t keyHash : batch.rowHashes) {
				_lastNodeInfo.erase(keyHash);
			}
			toRequeue.insert(
				toRequeue.end(), std::make_move_iterator(toWrite.begin() + static_cast<std::ptrdiff_t>(batch.entryStart)),
				std::make_move_iterator(
					toWrite.begin() + static_cast<std::ptrdiff_t>(batch.entryStart + batch.entryCount)));
			continue;
		}
		ZTC_LOG(
			"BigTable batch of %zu rows (%zu mutations) completed with %zu failures in %lld ms\n",
			batch.rowHashes.size(), batch.mutationCount, batch.failures.size(), (long long)batch.durationMs);
		totalFailures += batch.failures.size();
		for (auto const& r : batch.failures) {
			std::cerr << ::ZeroTier::controllerLogId() << " Error writing to BigTable: " << r.status() << "\n";
			const int idx = r.original_index();
			if (idx < 0 || static_cast<size_t>(idx) >= batch.rowHashes.size()) {
				continue;
			}
			// Drop the cache entry for any failed row so its node_info is rewritten
			// next cycle rather than being assumed durably written.
			_lastNodeInfo.erase(batch.rowHashes[idx]);
			// Re-queue the failed entry unless the server called it malformed --
			// batches are far below the request limits, so INVALID_ARGUMENT here
			// means the row itself is unprocessable and would just loop forever.
			if (r.status().code() != google::cloud::StatusCode::kInvalidArgument) {
				toRequeue.push_back(std::move(toWrite[batch.entryStart + static_cast<size_t>(idx)]));
			}
		}
	}

	ZTC_LOG("BigTable write completed with %zu failures\n", totalFailures);
	if (! toRequeue.empty()) {
		ZTC_LOG("BigTableStatusWriter: re-queueing %zu entries for retry next cycle\n", toRequeue.size());
		requeuePendingStatus(_pending, _lock, std::move(toRequeue), "BigTableStatusWriter");
	}

	// Periodically evict rows we haven't seen lately so the cache tracks the
	// currently-active node set rather than growing for the process's lifetime.
	if ((nowMs - _lastEvictionMs) > kEvictionIntervalMs) {
		for (auto it = _lastNodeInfo.begin(); it != _lastNodeInfo.end();) {
			if ((nowMs - it->second.lastSeenMs) > kCacheEntryTtlMs) {
				it = _lastNodeInfo.erase(it);
			}
			else {
				++it;
			}
		}
		_lastEvictionMs = nowMs;
	}
}

}	// namespace ZeroTier