/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c) ZeroTier, Inc.
 * https://www.zerotier.com/
 */

#ifndef LINUX_DNS_HELPER_H_
#define LINUX_DNS_HELPER_H_

#include "../node/InetAddress.hpp"

#include <string>
#include <vector>

namespace ZeroTier {

class LinuxDNSHelper {
  public:
	static void setDNS(const char* interfaceName, const char* domain, const std::vector<InetAddress>& servers);
	static void removeDNS(const char* interfaceName);

  private:
	static bool isSystemdResolved();
	static int runResolvectl(const std::vector<std::string>& args);
};

}	// namespace ZeroTier

#endif
