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

#ifndef FIREWALL_MANAGER_FACTORY_HPP
#define FIREWALL_MANAGER_FACTORY_HPP

#include <string>
#include <memory>
#include "CloudFirewallManager.hpp"
#include "LinodeFirewallManager.hpp"

/**
 * FirewallManagerFactory
 * 
 * Factory class for creating cloud provider-specific firewall managers.
 * This allows the TCP proxy to work with different cloud providers
 * without changing the core logic.
 */
class FirewallManagerFactory {
public:
    /**
     * Create a firewall manager for the specified cloud provider
     * 
     * @param provider The cloud provider name (e.g., "linode")
     * @param apiToken The API token for the cloud provider
     * @param firewallId The ID of the firewall to manage
     * @param internalTcpPort The internal TCP port the proxy is listening on
     * @param externalTcpPort The external TCP port to expose in the firewall (defaults to same as internal)
     * @param additionalParams JSON string with additional provider-specific parameters
     * @return A unique pointer to a CloudFirewallManager instance, or nullptr if provider is not supported
     */
    static std::unique_ptr<CloudFirewallManager> createFirewallManager(
        const std::string& provider,
        const std::string& apiToken,
        const std::string& firewallId,
        uint16_t internalTcpPort = 443,
        uint16_t externalTcpPort = 0,
        const std::string& additionalParams = "") {
        
        // If external port is not specified, use the same as internal port
        if (externalTcpPort == 0) {
            externalTcpPort = internalTcpPort;
        }
        
        if (provider == "linode") {
            // C++11 compatible way to create a unique_ptr
            return std::unique_ptr<CloudFirewallManager>(new LinodeFirewallManager(apiToken, firewallId, internalTcpPort, externalTcpPort));
        }
        
        // Add support for other cloud providers here
        // Example:
        // if (provider == "aws") {
        //     return std::make_unique<AWSFirewallManager>(apiToken, firewallId, internalTcpPort, externalTcpPort, additionalParams);
        // }
        
        return nullptr; // Provider not supported
    }
};

#endif // FIREWALL_MANAGER_FACTORY_HPP