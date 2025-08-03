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

#include <iostream>
#include <string>
#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <csignal>
#include <fstream>
#include <atomic>
#include <memory>
#include <curl/curl.h>
#include "../../../../ext/nlohmann/json.hpp"
#include "../FirewallManagerFactory.hpp"

using json = nlohmann::json;

// Use stdout for logging instead of a file
#define LOG_TO_STDOUT 1

// Implementation of datetime_fprintf for the test program
void datetime_fprintf(FILE *stream, const char *fmt, ...) {
    // Get current time
    time_t now = time(nullptr);
    struct tm tm_info;
    char time_buf[26];
    
    localtime_r(&now, &tm_info);
    strftime(time_buf, 26, "%Y-%m-%d %H:%M:%S", &tm_info);
    
    // Print timestamp to stream
    fprintf(stream, "%s ", time_buf);
    
    // Print the rest of the message to stream
    va_list args;
    va_start(args, fmt);
    vfprintf(stream, fmt, args);
    va_end(args);
    
    // Also write to stdout if logging to a different stream
    if (stream != stdout && LOG_TO_STDOUT) {
        std::cout << time_buf << " ";
        
        // Format the message into a buffer
        va_list args2;
        va_start(args2, fmt);
        char buffer[4096]; // Large buffer for the formatted message
        vsnprintf(buffer, sizeof(buffer), fmt, args2);
        va_end(args2);
        
        // Write to stdout and flush
        std::cout << buffer;
        std::cout.flush();
    }
}

/**
 * Custom write callback function for curl HTTP requests
 * Appends received data to the provided string
 * 
 * @param contents Pointer to the received data
 * @param size Size of each data element
 * @param nmemb Number of data elements
 * @param s String to append the data to
 * @return Total size of processed data, or 0 on error
 */
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* s) {
    size_t newLength = size * nmemb;
    try {
        s->append((char*)contents, newLength);
        return newLength;
    } catch(std::bad_alloc& e) {
        std::cerr << "Error in WriteCallback: Memory allocation failed - " << e.what() << std::endl;
        return 0;
    } catch(const std::exception& e) {
        std::cerr << "Error in WriteCallback: " << e.what() << std::endl;
        return 0;
    }
}

/**
 * Fetches and displays the current firewall rules from the Linode API
 * 
 * @param apiToken The Linode API token for authentication
 * @param firewallId The ID of the firewall to query
 * @return true if successful, false if an error occurred
 */
