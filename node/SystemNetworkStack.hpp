/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2011-2014  ZeroTier Networks LLC
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
 *
 * --
 *
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

#ifndef ZT_SYSTEMNETWORKSTACK_HPP
#define ZT_SYSTEMNETWORKSTACK_HPP

#include <stdint.h>

#include <vector>
#include <string>
#include <set>

#include "InetAddress.hpp"
#include "NonCopyable.hpp"

namespace ZeroTier {

/**
 * Base class for OS-dependent interfaces to the system network configuration
 */
class SystemNetworkStack : NonCopyable
{
public:
  struct RoutingTableEntry
  {
    char device[128];
    InetAddress destination;
    InetAddress gateway;
    unsigned int deviceMetric;
  };

  SystemNetworkStack() {}
  virtual ~SystemNetworkStack() {}

  /**
   * @return All routing table entries sorted in order of destination
   */
  virtual std::vector<RoutingTableEntry> routingTable() const = 0;

  /**
   * Add or update a routing table entry
   *
   * Note that metrics may only be changed at the device level,
   * so changes to deviceMetric are ignored.
   *
   * @param re Entry to add/update
   * @return True if successful
   */
  virtual bool addUpdateRoute(const RoutingTableEntry &re) = 0;

  /**
   * @param ifname Name of interface (Unix-style device or Windows device name)
   * @return Interface metric (higher = lower priority)
   */
  virtual unsigned int interfaceMetric(const char *ifname) const = 0;

  /**
   * @param ifname Name of interface (Unix-style device or Windows device name)
   * @param metric New metric (higher = lower priority)
   * @return True if successful
   */
  virtual bool setInterfaceMetric(const char *ifname,unsigned int metric) = 0;

  /**
   * @return Interface names sorted in ascending order
   */
  virtual std::vector<std::string> interfaces() const = 0;

  /**
   * @param ignoreInterfaces List of interfaces to exclude from fingerprint
   * @return Integer CRC-type fingerprint of current network environment
   */
  inline uint64_t networkEnvironmentFingerprint(const std::set<std::string> &ignoreInterfaces) const
  {
    std::vector<RoutingTableEntry> rtab(routingTable());
  };
};

} // namespace ZeroTier

#endif
