/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c) ZeroTier, Inc.
 * https://www.zerotier.com/
 */

#ifdef __linux__

#include "LinuxDNSHelper.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace ZeroTier {

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
		::close(STDOUT_FILENO);
		::close(STDERR_FILENO);
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
