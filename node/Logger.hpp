/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2011-2015  ZeroTier Networks
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

#ifndef ZT_LOGGER_HPP
#define ZT_LOGGER_HPP

#include <stdio.h>

#include <string>
#include <stdexcept>

#include "NonCopyable.hpp"
#include "Mutex.hpp"

#undef LOG
#define LOG(f,...) if (RR->log) RR->log->log(f,##__VA_ARGS__)

#undef TRACE
#ifdef ZT_TRACE
#define TRACE(f,...) if (RR->log) RR->log->trace(__FILE__,__LINE__,f,##__VA_ARGS__)
#else
#define TRACE(f,...) {}
#endif

namespace ZeroTier {

/**
 * Utility for outputting logs to a file or stdout/stderr
 */
class Logger : NonCopyable
{
public:
	/**
	 * Construct a logger to log to a file or stdout
	 * 
	 * If a path is supplied to log to a file, maxLogSize indicates the size
	 * at which this file is closed, renamed to .old, and then a new log is
	 * opened (essentially a log rotation). If stdout is used, this is ignored.
	 *
	 * @param p Path to log to or NULL to use stdout
	 * @param prefix Prefix to prepend to log lines or NULL for none
	 * @param maxLogSize Maximum log size (0 for no limit)
	 */
	Logger(const char *p,const char *prefix,unsigned long maxLogSize);
	~Logger();

	void log(const char *fmt,...);

#ifdef ZT_TRACE
	void trace(const char *module,unsigned int line,const char *fmt,...);
#else
	inline void trace(const char *module,unsigned int line,const char *fmt,...) {}
#endif

private:
	void _rotateIfNeeded();

	std::string _path;
	std::string _prefix;
	unsigned long _maxLogSize;
	Mutex _log_m;
	FILE *_log;
};

} // namespace ZeroTier

#endif

