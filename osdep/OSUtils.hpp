/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_OSUTILS_HPP
#define ZT_OSUTILS_HPP

#include "../core/Constants.hpp"
#include "../core/Containers.hpp"

#include <stdarg.h>
#include <time.h>

#ifndef __WINDOWS__
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#endif

#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/clock.h>
#include <mach/mach_time.h>
#endif

namespace ZeroTier {

/**
 * Miscellaneous utility functions and global constants
 */
class OSUtils
{
private:
#ifdef __APPLE__
	static clock_serv_t s_machRealtimeClock;
#endif

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
	static bool redirectUnixOutputs(const char *stdoutPath,const char *stderrPath = nullptr);
#endif // __UNIX_LIKE__

	/**
	 * Delete a file
	 *
	 * @param path Path to delete
	 * @return True if delete was successful
	 */
	static ZT_INLINE bool rm(const char *path)
	{
#ifdef __WINDOWS__
		return (DeleteFileA(path) != FALSE);
#else
		return (unlink(path) == 0);
#endif
	}

	static ZT_INLINE bool mkdir(const char *path)
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

	static ZT_INLINE bool rename(const char *o,const char *n)
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
	static ZeroTier::Vector<ZeroTier::String> listDirectory(const char *path,bool includeDirectories = false);

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
	 * @param path Path to check
	 * @param followLinks Follow links (on platforms with that concept)
	 * @return True if file or directory exists at path location
	 */
	static bool fileExists(const char *path,bool followLinks = true);

	/**
	 * @return Current time in milliseconds since epoch
	 */
	static ZT_INLINE int64_t now()
	{
#ifdef __WINDOWS__
		FILETIME ft;
		GetSystemTimeAsFileTime(&ft);
		return (((LONGLONG)ft.dwLowDateTime + ((LONGLONG)(ft.dwHighDateTime) << 32)) / 10000LL) - 116444736000000000LL;
#else
#ifdef __LINUX__
		timespec ts;
#ifdef CLOCK_REALTIME_COARSE
		clock_gettime(CLOCK_REALTIME_COARSE,&ts);
#else
		clock_gettime(CLOCK_REALTIME,&ts);
#endif
		return ( (1000LL * (int64_t)ts.tv_sec) + ((int64_t)(ts.tv_nsec / 1000000)) );
#else
#ifdef __APPLE__
		mach_timespec_t mts;
		clock_get_time(s_machRealtimeClock,&mts);
		return ( (1000LL * (int64_t)mts.tv_sec) + ((int64_t)(mts.tv_nsec / 1000000)) );
#else
		timeval tv;
		gettimeofday(&tv,(struct timezone *)0);
		return ( (1000LL * (int64_t)tv.tv_sec) + (int64_t)(tv.tv_usec / 1000) );
#endif
#endif
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
	static bool readFile(const char *path,ZeroTier::String &buf);

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
	static ZeroTier::Vector<ZeroTier::String> split(const char *s,const char *sep,const char *esc,const char *quot);

	/**
	 * Write a block of data to disk, replacing any current file contents
	 *
	 * @param path Path to write
	 * @param s Data to write
	 * @return True if entire file was successfully written
	 */
	static ZT_INLINE bool writeFile(const char *path,const ZeroTier::String &s) { return writeFile(path,s.data(),(unsigned int)s.length()); }

	/**
	 * @return Platform default ZeroTier One home path
	 */
	static ZeroTier::String platformDefaultHomePath();
};

} // namespace ZeroTier

#endif
