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

#ifndef ZT_BACKGROUNDRESOLVER_HPP
#define ZT_BACKGROUNDRESOLVER_HPP

#include <vector>
#include <string>

#include "../node/Constants.hpp"
#include "../node/Mutex.hpp"
#include "../node/InetAddress.hpp"
#include "../node/NonCopyable.hpp"
#include "Thread.hpp"

namespace ZeroTier {

class BackgroundResolverJob;

/**
 * A simple background resolver
 */
class BackgroundResolver : NonCopyable
{
	friend class BackgroundResolverJob;

public:
	/**
	 * Construct a new resolver
	 *
	 * resolveNow() must be called to actually initiate background resolution.
	 *
	 * @param name Name to resolve
	 */
	BackgroundResolver(const char *name);

	~BackgroundResolver();

	/**
	 * @return Most recent resolver results or empty vector if none
	 */
	std::vector<InetAddress> get() const;

	/**
	 * Launch a background resolve job now
	 *
	 * If a resolve job is currently in progress, it is aborted and another
	 * job is started.
	 *
	 * Note that jobs can't actually be aborted due to the limitations of the
	 * ancient synchronous OS resolver APIs. As a result, in progress jobs
	 * that are aborted are simply abandoned. Don't call this too frequently
	 * or background threads might pile up.
	 *
	 * @param callback Callback function to receive notification or NULL if none
	 * @praam arg Second argument to callback function
	 */
	void resolveNow(void (*callback)(BackgroundResolver *,void *) = 0,void *arg = 0);

	/**
	 * Abort (abandon) any current resolve jobs
	 */
	void abort();

	/**
	 * @return True if a background job is in progress
	 */
	inline bool running() const
	{
		Mutex::Lock _l(_lock);
		return (_job != (BackgroundResolverJob *)0);
	}

	/**
	 * Wait for pending job to complete (if any)
	 */
	inline void wait() const
	{
		Thread t;
		{
			Mutex::Lock _l(_lock);
			if (!_job)
				return;
			t = _jobThread;
		}
		Thread::join(t);
	}

private:
	void _postResult(const std::vector<InetAddress> &ips);

	std::string _name;
	BackgroundResolverJob *_job;
	Thread _jobThread;
	void (*_callback)(BackgroundResolver *,void *);
	void *_arg;
	std::vector<InetAddress> _ips;
	Mutex _lock;
};

} // namespace ZeroTier

#endif
