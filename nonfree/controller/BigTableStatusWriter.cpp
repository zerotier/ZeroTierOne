/* (c) ZeroTier, Inc.
 * See LICENSE.txt in nonfree/
 */

#include "BigTableStatusWriter.hpp"

#include "../../osdep/OSUtils.hpp"
#include "ControllerConfig.hpp"
#include "CtlUtil.hpp"
#include "PubSubWriter.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <deque>
#include <functional>
#include <google/cloud/bigtable/mutations.h>
#include <google/cloud/bigtable/row.h>
#include <google/cloud/bigtable/table.h>
#include <iterator>
#include <opentelemetry/trace/provider.h>

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
// single request -- it never splits by mutation count.  Rather than one big
// batch per cycle, send small batches (500 rows x <=5 SetCells each, far below
// the limit) and keep several in flight at once: small requests spread load
// across tablet servers and a slow tablet only stalls its own batch, while the
// parallel window keeps the total flush time low.
static const size_t kRowsPerBatch = 500;
static const size_t kMaxParallelBatches = 8;

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

	// One small AsyncBulkApply per kRowsPerBatch rows, with up to
	// kMaxParallelBatches in flight at once.  The connection's background
	// threads only fulfill the futures; results are always processed on this
	// thread, so _lastNodeInfo and the re-queue list need no extra locking.
	struct InFlightBatch {
		google::cloud::future<std::vector<cbt::FailedMutation>> result;
		// Row-key hash per row, aligned with the batch's indices, so a failed
		// mutation can invalidate its cache entry
		// (FailedMutation::original_index() is relative to this batch's request).
		std::vector<uint64_t> rowHashes;
		size_t entryStart;	// index into toWrite of this batch's first entry
		size_t entryCount;
		size_t mutationCount;
	};
	std::deque<InFlightBatch> inFlight;
	// Entries whose batch outcome is unknown (the write threw); re-queued for
	// the next cycle in a single call once every batch has been drained.
	std::vector<PendingStatusEntry> toRequeue;
	size_t totalFailures = 0;

	// Wait for a batch's result and handle it.  Per-mutation failures are
	// logged and their cache entries dropped.  If the write threw, the batch's
	// outcome is unknown: invalidate its cache entries and collect its entries
	// for re-queueing.  Other batches are unaffected either way.
	auto processBatch = [&](InFlightBatch& batch) {
		try {
			std::vector<cbt::FailedMutation> failures = batch.result.get();
			ZTC_LOG(
				"BigTable batch of %zu rows (%zu mutations) completed with %zu failures\n", batch.rowHashes.size(),
				batch.mutationCount, failures.size());
			totalFailures += failures.size();
			for (auto const& r : failures) {
				std::cerr << ::ZeroTier::controllerLogId() << " Error writing to BigTable: " << r.status() << "\n";
				// Drop the cache entry for any failed row so its node_info is rewritten
				// next cycle rather than being assumed durably written.
				const int idx = r.original_index();
				if (idx >= 0 && static_cast<size_t>(idx) < batch.rowHashes.size()) {
					_lastNodeInfo.erase(batch.rowHashes[idx]);
				}
			}
		}
		catch (const std::exception& e) {
			ZTC_LOG("Exception writing to BigTable: %s\n", e.what());
			span->SetAttribute("error", e.what());
			span->SetStatus(opentelemetry::trace::StatusCode::kError, e.what());
			// The batch's outcome is unknown, so clear the rows it covered to force a
			// node_info rewrite next cycle rather than trusting an optimistic update.
			for (uint64_t keyHash : batch.rowHashes) {
				_lastNodeInfo.erase(keyHash);
			}
			// Don't drop the batch -- collect its entries for the next cycle.
			toRequeue.insert(
				toRequeue.end(), std::make_move_iterator(toWrite.begin() + static_cast<std::ptrdiff_t>(batch.entryStart)),
				std::make_move_iterator(
					toWrite.begin() + static_cast<std::ptrdiff_t>(batch.entryStart + batch.entryCount)));
		}
	};

	ZTC_LOG("Applying %zu rows to BigTable in batches of %zu\n", toWrite.size(), kRowsPerBatch);

	for (size_t batchStart = 0; batchStart < toWrite.size(); batchStart += kRowsPerBatch) {
		const size_t batchEnd = std::min(batchStart + kRowsPerBatch, toWrite.size());

		cbt::BulkMutation bulk;
		InFlightBatch batch;
		batch.entryStart = batchStart;
		batch.entryCount = batchEnd - batchStart;
		batch.mutationCount = 0;
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

			bulk.emplace_back(m);
			batch.rowHashes.push_back(keyHash);
			batch.mutationCount += rowMutations;
		}

		// Keep at most kMaxParallelBatches in flight: before launching another,
		// wait for the oldest and handle its result.
		if (inFlight.size() >= kMaxParallelBatches) {
			processBatch(inFlight.front());
			inFlight.pop_front();
		}
		batch.result = _table->AsyncBulkApply(std::move(bulk));
		inFlight.push_back(std::move(batch));
	}

	while (! inFlight.empty()) {
		processBatch(inFlight.front());
		inFlight.pop_front();
	}

	ZTC_LOG("BigTable write completed with %zu failures\n", totalFailures);
	if (! toRequeue.empty()) {
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