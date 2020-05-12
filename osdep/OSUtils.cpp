/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2023-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <stdlib.h>

#include "../node/Constants.hpp"
#include "../node/Utils.hpp"

#ifdef __UNIX_LIKE__
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <dirent.h>
#include <netdb.h>
#endif

#ifdef __WINDOWS__
#include <windows.h>
#include <wincrypt.h>
#include <ShlObj.h>
#include <netioapi.h>
#include <iphlpapi.h>
#endif

#include "OSUtils.hpp"

namespace ZeroTier {

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
			if ((strcmp(dptr->d_name,"."))&&(strcmp(dptr->d_name,".."))&&((dptr->d_type != DT_DIR)||(includeDirectories)))
				r.push_back(std::string(dptr->d_name));
		} else break;
	}
	closedir(d);
#endif

	return r;
}

long OSUtils::cleanDirectory(const char *path,const int64_t olderThan)
{
	long cleaned = 0;

#ifdef __WINDOWS__
	HANDLE hFind;
	WIN32_FIND_DATAA ffd;
	LARGE_INTEGER date,adjust;
	adjust.QuadPart = 11644473600000 * 10000;
	char tmp[4096];
	if ((hFind = FindFirstFileA((std::string(path) + "\\*").c_str(),&ffd)) != INVALID_HANDLE_VALUE) {
		do {
			if ((strcmp(ffd.cFileName,"."))&&(strcmp(ffd.cFileName,".."))&&((ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)) {
					date.HighPart = ffd.ftLastWriteTime.dwHighDateTime;
					date.LowPart = ffd.ftLastWriteTime.dwLowDateTime;
					if (date.QuadPart > 0) {
							date.QuadPart -= adjust.QuadPart;
							if ((int64_t)((date.QuadPart / 10000000) * 1000) < olderThan) {
									ztsnprintf(tmp, sizeof(tmp), "%s\\%s", path, ffd.cFileName);
									if (DeleteFileA(tmp))
											++cleaned;
							}
					}
			}
		} while (FindNextFileA(hFind,&ffd));
		FindClose(hFind);
	}
#else
	struct dirent de;
	struct dirent *dptr;
	struct stat st;
	char tmp[4096];
	DIR *d = opendir(path);
	if (!d)
		return -1;
	dptr = (struct dirent *)0;
	for(;;) {
		if (readdir_r(d,&de,&dptr))
			break;
		if (dptr) {
			if ((strcmp(dptr->d_name,"."))&&(strcmp(dptr->d_name,".."))&&(dptr->d_type == DT_REG)) {
				ztsnprintf(tmp,sizeof(tmp),"%s/%s",path,dptr->d_name);
				if (stat(tmp,&st) == 0) {
					int64_t mt = (int64_t)(st.st_mtime);
					if ((mt > 0)&&((mt * 1000) < olderThan)) {
						if (unlink(tmp) == 0)
							++cleaned;
					}
				}
			}
		} else break;
	}
	closedir(d);
#endif

	return cleaned;
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
	struct dirent de;
	struct dirent *dptr;
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
#ifdef __UBIQUITI__
	// Only persistent location after firmware upgrades
	return std::string("/config/zerotier-one");
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
	return std::string("/Library/Application Support/ZeroTier/One");
#else

#ifdef __BSD__
	// BSD likes /var/db instead of /var/lib
	return std::string("/var/db/zerotier-one");
#else
	// Use /var/lib for Linux and other *nix
	return std::string("/var/lib/zerotier-one");
#endif

#endif

#else // not __UNIX_LIKE__

#ifdef __WINDOWS__
	// Look up app data folder on Windows, e.g. C:\ProgramData\...
	char buf[16384];
	if (SUCCEEDED(SHGetFolderPathA(NULL,CSIDL_COMMON_APPDATA,NULL,0,buf)))
		return (std::string(buf) + "\\ZeroTier\\One");
	else return std::string("C:\\ZeroTier\\One");
#else

	return (std::string(ZT_PATH_SEPARATOR_S) + "ZeroTier" + ZT_PATH_SEPARATOR_S + "One"); // UNKNOWN PLATFORM

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
			return Utils::strToU64(s.c_str());
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

std::string OSUtils::jsonBinFromHex(const nlohmann::json &jv)
{
	std::string s(jsonString(jv,""));
	if (s.length() > 0) {
		unsigned int buflen = (unsigned int)((s.length() / 2) + 1);
		char *buf = new char[buflen];
		try {
			unsigned int l = Utils::unhex(s.c_str(),buf,buflen);
			std::string b(buf,l);
			delete [] buf;
			return b;
		} catch ( ... ) {
			delete [] buf;
		}
	}
	return std::string();
}

#endif // OMIT_JSON_SUPPORT

// Used to convert HTTP header names to ASCII lower case
const unsigned char OSUtils::TOLOWER_TABLE[256] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, ' ', '!', '"', '#', '$', '%', '&', 0x27, '(', ')', '*', '+', ',', '-', '.', '/', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=', '>', '?', '@', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '{', '|', '}', '~', '_', '`', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '{', '|', '}', '~', 0x7f, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, 0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff };

} // namespace ZeroTier
