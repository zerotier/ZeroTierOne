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
#include <sys/stat.h>
#include <sys/uio.h>
#include <dirent.h>
#endif

#ifdef __WINDOWS__
#include <wincrypt.h>
#endif

#include "Utils.hpp"
#include "Mutex.hpp"

namespace ZeroTier {

const char Utils::HEXCHARS[16] = { '0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f' };

#ifdef __UNIX_LIKE__
bool Utils::redirectUnixOutputs(const char *stdoutPath,const char *stderrPath)
	throw()
{
	int fdout = ::open(stdoutPath,O_WRONLY|O_CREAT,0600);
	if (fdout > 0) {
		int fderr;
		if (stderrPath) {
			fderr = ::open(stderrPath,O_WRONLY|O_CREAT,0600);
			if (fderr <= 0) {
				::close(fdout);
				return false;
			}
		} else fderr = fdout;
		::close(STDOUT_FILENO);
		::close(STDERR_FILENO);
		::dup2(fdout,STDOUT_FILENO);
		::dup2(fderr,STDERR_FILENO);
		return true;
	}
	return false;
}
#endif // __UNIX_LIKE__

static void _Utils_doBurn(char *ptr,unsigned int len)
{
	for(unsigned int i=0;i<len;++i)
		ptr[i] = (char)0;
}
void (*volatile _Utils_doBurn_ptr)(char *,unsigned int) = _Utils_doBurn;
void Utils::burn(void *ptr,unsigned int len)
	throw()
{
	// Ridiculous hack: call _doBurn() via a volatile function pointer to
	// hold down compiler optimizers and beat them mercilessly until they
	// cry and mumble something about never eliding secure memory zeroing
	// again.
	(_Utils_doBurn_ptr)((char *)ptr,len);
}

std::map<std::string,bool> Utils::listDirectory(const char *path)
{
	std::map<std::string,bool> r;

#ifdef __WINDOWS__
	HANDLE hFind;
	WIN32_FIND_DATAA ffd;
	if ((hFind = FindFirstFileA((std::string(path) + "\\*").c_str(),&ffd)) != INVALID_HANDLE_VALUE) {
		do {
			if ((strcmp(ffd.cFileName,"."))&&(strcmp(ffd.cFileName,"..")))
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

std::string Utils::unhex(const char *hex,unsigned int maxlen)
{
	int n = 1;
	unsigned char c,b = 0;
	const char *eof = hex + maxlen;
	std::string r;

	if (!maxlen)
		return r;

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
		if (hex == eof)
			break;
	}

	return r;
}

unsigned int Utils::unhex(const char *hex,unsigned int maxlen,void *buf,unsigned int len)
{
	int n = 1;
	unsigned char c,b = 0;
	unsigned int l = 0;
	const char *eof = hex + maxlen;

	if (!maxlen)
		return 0;

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
		if (hex == eof)
			break;
	}

	return l;
}

void Utils::getSecureRandom(void *buf,unsigned int bytes)
{
#ifdef __WINDOWS__

	static HCRYPTPROV cryptProvider = NULL;
	static Mutex globalLock;

	Mutex::Lock _l(globalLock);

	if (cryptProvider == NULL) {
		if (!CryptAcquireContextA(&cryptProvider,NULL,NULL,PROV_RSA_FULL,CRYPT_VERIFYCONTEXT|CRYPT_SILENT)) {
			fprintf(stderr,"FATAL ERROR: Utils::getSecureRandom() unable to obtain WinCrypt context!\r\n");
			exit(1);
			return;
		}
	}

	if (!CryptGenRandom(cryptProvider,(DWORD)bytes,(BYTE *)buf)) {
		fprintf(stderr,"FATAL ERROR: Utils::getSecureRandom() CryptGenRandom failed!\r\n");
		exit(1);
	}

#else // not __WINDOWS__

#ifdef __UNIX_LIKE__

	static char randomBuf[65536];
	static unsigned int randomPtr = sizeof(randomBuf);
	static int devURandomFd = -1;
	static Mutex globalLock;

	Mutex::Lock _l(globalLock);

	if (devURandomFd <= 0) {
		devURandomFd = ::open("/dev/urandom",O_RDONLY);
		if (devURandomFd <= 0) {
			fprintf(stderr,"FATAL ERROR: Utils::getSecureRandom() unable to open /dev/urandom\r\n");
			exit(1);
			return;
		}
	}

	for(unsigned int i=0;i<bytes;++i) {
		if (randomPtr >= sizeof(randomBuf)) {
			if ((int)::read(devURandomFd,randomBuf,sizeof(randomBuf)) != (int)sizeof(randomBuf)) {
				fprintf(stderr,"FATAL ERROR: Utils::getSecureRandom() unable to read from /dev/urandom\r\n");
				exit(1);
				return;
			}
			randomPtr = 0;
		}
		((char *)buf)[i] = randomBuf[randomPtr++];
	}

#else // not __UNIX_LIKE__

#error No getSecureRandom() implementation available.

#endif // __UNIX_LIKE__
#endif // __WINDOWS__
}

void Utils::lockDownFile(const char *path,bool isDir)
{
#ifdef __UNIX_LIKE__
	chmod(path,isDir ? 0700 : 0600);
#else
#ifdef __WINDOWS__
	{
		STARTUPINFOA startupInfo;
		PROCESS_INFORMATION processInfo;

		startupInfo.cb = sizeof(startupInfo);
		memset(&startupInfo,0,sizeof(STARTUPINFOA));
		memset(&processInfo,0,sizeof(PROCESS_INFORMATION));
		if (CreateProcessA(NULL,(LPSTR)(std::string("C:\\Windows\\System32\\icacls.exe \"") + path + "\" /inheritance:d /Q").c_str(),NULL,NULL,FALSE,0,NULL,NULL,&startupInfo,&processInfo)) {
			WaitForSingleObject(processInfo.hProcess,INFINITE);
			CloseHandle(processInfo.hProcess);
			CloseHandle(processInfo.hThread);
		}

		startupInfo.cb = sizeof(startupInfo);
		memset(&startupInfo,0,sizeof(STARTUPINFOA));
		memset(&processInfo,0,sizeof(PROCESS_INFORMATION));
		if (CreateProcessA(NULL,(LPSTR)(std::string("C:\\Windows\\System32\\icacls.exe \"") + path + "\" /remove *S-1-5-32-545 /Q").c_str(),NULL,NULL,FALSE,0,NULL,NULL,&startupInfo,&processInfo)) {
			WaitForSingleObject(processInfo.hProcess,INFINITE);
			CloseHandle(processInfo.hProcess);
			CloseHandle(processInfo.hThread);
		}
	}
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

bool Utils::fileExists(const char *path,bool followLinks)
{
	struct stat s;
#ifdef __UNIX_LIKE__
	if (!followLinks)
		return (lstat(path,&s) == 0);
#endif
	return (stat(path,&s) == 0);
}

int64_t Utils::getFileSize(const char *path)
{
	struct stat s;
	if (stat(path,&s))
		return -1;
#ifdef __WINDOWS__
	return s.st_size;
#else
	if (S_ISREG(s.st_mode))
		return s.st_size;
#endif
	return -1;
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