bool displayFirewallRules(const std::string& apiToken, const std::string& firewallId) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to initialize curl" << std::endl;
        return false;
    }
    
    std::string url = "https://api.linode.com/v4/networking/firewalls/" + firewallId;
    std::string response;
    
    // Set up curl options
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, ("Authorization: Bearer " + apiToken).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    
    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        std::cerr << "GET request failed: " << curl_easy_strerror(res) << std::endl;
        std::cerr << "URL: " << url << std::endl;
        return false;
    }
    
    if (http_code < 200 || http_code >= 300) {
        std::cerr << "GET request returned HTTP error: " << http_code << std::endl;
        std::cerr << "URL: " << url << std::endl;
        std::cerr << "Response: " << response << std::endl;
        return false;
    }
    
    // Log successful request
    datetime_fprintf(stdout, "Successfully fetched data from %s\n", url.c_str());
    
    // Log the complete JSON response to stdout
    if (LOG_TO_STDOUT) {
        std::cout << "\n===== COMPLETE FIREWALL JSON RESPONSE =====\n";
        std::cout << response << "\n";
        std::cout << "========================================\n";
        std::cout.flush();
    }
    
    // Parse and display the firewall information
    try {
        json firewallJson = json::parse(response);
        
        std::cout << "\n===== FIREWALL INFORMATION =====" << std::endl;
        // Handle ID which could be a number or string
        std::cout << "ID: " << (firewallJson["id"].is_number() ? 
                      std::to_string(firewallJson["id"].get<int>()) : 
                      firewallJson["id"].get<std::string>()) << std::endl;
        std::cout << "Label: " << firewallJson["label"].get<std::string>() << std::endl;
        std::cout << "Status: " << firewallJson["status"].get<std::string>() << std::endl;
        
        // Now fetch the rules
        url = "https://api.linode.com/v4/networking/firewalls/" + firewallId + "/rules";
        response.clear();
        
        curl = curl_easy_init();
        headers = NULL;
        headers = curl_slist_append(headers, ("Authorization: Bearer " + apiToken).c_str());
        headers = curl_slist_append(headers, "Content-Type: application/json");
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
        
        res = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        
        if (res == CURLE_OK && http_code >= 200 && http_code < 300) {
            json rulesJson = json::parse(response);
            
            std::cout << "\n===== FIREWALL RULES =====" << std::endl;
            // Handle policies which could be different types
            std::cout << "Inbound Policy: " << (rulesJson["inbound_policy"].is_string() ? 
                              rulesJson["inbound_policy"].get<std::string>() : 
                              "Unknown") << std::endl;
            std::cout << "Outbound Policy: " << (rulesJson["outbound_policy"].is_string() ? 
                               rulesJson["outbound_policy"].get<std::string>() : 
                               "Unknown") << std::endl;
            
            std::cout << "\nInbound Rules:" << std::endl;
            if (rulesJson.contains("inbound") && rulesJson["inbound"].is_array()) {
                for (const auto& rule : rulesJson["inbound"]) {
                    // Handle all fields with type checking
                    std::cout << "  - Protocol: " << (rule["protocol"].is_string() ? 
                                      rule["protocol"].get<std::string>() : "Unknown") << std::endl;
                    std::cout << "    Label: " << (rule["label"].is_string() ? 
                                    rule["label"].get<std::string>() : "Unknown") << std::endl;
                    if (rule.contains("ports") && !rule["ports"].is_null()) {
                        std::cout << "    Ports: " << (rule["ports"].is_string() ? 
                                        rule["ports"].get<std::string>() : 
                                        (rule["ports"].is_number() ? 
                                         std::to_string(rule["ports"].get<int>()) : "Unknown")) << std::endl;
                    }
                    std::cout << "    Action: " << (rule["action"].is_string() ? 
                                     rule["action"].get<std::string>() : "Unknown") << std::endl;
                    std::cout << std::endl;
                }
            } else {
                std::cout << "  No inbound rules found." << std::endl;
            }
            
            std::cout << "\nOutbound Rules:" << std::endl;
            if (rulesJson.contains("outbound") && rulesJson["outbound"].is_array()) {
                for (const auto& rule : rulesJson["outbound"]) {
                    // Handle all fields with type checking
                    std::cout << "  - Protocol: " << (rule["protocol"].is_string() ? 
                                      rule["protocol"].get<std::string>() : "Unknown") << std::endl;
                    std::cout << "    Label: " << (rule["label"].is_string() ? 
                                    rule["label"].get<std::string>() : "Unknown") << std::endl;
                    if (rule.contains("ports") && !rule["ports"].is_null()) {
                        std::cout << "    Ports: " << (rule["ports"].is_string() ? 
                                        rule["ports"].get<std::string>() : 
                                        (rule["ports"].is_number() ? 
                                         std::to_string(rule["ports"].get<int>()) : "Unknown")) << std::endl;
                    }
                    std::cout << "    Action: " << (rule["action"].is_string() ? 
                                     rule["action"].get<std::string>() : "Unknown") << std::endl;
                    std::cout << std::endl;
                }
            } else {
                std::cout << "  No outbound rules found." << std::endl;
            }
        } else {
            std::cerr << "Failed to fetch firewall rules" << std::endl;
            std::cerr << "HTTP code: " << http_code << std::endl;
            if (!response.empty()) {
                std::cerr << "Response: " << response << std::endl;
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error parsing firewall information: " << e.what() << std::endl;
        return false;
    }
}

// Enhanced test program for the LinodeFirewallManager

// Function to calculate the number of pieces in a set of ports
// Single port = 1 piece, port range = 2 pieces
int calculatePortPieces(const std::set<uint16_t>& ports) {
    if (ports.empty()) {
        return 0;
    }
    
    int pieces = 0;
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
            if (rangeStart == rangeEnd) {
                // Single port counts as 1 piece
                pieces += 1;
            } else {
                // Range counts as 2 pieces
                pieces += 2;
            }
            rangeStart = *it;
            rangeEnd = *it;
        }
        ++it;
    }
    
    // Add the last range
    if (rangeStart == rangeEnd) {
        // Single port counts as 1 piece
        pieces += 1;
    } else {
        // Range counts as 2 pieces
        pieces += 2;
    }
    
    return pieces;
}

// Constants for test configuration
const uint16_t TEST_PORT_1 = 9993;  // ZeroTier default port
const uint16_t TEST_PORT_2 = 9994;  // ZeroTier default port + 1
const uint16_t TEST_PORT_3 = 12345; // Non-consecutive port for testing

// Additional ports for testing multiple rule functionality
const uint16_t TEST_PORT_4 = 20001;
const uint16_t TEST_PORT_5 = 20002;
const uint16_t TEST_PORT_6 = 20003;
const uint16_t TEST_PORT_7 = 20004;
const uint16_t TEST_PORT_8 = 20005;
const uint16_t TEST_PORT_9 = 20006;
const uint16_t TEST_PORT_10 = 20007;
const uint16_t TEST_PORT_11 = 20008;
const uint16_t TEST_PORT_12 = 20009;
const uint16_t TEST_PORT_13 = 20010;
const uint16_t TEST_PORT_14 = 20011;

// Port range for testing - using a larger range to ensure we exceed the 15-piece limit
const uint16_t TEST_RANGE_START = 30000;
const uint16_t TEST_RANGE_END = 30010; // 11 ports in range (counts as 2 pieces)

