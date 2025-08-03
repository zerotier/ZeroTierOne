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

#include "LinodeFirewallManager.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstring>

// External function for logging with timestamp
extern void datetime_fprintf(FILE *stream, const char *fmt, ...) __attribute__((format(printf,2,3)));
#define datetime_printf(...) datetime_fprintf(stderr, __VA_ARGS__)

// Constructor
LinodeFirewallManager::LinodeFirewallManager(const std::string& token, const std::string& id, uint16_t internalPort, uint16_t externalPort)
    : apiToken(token),
      firewallId(id),
      initialized(false),
      internalTcpPort(internalPort),
      externalTcpPort(externalPort) {
    // If external port is not specified, use the same as internal port
    if (externalTcpPort == 0) {
        externalTcpPort = internalTcpPort;
    }
    // Initialize curl globally (should be called once per application)
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

// Destructor
LinodeFirewallManager::~LinodeFirewallManager() {
    // Clean up curl
    curl_global_cleanup();
}

// Static callback function for curl to write response data
size_t LinodeFirewallManager::WriteCallback(void* contents, size_t size, size_t nmemb, std::string* s) {
    size_t newLength = size * nmemb;
    try {
        s->append((char*)contents, newLength);
        return newLength;
    } catch(std::bad_alloc& e) {
        // Handle memory problem
        return 0;
    }
}

// Perform GET request to Linode API
bool LinodeFirewallManager::performGetRequest(const std::string& url, std::string& response) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        datetime_printf("Failed to initialize curl for GET request\n");
        return false;
    }

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, ("Authorization: Bearer " + apiToken).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L); // 10 seconds timeout

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        datetime_printf("GET request failed: %s\n", curl_easy_strerror(res));
        return false;
    }

    if (http_code < 200 || http_code >= 300) {
        datetime_printf("GET request returned HTTP error: %ld\n", http_code);
        datetime_printf("Response: %s\n", response.c_str());
        return false;
    }

    return true;
}

// Perform PUT request to Linode API
bool LinodeFirewallManager::performPutRequest(const std::string& url, const std::string& data, std::string& response) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        datetime_printf("Failed to initialize curl for PUT request\n");
        return false;
    }

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, ("Authorization: Bearer " + apiToken).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L); // 10 seconds timeout

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        datetime_printf("PUT request failed: %s\n", curl_easy_strerror(res));
        return false;
    }

    if (http_code < 200 || http_code >= 300) {
        datetime_printf("PUT request returned HTTP error: %ld\n", http_code);
        datetime_printf("Response: %s\n", response.c_str());
        return false;
    }

    return true;
}

// Initialize the firewall manager
bool LinodeFirewallManager::initialize() {
    std::lock_guard<std::mutex> lock(apiMutex);
    
    datetime_printf("Starting Linode Firewall Manager initialization\n");
    
    if (apiToken.empty()) {
        datetime_printf("Error: API token is empty\n");
        return false;
    }
    
    if (firewallId.empty()) {
        datetime_printf("Error: Firewall ID is empty\n");
        return false;
    }

    datetime_printf("Initializing Linode Firewall Manager for firewall ID: %s\n", firewallId.c_str());
    datetime_printf("API token length: %zu characters\n", apiToken.length());
    
    // Fetch current rules
    datetime_printf("Fetching current firewall rules...\n");
    if (!fetchCurrentRules()) {
        datetime_printf("Failed to fetch current firewall rules\n");
        return false;
    }

    initialized = true;
    datetime_printf("Linode Firewall Manager initialized successfully with %zu active UDP ports\n", activePorts.size());
    return true;
}

// Fetch current firewall rules from Linode API
bool LinodeFirewallManager::fetchCurrentRules() {
    std::string url = "https://api.linode.com/v4/networking/firewalls/" + firewallId + "/rules";
    std::string response;

    if (!performGetRequest(url, response)) {
        return false;
    }

    try {
        json rulesJson = json::parse(response);
        
        // Extract fingerprint for future updates
        if (rulesJson.contains("fingerprint")) {
            fingerprint = rulesJson["fingerprint"];
        }

        // Parse existing UDP ports
        return parseExistingPorts(rulesJson);
    } catch (const std::exception& e) {
        datetime_printf("Error parsing firewall rules JSON: %s\n", e.what());
        return false;
    }
}

