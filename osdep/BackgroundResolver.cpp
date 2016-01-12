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

#include "OSUtils.hpp"
#include "Thread.hpp"
#include "BackgroundResolver.hpp"

namespace ZeroTier {

/*
 * We can't actually abort a job. This is a legacy characteristic of the
 * ancient synchronous resolver APIs. So to abort jobs, we just abandon
 * them by setting their parent to null.
 */
class BackgroundResolverJob
{
public:
	std::string name;
	BackgroundResolver *volatile parent;
	Mutex lock;

	void threadMain()
		throw()
	{
		std::vector<InetAddress> ips;
		try {
			ips = OSUtils::resolve(name.c_str());
		} catch ( ... ) {}
		{
			Mutex::Lock _l(lock);
			BackgroundResolver *p = parent;
			if (p)
				p->_postResult(ips);
		}
		delete this;
	}
};

BackgroundResolver::BackgroundResolver(const char *name) :
	_name(name),
	_job((BackgroundResolverJob *)0),
	_callback(0),
	_arg((void *)0),
	_ips(),
	_lock()
{
}

BackgroundResolver::~BackgroundResolver()
{
	abort();
}

std::vector<InetAddress> BackgroundResolver::get() const
{
	Mutex::Lock _l(_lock);
	return _ips;
}

void BackgroundResolver::resolveNow(void (*callback)(BackgroundResolver *,void *),void *arg)
{
	Mutex::Lock _l(_lock);

	if (_job) {
		Mutex::Lock _l2(_job->lock);
		_job->parent = (BackgroundResolver *)0;
		_job = (BackgroundResolverJob *)0;
	}

	BackgroundResolverJob *j = new BackgroundResolverJob();
	j->name = _name;
	j->parent = this;

	_job = j;
	_callback = callback;
	_arg = arg;

	_jobThread = Thread::start(j);
}

void BackgroundResolver::abort()
{
	Mutex::Lock _l(_lock);
	if (_job) {
		Mutex::Lock _l2(_job->lock);
		_job->parent = (BackgroundResolver *)0;
		_job = (BackgroundResolverJob *)0;
	}
}

void BackgroundResolver::_postResult(const std::vector<InetAddress> &ips)
{
	void (*cb)(BackgroundResolver *,void *);
	void *a;
	{
		Mutex::Lock _l(_lock);
		_job = (BackgroundResolverJob *)0;
		cb = _callback;
		a = _arg;
		_ips = ips;
	}
	if (cb)
		cb(this,a);
}

} // namespace ZeroTier