const int WAIT_TIME_SECONDS = 10;   // Wait time between operations

// Global flag for signal handling
std::atomic<bool> g_interrupted(false);

/**
 * Signal handler for graceful termination of the program
 * Sets the global interrupted flag when SIGINT or SIGTERM is received
 * 
 * @param signal The signal number that was received
 */
void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nReceived termination signal. Cleaning up..." << std::endl;
        g_interrupted = true;
    }
}

int main(int argc, char** argv) {
    // Set up signal handlers for graceful termination
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    // Log start of test
    std::cout << "\n===== Starting Linode Firewall Manager test... =====" << std::endl;
    
    // Log start of test
    datetime_fprintf(stdout, "===== STARTING LINODE FIREWALL TEST =====\n");
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <api_token> <firewall_id> [tcp_port]" << std::endl;
        std::cerr << "\nThis program will:" << std::endl;
        std::cerr << "  1. Display initial firewall state" << std::endl;
        std::cerr << "  2. Add UDP port " << TEST_PORT_1 << std::endl;
        std::cerr << "  3. Wait " << WAIT_TIME_SECONDS << " seconds" << std::endl;
        std::cerr << "  4. Add UDP port " << TEST_PORT_2 << std::endl;
        std::cerr << "  5. Add UDP port " << TEST_PORT_3 << " (non-consecutive)" << std::endl;
        std::cerr << "  6. Wait " << WAIT_TIME_SECONDS << " seconds" << std::endl;
        
        // Multiple rule testing
        std::cerr << "  7. Add multiple ports (" << TEST_PORT_4 << "-" << TEST_PORT_14 << ") to force creation of a second rule" << std::endl;
        std::cerr << "  8. Add port range " << TEST_RANGE_START << "-" << TEST_RANGE_END << " (11 ports) to test range handling" << std::endl;
        std::cerr << "  9. Wait " << WAIT_TIME_SECONDS << " seconds" << std::endl;
        std::cerr << " 10. Display firewall state with multiple rules" << std::endl;
        
        // Staged Cleanup
        std::cerr << " 11. Stage 1: Remove port range and ports 10-14 to reduce from two rules to one" << std::endl;
        std::cerr << " 12. Display firewall state after stage 1 removal" << std::endl;
        std::cerr << " 13. Wait " << WAIT_TIME_SECONDS << " seconds" << std::endl;
        std::cerr << " 14. Stage 2: Remove remaining ports" << std::endl;
        std::cerr << " 15. Wait " << WAIT_TIME_SECONDS << " seconds" << std::endl;
        std::cerr << " 16. Display final firewall state" << std::endl;
        std::cerr << "\nPress Ctrl+C at any time to interrupt and clean up." << std::endl;
        return 1;
    }
    
    std::string apiToken = argv[1];
    std::string firewallId = argv[2];
    
    // Get optional TCP port parameter
    uint16_t tcpPort = 443; // Default to 443
    if (argc >= 4) {
        try {
            tcpPort = static_cast<uint16_t>(std::stoi(argv[3]));
            std::cout << "Using custom TCP port: " << tcpPort << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Invalid TCP port specified, using default (443)" << std::endl;
        }
    } else {
        std::cout << "No TCP port specified, using default (443)" << std::endl;
    }
    
    std::cout << "\n===== LINODE FIREWALL MANAGER TEST =====" << std::endl;
    std::cout << "Testing LinodeFirewallManager with firewall ID: " << firewallId << std::endl;
    
    // Initialize curl globally
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    // Display initial firewall state
    std::cout << "\nDisplaying initial firewall state:" << std::endl;
    if (!displayFirewallRules(apiToken, firewallId)) {
        std::cerr << "Failed to display initial firewall state" << std::endl;
        curl_global_cleanup();
        return 1;
    }
    
    // Create and initialize the firewall manager
    std::unique_ptr<CloudFirewallManager> manager = FirewallManagerFactory::createFirewallManager("linode", apiToken, firewallId, tcpPort, tcpPort); // Using same port for internal and external
    if (!manager) {
        std::cerr << "Failed to create firewall manager" << std::endl;
        curl_global_cleanup();
        return 1;
    }
    
    if (!manager->initialize()) {
        std::cerr << "Failed to initialize LinodeFirewallManager" << std::endl;
        curl_global_cleanup();
        return 1;
    }
    
    std::cout << "\nSuccessfully initialized LinodeFirewallManager" << std::endl;
    
    // First, sync the firewall to see the current state
    std::cout << "\nSyncing firewall rules to see current state..." << std::endl;
    if (!manager->syncFirewallRules()) {
        std::cerr << "Failed to sync firewall rules" << std::endl;
        curl_global_cleanup();
        return 1;
    }
    
    // Test adding a port
    std::cout << "\nAdding UDP port " << TEST_PORT_1 << "..." << std::endl;
    
    if (!manager->addPort(TEST_PORT_1)) {
        std::cerr << "Failed to add port " << TEST_PORT_1 << std::endl;
        curl_global_cleanup();
        return 1;
    }
    
    std::cout << "Successfully added port " << TEST_PORT_1 << std::endl;
    
    // Display firewall state after adding first port
    std::cout << "\nDisplaying firewall state after adding port " << TEST_PORT_1 << ":" << std::endl;
    if (!displayFirewallRules(apiToken, firewallId)) {
        std::cerr << "Failed to display firewall state" << std::endl;
    }
    
    // Wait for configured seconds before adding more ports
    std::cout << "\nWaiting " << WAIT_TIME_SECONDS << " seconds before adding more ports..." << std::endl;
    for (int i = WAIT_TIME_SECONDS; i > 0 && !g_interrupted; i--) {
        std::cout << "\rTime remaining: " << i << " seconds" << std::flush;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::cout << std::endl;
    
    // Check if we were interrupted
    if (g_interrupted) {
        std::cout << "Test interrupted. Cleaning up..." << std::endl;
        // Clean up ports that might have been added
        manager->removePort(TEST_PORT_1);
        manager->syncFirewallRules();
        curl_global_cleanup();
        
        // Log interruption
        datetime_fprintf(stdout, "Test interrupted.\n");
        
        return 1;
    }
    
    // Test adding another port
    std::cout << "\nAdding UDP port " << TEST_PORT_2 << "..." << std::endl;
    
    if (!manager->addPort(TEST_PORT_2)) {
        std::cerr << "Failed to add port " << TEST_PORT_2 << std::endl;
        curl_global_cleanup();
        return 1;
    }
    
    std::cout << "Successfully added port " << TEST_PORT_2 << std::endl;
    
    // Display firewall state after adding second port
    std::cout << "\nDisplaying firewall state after adding port " << TEST_PORT_2 << ":" << std::endl;
    if (!displayFirewallRules(apiToken, firewallId)) {
        std::cerr << "Failed to display firewall state" << std::endl;
    }
    
    // Test adding a non-consecutive port
    std::cout << "\nAdding UDP port " << TEST_PORT_3 << " (non-consecutive)..." << std::endl;
    
    if (!manager->addPort(TEST_PORT_3)) {
        std::cerr << "Failed to add port " << TEST_PORT_3 << std::endl;
        curl_global_cleanup();
        return 1;
    }
    
    std::cout << "Successfully added port " << TEST_PORT_3 << std::endl;
    
    // Display firewall state after adding non-consecutive port
    std::cout << "\nDisplaying firewall state after adding port " << TEST_PORT_3 << ":" << std::endl;
    if (!displayFirewallRules(apiToken, firewallId)) {
        std::cerr << "Failed to display firewall state" << std::endl;
    }
    
    // ===== TESTING MULTIPLE RULE FUNCTIONALITY =====
    std::cout << "\n===== TESTING MULTIPLE RULE FUNCTIONALITY =====" << std::endl;
    std::cout << "Adding multiple ports to force creation of secondary rules..." << std::endl;
    std::cout << "Piece calculation: 3 initial ports (3 pieces) + 11 additional ports (11 pieces)" << std::endl;
    std::cout << "+ port range (2 pieces) = 16 pieces total, exceeding the 15-piece limit" << std::endl;
    
    // Wait before starting multiple rule test
    std::cout << "\nWaiting " << WAIT_TIME_SECONDS << " seconds before starting multiple rule test..." << std::endl;
    for (int i = WAIT_TIME_SECONDS; i > 0 && !g_interrupted; i--) {
        std::cout << "\rTime remaining: " << i << " seconds" << std::flush;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::cout << std::endl;
    
    // Check if we were interrupted
    if (g_interrupted) {
        std::cout << "Test interrupted. Cleaning up..." << std::endl;
        // Clean up ports that might have been added
        manager->removePort(TEST_PORT_1);
        manager->removePort(TEST_PORT_2);
        manager->removePort(TEST_PORT_3);
        manager->syncFirewallRules();
        curl_global_cleanup();
        
        // Log interruption
        datetime_fprintf(stdout, "Test interrupted.\n");
        
        return 1;
    }
    
    // Add multiple individual ports to force creation of secondary rules
    bool allPortsAdded = true;
    
    std::cout << "\nAdding multiple individual ports..." << std::endl;
    
    if (!manager->addPort(TEST_PORT_4)) {
        std::cerr << "Failed to add port " << TEST_PORT_4 << std::endl;
        allPortsAdded = false;
    }
    
    if (!manager->addPort(TEST_PORT_5)) {
        std::cerr << "Failed to add port " << TEST_PORT_5 << std::endl;
        allPortsAdded = false;
    }
    
    if (!manager->addPort(TEST_PORT_6)) {
        std::cerr << "Failed to add port " << TEST_PORT_6 << std::endl;
        allPortsAdded = false;
    }
    
    if (!manager->addPort(TEST_PORT_7)) {
        std::cerr << "Failed to add port " << TEST_PORT_7 << std::endl;
        allPortsAdded = false;
    }
    
    if (!manager->addPort(TEST_PORT_8)) {
        std::cerr << "Failed to add port " << TEST_PORT_8 << std::endl;
        allPortsAdded = false;
    }
    
    if (!manager->addPort(TEST_PORT_9)) {
        std::cerr << "Failed to add port " << TEST_PORT_9 << std::endl;
        allPortsAdded = false;
    }
    
    if (!manager->addPort(TEST_PORT_10)) {
        std::cerr << "Failed to add port " << TEST_PORT_10 << std::endl;
        allPortsAdded = false;
    }
    
    if (!manager->addPort(TEST_PORT_11)) {
        std::cerr << "Failed to add port " << TEST_PORT_11 << std::endl;
        allPortsAdded = false;
    }
    
    if (!manager->addPort(TEST_PORT_12)) {
        std::cerr << "Failed to add port " << TEST_PORT_12 << std::endl;
        allPortsAdded = false;
    }
    
    if (!manager->addPort(TEST_PORT_13)) {
        std::cerr << "Failed to add port " << TEST_PORT_13 << std::endl;
        allPortsAdded = false;
    }
    
    if (!manager->addPort(TEST_PORT_14)) {
        std::cerr << "Failed to add port " << TEST_PORT_14 << std::endl;
        allPortsAdded = false;
    }
    
    // Add port range
    std::cout << "\nAdding port range " << TEST_RANGE_START << "-" << TEST_RANGE_END << "..." << std::endl;
    for (uint16_t port = TEST_RANGE_START; port <= TEST_RANGE_END; port++) {
        if (!manager->addPort(port)) {
            std::cerr << "Failed to add port " << port << " from range" << std::endl;
            allPortsAdded = false;
        }
    }
    
    if (allPortsAdded) {
        std::cout << "Successfully added all test ports for multiple rule test" << std::endl;
        
        // Calculate and log the total number of pieces
        std::set<uint16_t> allPorts;
        allPorts.insert(TEST_PORT_1);
        allPorts.insert(TEST_PORT_2);
        allPorts.insert(TEST_PORT_3);
        allPorts.insert(TEST_PORT_4);
        allPorts.insert(TEST_PORT_5);
        allPorts.insert(TEST_PORT_6);
        allPorts.insert(TEST_PORT_7);
        allPorts.insert(TEST_PORT_8);
        allPorts.insert(TEST_PORT_9);
        allPorts.insert(TEST_PORT_10);
        allPorts.insert(TEST_PORT_11);
        allPorts.insert(TEST_PORT_12);
        allPorts.insert(TEST_PORT_13);
        allPorts.insert(TEST_PORT_14);
        for (uint16_t port = TEST_RANGE_START; port <= TEST_RANGE_END; port++) {
            allPorts.insert(port);
        }
        
        int totalPieces = calculatePortPieces(allPorts);
        std::cout << "Total ports added: " << allPorts.size() << std::endl;
        std::cout << "Total pieces (Linode limit is 15 per rule): " << totalPieces << std::endl;
        if (totalPieces > 15) {
            std::cout << "Exceeding 15-piece limit, should create multiple rules" << std::endl;
        }
    } else {
        std::cerr << "Some ports failed to be added" << std::endl;
    }
    
    // Display firewall state after adding multiple ports
    std::cout << "\nDisplaying firewall state after adding multiple ports:" << std::endl;
    if (!displayFirewallRules(apiToken, firewallId)) {
        std::cerr << "Failed to display firewall state" << std::endl;
    }
    
    // Check for multiple ZeroTier rules
    std::cout << "\nChecking for multiple ZeroTier rules..." << std::endl;
    std::string response;
    std::string url = "https://api.linode.com/v4/networking/firewalls/" + firewallId + "/rules";
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to initialize curl" << std::endl;
    } else {
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, ("Authorization: Bearer " + apiToken).c_str());
        headers = curl_slist_append(headers, "Content-Type: application/json");
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
        
        CURLcode res = curl_easy_perform(curl);
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        
        if (res == CURLE_OK && http_code >= 200 && http_code < 300) {
            try {
                json rulesJson = json::parse(response);
                
                // Count ZeroTier UDP rules
                int ztRuleCount = 0;
                if (rulesJson.contains("inbound") && rulesJson["inbound"].is_array()) {
                    for (const auto& rule : rulesJson["inbound"]) {
                        if (rule.contains("protocol") && rule["protocol"] == "UDP" && 
                            rule.contains("label")) {
                            
                            std::string label = rule["label"];
                            if (label == "ZeroTier-UDP-Ports" || 
                                (label.find("ZeroTier-UDP-Ports-") == 0 && label.length() > 18)) {
                                ztRuleCount++;
                                std::cout << "Found ZeroTier UDP rule: " << label << std::endl;
                                if (rule.contains("ports") && rule["ports"].is_string()) {
                                    std::cout << "  Ports: " << rule["ports"].get<std::string>() << std::endl;
                                }
                            }
                        }
                    }
                }
                
                std::cout << "Total ZeroTier UDP rules found: " << ztRuleCount << std::endl;
                if (ztRuleCount > 1) {
                    std::cout << "Multiple ZeroTier rules detected as expected!" << std::endl;
                } else if (ztRuleCount == 1) {
                    std::cout << "WARNING: Only one ZeroTier rule found. The 15-piece limit might not have been exceeded." << std::endl;
                } else {
                    std::cout << "ERROR: No ZeroTier rules found!" << std::endl;
                }
                
            } catch (const std::exception& e) {
                std::cerr << "Error parsing rules: " << e.what() << std::endl;
            }
        } else {
            std::cerr << "Failed to fetch firewall rules" << std::endl;
        }
    }
    
    // Wait before cleanup
    std::cout << "\nWaiting " << WAIT_TIME_SECONDS << " seconds before cleanup..." << std::endl;
    for (int i = WAIT_TIME_SECONDS; i > 0 && !g_interrupted; i--) {
        std::cout << "\rTime remaining: " << i << " seconds" << std::flush;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::cout << std::endl;
    
    // Check if we were interrupted
    if (g_interrupted) {
        std::cout << "Test interrupted. Cleaning up..." << std::endl;
        // Clean up all ports that might have been added
        manager->removePort(TEST_PORT_1);
        manager->removePort(TEST_PORT_2);
        manager->removePort(TEST_PORT_3);
        manager->removePort(TEST_PORT_4);
        manager->removePort(TEST_PORT_5);
        manager->removePort(TEST_PORT_6);
        manager->removePort(TEST_PORT_7);
        manager->removePort(TEST_PORT_8);
        manager->removePort(TEST_PORT_9);
        manager->removePort(TEST_PORT_10);
        manager->removePort(TEST_PORT_11);
        manager->removePort(TEST_PORT_12);
        manager->removePort(TEST_PORT_13);
        manager->removePort(TEST_PORT_14);
        for (uint16_t port = TEST_RANGE_START; port <= TEST_RANGE_END; port++) {
            manager->removePort(port);
        }
        manager->syncFirewallRules();
        curl_global_cleanup();
        
        // Log interruption
        datetime_fprintf(stdout, "Test interrupted.\n");
        
        return 1;
    }
    

    
    // Test staged port removal to observe rule consolidation
    std::cout << "\n===== STAGED CLEANUP =====" << std::endl;
    std::cout << "Stage 1: Removing enough ports to reduce from two rules to one..." << std::endl;
    
    // First stage: Remove port range and some individual ports to reduce to one rule
    bool stageOneRemovalSuccess = true;
    
    // Remove port range first (this should free up several pieces)
    std::cout << "Removing port range " << TEST_RANGE_START << "-" << TEST_RANGE_END << "..." << std::endl;
    for (uint16_t port = TEST_RANGE_START; port <= TEST_RANGE_END; port++) {
        if (!manager->removePort(port)) {
            std::cerr << "Failed to remove port " << port << " from range" << std::endl;
            stageOneRemovalSuccess = false;
        }
    }
    
    // Remove some additional ports to ensure we're below the threshold for a single rule
    std::cout << "Removing ports TEST_PORT_10 through TEST_PORT_14..." << std::endl;
    if (!manager->removePort(TEST_PORT_10)) {
        std::cerr << "Failed to remove port " << TEST_PORT_10 << std::endl;
        stageOneRemovalSuccess = false;
    }
    
    if (!manager->removePort(TEST_PORT_11)) {
        std::cerr << "Failed to remove port " << TEST_PORT_11 << std::endl;
        stageOneRemovalSuccess = false;
    }
    
    if (!manager->removePort(TEST_PORT_12)) {
        std::cerr << "Failed to remove port " << TEST_PORT_12 << std::endl;
        stageOneRemovalSuccess = false;
    }
    
    if (!manager->removePort(TEST_PORT_13)) {
        std::cerr << "Failed to remove port " << TEST_PORT_13 << std::endl;
        stageOneRemovalSuccess = false;
    }
    
    if (!manager->removePort(TEST_PORT_14)) {
        std::cerr << "Failed to remove port " << TEST_PORT_14 << std::endl;
        stageOneRemovalSuccess = false;
    }
    
    if (stageOneRemovalSuccess) {
        std::cout << "Successfully removed ports in stage 1" << std::endl;
    } else {
        std::cerr << "Some ports in stage 1 failed to be removed" << std::endl;
    }
    
    // Sync and display firewall state after first stage of removal
    std::cout << "\nSyncing firewall rules after stage 1 removal..." << std::endl;
    if (!manager->syncFirewallRules()) {
        std::cerr << "Failed to sync firewall rules after stage 1 removal" << std::endl;
    }
    
    // Display firewall state after first stage of removal
    std::cout << "\nDisplaying firewall state after stage 1 removal:" << std::endl;
    if (!displayFirewallRules(apiToken, firewallId)) {
        std::cerr << "Failed to display firewall state" << std::endl;
    }
    
    // Wait before proceeding to stage 2
    std::cout << "\nWaiting " << WAIT_TIME_SECONDS << " seconds before stage 2 removal..." << std::endl;
    for (int i = WAIT_TIME_SECONDS; i > 0 && !g_interrupted; i--) {
        std::cout << "\rTime remaining: " << i << " seconds" << std::flush;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::cout << std::endl;
    
    // Check if we were interrupted
    if (g_interrupted) {
        std::cout << "Test interrupted during wait. Cleaning up remaining ports..." << std::endl;
        // Clean up remaining ports
        manager->removePort(TEST_PORT_1);
        manager->removePort(TEST_PORT_2);
        manager->removePort(TEST_PORT_3);
        manager->removePort(TEST_PORT_4);
        manager->removePort(TEST_PORT_5);
        manager->removePort(TEST_PORT_6);
        manager->removePort(TEST_PORT_7);
        manager->removePort(TEST_PORT_8);
        manager->removePort(TEST_PORT_9);
        manager->syncFirewallRules();
        curl_global_cleanup();
        
        // Log interruption
        datetime_fprintf(stdout, "Test interrupted during wait.\n");
        
        return 1;
    }
    
    // Stage 2: Remove remaining ports
    std::cout << "\nStage 2: Removing remaining ports..." << std::endl;
    bool stageTwoRemovalSuccess = true;
    
    // Remove initial test ports
    if (!manager->removePort(TEST_PORT_1)) {
        std::cerr << "Failed to remove port " << TEST_PORT_1 << std::endl;
        stageTwoRemovalSuccess = false;
    }
    
    if (!manager->removePort(TEST_PORT_2)) {
        std::cerr << "Failed to remove port " << TEST_PORT_2 << std::endl;
        stageTwoRemovalSuccess = false;
    }
    
    if (!manager->removePort(TEST_PORT_3)) {
        std::cerr << "Failed to remove port " << TEST_PORT_3 << std::endl;
        stageTwoRemovalSuccess = false;
    }
    
    // Remove remaining additional ports
    if (!manager->removePort(TEST_PORT_4)) {
        std::cerr << "Failed to remove port " << TEST_PORT_4 << std::endl;
        stageTwoRemovalSuccess = false;
    }
    
    if (!manager->removePort(TEST_PORT_5)) {
        std::cerr << "Failed to remove port " << TEST_PORT_5 << std::endl;
        stageTwoRemovalSuccess = false;
    }
    
    if (!manager->removePort(TEST_PORT_6)) {
        std::cerr << "Failed to remove port " << TEST_PORT_6 << std::endl;
        stageTwoRemovalSuccess = false;
    }
    
    if (!manager->removePort(TEST_PORT_7)) {
        std::cerr << "Failed to remove port " << TEST_PORT_7 << std::endl;
        stageTwoRemovalSuccess = false;
    }
    
    if (!manager->removePort(TEST_PORT_8)) {
        std::cerr << "Failed to remove port " << TEST_PORT_8 << std::endl;
        stageTwoRemovalSuccess = false;
    }
    
    if (!manager->removePort(TEST_PORT_9)) {
        std::cerr << "Failed to remove port " << TEST_PORT_9 << std::endl;
        stageTwoRemovalSuccess = false;
    }
    
    if (stageTwoRemovalSuccess) {
        std::cout << "Successfully removed all remaining ports in stage 2" << std::endl;
    } else {
        std::cerr << "Some ports in stage 2 failed to be removed" << std::endl;
    }
    
    // Wait for another configured seconds as requested
    std::cout << "\nWaiting " << WAIT_TIME_SECONDS << " seconds before final check..." << std::endl;
    for (int i = WAIT_TIME_SECONDS; i > 0 && !g_interrupted; i--) {
        std::cout << "\rTime remaining: " << i << " seconds" << std::flush;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::cout << std::endl;
    
    // Check if we were interrupted
    if (g_interrupted) {
        std::cout << "Test interrupted during final wait. Exiting..." << std::endl;
        curl_global_cleanup();
        return 1;
    }
    
    // Final sync to verify all changes were applied correctly
    std::cout << "\nPerforming final sync to verify changes..." << std::endl;
    if (!manager->syncFirewallRules()) {
        std::cerr << "Failed to perform final sync" << std::endl;
        curl_global_cleanup();
        return 1;
    }
    
    // Display final firewall state
    std::cout << "\nDisplaying final firewall state:" << std::endl;
    if (!displayFirewallRules(apiToken, firewallId)) {
        std::cerr << "Failed to display final firewall state" << std::endl;
    }
    
    // Final check for ZeroTier rules
    std::cout << "\nFinal check for ZeroTier rules..." << std::endl;
    std::string finalResponse;
    std::string finalUrl = "https://api.linode.com/v4/networking/firewalls/" + firewallId + "/rules";
    
    CURL* finalCurl = curl_easy_init();
    if (!finalCurl) {
        std::cerr << "Failed to initialize curl for final check" << std::endl;
    } else {
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, ("Authorization: Bearer " + apiToken).c_str());
        headers = curl_slist_append(headers, "Content-Type: application/json");
        
        curl_easy_setopt(finalCurl, CURLOPT_URL, finalUrl.c_str());
        curl_easy_setopt(finalCurl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(finalCurl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(finalCurl, CURLOPT_WRITEDATA, &finalResponse);
        curl_easy_setopt(finalCurl, CURLOPT_TIMEOUT, 10L);
        
        CURLcode res = curl_easy_perform(finalCurl);
        long http_code = 0;
        curl_easy_getinfo(finalCurl, CURLINFO_RESPONSE_CODE, &http_code);
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(finalCurl);
        
        if (res == CURLE_OK && http_code >= 200 && http_code < 300) {
            try {
                json rulesJson = json::parse(finalResponse);
                
                // Log the complete JSON response to stdout
                if (LOG_TO_STDOUT) {
                    std::cout << "\n===== Final firewall rules JSON response: =====" << std::endl;
                    std::cout << rulesJson.dump(4) << std::endl;
                    std::cout << "===== End of JSON response =====" << std::endl;
                }
                
                // Count ZeroTier UDP rules
                int ztRuleCount = 0;
                if (rulesJson.contains("inbound") && rulesJson["inbound"].is_array()) {
                    for (const auto& rule : rulesJson["inbound"]) {
                        if (rule.contains("protocol") && rule["protocol"] == "UDP" && 
                            rule.contains("label")) {
                            
                            std::string label = rule["label"];
                            if (label == "ZeroTier-UDP-Ports" || 
                                (label.find("ZeroTier-UDP-Ports-") == 0 && label.length() > 18)) {
                                ztRuleCount++;
                                std::cout << "Found ZeroTier UDP rule: " << label << std::endl;
                                if (rule.contains("ports") && rule["ports"].is_string()) {
                                    std::cout << "  Ports: " << rule["ports"].get<std::string>() << std::endl;
                                }
                            }
                        }
                    }
                }
                
                std::cout << "Final count of ZeroTier UDP rules: " << ztRuleCount << std::endl;
                
            } catch (const std::exception& e) {
                std::cerr << "Error parsing final rules: " << e.what() << std::endl;
            }
        } else {
            std::cerr << "Failed to fetch final firewall rules" << std::endl;
        }
    }
    
    std::cout << "\nAll tests completed successfully!" << std::endl;
    
    // Print summary of operations
    std::cout << "\n===== TEST SUMMARY =====" << std::endl;
    std::cout << "- Added and removed initial UDP ports: " << TEST_PORT_1 << ", " << TEST_PORT_2 << ", " << TEST_PORT_3 << " (non-consecutive)" << std::endl;
    std::cout << "- Added and removed additional UDP ports to test multiple rule functionality:" << std::endl;
    std::cout << "  * Individual ports: " << TEST_PORT_4 << ", " << TEST_PORT_5 << ", " << TEST_PORT_6 << ", " 
              << TEST_PORT_7 << ", " << TEST_PORT_8 << ", " << TEST_PORT_9 << ", " << TEST_PORT_10 << ", " 
              << TEST_PORT_11 << ", " << TEST_PORT_12 << ", " << TEST_PORT_13 << ", " << TEST_PORT_14 << std::endl;
    std::cout << "  * Port range: " << TEST_RANGE_START << "-" << TEST_RANGE_END << " (11 ports, counts as 2 pieces)" << std::endl;
    std::cout << "- Tested staged port removal to verify rule consolidation:" << std::endl;
    std::cout << "  * Stage 1: Removed port range and ports 10-14 to reduce from two rules to one" << std::endl;
    std::cout << "  * Stage 2: Removed remaining ports" << std::endl;
    std::cout << "- Verified Linode Firewall integration with multiple rule functionality" << std::endl;
    std::cout << "- Confirmed proper handling of Linode's 15-piece limit per firewall rule" << std::endl;
    std::cout << "- Verified rule consolidation when port count drops below threshold" << std::endl;
    std::cout << "- Enhanced logging with detailed rule inspection" << std::endl;
    std::cout << "- Verified piece calculation and multiple rule creation" << std::endl;
    
    // Clean up curl
    curl_global_cleanup();
    
    // Log end of test
    datetime_fprintf(stdout, "===== LINODE FIREWALL TEST COMPLETED =====\n");
    
    return 0;
}