// Parse existing rules to extract UDP ports and store original rules
bool LinodeFirewallManager::parseExistingPorts(const json& rules) {
    try {
        // Clear current active ports
        activePorts.clear();
        
        // Store the original rules structure
        originalRules = rules;
        
        datetime_printf("Storing original firewall rules configuration\n");
        
        // Check if inbound rules exist
        if (!rules.contains("inbound") || !rules["inbound"].is_array()) {
            datetime_printf("No inbound rules found, will create new rule set\n");
            return true; // Not an error, just no rules
        }

        // Look for all UDP rules and specifically identify our ZeroTier rule
        bool foundZeroTierUdpRule = false;
        int udpRuleCount = 0;
        
        // First, log all UDP rules for better visibility during initialization
        for (const auto& rule : rules["inbound"]) {
            if (rule.contains("protocol") && rule["protocol"] == "UDP") {
                udpRuleCount++;
                std::string label = rule.contains("label") ? 
                    rule["label"].get<std::string>() : "unlabeled";
                std::string ports = rule.contains("ports") && rule["ports"].is_string() ? 
                    rule["ports"].get<std::string>() : "unknown";
                
                datetime_printf("Found UDP rule: %s (ports: %s)\n", label.c_str(), ports.c_str());
            }
        }
        
        datetime_printf("Total UDP rules found: %d\n", udpRuleCount);
        
        // Now process all ZeroTier UDP rules if they exist
        // We look for both the main rule "ZeroTier-UDP-Ports" and any additional rules "ZeroTier-UDP-Ports-X"
        int zeroTierRuleCount = 0;
        
        for (const auto& rule : rules["inbound"]) {
            if (rule.contains("protocol") && rule["protocol"] == "UDP" && 
                rule.contains("label") && 
                rule.contains("ports") && rule["ports"].is_string()) {
                
                std::string label = rule["label"];
                
                // Check if this is one of our ZeroTier rules
                // Either the main rule or a numbered rule (ZeroTier-UDP-Ports-2, ZeroTier-UDP-Ports-3, etc.)
                if (label == "ZeroTier-UDP-Ports" || 
                    (label.find("ZeroTier-UDP-Ports-") == 0 && label.length() > 18)) {
                    
                    zeroTierRuleCount++;
                    foundZeroTierUdpRule = true;
                    std::string portsStr = rule["ports"];
                    datetime_printf("Processing %s rule with ports: %s\n", label.c_str(), portsStr.c_str());
                    
                    // Parse ports string (e.g., "9993, 9994, 9995, 9999" or "1024-65535")
                    std::istringstream ss(portsStr);
                    std::string portItem;
                    
                    while (std::getline(ss, portItem, ',')) {
                        // Trim whitespace
                        portItem.erase(0, portItem.find_first_not_of(" \t\n\r\f\v"));
                        portItem.erase(portItem.find_last_not_of(" \t\n\r\f\v") + 1);
                        
                        // Check if it's a range (e.g., "1024-65535")
                        size_t rangePos = portItem.find('-');
                        if (rangePos != std::string::npos) {
                            std::string startStr = portItem.substr(0, rangePos);
                            std::string endStr = portItem.substr(rangePos + 1);
                            
                            try {
                                uint16_t startPort = static_cast<uint16_t>(std::stoi(startStr));
                                uint16_t endPort = static_cast<uint16_t>(std::stoi(endStr));
                                
                                for (uint16_t p = startPort; p <= endPort; ++p) {
                                    activePorts.insert(p);
                                }
                            } catch (const std::exception& e) {
                                datetime_printf("Error parsing port range: %s\n", e.what());
                            }
                        } else {
                            // Single port
                            try {
                                uint16_t port = static_cast<uint16_t>(std::stoi(portItem));
                                activePorts.insert(port);
                            } catch (const std::exception& e) {
                                datetime_printf("Error parsing port: %s\n", e.what());
                            }
                        }
                    }
                }
            }
        }
        
        if (zeroTierRuleCount > 0) {
            datetime_printf("Found %d ZeroTier UDP rules\n", zeroTierRuleCount);
        }
        
        if (!foundZeroTierUdpRule) {
            datetime_printf("No ZeroTier UDP rule found, will create one when needed\n");
        }

        datetime_printf("Parsed %zu active UDP ports from firewall rules\n", activePorts.size());
        return true;
    } catch (const std::exception& e) {
        datetime_printf("Error parsing existing ports: %s\n", e.what());
        return false;
    }
}

