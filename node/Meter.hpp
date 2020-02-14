/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2024-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_METER_HPP
#define ZT_METER_HPP

#include "Constants.hpp"
#include "Mutex.hpp"

#include <algorithm>

namespace ZeroTier {

/**
 * Transfer rate and total transferred amount meter
 *
 * @tparam TUNIT Unit of time in milliseconds (default: 1000 for one second)
 * @tparam LSIZE Log size in units of time (default: 60 for one minute worth of data)
 */
template<int64_t TUNIT = 1000,unsigned long LSIZE = 60>
class Meter
{
public:
	/**
	 * Create and initialize a new meter
	 *
	 * @param now Start time
	 */
	ZT_ALWAYS_INLINE Meter(const int64_t now) noexcept : startTime(now) {}

	/**
	 * Add a measurement
	 *
	 * @tparam I Type of 'count' (usually inferred)
	 * @param now Current time
	 * @param count Count of items (usually bytes)
	 */
	template<typename I>
	ZT_ALWAYS_INLINE void log(const int64_t now,I count) noexcept
	{
		_total += (uint64_t)count;

		// We log by choosing a log bucket based on the current time in units modulo
		// the log size and then if it's a new bucket setting it or otherwise adding
		// to it.
		const unsigned long bucket = ((unsigned int)((uint64_t)(now / TUNIT))) % LSIZE;
		if (_bucket.exchange(bucket) != bucket)
			_counts[bucket].store((uint64_t)count);
		else _counts[bucket].fetch_add((uint64_t)count);
	}

	/**
	 * Get rate per TUNIT time
	 *
	 * @param now Current time
	 * @return Count per TUNIT time (rate)
	 */
	ZT_ALWAYS_INLINE double rate(const int64_t now) const noexcept
	{
		// Rate is computed by looking back at N buckets where N is the smaller of
		// the size of the log or the number of units since the start time.
		const unsigned long lookback = std::min((unsigned long)((now - startTime) / TUNIT),LSIZE);
		if (lookback == 0)
			return 0.0;
		unsigned long bi = ((unsigned int)((uint64_t)(now / TUNIT)));
		double sum = 0.0;
		for(unsigned long l=0;l<lookback;++l)
			sum += (double)_counts[bi-- % LSIZE].load();
		return sum / (double)lookback;
	}

	/**
	 * @return Total count since meter was created
	 */
	ZT_ALWAYS_INLINE uint64_t total() const noexcept { return _total.load(); }

private:
	const int64_t startTime;
	std::atomic<uint64_t> _total;
	std::atomic<uint64_t> _counts[LSIZE];
	std::atomic<unsigned long> _bucket;
};

} // namespace ZeroTier

#endif
