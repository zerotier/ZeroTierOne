/*
 * Copyright (c)2013-2021 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#include "../core/Constants.hpp"
#include "../core/Containers.hpp"
#include "OSUtils.hpp"

#include <sys/stat.h>

#ifndef __WINDOWS__

#include <dirent.h>
#include <fcntl.h>

#endif

#include <algorithm>
#include <utility>

#if defined(__GCC__) || defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

namespace ZeroTier {

#ifdef __APPLE__

static clock_serv_t _machGetRealtimeClock() noexcept
{
	clock_serv_t c;
	host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &c);
	return c;
}

static clock_serv_t _machGetMonotonicClock() noexcept
{
	clock_serv_t c;
	host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &c);
	return c;
}

clock_serv_t OSUtils::s_machRealtimeClock = _machGetRealtimeClock();
clock_serv_t OSUtils::s_machMonotonicClock = _machGetMonotonicClock();

#endif

unsigned int OSUtils::ztsnprintf(char *buf, unsigned int len, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	int n = (int)vsnprintf(buf, len, fmt, ap);
	va_end(ap);

	if ((n >= (int)len) || (n < 0)) {
		if (len)
			buf[len - 1] = (char)0;
		throw std::length_error("buf[] overflow");
	}

	return (unsigned int)n;
}

#ifdef __UNIX_LIKE__

bool OSUtils::redirectUnixOutputs(const char *stdoutPath, const char *stderrPath)
{
	int fdout = open(stdoutPath, O_WRONLY | O_CREAT, 0600);
	if (fdout > 0) {
		int fderr;
		if (stderrPath) {
			fderr = open(stderrPath, O_WRONLY | O_CREAT, 0600);
			if (fderr <= 0) {
				::close(fdout);
				return false;
			}
		} else fderr = fdout;
		::close(STDOUT_FILENO);
		::close(STDERR_FILENO);
		::dup2(fdout, STDOUT_FILENO);
		::dup2(fderr, STDERR_FILENO);
		return true;
	}
	return false;
}

#endif // __UNIX_LIKE__

Vector< String > OSUtils::listDirectory(const char *path, bool includeDirectories)
{
	Vector< String > r;

#ifdef __WINDOWS__
	HANDLE hFind;
	WIN32_FIND_DATAA ffd;
	if ((hFind = FindFirstFileA((String(path) + "\\*").c_str(),&ffd)) != INVALID_HANDLE_VALUE) {
		do {
			if ( (strcmp(ffd.cFileName,".")) && (strcmp(ffd.cFileName,"..")) && (((ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)||(((ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)&&(includeDirectories))) )
				r.push_back(String(ffd.cFileName));
		} while (FindNextFileA(hFind,&ffd));
		FindClose(hFind);
	}
#else
	dirent de;
	dirent *dptr;
	DIR *d = opendir(path);
	if (!d)
		return r;
	dptr = (struct dirent *)0;
	for (;;) {
		if (readdir_r(d, &de, &dptr))
			break;
		if (dptr) {
			if ((strcmp(dptr->d_name, ".") != 0) && (strcmp(dptr->d_name, "..") != 0) && ((dptr->d_type != DT_DIR) || (includeDirectories)))
				r.push_back(String(dptr->d_name));
		} else break;
	}
	closedir(d);
#endif

	return r;
}

bool OSUtils::rmDashRf(const char *path)
{
#ifdef __WINDOWS__
	HANDLE hFind;
	WIN32_FIND_DATAA ffd;
	if ((hFind = FindFirstFileA((String(path) + "\\*").c_str(),&ffd)) != INVALID_HANDLE_VALUE) {
		do {
			if ((strcmp(ffd.cFileName,".") != 0)&&(strcmp(ffd.cFileName,"..") != 0)) {
				if ((ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
					if (DeleteFileA((String(path) + ZT_PATH_SEPARATOR_S + ffd.cFileName).c_str()) == FALSE)
						return false;
				} else {
					if (!rmDashRf((String(path) + ZT_PATH_SEPARATOR_S + ffd.cFileName).c_str()))
						return false;
				}
			}
		} while (FindNextFileA(hFind,&ffd));
		FindClose(hFind);
	}
	return (RemoveDirectoryA(path) != FALSE);
#else
	dirent de;
	dirent *dptr;
	DIR *d = opendir(path);
	if (!d)
		return true;
	dptr = (struct dirent *)0;
	for (;;) {
		if (readdir_r(d, &de, &dptr) != 0)
			break;
		if (!dptr)
			break;
		if ((strcmp(dptr->d_name, ".") != 0) && (strcmp(dptr->d_name, "..") != 0) && (strlen(dptr->d_name) > 0)) {
			String p(path);
			p.push_back(ZT_PATH_SEPARATOR);
			p.append(dptr->d_name);
			if (unlink(p.c_str()) != 0) { // unlink first will remove symlinks instead of recursing them
				if (!rmDashRf(p.c_str()))
					return false;
			}
		}
	}
	closedir(d);
	return (rmdir(path) == 0);
#endif
}

void OSUtils::lockDownFile(const char *path, bool isDir)
{
#ifdef __UNIX_LIKE__
	chmod(path, isDir ? 0700 : 0600);
#else
#ifdef __WINDOWS__
	{
		STARTUPINFOA startupInfo;
		PROCESS_INFORMATION processInfo;

		startupInfo.cb = sizeof(startupInfo);
		memset(&startupInfo,0,sizeof(STARTUPINFOA));
		memset(&processInfo,0,sizeof(PROCESS_INFORMATION));
		if (CreateProcessA(NULL,(LPSTR)(String("C:\\Windows\\System32\\icacls.exe \"") + path + "\" /inheritance:d /Q").c_str(),NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&startupInfo,&processInfo)) {
			WaitForSingleObject(processInfo.hProcess,INFINITE);
			CloseHandle(processInfo.hProcess);
			CloseHandle(processInfo.hThread);
		}

		startupInfo.cb = sizeof(startupInfo);
		memset(&startupInfo,0,sizeof(STARTUPINFOA));
		memset(&processInfo,0,sizeof(PROCESS_INFORMATION));
		if (CreateProcessA(NULL,(LPSTR)(String("C:\\Windows\\System32\\icacls.exe \"") + path + "\" /remove *S-1-5-32-545 /Q").c_str(),NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&startupInfo,&processInfo)) {
			WaitForSingleObject(processInfo.hProcess,INFINITE);
			CloseHandle(processInfo.hProcess);
			CloseHandle(processInfo.hThread);
		}
	}
#endif
#endif
}

bool OSUtils::fileExists(const char *path, bool followLinks)
{
	struct stat s;
#ifdef __UNIX_LIKE__
	if (!followLinks)
		return (lstat(path, &s) == 0);
#endif
	return (stat(path, &s) == 0);
}

bool OSUtils::readFile(const char *path, String &buf)
{
	char tmp[16384];
	FILE *f = fopen(path, "rb");
	if (f) {
		for (;;) {
			long n = (long)fread(tmp, 1, sizeof(tmp), f);
			if (n > 0)
				buf.append(tmp, n);
			else break;
		}
		fclose(f);
		return true;
	}
	return false;
}

bool OSUtils::writeFile(const char *path, const void *buf, unsigned int len)
{
	FILE *f = fopen(path, "wb");
	if (f) {
		if ((long)fwrite(buf, 1, len, f) != (long)len) {
			fclose(f);
			return false;
		} else {
			fclose(f);
			return true;
		}
	}
	return false;
}

Vector< String > OSUtils::split(const char *s, const char *const sep, const char *esc, const char *quot)
{
	Vector< String > fields;
	String buf;

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
			if (strchr(esc, *s))
				escapeState = true;
			else if ((buf.size() <= 0) && ((quotTmp = strchr(quot, *s))))
				quoteState = *quotTmp;
			else if (strchr(sep, *s)) {
				if (buf.size() > 0) {
					fields.push_back(buf);
					buf.clear();
				} // else skip runs of separators
			} else buf.push_back(*s);
		}
		++s;
	}

	if (buf.size())
		fields.push_back(buf);

	return fields;
}

ZeroTier::String OSUtils::platformDefaultHomePath()
{
#ifdef __QNAP__
	char *cmd = "/sbin/getcfg zerotier Install_Path -f /etc/config/qpkg.conf";
		char buf[128];
		FILE *fp;
		if ((fp = popen(cmd, "r")) == NULL) {
				printf("Error opening pipe!\n");
				return NULL;
		}
		while (fgets(buf, 128, fp) != NULL) { }
		if(pclose(fp))  {
				printf("Command not found or exited with error status\n");
				return NULL;
		}
		String homeDir = String(buf);
		homeDir.erase(std::remove(homeDir.begin(), homeDir.end(), '\n'), homeDir.end());
		return homeDir;
#endif

	// Check for user-defined environment variable before using defaults
#ifdef __WINDOWS__
	DWORD bufferSize = 65535;
	ZeroTier::String userDefinedPath;
	bufferSize = GetEnvironmentVariable("ZEROTIER_HOME", &userDefinedPath[0], bufferSize);
	if (bufferSize)
		return userDefinedPath;
#else
	if (const char *userDefinedPath = getenv("ZEROTIER_HOME"))
		return String(userDefinedPath);
#endif

	// Finally, resort to using default paths if no user-defined path was provided
#ifdef __UNIX_LIKE__

#ifdef __APPLE__
	// /Library/... on Apple
	return ZeroTier::String("/Library/Application Support/ZeroTier");
#else

#ifdef __BSD__
	// BSD likes /var/db instead of /var/lib
	return ZeroTier::String("/var/db/zerotier");
#else
	// Use /var/lib for Linux and other *nix
	return ZeroTier::String("/var/lib/zerotier");
#endif

#endif

#else // not __UNIX_LIKE__

#ifdef __WINDOWS__
	// Look up app data folder on Windows, e.g. C:\ProgramData\...
	char buf[16384];
	if (SUCCEEDED(SHGetFolderPathA(NULL,CSIDL_COMMON_APPDATA,NULL,0,buf))) {
		ZeroTier::String tmp(buf);
		tmp.append("\\ZeroTier");
		return tmp;
	} else {
		return ZeroTier::String("C:\\ZeroTier");
	}
#else
	return (ZeroTier::String(ZT_PATH_SEPARATOR_S) + "ZeroTier"); // UNKNOWN PLATFORM
#endif

#endif // __UNIX_LIKE__ or not...
}

} // namespace ZeroTier