// Add a UDP port to the ZeroTier-UDP-Ports firewall rule
// Note: This only modifies the dedicated ZeroTier rule and preserves all other UDP rules
bool LinodeFirewallManager::addPort(uint16_t port) {
    std::lock_guard<std::mutex> lock(apiMutex);
    
    if (!initialized) {
        datetime_printf("Firewall manager not initialized\n");
        return false;
    }

    // Check if port is already in the set
    if (activePorts.find(port) != activePorts.end()) {
        // Port already added, no need to update
        datetime_printf("UDP port %d is already in the ZeroTier-UDP-Ports rules\n", port);
        return true;
    }

    datetime_printf("Adding UDP port %d to ZeroTier-UDP-Ports rules (other UDP rules remain unchanged)\n", port);
    activePorts.insert(port);
    
    return updateRules();
}

// Remove a UDP port from the ZeroTier-UDP-Ports firewall rule
// Note: This only modifies the dedicated ZeroTier rule and preserves all other UDP rules
bool LinodeFirewallManager::removePort(uint16_t port) {
    std::lock_guard<std::mutex> lock(apiMutex);
    
    if (!initialized) {
        datetime_printf("Firewall manager not initialized\n");
        return false;
    }

    // Check if port is in the set
    if (activePorts.find(port) == activePorts.end()) {
        // Port not in set, no need to update
        datetime_printf("UDP port %d is not in the ZeroTier-UDP-Ports rules\n", port);
        return true;
    }

    datetime_printf("Removing UDP port %d from ZeroTier-UDP-Ports rules (other UDP rules remain unchanged)\n", port);
    activePorts.erase(port);
    
    return updateRules();
}

// Force synchronization of firewall rules with Linode
// Note: This preserves all existing non-ZeroTier UDP rules
bool LinodeFirewallManager::syncFirewallRules() {
    std::lock_guard<std::mutex> lock(apiMutex);
    
    if (!initialized) {
        datetime_printf("Firewall manager not initialized\n");
        return false;
    }

    // Fetch current rules first to ensure we have the latest fingerprint
    // This also ensures we have the latest set of non-ZeroTier rules to preserve
    if (!fetchCurrentRules()) {
        datetime_printf("Failed to fetch current rules during sync\n");
        return false;
    }

    datetime_printf("Synchronizing firewall rules (preserving all non-ZeroTier UDP rules)\n");
    return updateRules();
}

// Update firewall rules via Linode API
// This function preserves all existing non-ZeroTier UDP rules
// It only modifies the dedicated "ZeroTier-UDP-Ports" rules (including any numbered variants)
bool LinodeFirewallManager::updateRules() {
    std::string url = "https://api.linode.com/v4/networking/firewalls/" + firewallId + "/rules";
    std::string jsonData = buildRulesJson();
    std::string response;

    datetime_printf("Updating firewall rules with %zu ZeroTier UDP ports (preserving all other UDP rules)\n", activePorts.size());
    
    if (!performPutRequest(url, jsonData, response)) {
        datetime_printf("Failed to update firewall rules\n");
        return false;
    }

    try {
        json responseJson = json::parse(response);
        
        // Update fingerprint for future updates
        if (responseJson.contains("fingerprint")) {
            std::string newFingerprint = responseJson["fingerprint"];
            datetime_printf("Received new fingerprint: %s\n", newFingerprint.c_str());
            fingerprint = newFingerprint;
        } else {
            datetime_printf("Warning: No fingerprint in response\n");
        }
        
        // Log inbound rules count for verification
        if (responseJson.contains("inbound") && responseJson["inbound"].is_array()) {
            datetime_printf("Updated firewall has %zu inbound rules\n", responseJson["inbound"].size());
        }
        
        datetime_printf("Firewall rules updated successfully\n");
        return true;
    } catch (const std::exception& e) {
        datetime_printf("Error parsing update response: %s\n", e.what());
        return false;
    }
}

// Convert ports to ranges for optimization
std::vector<std::pair<uint16_t, uint16_t>> LinodeFirewallManager::portsToRanges(const std::set<uint16_t>& ports) {
    std::vector<std::pair<uint16_t, uint16_t>> ranges;
    
    if (ports.empty()) {
        return ranges;
    }

    auto it = ports.begin();
    uint16_t rangeStart = *it;
    uint16_t rangeEnd = *it;
    
    ++it;
    while (it != ports.end()) {
        if (*it == rangeEnd + 1) {
            // Extend current range
            rangeEnd = *it;
        } else {
            // End current range and start a new one
            ranges.push_back({rangeStart, rangeEnd});
            rangeStart = *it;
            rangeEnd = *it;
        }
        ++it;
    }
    
    // Add the last range
    ranges.push_back({rangeStart, rangeEnd});
    
    return ranges;
}

