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

#include "../node/Constants.hpp"

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

#include "OSUtils.hpp"

namespace ZeroTier {

#ifdef __UNIX_LIKE__
bool OSUtils::redirectUnixOutputs(const char *stdoutPath,const char *stderrPath)
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

std::map<std::string,bool> OSUtils::listDirectory(const char *path)
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

	closedir(d);
#endif

	return r;
}

void OSUtils::lockDownFile(const char *path,bool isDir)
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

uint64_t OSUtils::getLastModified(const char *path)
{
	struct stat s;
	if (stat(path,&s))
		return 0;
	return (((uint64_t)s.st_mtime) * 1000ULL);
}

bool OSUtils::fileExists(const char *path,bool followLinks)
{
	struct stat s;
#ifdef __UNIX_LIKE__
	if (!followLinks)
		return (lstat(path,&s) == 0);
#endif
	return (stat(path,&s) == 0);
}

int64_t OSUtils::getFileSize(const char *path)
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

bool OSUtils::readFile(const char *path,std::string &buf)
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

bool OSUtils::writeFile(const char *path,const void *buf,unsigned int len)
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

} // namespace ZeroTier
