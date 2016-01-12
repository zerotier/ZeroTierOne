/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2016  ZeroTier, Inc.  https://www.zerotier.com/
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
 */

#ifndef ZT_OSUTILS_HPP
#define ZT_OSUTILS_HPP

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include <string>
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
#endif

namespace ZeroTier {

/**
 * Miscellaneous utility functions and global constants
 */
class OSUtils
{
public:
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
		throw()
	{
#ifdef __WINDOWS__
		return (DeleteFileA(path) != FALSE);
#else
		return (unlink(path) == 0);
#endif
	}
	static inline bool rm(const std::string &path) throw() { return rm(path.c_str()); }

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
	static inline bool mkdir(const std::string &path) throw() { return OSUtils::mkdir(path.c_str()); }

	/**
	 * List a directory's contents
	 *
	 * This returns only files, not sub-directories.
	 *
	 * @param path Path to list
	 * @return Names of files in directory
	 */
	static std::vector<std::string> listDirectory(const char *path);

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
	static inline uint64_t now()
		throw()
	{
#ifdef __WINDOWS__
		FILETIME ft;
		SYSTEMTIME st;
		ULARGE_INTEGER tmp;
		GetSystemTime(&st);
		SystemTimeToFileTime(&st,&ft);
		tmp.LowPart = ft.dwLowDateTime;
		tmp.HighPart = ft.dwHighDateTime;
		return ( ((tmp.QuadPart - 116444736000000000ULL) / 10000L) + st.wMilliseconds );
#else
		struct timeval tv;
		gettimeofday(&tv,(struct timezone *)0);
		return ( (1000ULL * (uint64_t)tv.tv_sec) + (uint64_t)(tv.tv_usec / 1000) );
#endif
	};

	/**
	 * @return Current time in seconds since epoch, to the highest available resolution
	 */
	static inline double nowf()
		throw()
	{
#ifdef __WINDOWS__
		FILETIME ft;
		SYSTEMTIME st;
		ULARGE_INTEGER tmp;
		GetSystemTime(&st);
		SystemTimeToFileTime(&st,&ft);
		tmp.LowPart = ft.dwLowDateTime;
		tmp.HighPart = ft.dwHighDateTime;
		return (((double)(tmp.QuadPart - 116444736000000000ULL)) / 10000000.0);
#else
		struct timeval tv;
		gettimeofday(&tv,(struct timezone *)0);
		return ( ((double)tv.tv_sec) + (((double)tv.tv_usec) / 1000000.0) );
#endif
	}

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

private:
	static const unsigned char TOLOWER_TABLE[256];
};

} // namespace ZeroTier

#endif