// Count the number of pieces in a port set (single port = 1 piece, range = 2 pieces)
int LinodeFirewallManager::countPortPieces(const std::vector<std::pair<uint16_t, uint16_t>>& ranges) {
    int pieces = 0;
    
    for (const auto& range : ranges) {
        if (range.first == range.second) {
            // Single port counts as 1 piece
            pieces += 1;
        } else {
            // Range counts as 2 pieces
            pieces += 2;
        }
    }
    
    return pieces;
}

// Convert port set to string format for API
std::string LinodeFirewallManager::portsToString(const std::set<uint16_t>& ports) {
    if (ports.empty()) {
        return "";
    }

    // Convert ports to ranges
    std::vector<std::pair<uint16_t, uint16_t>> ranges = portsToRanges(ports);
    
    // Count pieces to check against Linode's limit
    int pieces = countPortPieces(ranges);
    if (pieces > MAX_PORT_PIECES_PER_RULE) {
        datetime_printf("Warning: Port specification exceeds Linode's %d-piece limit (%d pieces)\n", 
                       MAX_PORT_PIECES_PER_RULE, pieces);
        // Note: We'll still generate the string, but it may be rejected by the API
        // The splitPortsIntoGroups method should be used to avoid this situation
    }
    
    // Build the string
    std::ostringstream oss;
    for (size_t i = 0; i < ranges.size(); ++i) {
        if (i > 0) {
            oss << ", ";
        }
        
        if (ranges[i].first == ranges[i].second) {
            // Single port
            oss << ranges[i].first;
        } else {
            // Port range
            oss << ranges[i].first << "-" << ranges[i].second;
        }
    }
    
    return oss.str();
}

// Split ports into multiple groups to stay within the piece limit
std::vector<std::set<uint16_t>> LinodeFirewallManager::splitPortsIntoGroups(const std::set<uint16_t>& ports) {
    std::vector<std::set<uint16_t>> groups;
    
    if (ports.empty()) {
        return groups;
    }
    
    // First, try to optimize by converting to ranges
    std::vector<std::pair<uint16_t, uint16_t>> allRanges = portsToRanges(ports);
    
    // Debug: Print all ranges and their piece counts
    datetime_printf("DEBUG: Port ranges before splitting:\n");
    int totalPiecesBeforeSplit = 0;
    for (const auto& range : allRanges) {
        int rangePieces = (range.first == range.second) ? 1 : 2;
        totalPiecesBeforeSplit += rangePieces;
        if (range.first == range.second) {
            datetime_printf("  Single port: %d (1 piece)\n", range.first);
        } else {
            datetime_printf("  Port range: %d-%d (2 pieces)\n", range.first, range.second);
        }
    }
    datetime_printf("DEBUG: Total pieces before splitting: %d (MAX_PORT_PIECES_PER_RULE: %d)\n", 
                   totalPiecesBeforeSplit, MAX_PORT_PIECES_PER_RULE);
    
    // Create groups that stay within the piece limit
    std::set<uint16_t> currentGroup;
    int currentPieces = 0;
    int groupNumber = 1;
    
    for (const auto& range : allRanges) {
        // Calculate how many pieces this range would add
        int rangePieces = (range.first == range.second) ? 1 : 2;
        
        // If adding this range would exceed the limit, start a new group
        if (currentPieces + rangePieces > MAX_PORT_PIECES_PER_RULE) {
            if (!currentGroup.empty()) {
                datetime_printf("DEBUG: Group %d has %d pieces, starting new group\n", 
                               groupNumber, currentPieces);
                groups.push_back(currentGroup);
                currentGroup.clear();
                currentPieces = 0;
                groupNumber++;
            }
        }
        
        // Add the range to the current group
        if (range.first == range.second) {
            // Single port
            currentGroup.insert(range.first);
            datetime_printf("DEBUG: Adding single port %d to group %d (now %d pieces)\n", 
                           range.first, groupNumber, currentPieces + rangePieces);
        } else {
            // Port range - add all ports in the range
            datetime_printf("DEBUG: Adding port range %d-%d to group %d (now %d pieces)\n", 
                           range.first, range.second, groupNumber, currentPieces + rangePieces);
            for (uint16_t p = range.first; p <= range.second; ++p) {
                currentGroup.insert(p);
            }
        }
        
        // Update piece count
        currentPieces += rangePieces;
    }
    
    // Add the last group if not empty
    if (!currentGroup.empty()) {
        datetime_printf("DEBUG: Final group %d has %d pieces\n", groupNumber, currentPieces);
        groups.push_back(currentGroup);
    }
    
    datetime_printf("Split %zu ports into %zu groups to stay within %d-piece limit\n", 
                   ports.size(), groups.size(), MAX_PORT_PIECES_PER_RULE);
    
    return groups;
}

