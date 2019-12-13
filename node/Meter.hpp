/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2023-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_METER_HPP
#define ZT_METER_HPP

#include "Constants.hpp"
#include "Mutex.hpp"
#include "AtomicCounter.hpp"

#define ZT_METER_HISTORY_LENGTH 4
#define ZT_METER_HISTORY_TICK_DURATION 1000

namespace ZeroTier {

/**
 * Transfer rate meter (thread-safe)
 */
class Meter
{
public:
	inline Meter()
	{
		for(int i=0;i<ZT_METER_HISTORY_LENGTH;++i)
			_history[i] = 0.0;
		_ts = 0;
		_count = 0;
	}

	template<typename I>
	inline void log(const int64_t now,I count)
	{
		const int64_t since = now - _ts;
		if (since >= ZT_METER_HISTORY_TICK_DURATION) {
			_ts = now;
			_history[(unsigned int)(++_hptr) % ZT_METER_HISTORY_LENGTH] = (double)_count / ((double)since / 1000.0);
			_count = (uint64_t)count;
		} else {
			_count += (uint64_t)count;
		}
	}

	inline double perSecond(const int64_t now) const
	{
		double r = 0.0,n = 0.0;
		const int64_t since = (now - _ts);
		if (since >= ZT_METER_HISTORY_TICK_DURATION) {
			r += (double)_count / ((double)since / 1000.0);
			n += 1.0;
		}
		for(int i=0;i<ZT_METER_HISTORY_LENGTH;++i) {
			r += _history[i];
			n += 1.0;
		}
		return r / n;
	}

private:
	volatile double _history[ZT_METER_HISTORY_LENGTH];
	volatile int64_t _ts;
	volatile uint64_t _count;
	AtomicCounter _hptr;
};

} // namespace ZeroTier

#endif
