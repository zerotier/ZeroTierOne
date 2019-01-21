/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2019  ZeroTier, Inc.  https://www.zerotier.com/
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

#ifndef ZT_BLOCKINGQUEUE_HPP
#define ZT_BLOCKINGQUEUE_HPP

#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>

#include "Thread.hpp"

namespace ZeroTier {

/**
 * Simple C++11 thread-safe queue
 *
 * Do not use in node/ since we have not gone C++11 there yet.
 */
template <class T>
class BlockingQueue
{
public:
	BlockingQueue(void) : r(true) {}

	inline void post(T t)
	{
		std::lock_guard<std::mutex> lock(m);
		q.push(t);
		c.notify_one();
	}

	inline void postLimit(T t,const unsigned long limit)
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

	inline void stop(void)
	{
		std::lock_guard<std::mutex> lock(m);
		r = false;
		c.notify_all();
		gc.notify_all();
	}

	inline bool get(T &value)
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

	enum TimedWaitResult
	{
		OK,
		TIMED_OUT,
		STOP
	};

	inline TimedWaitResult get(T &value,const unsigned long ms)
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
	volatile bool r;
	std::queue<T> q;
	mutable std::mutex m;
	mutable std::condition_variable c,gc;
};

} // namespace ZeroTier

#endif
