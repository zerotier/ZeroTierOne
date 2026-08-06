/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c) ZeroTier, Inc.
 * https://www.zerotier.com/
 */

#ifdef __linux__

#include "LinuxDNSHelper.hpp"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#include <linux/capability.h>

// Minimal PATH for subprocess execution, covering usr-merged and non-merged distros.
#define ZT_SUBPROCESS_SAFE_PATH "/usr/bin:/bin:/usr/sbin:/sbin"

namespace ZeroTier {

namespace {

// Same struct layout as one.cpp — avoids pulling in libcap
struct _zt_cap_header_struct {
	__u32 version;
	int pid;
};
struct _zt_cap_data_struct {
	__u32 effective;
	__u32 permitted;
	__u32 inheritable;
};

static void _dropAllCapabilities()
{
	::prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_CLEAR_ALL, 0, 0, 0);
	_zt_cap_header_struct hdr = { _LINUX_CAPABILITY_VERSION_1, 0 };
	_zt_cap_data_struct data = { 0, 0, 0 };
	::syscall(SYS_capset, &hdr, &data);
}

static void _closeExtraFDs(int minFd)
{
	// close_range() syscall directly for portability with older glibc.
#ifdef SYS_close_range
	if (::syscall(SYS_close_range, (unsigned int)minFd, ~0U, 0) == 0)
		return;
#endif
	// Fallback: iterate /proc/self/fd (always available on systemd systems)
	DIR* d = ::opendir("/proc/self/fd");
	if (!d)
		return;
	int dirfd_val = ::dirfd(d);
	struct dirent* de;
	while ((de = ::readdir(d)) != nullptr) {
		if (de->d_name[0] == '.')
			continue;
		int fd = ::atoi(de->d_name);
		if (fd >= minFd && fd != dirfd_val)
			::close(fd);
	}
	::closedir(d);
}

}	// anonymous namespace

bool LinuxDNSHelper::isSystemdResolved()
{
	struct stat sb;
	return (::stat("/run/systemd/resolve/stub-resolv.conf", &sb) == 0);
}

int LinuxDNSHelper::runResolvectl(const std::vector<std::string>& args)
{
	long p = (long)::fork();
	if (p > 0) {
		int exitcode = -1;
		::waitpid(p, &exitcode, 0);
		return WIFEXITED(exitcode) ? WEXITSTATUS(exitcode) : -1;
	}
	else if (p == 0) {
		_dropAllCapabilities();
		_closeExtraFDs(STDOUT_FILENO);

		// resolvectl only needs D-Bus access to systemd-resolved; a minimal
		// environment with a safe PATH is sufficient.
		::clearenv();
		::setenv("PATH", ZT_SUBPROCESS_SAFE_PATH, 1);

		std::vector<const char*> argv;
		argv.push_back("resolvectl");
		for (size_t i = 0; i < args.size(); ++i) {
			argv.push_back(args[i].c_str());
		}
		argv.push_back(nullptr);
		::execvp("resolvectl", const_cast<char* const*>(argv.data()));
		::_exit(-1);
	}
	return -1;
}

void LinuxDNSHelper::setDNS(const char* interfaceName, const char* domain, const std::vector<InetAddress>& servers)
{
	if (! isSystemdResolved()) {
		fprintf(
			stderr,
			"WARNING: systemd-resolved not detected. DNS configuration for ZeroTier networks requires systemd-resolved or manual configuration. "
			"See https://github.com/zerotier/ZeroTierOne/issues/2492 for details" ZT_EOL_S);
		return;
	}

	if (! interfaceName || ! interfaceName[0] || servers.empty()) {
		return;
	}

	// resolvectl dns <interface> <ip1> <ip2> ...
	{
		std::vector<std::string> args;
		args.push_back("dns");
		args.push_back(interfaceName);
		for (size_t i = 0; i < servers.size(); ++i) {
			char buf[64];
			args.push_back(servers[i].toIpString(buf));
		}
		int rc = runResolvectl(args);
		if (rc != 0) {
			fprintf(stderr, "WARNING: resolvectl dns failed (exit %d) for interface %s" ZT_EOL_S, rc, interfaceName);
			return;
		}
	}

	// resolvectl domain <interface> ~<domain>
	if (domain && domain[0]) {
		std::vector<std::string> args;
		args.push_back("domain");
		args.push_back(interfaceName);
		args.push_back(std::string("~") + domain);
		int rc = runResolvectl(args);
		if (rc != 0) {
			fprintf(stderr, "WARNING: resolvectl domain failed (exit %d) for interface %s" ZT_EOL_S, rc, interfaceName);
		}
	}
}

void LinuxDNSHelper::removeDNS(const char* interfaceName)
{
	if (! isSystemdResolved()) {
		return;
	}

	if (! interfaceName || ! interfaceName[0]) {
		return;
	}

	std::vector<std::string> args;
	args.push_back("revert");
	args.push_back(interfaceName);
	int rc = runResolvectl(args);
	if (rc != 0) {
		fprintf(stderr, "WARNING: resolvectl revert failed (exit %d) for interface %s" ZT_EOL_S, rc, interfaceName);
	}
}

}	// namespace ZeroTier

#endif
