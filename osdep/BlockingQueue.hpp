/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2016  ZeroTier, Inc.  https://www.zerotier.com/
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ZT_BLOCKINGQUEUE_HPP
#define ZT_BLOCKINGQUEUE_HPP

#include <queue>
#include <mutex>
#include <condition_variable>

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
	BlockingQueue(void) {}

	inline void post(T t)
	{
		std::lock_guard<std::mutex> lock(m);
		q.push(t);
		c.notify_one();
	}

	inline T get(void)
	{
		std::unique_lock<std::mutex> lock(m);
		while(q.empty())
			c.wait(lock);
		T val = q.front();
		q.pop();
		return val;
	}

private:
	std::queue<T> q;
	mutable std::mutex m;
	std::condition_variable c;
};

} // namespace ZeroTier

#endif
