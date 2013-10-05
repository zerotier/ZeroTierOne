/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2012-2013  ZeroTier Networks LLC
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
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/stat.h>

#include "Constants.hpp"

#ifdef __UNIX_LIKE__
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <dirent.h>
#endif

#include "Utils.hpp"
#include "Mutex.hpp"
#include "Salsa20.hpp"

namespace ZeroTier {

const char Utils::HEXCHARS[16] = { '0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f' };

static const char *DAY_NAMES[7] = { "Sun","Mon","Tue","Wed","Thu","Fri","Sat" };
static const char *MONTH_NAMES[12] = { "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec" };

std::map<std::string,bool> Utils::listDirectory(const char *path)
{
	std::map<std::string,bool> r;

#ifdef __WINDOWS__
	HANDLE hFind;
	WIN32_FIND_DATAA ffd;
	if ((hFind = FindFirstFileA((std::string(path) + "\\*").c_str(),&ffd)) != INVALID_HANDLE_VALUE) {
		do {
			r[std::string(ffd.cFileName)] = ((ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0);
		} while (FindNextFileA(hFind,&ffd));
		FindClose(hFind);
	}
#else
	struct dirent de;
	struct dirent *dptr;

	DIR *d = opendir(path);
	if (!d)
		return r;

	dptr = (struct dirent *)0;
	for(;;) {
		if (readdir_r(d,&de,&dptr))
			break;
		if (dptr) {
			if ((strcmp(dptr->d_name,"."))&&(strcmp(dptr->d_name,"..")))
				r[std::string(dptr->d_name)] = (dptr->d_type == DT_DIR);
		} else break;
	}
#endif

	return r;
}

std::string Utils::hex(const void *data,unsigned int len)
{
	std::string r;
	r.reserve(len * 2);
	for(unsigned int i=0;i<len;++i) {
		r.push_back(HEXCHARS[(((const unsigned char *)data)[i] & 0xf0) >> 4]);
		r.push_back(HEXCHARS[((const unsigned char *)data)[i] & 0x0f]);
	}
	return r;
}

std::string Utils::unhex(const char *hex)
{
	int n = 1;
	unsigned char c,b = 0;
	std::string r;

	while ((c = (unsigned char)*(hex++))) {
		if ((c >= 48)&&(c <= 57)) { // 0..9
			if ((n ^= 1))
				r.push_back((char)(b | (c - 48)));
			else b = (c - 48) << 4;
		} else if ((c >= 65)&&(c <= 70)) { // A..F
			if ((n ^= 1))
				r.push_back((char)(b | (c - (65 - 10))));
			else b = (c - (65 - 10)) << 4;
		} else if ((c >= 97)&&(c <= 102)) { // a..f
			if ((n ^= 1))
				r.push_back((char)(b | (c - (97 - 10))));
			else b = (c - (97 - 10)) << 4;
		}
	}

	return r;
}

unsigned int Utils::unhex(const char *hex,void *buf,unsigned int len)
{
	int n = 1;
	unsigned char c,b = 0;
	unsigned int l = 0;

	while ((c = (unsigned char)*(hex++))) {
		if ((c >= 48)&&(c <= 57)) { // 0..9
			if ((n ^= 1)) {
				if (l >= len) break;
				((unsigned char *)buf)[l++] = (b | (c - 48));
			} else b = (c - 48) << 4;
		} else if ((c >= 65)&&(c <= 70)) { // A..F
			if ((n ^= 1)) {
				if (l >= len) break;
				((unsigned char *)buf)[l++] = (b | (c - (65 - 10)));
			} else b = (c - (65 - 10)) << 4;
		} else if ((c >= 97)&&(c <= 102)) { // a..f
			if ((n ^= 1)) {
				if (l >= len) break;
				((unsigned char *)buf)[l++] = (b | (c - (97 - 10)));
			} else b = (c - (97 - 10)) << 4;
		}
	}

	return l;
}

unsigned int Utils::unhex(const char *hex,unsigned int hexlen,void *buf,unsigned int len)
	throw()
{
	int n = 1;
	unsigned char c,b = 0;
	unsigned int l = 0;
	const char *const end = hex + hexlen;

	while (hex != end) {
		c = (unsigned char)*(hex++);
		if ((c >= 48)&&(c <= 57)) { // 0..9
			if ((n ^= 1)) {
				if (l >= len) break;
				((unsigned char *)buf)[l++] = (b | (c - 48));
			} else b = (c - 48) << 4;
		} else if ((c >= 65)&&(c <= 70)) { // A..F
			if ((n ^= 1)) {
				if (l >= len) break;
				((unsigned char *)buf)[l++] = (b | (c - (65 - 10)));
			} else b = (c - (65 - 10)) << 4;
		} else if ((c >= 97)&&(c <= 102)) { // a..f
			if ((n ^= 1)) {
				if (l >= len) break;
				((unsigned char *)buf)[l++] = (b | (c - (97 - 10)));
			} else b = (c - (97 - 10)) << 4;
		}
	}

	return l;
}

void Utils::getSecureRandom(void *buf,unsigned int bytes)
{
	static Mutex randomLock;
	static char randbuf[32768];
	static unsigned int randptr = sizeof(randbuf);
#ifdef __WINDOWS__
	static Salsa20 s20;
	volatile bool s20Initialized = false;
#endif

	Mutex::Lock _l(randomLock);
	for(unsigned int i=0;i<bytes;++i) {
		if (randptr >= sizeof(randbuf)) {
#ifdef __UNIX_LIKE__
			int fd = ::open("/dev/urandom",O_RDONLY);
			if (fd < 0) {
				fprintf(stderr,"FATAL ERROR: unable to open /dev/urandom: %s"ZT_EOL_S,strerror(errno));
				exit(-1);
			}
			if ((int)::read(fd,randbuf,sizeof(randbuf)) != (int)sizeof(randbuf)) {
				fprintf(stderr,"FATAL ERROR: unable to read from /dev/urandom"ZT_EOL_S);
				exit(-1);
			}
			::close(fd);
#else
#ifdef __WINDOWS__
			if (!s20Initialized) {
				s20Initialized = true;
				char ktmp[32];
				char ivtmp[8];
				for(int i=0;i<32;++i) ktmp[i] = (char)rand();
				for(int i=0;i<8;++i) ivtmp[i] = (char)rand();
				double now = Utils::nowf();
				memcpy(ktmp,&now,sizeof(now));
				DWORD tmp = GetCurrentProcessId();
				memcpy(ktmp + sizeof(double),&tmp,sizeof(tmp));
				tmp = GetTickCount();
				memcpy(ktmp + sizeof(double) + sizeof(DWORD),&tmp,sizeof(tmp));
				s20.init(ktmp,256,ivtmp);
				for(int i=0;i<sizeof(randbuf);++i) randbuf[i] = (char)rand();
			}
			s20.encrypt(randbuf,randbuf,sizeof(randbuf));
#else
no getSecureRandom() implementation;
#endif
#endif
			randptr = 0;
		}
		((char *)buf)[i] = randbuf[randptr++];
	}
}

void Utils::lockDownFile(const char *path,bool isDir)
{
#if defined(__APPLE__) || defined(__linux__) || defined(linux) || defined(__LINUX__) || defined(__linux)
	chmod(path,isDir ? 0700 : 0600);
#else
#ifdef _WIN32
	// TODO: windows ACL hell...
#endif
#endif
}

uint64_t Utils::getLastModified(const char *path)
{
	struct stat s;
	if (stat(path,&s))
		return 0;
	return (((uint64_t)s.st_mtime) * 1000ULL);
}

std::string Utils::toRfc1123(uint64_t t64)
{
	struct tm t;
	char buf[128];
	time_t utc = (time_t)(t64 / 1000ULL);
#ifdef __WINDOWS__
	gmtime_s(&t,&utc);
#else
	gmtime_r(&utc,&t);
#endif
	Utils::snprintf(buf,sizeof(buf),"%3s, %02d %3s %4d %02d:%02d:%02d GMT",DAY_NAMES[t.tm_wday],t.tm_mday,MONTH_NAMES[t.tm_mon],t.tm_year + 1900,t.tm_hour,t.tm_min,t.tm_sec);
	return std::string(buf);
}

#ifdef __WINDOWS__
static int is_leap(unsigned y) {
        y += 1900;
        return (y % 4) == 0 && ((y % 100) != 0 || (y % 400) == 0);
}
static time_t timegm(struct tm *tm) {
        static const unsigned ndays[2][12] = {
                {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
                {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
        };
        time_t res = 0;
        int i;
        for (i = 70; i < tm->tm_year; ++i)
                res += is_leap(i) ? 366 : 365;

        for (i = 0; i < tm->tm_mon; ++i)
                res += ndays[is_leap(tm->tm_year)][i];
        res += tm->tm_mday - 1;
        res *= 24;
        res += tm->tm_hour;
        res *= 60;
        res += tm->tm_min;
        res *= 60;
        res += tm->tm_sec;
        return res;
}
#endif

uint64_t Utils::fromRfc1123(const char *tstr)
{
	struct tm t;
	char wdays[128],mons[128];

	int l = (int)strlen(tstr);
	if ((l < 29)||(l > 64))
		return 0;
	int assigned = sscanf(tstr,"%3s, %02d %3s %4d %02d:%02d:%02d GMT",wdays,&t.tm_mday,mons,&t.tm_year,&t.tm_hour,&t.tm_min,&t.tm_sec);
	if (assigned != 7)
		return 0;

	wdays[3] = '\0';
	for(t.tm_wday=0;t.tm_wday<7;++t.tm_wday) {
#ifdef __WINDOWS__
		if (!_stricmp(DAY_NAMES[t.tm_wday],wdays))
			break;
#else
		if (!strcasecmp(DAY_NAMES[t.tm_wday],wdays))
			break;
#endif
	}
	if (t.tm_wday == 7)
		return 0;
	mons[3] = '\0';
	for(t.tm_mon=0;t.tm_mon<12;++t.tm_mon) {
#ifdef __WINDOWS__
		if (!_stricmp(MONTH_NAMES[t.tm_mday],mons))
			break;
#else
		if (!strcasecmp(MONTH_NAMES[t.tm_mday],mons))
			break;
#endif
	}
	if (t.tm_mon == 12)
		return 0;

	t.tm_wday = 0; // ignored by timegm
	t.tm_yday = 0; // ignored by timegm
	t.tm_isdst = 0; // ignored by timegm

	time_t utc = timegm(&t);

	return ((utc > 0) ? (1000ULL * (uint64_t)utc) : 0ULL);
}

bool Utils::readFile(const char *path,std::string &buf)
{
	char tmp[4096];
	FILE *f = fopen(path,"rb");
	if (f) {
		for(;;) {
			long n = (long)fread(tmp,1,sizeof(tmp),f);
			if (n > 0)
				buf.append(tmp,n);
			else break;
		}
		fclose(f);
		return true;
	}
	return false;
}

bool Utils::writeFile(const char *path,const void *buf,unsigned int len)
{
	FILE *f = fopen(path,"wb");
	if (f) {
		if ((long)fwrite(buf,1,len,f) != (long)len) {
			fclose(f);
			return false;
		} else {
			fclose(f);
			return true;
		}
	}
	return false;
}

std::vector<std::string> Utils::split(const char *s,const char *const sep,const char *esc,const char *quot)
{
	std::vector<std::string> fields;
	std::string buf;

	if (!esc)
		esc = "";
	if (!quot)
		quot = "";

	bool escapeState = false;
	char quoteState = 0;
	while (*s) {
		if (escapeState) {
			escapeState = false;
			buf.push_back(*s);
		} else if (quoteState) {
			if (*s == quoteState) {
				quoteState = 0;
				fields.push_back(buf);
				buf.clear();
			} else buf.push_back(*s);
		} else {
			const char *quotTmp;
			if (strchr(esc,*s))
				escapeState = true;
			else if ((buf.size() <= 0)&&((quotTmp = strchr(quot,*s))))
				quoteState = *quotTmp;
			else if (strchr(sep,*s)) {
				if (buf.size() > 0) {
					fields.push_back(buf);
					buf.clear();
				} // else skip runs of seperators
			} else buf.push_back(*s);
		}
		++s;
	}

	if (buf.size())
		fields.push_back(buf);

	return fields;
}

std::string Utils::trim(const std::string &s)
{
	unsigned long end = (unsigned long)s.length();
	while (end) {
		char c = s[end - 1];
		if ((c == ' ')||(c == '\r')||(c == '\n')||(!c)||(c == '\t'))
			--end;
		else break;
	}
	unsigned long start = 0;
	while (start < end) {
		char c = s[start];
		if ((c == ' ')||(c == '\r')||(c == '\n')||(!c)||(c == '\t'))
			++start;
		else break;
	}
	return s.substr(start,end - start);
}

void Utils::stdsprintf(std::string &s,const char *fmt,...)
	throw(std::bad_alloc,std::length_error)
{
	char buf[65536];
	va_list ap;

	va_start(ap,fmt);
	int n = vsnprintf(buf,sizeof(buf),fmt,ap);
	va_end(ap);

	if ((n >= (int)sizeof(buf))||(n < 0))
		throw std::length_error("printf result too large");

	s.append(buf);
}

unsigned int Utils::snprintf(char *buf,unsigned int len,const char *fmt,...)
	throw(std::length_error)
{
	va_list ap;

	va_start(ap,fmt);
	int n = (int)vsnprintf(buf,len,fmt,ap);
	va_end(ap);

	if ((n >= (int)len)||(n < 0)) {
		if (len)
			buf[len - 1] = (char)0;
		throw std::length_error("buf[] overflow in Utils::snprintf");
	}

	return (unsigned int)n;
}

} // namespace ZeroTier
