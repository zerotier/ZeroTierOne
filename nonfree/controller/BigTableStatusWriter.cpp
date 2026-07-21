/* (c) ZeroTier, Inc.
 * See LICENSE.txt in nonfree/
 */

#include "BigTableStatusWriter.hpp"

#include "../../osdep/OSUtils.hpp"
#include "ControllerConfig.hpp"
#include "CtlUtil.hpp"
#include "PubSubWriter.hpp"

#include <chrono>
#include <cstddef>
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
// single request -- it never splits by mutation count.  Each row here adds up
// to 5 SetCells, so a cold node_info cache (e.g. the first flush after a
// restart) can push one batch far past the limit.  Flush in chunks
// comfortably under the limit.
static const size_t kMaxMutationsPerBulk = 25000;
static const size_t kMaxMutationsPerRow = 5;	// 3 node_info + 1 ip + 1 last_seen

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

	cbt::BulkMutation bulk;
	// Row-key hash for each row in the current chunk, aligned with bulk's indices,
	// so a failed mutation can invalidate its cache entry
	// (FailedMutation::original_index() is relative to the chunk's BulkApply).
	std::vector<uint64_t> bulkRowHashes;
	bulkRowHashes.reserve(toWrite.size());
	size_t bulkMutationCount = 0;	// SetCells accumulated in the current chunk
	size_t chunkStart = 0;			// index into toWrite of the current chunk's first entry

	// Flush the accumulated chunk.  Per-mutation failures are logged and their
	// cache entries dropped.  If BulkApply throws, the chunk's outcome is unknown:
	// invalidate its cache entries and re-queue the not-yet-written tail of
	// toWrite, then return false so the caller aborts (earlier chunks were
	// already applied, so they are not re-queued).
	auto flushChunk = [&]() -> bool {
		if (bulk.size() == 0) {
			return true;
		}
		ZTC_LOG("Applying %zu rows (%zu mutations) to BigTable\n", bulk.size(), bulkMutationCount);
		try {
			std::vector<cbt::FailedMutation> failures = _table->BulkApply(std::move(bulk));
			ZTC_LOG("BigTable write completed with %zu failures\n", failures.size());
			for (auto const& r : failures) {
				std::cerr << ::ZeroTier::controllerLogId() << " Error writing to BigTable: " << r.status() << "\n";
				// Drop the cache entry for any failed row so its node_info is rewritten
				// next cycle rather than being assumed durably written.
				const int idx = r.original_index();
				if (idx >= 0 && static_cast<size_t>(idx) < bulkRowHashes.size()) {
					_lastNodeInfo.erase(bulkRowHashes[idx]);
				}
			}
		}
		catch (const std::exception& e) {
			ZTC_LOG("Exception writing to BigTable: %s\n", e.what());
			span->SetAttribute("error", e.what());
			span->SetStatus(opentelemetry::trace::StatusCode::kError, e.what());
			// The chunk's outcome is unknown, so clear the rows it covered to force a
			// node_info rewrite next cycle rather than trusting an optimistic update.
			for (uint64_t keyHash : bulkRowHashes) {
				_lastNodeInfo.erase(keyHash);
			}
			// Don't drop the unwritten entries -- re-queue this chunk and everything
			// after it for the next cycle.
			std::vector<PendingStatusEntry> tail(
				std::make_move_iterator(toWrite.begin() + static_cast<std::ptrdiff_t>(chunkStart)),
				std::make_move_iterator(toWrite.end()));
			requeuePendingStatus(_pending, _lock, std::move(tail), "BigTableStatusWriter");
			return false;
		}
		bulk = cbt::BulkMutation();
		bulkRowHashes.clear();
		bulkMutationCount = 0;
		return true;
	};

	for (size_t i = 0; i < toWrite.size(); ++i) {
		const auto& entry = toWrite[i];

		// Flush first if this row could push the chunk past the cap.  Uses the
		// worst-case per-row count and runs before any _lastNodeInfo access for
		// this row, so a failed flush never strands an optimistic cache update
		// for a row that was never sent.
		if (bulkMutationCount + kMaxMutationsPerRow > kMaxMutationsPerBulk) {
			if (! flushChunk()) {
				return;
			}
			chunkStart = i;
		}

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
		bulkRowHashes.push_back(keyHash);
		bulkMutationCount += rowMutations;
	}

	if (! flushChunk()) {
		return;
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