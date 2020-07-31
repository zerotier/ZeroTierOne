/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_BLOCKINGQUEUE_HPP
#define ZT_BLOCKINGQUEUE_HPP

#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <atomic>

#include "../core/Constants.hpp"

namespace ZeroTier {

/**
 * Simple C++11 thread-safe queue
 *
 * Do not use in core/ since we have not gone C++11 there yet.
 */
template <class T>
class BlockingQueue
{
public:
	enum TimedWaitResult
	{
		OK,
		TIMED_OUT,
		STOP
	};

	ZT_INLINE BlockingQueue(void) : r(true) {}

	ZT_INLINE void post(T t)
	{
		std::lock_guard<std::mutex> lock(m);
		q.push(t);
		c.notify_one();
	}

	ZT_INLINE void postLimit(T t,const unsigned long limit)
	{
		std::unique_lock<std::mutex> lock(m);
		for(;;) {
			if (q.size() < limit) {
				q.push(t);
				c.notify_one();
				break;
			}
			if (!r)
				break;
			gc.wait(lock);
		}
	}

	ZT_INLINE void stop(void)
	{
		std::lock_guard<std::mutex> lock(m);
		r = false;
		c.notify_all();
		gc.notify_all();
	}

	ZT_INLINE bool get(T &value)
	{
		std::unique_lock<std::mutex> lock(m);
		if (!r) return false;
		while (q.empty()) {
			c.wait(lock);
			if (!r) {
				gc.notify_all();
				return false;
			}
		}
		value = q.front();
		q.pop();
		gc.notify_all();
		return true;
	}

	ZT_INLINE TimedWaitResult get(T &value,const unsigned long ms)
	{
		const std::chrono::milliseconds ms2{ms};
		std::unique_lock<std::mutex> lock(m);
		if (!r) return STOP;
		while (q.empty()) {
			if (c.wait_for(lock,ms2) == std::cv_status::timeout)
				return ((r) ? TIMED_OUT : STOP);
			else if (!r)
				return STOP;
		}
		value = q.front();
		q.pop();
		return OK;
	}

private:
	std::atomic_bool r;
	std::queue<T> q;
	mutable std::mutex m;
	mutable std::condition_variable c,gc;
};

} // namespace ZeroTier

#endif
