/**
 * CloudProviderTemplate.cpp
 * 
 * Implementation template for a new cloud provider's firewall manager
 */

#include "CloudProviderTemplate.hpp"
#include <curl/curl.h>
#include <iostream>
#include <sstream>
#include "../../../ext/nlohmann/json.hpp"

// For JSON parsing
using json = nlohmann::json;

// External function for logging (defined in tcp-proxy.cpp)
extern void datetime_fprintf(FILE *stream, const char *fmt, ...);

// Callback function for curl HTTP requests
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, std::string *s) {
    size_t newLength = size * nmemb;
    try {
        s->append((char*)contents, newLength);
        return newLength;
    } catch(std::bad_alloc &e) {
        return 0;
    }
}

// Constructor
CloudProviderFirewallManager::CloudProviderFirewallManager(const std::string& apiToken, const std::string& firewallId)
    : apiToken(apiToken), firewallId(firewallId) {
    // Initialization code
}

// Initialize the firewall manager
bool CloudProviderFirewallManager::initialize() {
    datetime_fprintf(stdout, "Initializing Cloud Provider firewall manager...\n");
    
    // Verify API credentials and firewall existence
    std::string response;
    if (!makeApiRequest("/firewalls/" + firewallId, "GET", "", response)) {
        datetime_fprintf(stderr, "Failed to verify firewall existence\n");
        return false;
    }
    
    // Parse response to verify firewall exists
    try {
        json j = json::parse(response);
        // Verify firewall exists and is accessible
        // Customize based on your cloud provider's API response format
        
        datetime_fprintf(stdout, "Successfully initialized Cloud Provider firewall manager\n");
        return true;
    } catch (json::parse_error& e) {
        datetime_fprintf(stderr, "JSON parse error: %s\n", e.what());
        return false;
    }
}

// Add a port to the managed set
bool CloudProviderFirewallManager::addPort(uint16_t port) {
    datetime_fprintf(stdout, "Adding port %d to managed set\n", port);
    activePorts.insert(port);
    return true;
}

// Remove a port from the managed set
bool CloudProviderFirewallManager::removePort(uint16_t port) {
    datetime_fprintf(stdout, "Removing port %d from managed set\n", port);
    activePorts.erase(port);
    return true;
}

// Synchronize the current set of ports with the cloud provider
bool CloudProviderFirewallManager::syncFirewallRules() {
    datetime_fprintf(stdout, "Synchronizing firewall rules with Cloud Provider...\n");
    
    // Fetch current rules
    if (!fetchCurrentRules()) {
        datetime_fprintf(stderr, "Failed to fetch current firewall rules\n");
        return false;
    }
    
    // Prepare the update payload
    // This will vary significantly based on your cloud provider's API
    json payload;
    
    // Example: Create a rule for each port or port range
    json rules = json::array();
    
    // Convert individual ports to ranges where possible
    // This is a simplified example - implement proper range consolidation
    if (!activePorts.empty()) {
        uint16_t rangeStart = *activePorts.begin();
        uint16_t rangeEnd = rangeStart;
        
        for (auto it = std::next(activePorts.begin()); it != activePorts.end(); ++it) {
            if (*it == rangeEnd + 1) {
                // Extend the current range
                rangeEnd = *it;
            } else {
                // Add the completed range and start a new one
                json rule;
                rule["protocol"] = "udp";
                rule["ports"] = (rangeStart == rangeEnd) ? 
                    std::to_string(rangeStart) : 
                    std::to_string(rangeStart) + "-" + std::to_string(rangeEnd);
                rule["description"] = "ZeroTier UDP Port";
                rules.push_back(rule);
                
                rangeStart = *it;
                rangeEnd = *it;
            }
        }
        
        // Add the last range
        json rule;
        rule["protocol"] = "udp";
        rule["ports"] = (rangeStart == rangeEnd) ? 
            std::to_string(rangeStart) : 
            std::to_string(rangeStart) + "-" + std::to_string(rangeEnd);
        rule["description"] = "ZeroTier UDP Port";
        rules.push_back(rule);
    }
    
    // Add rules to payload
    payload["rules"] = rules;
    
    // Send update to cloud provider
    std::string response;
    if (!makeApiRequest("/firewalls/" + firewallId + "/rules", "PUT", payload.dump(), response)) {
        datetime_fprintf(stderr, "Failed to update firewall rules\n");
        return false;
    }
    
    datetime_fprintf(stdout, "Successfully synchronized firewall rules\n");
    return true;
}

// Fetch current firewall rules from the cloud provider
bool CloudProviderFirewallManager::fetchCurrentRules() {
    datetime_fprintf(stdout, "Fetching current firewall rules...\n");
    
    std::string response;
    if (!makeApiRequest("/firewalls/" + firewallId + "/rules", "GET", "", response)) {
        datetime_fprintf(stderr, "Failed to fetch firewall rules\n");
        return false;
    }
    
    // Parse response to extract current rules
    // This will vary based on your cloud provider's API
    try {
        json j = json::parse(response);
        // Process rules based on your cloud provider's API response format
        
        return true;
    } catch (json::parse_error& e) {
        datetime_fprintf(stderr, "JSON parse error: %s\n", e.what());
        return false;
    }
}

// Helper method for making API requests to the cloud provider
bool CloudProviderFirewallManager::makeApiRequest(
    const std::string& endpoint, 
    const std::string& method, 
    const std::string& data, 
    std::string& response) {
    
    CURL *curl;
    CURLcode res;
    bool success = false;
    
    curl = curl_easy_init();
    if(curl) {
        // Set the API endpoint URL
        std::string url = "https://api.yourcloudprovider.com/v1" + endpoint;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        
        // Set request method
        if (method == "POST" || method == "PUT") {
            if (method == "POST") {
                curl_easy_setopt(curl, CURLOPT_POST, 1L);
            } else if (method == "PUT") {
                curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
            }
            
            // Set request body for POST/PUT
            if (!data.empty()) {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
            }
        } else if (method == "DELETE") {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        }
        
        // Set headers
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        std::string authHeader = "Authorization: Bearer " + apiToken;
        headers = curl_slist_append(headers, authHeader.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        
        // Set response callback
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        
        // Perform the request
        res = curl_easy_perform(curl);
        
        if(res != CURLE_OK) {
            datetime_fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        } else {
            long http_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            
            if (http_code >= 200 && http_code < 300) {
                success = true;
            } else {
                datetime_fprintf(stderr, "API request failed with HTTP code %ld: %s\n", 
                                http_code, response.c_str());
            }
        }
        
        // Clean up
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
    
    return success;
}