/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2015  ZeroTier, Inc.
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
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

#ifdef ZT_ENABLE_CLUSTER

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#include <iostream>

#include "ClusterGeoIpService.hpp"
#include "../node/Utils.hpp"
#include "../osdep/OSUtils.hpp"

#define ZT_CLUSTERGEOIPSERVICE_INTERNAL_CACHE_TTL (60 * 60 * 1000)

namespace ZeroTier {

ClusterGeoIpService::ClusterGeoIpService(const char *pathToExe) :
	_pathToExe(pathToExe),
	_sOutputFd(-1),
	_sInputFd(-1),
	_sPid(0),
	_run(true)
{
	_thread = Thread::start(this);
}

ClusterGeoIpService::~ClusterGeoIpService()
{
	_run = false;
	long p = _sPid;
	if (p > 0) {
		::kill(p,SIGTERM);
		Thread::sleep(500);
		::kill(p,SIGKILL);
	}
	Thread::join(_thread);
}

bool ClusterGeoIpService::locate(const InetAddress &ip,int &x,int &y,int &z)
{
	InetAddress ipNoPort(ip);
	ipNoPort.setPort(0); // we index cache by IP only
	const uint64_t now = OSUtils::now();

	bool r = false;
	{
		Mutex::Lock _l(_cache_m);
		std::map< InetAddress,_CE >::iterator c(_cache.find(ipNoPort));
		if (c != _cache.end()) {
			x = c->second.x;
			y = c->second.y;
			z = c->second.z;
			if ((now - c->second.ts) < ZT_CLUSTERGEOIPSERVICE_INTERNAL_CACHE_TTL)
				return true;
			else r = true; // return true but refresh as well
		}
	}

	{
		Mutex::Lock _l(_sOutputLock);
		if (_sOutputFd >= 0) {
			std::string ips(ipNoPort.toIpString());
			ips.push_back('\n');
			//fprintf(stderr,"ClusterGeoIpService: << %s",ips.c_str());
			::write(_sOutputFd,ips.data(),ips.length());
		}
	}

	return r;
}

void ClusterGeoIpService::threadMain()
	throw()
{
	char linebuf[65536];
	char buf[65536];
	long n,lineptr;

	while (_run) {
		{
			Mutex::Lock _l(_sOutputLock);

			_sOutputFd = -1;
			_sInputFd = -1;
			_sPid = 0;

			int stdinfds[2] = { 0,0 };  // sub-process's stdin, our output
			int stdoutfds[2] = { 0,0 }; // sub-process's stdout, our input
			::pipe(stdinfds);
			::pipe(stdoutfds);

			long p = (long)::vfork();
			if (p < 0) {
				Thread::sleep(500);
				continue;
			} else if (p == 0) {
				::close(stdinfds[1]);
				::close(stdoutfds[0]);
				::dup2(stdinfds[0],STDIN_FILENO);
				::dup2(stdoutfds[1],STDOUT_FILENO);
				::execl(_pathToExe.c_str(),_pathToExe.c_str(),(const char *)0);
				::exit(1);
			} else {
				::close(stdinfds[0]);
				::close(stdoutfds[1]);
				_sOutputFd = stdinfds[1];
				_sInputFd = stdoutfds[0];
				_sPid = p;
			}
		}

		lineptr = 0;
		while (_run) {
			n = ::read(_sInputFd,buf,sizeof(buf));
			if (n <= 0) {
				if (errno == EINTR)
					continue;
				else break;
			}
			for(long i=0;i<n;++i) {
				if (lineptr > (long)sizeof(linebuf))
					lineptr = 0;
				if ((buf[i] == '\n')||(buf[i] == '\r')) {
					linebuf[lineptr] = (char)0;
					if (lineptr > 0) {
						//fprintf(stderr,"ClusterGeoIpService: >> %s\n",linebuf);
						try {
							std::vector<std::string> result(Utils::split(linebuf,",","",""));
							if ((result.size() >= 7)&&(result[1] == "1")) {
								InetAddress rip(result[0],0);
								if ((rip.ss_family == AF_INET)||(rip.ss_family == AF_INET6)) {
									_CE ce;
									ce.ts = OSUtils::now();
									ce.x = (int)::strtol(result[4].c_str(),(char **)0,10);
									ce.y = (int)::strtol(result[5].c_str(),(char **)0,10);
									ce.z = (int)::strtol(result[6].c_str(),(char **)0,10);
									//fprintf(stderr,"ClusterGeoIpService: %s is at %d,%d,%d\n",rip.toIpString().c_str(),ce.x,ce.y,ce.z);
									{
										Mutex::Lock _l2(_cache_m);
										_cache[rip] = ce;
									}
								}
							}
						} catch ( ... ) {}
					}
					lineptr = 0;
				} else linebuf[lineptr++] = buf[i];
			}
		}

		::close(_sOutputFd);
		::close(_sInputFd);
		::kill(_sPid,SIGTERM);
		Thread::sleep(250);
		::kill(_sPid,SIGKILL);
		::waitpid(_sPid,(int *)0,0);
	}
}

} // namespace ZeroTier

#endif // ZT_ENABLE_CLUSTER
