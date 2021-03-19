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

/* Fix for an issue with this structure not being present on MacOS */
#ifdef __APPLE__
struct prf_ra {
	unsigned char onlink : 1;
	unsigned char autonomous : 1;
	unsigned char reserved : 6;
} prf_ra;
#endif

#include "../core/Constants.hpp"
#include "../core/Mutex.hpp"
#include "../core/Containers.hpp"
#include "../core/SHA512.hpp"
#include "../core/AES.hpp"
#include "OSUtils.hpp"

#include "rust-osdep.h"

#ifdef __APPLE__
#ifndef SIOCAUTOCONF_START
#define SIOCAUTOCONF_START _IOWR('i', 132, struct in6_ifreq)    /* accept rtadvd on this interface */
#endif
#ifndef SIOCAUTOCONF_STOP
#define SIOCAUTOCONF_STOP _IOWR('i', 133, struct in6_ifreq)    /* stop accepting rtadv for this interface */
#endif
#endif

extern "C" {

#ifdef __APPLE__
const unsigned long c_BIOCSBLEN = BIOCSBLEN;
const unsigned long c_BIOCIMMEDIATE = BIOCIMMEDIATE;
const unsigned long c_BIOCSSEESENT = BIOCSSEESENT;
const unsigned long c_BIOCSETIF = BIOCSETIF;
const unsigned long c_BIOCSHDRCMPLT = BIOCSHDRCMPLT;
const unsigned long c_BIOCPROMISC = BIOCPROMISC;
const unsigned long c_SIOCGIFINFO_IN6 = SIOCGIFINFO_IN6;
const unsigned long c_SIOCSIFINFO_FLAGS = SIOCSIFINFO_FLAGS;
const unsigned long c_SIOCAUTOCONF_START = SIOCAUTOCONF_START;
const unsigned long c_SIOCAUTOCONF_STOP = SIOCAUTOCONF_STOP;
#endif

const char *platformDefaultHomePath()
{
	static ZeroTier::Mutex s_lock;
	static ZeroTier::String s_homePath;

	ZeroTier::Mutex::Lock l(s_lock);
	if (s_homePath.empty()) {
#ifdef __QNAP__

		char *cmd = "/sbin/getcfg zerotier Install_Path -f /etc/config/qpkg.conf";
		char buf[128];
		FILE *fp;
		if ((fp = popen(cmd, "r")) == NULL) {
			printf("Error opening pipe!\n");
			return NULL;
		}
		while (fgets(buf, 128, fp) != NULL) { }
		if(pclose(fp)) {
			printf("Command not found or exited with error status\n");
			return NULL;
		}
		String homeDir = String(buf);
		homeDir.erase(std::remove(homeDir.begin(), homeDir.end(), '\n'), homeDir.end());
		s_homePath = homeDir;

#else

#ifdef __WINDOWS__

		DWORD bufferSize = 65535;
		ZeroTier::String userDefinedPath;
		bufferSize = GetEnvironmentVariable("ZEROTIER_HOME", &userDefinedPath[0], bufferSize);
		if (bufferSize) {
			s_homePath = userDefinedPath;
		} else {
			char buf[16384];
			if (SUCCEEDED(SHGetFolderPathA(NULL,CSIDL_COMMON_APPDATA,NULL,0,buf))) {
				ZeroTier::String tmp(buf);
				tmp.append("\\ZeroTier");
				s_homePath = tmp;
			} else {
				s_homePath = "C:\\ZeroTier";
			}
		}

#else

		if (const char *userDefinedPath = getenv("ZEROTIER_HOME")) {
			s_homePath = userDefinedPath;
		} else {
#ifdef __APPLE__
			s_homePath = "/Library/Application Support/ZeroTier";
#else
#ifdef __BSD__
			s_homePath = "/var/db/zerotier";
#else
			s_homePath = "/var/lib/zerotier";
#endif // __BSD__ or not
#endif // __APPLE__ or not
		}

#endif // __WINDOWS__ or not

#endif // __QNAP__ or not

		if (s_homePath.empty())
			s_homePath = "." ZT_PATH_SEPARATOR_S;
	}

	return s_homePath.c_str();
}

int64_t msSinceEpoch()
{ return ZeroTier::OSUtils::now(); }

void lockDownFile(const char *path, int isDir)
{ ZeroTier::OSUtils::lockDownFile(path, isDir != 0); }

void getSecureRandom(void *buf, unsigned int len)
{ ZeroTier::Utils::getSecureRandom(buf, len); }

void sha384(const void *in, unsigned int len, void *out)
{ ZeroTier::SHA384(out, in, len); }

void sha512(const void *in, unsigned int len, void *out)
{ ZeroTier::SHA512(out, in, len); }

static ZT_INLINE ZeroTier::AES _makeHttpAuthCipher() noexcept
{
	uint8_t key[32];
	ZeroTier::Utils::getSecureRandom(key, 32);
	return ZeroTier::AES(key);
}
static const ZeroTier::AES HTTP_AUTH_CIPHER = _makeHttpAuthCipher();

void encryptHttpAuthNonce(void *block)
{ HTTP_AUTH_CIPHER.encrypt(block, block); }

void decryptHttpAuthNonce(void *block)
{ HTTP_AUTH_CIPHER.decrypt(block, block); }

} /* extern "C" */
