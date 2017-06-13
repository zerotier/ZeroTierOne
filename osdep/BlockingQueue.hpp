/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2017  ZeroTier, Inc.  https://www.zerotier.com/
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

	inline void stop(void)
	{
		std::lock_guard<std::mutex> lock(m);
		r = false;
		c.notify_all();
	}

	/**
	 * @param value Value to set to next queue item if return value is true
	 * @return False if stop() has been called, true otherwise
	 */
	inline bool get(T &value)
	{
		std::unique_lock<std::mutex> lock(m);
		if (!r) return false;
		while (q.empty()) {
			c.wait(lock);
			if (!r) return false;
		}
		value = q.front();
		q.pop();
		return true;
	}

private:
	volatile bool r;
	std::queue<T> q;
	std::mutex m;
	std::condition_variable c;
};

} // namespace ZeroTier

#endif
