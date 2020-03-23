/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2024-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#include "../node/Constants.hpp"
#include "../node/Utils.hpp"
#include "OSUtils.hpp"

#ifdef __WINDOWS__
#include <WinSock2.h>
#include <Windows.h>
#include <Shlwapi.h>
#else
#include <dirent.h>
#include <fcntl.h>
#endif

#ifdef __GCC__
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

namespace ZeroTier {

#ifdef __APPLE__
static clock_serv_t _machGetRealtimeClock() noexcept
{
	clock_serv_t c;
	host_get_clock_service(mach_host_self(),CALENDAR_CLOCK,&c);
	return c;
}
clock_serv_t OSUtils::s_machRealtimeClock = _machGetRealtimeClock();
#endif

unsigned int OSUtils::ztsnprintf(char *buf,unsigned int len,const char *fmt,...)
{
	va_list ap;

	va_start(ap,fmt);
	int n = (int)vsnprintf(buf,len,fmt,ap);
	va_end(ap);

	if ((n >= (int)len)||(n < 0)) {
		if (len)
			buf[len - 1] = (char)0;
		throw std::length_error("buf[] overflow");
	}

	return (unsigned int)n;
}

#ifdef __UNIX_LIKE__
bool OSUtils::redirectUnixOutputs(const char *stdoutPath,const char *stderrPath)
{
	int fdout = open(stdoutPath,O_WRONLY|O_CREAT,0600);
	if (fdout > 0) {
		int fderr;
		if (stderrPath) {
			fderr = open(stderrPath,O_WRONLY|O_CREAT,0600);
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

std::vector<std::string> OSUtils::listDirectory(const char *path,bool includeDirectories)
{
	std::vector<std::string> r;

#ifdef __WINDOWS__
	HANDLE hFind;
	WIN32_FIND_DATAA ffd;
	if ((hFind = FindFirstFileA((std::string(path) + "\\*").c_str(),&ffd)) != INVALID_HANDLE_VALUE) {
		do {
			if ( (strcmp(ffd.cFileName,".")) && (strcmp(ffd.cFileName,"..")) && (((ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)||(((ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)&&(includeDirectories))) )
				r.push_back(std::string(ffd.cFileName));
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
	for(;;) {
		if (readdir_r(d,&de,&dptr))
			break;
		if (dptr) {
			if ((strcmp(dptr->d_name,".") != 0)&&(strcmp(dptr->d_name,"..") != 0)&&((dptr->d_type != DT_DIR)||(includeDirectories)))
				r.push_back(std::string(dptr->d_name));
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
	if ((hFind = FindFirstFileA((std::string(path) + "\\*").c_str(),&ffd)) != INVALID_HANDLE_VALUE) {
		do {
			if ((strcmp(ffd.cFileName,".") != 0)&&(strcmp(ffd.cFileName,"..") != 0)) {
				if ((ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
					if (DeleteFileA((std::string(path) + ZT_PATH_SEPARATOR_S + ffd.cFileName).c_str()) == FALSE)
						return false;
				} else {
					if (!rmDashRf((std::string(path) + ZT_PATH_SEPARATOR_S + ffd.cFileName).c_str()))
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
	for(;;) {
		if (readdir_r(d,&de,&dptr) != 0)
			break;
		if (!dptr)
			break;
		if ((strcmp(dptr->d_name,".") != 0)&&(strcmp(dptr->d_name,"..") != 0)&&(strlen(dptr->d_name) > 0)) {
			std::string p(path);
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
		if (CreateProcessA(NULL,(LPSTR)(std::string("C:\\Windows\\System32\\icacls.exe \"") + path + "\" /inheritance:d /Q").c_str(),NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&startupInfo,&processInfo)) {
			WaitForSingleObject(processInfo.hProcess,INFINITE);
			CloseHandle(processInfo.hProcess);
			CloseHandle(processInfo.hThread);
		}

		startupInfo.cb = sizeof(startupInfo);
		memset(&startupInfo,0,sizeof(STARTUPINFOA));
		memset(&processInfo,0,sizeof(PROCESS_INFORMATION));
		if (CreateProcessA(NULL,(LPSTR)(std::string("C:\\Windows\\System32\\icacls.exe \"") + path + "\" /remove *S-1-5-32-545 /Q").c_str(),NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&startupInfo,&processInfo)) {
			WaitForSingleObject(processInfo.hProcess,INFINITE);
			CloseHandle(processInfo.hProcess);
			CloseHandle(processInfo.hThread);
		}
	}
#endif
#endif
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

bool OSUtils::readFile(const char *path,std::string &buf)
{
	char tmp[16384];
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

std::vector<std::string> OSUtils::split(const char *s,const char *const sep,const char *esc,const char *quot)
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
				} // else skip runs of separators
			} else buf.push_back(*s);
		}
		++s;
	}

	if (buf.size())
		fields.push_back(buf);

	return fields;
}

std::string OSUtils::platformDefaultHomePath()
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
    std::string homeDir = std::string(buf);
    homeDir.erase(std::remove(homeDir.begin(), homeDir.end(), '\n'), homeDir.end());
    return homeDir;
#endif

    // Check for user-defined environment variable before using defaults
#ifdef __WINDOWS__
	DWORD bufferSize = 65535;
	std::string userDefinedPath;
	bufferSize = GetEnvironmentVariable("ZEROTIER_HOME", &userDefinedPath[0], bufferSize);
	if (bufferSize) {
		return userDefinedPath;
	}
#else
	if(const char* userDefinedPath = getenv("ZEROTIER_HOME")) {
		return std::string(userDefinedPath);
	}
#endif

	// Finally, resort to using default paths if no user-defined path was provided
#ifdef __UNIX_LIKE__

#ifdef __APPLE__
	// /Library/... on Apple
	return std::string("/Library/Application Support/ZeroTier");
#else

#ifdef __BSD__
	// BSD likes /var/db instead of /var/lib
	return std::string("/var/db/zerotier");
#else
	// Use /var/lib for Linux and other *nix
	return std::string("/var/lib/zerotier");
#endif

#endif

#else // not __UNIX_LIKE__

#ifdef __WINDOWS__
	// Look up app data folder on Windows, e.g. C:\ProgramData\...
	char buf[16384];
	if (SUCCEEDED(SHGetFolderPathA(NULL,CSIDL_COMMON_APPDATA,NULL,0,buf)))
		return (std::string(buf) + "\\ZeroTier");
	else return std::string("C:\\ZeroTier");
#else

	return (std::string(ZT_PATH_SEPARATOR_S) + "ZeroTier"); // UNKNOWN PLATFORM

#endif

#endif // __UNIX_LIKE__ or not...
}

#ifndef OMIT_JSON_SUPPORT

// Inline these massive JSON operations in one place only to reduce binary footprint and compile time
nlohmann::json OSUtils::jsonParse(const std::string &buf) { return nlohmann::json::parse(buf.c_str()); }
std::string OSUtils::jsonDump(const nlohmann::json &j,int indentation) { return j.dump(indentation); }

uint64_t OSUtils::jsonInt(const nlohmann::json &jv,const uint64_t dfl)
{
	try {
		if (jv.is_number()) {
			return (uint64_t)jv;
		} else if (jv.is_string()) {
			std::string s = jv;
			return (uint64_t)strtoull(s.c_str(),nullptr,10);
		} else if (jv.is_boolean()) {
			return ((bool)jv ? 1ULL : 0ULL);
		}
	} catch ( ... ) {}
	return dfl;
}

uint64_t OSUtils::jsonIntHex(const nlohmann::json &jv,const uint64_t dfl)
{
	try {
		if (jv.is_number()) {
			return (uint64_t)jv;
		} else if (jv.is_string()) {
			std::string s = jv;
			return Utils::hexStrToU64(s.c_str());
		} else if (jv.is_boolean()) {
			return ((bool)jv ? 1ULL : 0ULL);
		}
	} catch ( ... ) {}
	return dfl;
}

bool OSUtils::jsonBool(const nlohmann::json &jv,const bool dfl)
{
	try {
		if (jv.is_boolean()) {
			return (bool)jv;
		} else if (jv.is_number()) {
			return ((uint64_t)jv > 0ULL);
		} else if (jv.is_string()) {
			std::string s = jv;
			if (s.length() > 0) {
				switch(s[0]) {
					case 't':
					case 'T':
					case '1':
						return true;
				}
			}
			return false;
		}
	} catch ( ... ) {}
	return dfl;
}

std::string OSUtils::jsonString(const nlohmann::json &jv,const char *dfl)
{
	try {
		if (jv.is_string()) {
			return jv;
		} else if (jv.is_number()) {
			char tmp[64];
			ztsnprintf(tmp,sizeof(tmp),"%llu",(uint64_t)jv);
			return tmp;
		} else if (jv.is_boolean()) {
			return ((bool)jv ? std::string("1") : std::string("0"));
		}
	} catch ( ... ) {}
	return std::string((dfl) ? dfl : "");
}

#endif // OMIT_JSON_SUPPORT

} // namespace ZeroTier
