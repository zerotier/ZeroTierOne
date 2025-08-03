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

#ifndef LINODE_FIREWALL_MANAGER_HPP
#define LINODE_FIREWALL_MANAGER_HPP

#include <string>
#include <set>
#include <mutex>
#include <curl/curl.h>
#include "../../../ext/nlohmann/json.hpp"
#include "CloudFirewallManager.hpp"

using json = nlohmann::json;

/**
 * LinodeFirewallManager
 * 
 * This class manages Linode Cloud Firewall rules via the Linode API.
 * It allows dynamic addition and removal of UDP ports to the firewall rules
 * as clients connect and disconnect from the ZeroTier TCP Proxy.
 * 
 * IMPORTANT: This class only modifies dedicated "ZeroTier-UDP-Ports" rules
 * and carefully preserves all other existing firewall rules, including any
 * pre-existing UDP rules for other services. It will never modify or remove
 * UDP rules that don't have the specific "ZeroTier-UDP-Ports" label.
 * 
 * To comply with Linode's 15-piece limit per rule, this class automatically
 * splits ports across multiple rules ("ZeroTier-UDP-Ports", "ZeroTier-UDP-Ports-2", etc.)
 * when necessary. A single port counts as 1 piece, and a port range counts as 2 pieces.
 */
class LinodeFirewallManager : public CloudFirewallManager {
private:
    std::string apiToken;        // Linode API token
    std::string firewallId;      // ID of the Linode Firewall to manage
    std::set<uint16_t> activePorts; // Currently active UDP ports
    std::string fingerprint;     // Current firewall rules fingerprint for updates
    bool initialized;            // Whether initialization was successful
    std::mutex apiMutex;         // Mutex for thread safety during API operations
    json originalRules;          // Original firewall rules structure to preserve non-ZeroTier rules
    uint16_t internalTcpPort;    // Internal TCP port for ZeroTier TCP Proxy (default: 443)
    uint16_t externalTcpPort;    // External TCP port exposed in firewall (default: same as internal)
    
    // Constants for Linode API limitations
    static const int MAX_PORT_PIECES_PER_RULE = 15;  // Maximum number of port pieces per rule
    static const int MAX_RULES = 25;                 // Maximum number of rules per firewall
    
    // Curl helpers
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* s);
    bool performGetRequest(const std::string& url, std::string& response);
    bool performPutRequest(const std::string& url, const std::string& data, std::string& response);
    
    // Rule management
    bool fetchCurrentRules();
    bool updateRules();
    std::string buildRulesJson();
    
    // Parse existing rules to extract UDP ports
    bool parseExistingPorts(const json& rules);
    
    // Convert port set to string format for API
    std::string portsToString(const std::set<uint16_t>& ports);
    
    // Count the number of pieces in a port set (single port = 1 piece, range = 2 pieces)
    int countPortPieces(const std::vector<std::pair<uint16_t, uint16_t>>& ranges);
    
    // Split ports into multiple groups to stay within the piece limit
    std::vector<std::set<uint16_t>> splitPortsIntoGroups(const std::set<uint16_t>& ports);
    
    // Convert ports to ranges for optimization
    std::vector<std::pair<uint16_t, uint16_t>> portsToRanges(const std::set<uint16_t>& ports);
    
public:
    LinodeFirewallManager(const std::string& token, const std::string& id, uint16_t internalPort = 443, uint16_t externalPort = 0);
    ~LinodeFirewallManager();
    
    /**
     * Initialize the firewall manager
     * Fetches current rules and prepares for updates
     * 
     * During initialization, all existing firewall rules are preserved.
     * The manager will identify any existing "ZeroTier-UDP-Ports" rule
     * and will only modify that specific rule in future operations.
     * All other UDP rules will remain untouched.
     * 
     * @return true if initialization was successful, false otherwise
     */
    bool initialize();
    
    /**
     * Add a UDP port to the ZeroTier-UDP-Ports firewall rules
     * 
     * This only modifies the dedicated ZeroTier rules and preserves all other UDP rules.
     * If necessary, ports will be split across multiple rules to comply with Linode's
     * 15-piece limit per rule.
     * 
     * @param port UDP port to add
     * @return true if successful, false otherwise
     */
    bool addPort(uint16_t port);
    
    /**
     * Remove a UDP port from the ZeroTier-UDP-Ports firewall rules
     * 
     * This only modifies the dedicated ZeroTier rules and preserves all other UDP rules.
     * After removal, the rules may be reorganized to optimize port groupings.
     * 
     * @param port UDP port to remove
     * @return true if successful, false otherwise
     */
    bool removePort(uint16_t port);
    
    /**
     * Force synchronization of firewall rules with Linode
     * 
     * This preserves all existing non-ZeroTier UDP rules and only updates
     * the dedicated ZeroTier-UDP-Ports rules. If necessary, ports will be split
     * across multiple rules ("ZeroTier-UDP-Ports", "ZeroTier-UDP-Ports-2", etc.)
     * to comply with Linode's 15-piece limit per rule.
     * 
     * @return true if successful, false otherwise
     */
    bool syncFirewallRules();
};

#endif // LINODE_FIREWALL_MANAGER_HPP