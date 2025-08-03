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

#ifndef CLOUD_FIREWALL_MANAGER_HPP
#define CLOUD_FIREWALL_MANAGER_HPP

#include <string>
#include <set>
#include <cstdint>

/**
 * CloudFirewallManager
 * 
 * Abstract base class for cloud firewall management.
 * Provides a common interface for different cloud provider implementations.
 * 
 * This class defines the interface that all cloud firewall manager implementations
 * must follow, allowing the TCP proxy to work with different cloud providers
 * without changing the core logic.
 */
class CloudFirewallManager {
public:
    /**
     * Initialize the firewall manager
     * Fetches current rules and prepares for updates
     * 
     * @return true if initialization was successful, false otherwise
     */
    virtual bool initialize() = 0;
    
    /**
     * Add a UDP port to the firewall rules
     * 
     * @param port UDP port to add
     * @return true if successful, false otherwise
     */
    virtual bool addPort(uint16_t port) = 0;
    
    /**
     * Remove a UDP port from the firewall rules
     * 
     * @param port UDP port to remove
     * @return true if successful, false otherwise
     */
    virtual bool removePort(uint16_t port) = 0;
    
    /**
     * Force synchronization of firewall rules with the cloud provider
     * 
     * @return true if successful, false otherwise
     */
    virtual bool syncFirewallRules() = 0;
    
    /**
     * Virtual destructor for proper cleanup in derived classes
     */
    virtual ~CloudFirewallManager() {}
};

#endif // CLOUD_FIREWALL_MANAGER_HPP