// Build JSON for updating firewall rules
// This function carefully preserves all existing firewall rules except for the ZeroTier-UDP-Ports rule
// It ensures that any pre-existing UDP rules for other services remain untouched
std::string LinodeFirewallManager::buildRulesJson() {
    json rulesJson;
    
    // Start with the original rules structure if available
    if (originalRules.contains("inbound_policy")) {
        rulesJson["inbound_policy"] = originalRules["inbound_policy"];
    } else {
        rulesJson["inbound_policy"] = "DROP";
        datetime_printf("No inbound policy found, defaulting to DROP\n");
    }
    
    if (originalRules.contains("outbound_policy")) {
        rulesJson["outbound_policy"] = originalRules["outbound_policy"];
    } else {
        rulesJson["outbound_policy"] = "ACCEPT";
        datetime_printf("No outbound policy found, defaulting to ACCEPT\n");
    }
    
    // Create inbound rules array
    json inboundRules = json::array();
    
    // First, copy all non-ZeroTier UDP rules from the original configuration
    // IMPORTANT: We preserve ALL existing rules EXCEPT the specific "ZeroTier-UDP-Ports" rules
    // This ensures that any pre-existing UDP rules (for other services) remain untouched
    bool hasTcp443Rule = false;
    if (originalRules.contains("inbound") && originalRules["inbound"].is_array()) {
        datetime_printf("Preserving existing firewall rules while updating ZeroTier rules\n");
        for (const auto& rule : originalRules["inbound"]) {
            // Skip all ZeroTier UDP rules as we'll add updated ones
            if (rule.contains("protocol") && rule["protocol"] == "UDP" && 
                rule.contains("label")) {
                
                std::string label = rule["label"];
                if (label == "ZeroTier-UDP-Ports" || 
                    (label.find("ZeroTier-UDP-Ports-") == 0 && label.length() > 18)) {
                    datetime_printf("Found existing %s rule, will update it\n", label.c_str());
                    continue;
                }
            }
            
            // Log preservation of UDP rules explicitly for better visibility
            if (rule.contains("protocol") && rule["protocol"] == "UDP") {
                std::string label = rule.contains("label") ? 
                    rule["label"].get<std::string>() : "unlabeled";
                std::string ports = rule.contains("ports") && rule["ports"].is_string() ? 
                    rule["ports"].get<std::string>() : "unknown";
                datetime_printf("Preserving existing UDP rule: %s (ports: %s)\n", 
                    label.c_str(), ports.c_str());
            }
            
            // Check if there's already a ZeroTier TCP Proxy rule by label
            if (rule.contains("label") && rule["label"] == "ZeroTier-TCP-Proxy-Port") {
                datetime_printf("Found existing ZeroTier TCP Proxy rule\n");
                hasTcp443Rule = true;
                
                // Check if the port in the rule matches our configured port
                if (rule.contains("protocol") && rule["protocol"] == "TCP" && 
                    rule.contains("ports") && rule["ports"].is_string()) {
                    std::string portsStr = rule["ports"];
                    std::string tcpPortStr = std::to_string(externalTcpPort);
                    
                    if (portsStr != tcpPortStr) {
                        // Update the port in the existing rule
                        datetime_printf("Updating TCP port from %s to %s in existing rule\n", 
                                       portsStr.c_str(), tcpPortStr.c_str());
                        json updatedRule = rule;
                        updatedRule["ports"] = tcpPortStr;
                        updatedRule["description"] = "TCP port " + tcpPortStr + " for ZeroTier TCP Proxy (internal port: " + std::to_string(internalTcpPort) + ")";
                        inboundRules.push_back(updatedRule);
                        continue; // Skip adding the original rule
                    } else {
                        datetime_printf("TCP port %d already configured correctly\n", externalTcpPort);
                    }
                }
            }
            // Also check for legacy TCP rules with the old naming format
            else if (rule.contains("protocol") && rule["protocol"] == "TCP" && 
                rule.contains("ports") && rule["ports"].is_string()) {
                std::string portsStr = rule["ports"];
                std::string tcpPortStr = std::to_string(externalTcpPort);
                if (portsStr == tcpPortStr || portsStr.find(tcpPortStr) != std::string::npos) {
                    datetime_printf("Found existing TCP %d rule with legacy format, will preserve it\n", externalTcpPort);
                    hasTcp443Rule = true;
                }
            }
            
            // Add this rule to our new configuration
            inboundRules.push_back(rule);
        }
    } else {
        datetime_printf("No existing inbound rules found, creating new rule set\n");
    }
    
    // Add UDP rules with our ports if we have any
    if (!activePorts.empty()) {
        // Split ports into groups to stay within the 15-piece limit
    std::vector<std::set<uint16_t>> portGroups = splitPortsIntoGroups(activePorts);
    
    // Calculate total pieces for debugging
    std::vector<std::pair<uint16_t, uint16_t>> allRanges = portsToRanges(activePorts);
    int totalPieces = countPortPieces(allRanges);
    datetime_printf("DEBUG: Total ports: %zu, Total pieces: %d, MAX_PORT_PIECES_PER_RULE: %d\n", 
                   activePorts.size(), totalPieces, MAX_PORT_PIECES_PER_RULE);
    
    datetime_printf("Adding %zu ZeroTier UDP rule(s) for %zu ports\n", 
                   portGroups.size(), activePorts.size());
        
        // Create a rule for each group
        for (size_t i = 0; i < portGroups.size(); ++i) {
            std::string portsString = portsToString(portGroups[i]);
            std::string ruleName = "ZeroTier-UDP-Ports";
            
            // Add suffix for additional rules
            if (i > 0) {
                ruleName += "-" + std::to_string(i + 1);
            }
            
            datetime_printf("Adding %s rule with ports: %s\n", ruleName.c_str(), portsString.c_str());
            
            json udpRule;
            udpRule["protocol"] = "UDP";
            udpRule["ports"] = portsString;
            udpRule["addresses"] = json::object();
            udpRule["addresses"]["ipv4"] = json::array({"0.0.0.0/0"});
            udpRule["addresses"]["ipv6"] = json::array({"::/0"});
            udpRule["action"] = "ACCEPT";
            udpRule["label"] = ruleName;
            udpRule["description"] = "Dynamically managed UDP ports for ZeroTier TCP Proxy";
            
            inboundRules.push_back(udpRule);
        }
    } else {
        datetime_printf("No active UDP ports to add to firewall rules\n");
    }
    
    // Add TCP rule for our configured port if it doesn't exist already
    if (!hasTcp443Rule) {
        datetime_printf("Adding TCP %d rule with fixed label 'ZeroTier-TCP-Proxy-Port' for ZeroTier TCP Proxy\n", externalTcpPort);
        
        json tcpRule;
        tcpRule["protocol"] = "TCP";
        tcpRule["ports"] = std::to_string(externalTcpPort);
        tcpRule["addresses"] = json::object();
        tcpRule["addresses"]["ipv4"] = json::array({"0.0.0.0/0"});
        tcpRule["addresses"]["ipv6"] = json::array({"::/0"});
        tcpRule["action"] = "ACCEPT";
        tcpRule["label"] = "ZeroTier-TCP-Proxy-Port";
        tcpRule["description"] = "TCP port " + std::to_string(externalTcpPort) + " for ZeroTier TCP Proxy" + 
                               (internalTcpPort != externalTcpPort ? " (internal port: " + std::to_string(internalTcpPort) + ")" : "");
        
        inboundRules.push_back(tcpRule);
    } else {
        datetime_printf("Using existing TCP %d rule\n", externalTcpPort);
    }
    
    rulesJson["inbound"] = inboundRules;
    datetime_printf("Total inbound rules: %zu\n", inboundRules.size());
    
    // Copy outbound rules from original configuration
    if (originalRules.contains("outbound") && originalRules["outbound"].is_array()) {
        rulesJson["outbound"] = originalRules["outbound"];
        datetime_printf("Preserving %zu existing outbound rules\n", originalRules["outbound"].size());
    } else {
        rulesJson["outbound"] = json::array();
        datetime_printf("No existing outbound rules found, using empty array\n");
    }
    
    datetime_printf("Firewall rules JSON prepared for update\n");
    return rulesJson.dump();
}