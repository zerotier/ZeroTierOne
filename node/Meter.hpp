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

#define ZT_METER_HISTORY_LENGTH 4
#define ZT_METER_HISTORY_TICK_DURATION 1000

namespace ZeroTier {

/**
 * Transfer rate meter (thread-safe)
 */
class Meter
{
public:
	ZT_ALWAYS_INLINE Meter()
	{
		for(int i=0;i<ZT_METER_HISTORY_LENGTH;++i)
			_history[i] = 0.0;
		_ts = 0;
		_count = 0;
	}
	ZT_ALWAYS_INLINE Meter(const Meter &m) { *this = m; }

	ZT_ALWAYS_INLINE Meter &operator=(const Meter &m)
	{
		m._lock.lock();
		for(int i=0;i<ZT_METER_HISTORY_LENGTH;++i)
			_history[i] = m._history[i];
		_ts = m._ts;
		_count = m._count;
		m._lock.unlock();
		return *this;
	}

	template<typename I>
	ZT_ALWAYS_INLINE void log(const int64_t now,I count)
	{
		_lock.lock();
		const int64_t since = now - _ts;
		if (since >= ZT_METER_HISTORY_TICK_DURATION) {
			_ts = now;
			for(int i=1;i<ZT_METER_HISTORY_LENGTH;++i)
				_history[i-1] = _history[i];
			_history[ZT_METER_HISTORY_LENGTH-1] = (double)_count / ((double)since / 1000.0);
			_count = 0;
		}
		_count += (uint64_t)count;
		_lock.unlock();
	}

	ZT_ALWAYS_INLINE double perSecond(const int64_t now) const
	{
		double r = 0.0,n = 0.0;

		_lock.lock();
		const int64_t since = (now - _ts);
		if (since >= ZT_METER_HISTORY_TICK_DURATION) {
			r += (double)_count / ((double)since / 1000.0);
			n += 1.0;
		}
		for(int i=0;i<ZT_METER_HISTORY_LENGTH;++i) {
			r += _history[i];
			n += 1.0;
		}
		_lock.unlock();

		return r / n;
	}

private:
	double _history[ZT_METER_HISTORY_LENGTH];
	int64_t _ts;
	uint64_t _count;
	Mutex _lock;
};

} // namespace ZeroTier

#endif
