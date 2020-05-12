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

#ifndef ZT_OSUTILS_HPP
#define ZT_OSUTILS_HPP

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include <stdexcept>
#include <vector>
#include <map>

#include "../node/Constants.hpp"
#include "../node/InetAddress.hpp"

#ifdef __WINDOWS__
#include <WinSock2.h>
#include <Windows.h>
#include <Shlwapi.h>
#else
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#ifdef __LINUX__
#include <sys/syscall.h>
#endif
#endif

#ifndef OMIT_JSON_SUPPORT
#include "../ext/json/json.hpp"
#endif

namespace ZeroTier {

/**
 * Miscellaneous utility functions and global constants
 */
class OSUtils
{
public:
	/**
	 * Variant of snprintf that is portable and throws an exception
	 *
	 * This just wraps the local implementation whatever it's called, while
	 * performing a few other checks and adding exceptions for overflow.
	 *
	 * @param buf Buffer to write to
	 * @param len Length of buffer in bytes
	 * @param fmt Format string
	 * @param ... Format arguments
	 * @throws std::length_error buf[] too short (buf[] will still be left null-terminated)
	 */
	static unsigned int ztsnprintf(char *buf,unsigned int len,const char *fmt,...);

#ifdef __UNIX_LIKE__
	/**
	 * Close STDOUT_FILENO and STDERR_FILENO and replace them with output to given path
	 *
	 * This can be called after fork() and prior to exec() to suppress output
	 * from a subprocess, such as auto-update.
	 *
	 * @param stdoutPath Path to file to use for stdout
	 * @param stderrPath Path to file to use for stderr, or NULL for same as stdout (default)
	 * @return True on success
	 */
	static bool redirectUnixOutputs(const char *stdoutPath,const char *stderrPath = (const char *)0)
		throw();
#endif // __UNIX_LIKE__

	/**
	 * Delete a file
	 *
	 * @param path Path to delete
	 * @return True if delete was successful
	 */
	static inline bool rm(const char *path)
	{
#ifdef __WINDOWS__
		return (DeleteFileA(path) != FALSE);
#else
		return (unlink(path) == 0);
#endif
	}
	static inline bool rm(const std::string &path) { return rm(path.c_str()); }

	static inline bool mkdir(const char *path)
	{
#ifdef __WINDOWS__
		if (::PathIsDirectoryA(path))
			return true;
		return (::CreateDirectoryA(path,NULL) == TRUE);
#else
		if (::mkdir(path,0755) != 0)
			return (errno == EEXIST);
		return true;
#endif
	}
	static inline bool mkdir(const std::string &path) { return OSUtils::mkdir(path.c_str()); }

	static inline bool rename(const char *o,const char *n)
	{
#ifdef __WINDOWS__
		DeleteFileA(n);
		return (::rename(o,n) == 0);
#else
		return (::rename(o,n) == 0);
#endif
	}

	/**
	 * List a directory's contents
	 *
	 * @param path Path to list
	 * @param includeDirectories If true, include directories as well as files
	 * @return Names of files in directory (without path prepended)
	 */
	static std::vector<std::string> listDirectory(const char *path,bool includeDirectories = false);

	/**
	 * Clean a directory of files whose last modified time is older than this
	 *
	 * This ignores directories, symbolic links, and other special files.
	 *
	 * @param olderThan Last modified older than timestamp (ms since epoch)
	 * @return Number of cleaned files or negative on fatal error
	 */
	static long cleanDirectory(const char *path,const int64_t olderThan);

	/**
	 * Delete a directory and all its files and subdirectories recursively
	 *
	 * @param path Path to delete
	 * @return True on success
	 */
	static bool rmDashRf(const char *path);

	/**
	 * Set modes on a file to something secure
	 *
	 * This locks a file so that only the owner can access it. What it actually
	 * does varies by platform.
	 *
	 * @param path Path to lock
	 * @param isDir True if this is a directory
	 */
	static void lockDownFile(const char *path,bool isDir);

	/**
	 * Get file last modification time
	 *
	 * Resolution is often only second, not millisecond, but the return is
	 * always in ms for comparison against now().
	 *
	 * @param path Path to file to get time
	 * @return Last modification time in ms since epoch or 0 if not found
	 */
	static uint64_t getLastModified(const char *path);

