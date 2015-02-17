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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#include "Constants.hpp"
#include "Logger.hpp"

namespace ZeroTier {

Logger::Logger(const char *p,const char *prefix,unsigned long maxLogSize) :
	_path((p) ? p : ""),
	_prefix((prefix) ? (std::string(prefix) + " ") : ""),
	_maxLogSize(maxLogSize),
	_log_m(),
	_log((FILE *)0)
{
	if (_path.length())
		_log = fopen(_path.c_str(),"a");
	else _log = stdout;
}

Logger::~Logger()
{
	fflush(_log);
	if ((_log)&&(_log != stdout)&&(_log != stderr))
		fclose(_log);
}

void Logger::log(const char *fmt,...)
{
	va_list ap;
	char tmp[128];

	if (_log) {
		Mutex::Lock _l(_log_m);
		_rotateIfNeeded();

		if (_log) {
			time_t now = time(0);
#ifdef __WINDOWS__
			ctime_s(tmp,sizeof(tmp),&now);
			char *nowstr = tmp;
#else
			char *nowstr = ctime_r(&now,tmp);
#endif
			for(char *c=nowstr;*c;++c) {
				if (*c == '\n')
					*c = '\0';
			}

			if (_prefix.length())
				fwrite(_prefix.data(),1,_prefix.length(),_log);

			fprintf(_log,"[%s] ",nowstr);
			va_start(ap,fmt);
			vfprintf(_log,fmt,ap);
			va_end(ap);
#ifdef _WIN32
			fwrite("\r\n",1,2,_log);
#else
			fwrite("\n",1,1,_log);
#endif

			fflush(_log);
		}
	}
}

#ifdef ZT_TRACE
void Logger::trace(const char *module,unsigned int line,const char *fmt,...)
{
	va_list ap;
	char tmp[128];

	if (_log) {
		Mutex::Lock _l(_log_m);
		_rotateIfNeeded();

		if (_log) {
			time_t now = time(0);
#ifdef __WINDOWS__
			ctime_s(tmp,sizeof(tmp),&now);
			char *nowstr = tmp;
#else
			char *nowstr = ctime_r(&now,tmp);
#endif
			for(char *c=nowstr;*c;++c) {
				if (*c == '\n')
					*c = '\0';
			}

			if (_prefix.length())
				fwrite(_prefix.data(),1,_prefix.length(),_log);

			fprintf(_log,"[%s] TRACE/%s:%u ",nowstr,module,line);
			va_start(ap,fmt);
			vfprintf(_log,fmt,ap);
			va_end(ap);
#ifdef _WIN32
			fwrite("\r\n",1,2,_log);
#else
			fwrite("\n",1,1,_log);
#endif

			fflush(_log);
		}
	}
}
#endif

void Logger::_rotateIfNeeded()
{
	if ((_maxLogSize)&&(_log != stdout)&&(_log != stderr)) {
		long pos = ftell(_log);
		if (pos > (long)_maxLogSize) {
			fclose(_log);
			rename(_path.c_str(),std::string(_path).append(".old").c_str());
			_log = fopen(_path.c_str(),"w");
		}
	}
}

} // namespace ZeroTier