	/**
	 * @param path Path to check
	 * @param followLinks Follow links (on platforms with that concept)
	 * @return True if file or directory exists at path location
	 */
	static bool fileExists(const char *path,bool followLinks = true);

	/**
	 * @param path Path to file
	 * @return File size or -1 if nonexistent or other failure
	 */
	static int64_t getFileSize(const char *path);

	/**
	 * Get IP (v4 and/or v6) addresses for a given host
	 *
	 * This is a blocking resolver.
	 *
	 * @param name Host name
	 * @return IP addresses in InetAddress sort order or empty vector if not found
	 */
	static std::vector<InetAddress> resolve(const char *name);

	/**
	 * @return Current time in milliseconds since epoch
	 */
	static inline int64_t now()
	{
#ifdef __WINDOWS__
		FILETIME ft;
		SYSTEMTIME st;
		ULARGE_INTEGER tmp;
		GetSystemTime(&st);
		SystemTimeToFileTime(&st,&ft);
		tmp.LowPart = ft.dwLowDateTime;
		tmp.HighPart = ft.dwHighDateTime;
		return (int64_t)( ((tmp.QuadPart - 116444736000000000LL) / 10000L) + st.wMilliseconds );
#else
		struct timeval tv;
#ifdef __LINUX__
		syscall(SYS_gettimeofday,&tv,0); /* fix for musl libc broken gettimeofday bug */
#else
		gettimeofday(&tv,(struct timezone *)0);
#endif
		return ( (1000LL * (int64_t)tv.tv_sec) + (int64_t)(tv.tv_usec / 1000) );
#endif
	};

	/**
	 * Read the full contents of a file into a string buffer
	 *
	 * The buffer isn't cleared, so if it already contains data the file's data will
	 * be appended.
	 *
	 * @param path Path of file to read
	 * @param buf Buffer to fill
	 * @return True if open and read successful
	 */
	static bool readFile(const char *path,std::string &buf);

	/**
	 * Write a block of data to disk, replacing any current file contents
	 *
	 * @param path Path to write
	 * @param buf Buffer containing data
	 * @param len Length of buffer
	 * @return True if entire file was successfully written
	 */
	static bool writeFile(const char *path,const void *buf,unsigned int len);

	/**
	 * Split a string by delimiter, with optional escape and quote characters
	 *
	 * @param s String to split
	 * @param sep One or more separators
	 * @param esc Zero or more escape characters
	 * @param quot Zero or more quote characters
	 * @return Vector of tokens
	 */
	static std::vector<std::string> split(const char *s,const char *const sep,const char *esc,const char *quot);

	/**
	 * Write a block of data to disk, replacing any current file contents
	 *
	 * @param path Path to write
	 * @param s Data to write
	 * @return True if entire file was successfully written
	 */
	static inline bool writeFile(const char *path,const std::string &s) { return writeFile(path,s.data(),(unsigned int)s.length()); }

	/**
	 * @param c ASCII character to convert
	 * @return Lower case ASCII character or unchanged if not a letter
	 */
	static inline char toLower(char c) throw() { return (char)OSUtils::TOLOWER_TABLE[(unsigned long)c]; }

	/**
	 * @return Platform default ZeroTier One home path
	 */
	static std::string platformDefaultHomePath();

#ifndef OMIT_JSON_SUPPORT
	static nlohmann::json jsonParse(const std::string &buf);
	static std::string jsonDump(const nlohmann::json &j,int indentation = 1);
	static uint64_t jsonInt(const nlohmann::json &jv,const uint64_t dfl);
	static uint64_t jsonIntHex(const nlohmann::json &jv,const uint64_t dfl);
	static bool jsonBool(const nlohmann::json &jv,const bool dfl);
	static std::string jsonString(const nlohmann::json &jv,const char *dfl);
	static std::string jsonBinFromHex(const nlohmann::json &jv);
#endif // OMIT_JSON_SUPPORT

private:
	static const unsigned char TOLOWER_TABLE[256];
};

} // namespace ZeroTier

#endif
