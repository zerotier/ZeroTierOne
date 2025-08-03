/**
 * test_cloud_firewall_template.cpp
 * 
 * Template for creating cloud provider-specific firewall manager tests
 * This file serves as a starting point for testing new cloud provider implementations
 */

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
// Include your cloud provider's header if needed for specific testing
// #include "YourCloudProviderFirewallManager.hpp"

// For JSON parsing
using json = nlohmann::json;

// Global variables for signal handling
static std::atomic<bool> running(true);

// Test ports
const uint16_t TEST_PORT_1 = 9993;
const uint16_t TEST_PORT_2 = 9994;
const uint16_t TEST_PORT_3 = 9995;

// Test port ranges
const uint16_t TEST_RANGE_START_1 = 10000;
const uint16_t TEST_RANGE_END_1 = 10010;
const uint16_t TEST_RANGE_START_2 = 10020;
const uint16_t TEST_RANGE_END_2 = 10030;

// Utility function for logging with timestamp
void datetime_fprintf(FILE *stream, const char *fmt, ...) {
    time_t t = time(0);
    struct tm *tm = localtime(&t);
    char datetime[64];
    strftime(datetime, sizeof(datetime), "%Y-%m-%d %H:%M:%S", tm);
    fprintf(stream, "[%s] ", datetime);
    
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stream, fmt, ap);
    va_end(ap);
    fflush(stream);
}

// Callback function for curl HTTP requests
size_t WriteCallback(void *contents, size_t size, size_t nmemb, std::string *s) {
    size_t newLength = size * nmemb;
    try {
        s->append((char*)contents, newLength);
        return newLength;
    } catch(std::bad_alloc &e) {
        // Handle memory problem
        return 0;
    }
}

// Function to display current firewall rules (customize for your cloud provider)
void displayFirewallRules(const std::string& apiToken, const std::string& firewallId) {
    // This is a placeholder - implement according to your cloud provider's API
    datetime_fprintf(stdout, "Displaying current firewall rules...\n");
    
    CURL *curl;
    CURLcode res;
    std::string readBuffer;
    
    curl = curl_easy_init();
    if(curl) {
        // Replace with your cloud provider's API endpoint
        std::string url = "https://api.yourcloudprovider.com/v1/firewalls/" + firewallId;
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        
        // Set headers including authorization
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        std::string authHeader = "Authorization: Bearer " + apiToken;
        headers = curl_slist_append(headers, authHeader.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        
        res = curl_easy_perform(curl);
        
        if(res != CURLE_OK) {
            datetime_fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        } else {
            // Parse and display the rules
            try {
                json j = json::parse(readBuffer);
                // Customize this part based on your cloud provider's API response format
                datetime_fprintf(stdout, "Current firewall rules: %s\n", j.dump(2).c_str());
            } catch (json::parse_error& e) {
                datetime_fprintf(stderr, "JSON parse error: %s\n", e.what());
            }
        }
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
}

// Signal handler for graceful termination
void signalHandler(int signum) {
    datetime_fprintf(stdout, "Interrupt signal (%d) received. Cleaning up...\n", signum);
    running = false;
}

int main(int argc, char *argv[]) {
    // Register signal handler
    signal(SIGINT, signalHandler);
    
    // Check command line arguments
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <cloud_provider> <api_token> <firewall_id>" << std::endl;
        return 1;
    }
    
    std::string cloudProvider = argv[1];
    std::string apiToken = argv[2];
    std::string firewallId = argv[3];
    
    datetime_fprintf(stdout, "Starting firewall manager test for %s provider\n", cloudProvider.c_str());
    datetime_fprintf(stdout, "Firewall ID: %s\n", firewallId.c_str());
    
    // Initialize curl
    curl_global_init(CURL_GLOBAL_ALL);
    
    // Create firewall manager using factory
    auto manager = FirewallManagerFactory::createFirewallManager(cloudProvider, apiToken, firewallId);
    
    if (!manager) {
        datetime_fprintf(stderr, "Failed to create firewall manager for provider: %s\n", cloudProvider.c_str());
        curl_global_cleanup();
        return 1;
    }
    
    // Initialize the manager
    if (!manager->initialize()) {
        datetime_fprintf(stderr, "Failed to initialize firewall manager\n");
        curl_global_cleanup();
        return 1;
    }
    
    datetime_fprintf(stdout, "Firewall manager initialized successfully\n");
    
    // Display initial firewall rules
    displayFirewallRules(apiToken, firewallId);
    
    // Test 1: Add a single port
    datetime_fprintf(stdout, "\n=== Test 1: Adding single port %d ===\n", TEST_PORT_1);
    if (manager->addPort(TEST_PORT_1)) {
        datetime_fprintf(stdout, "Successfully added port %d\n", TEST_PORT_1);
    } else {
        datetime_fprintf(stderr, "Failed to add port %d\n", TEST_PORT_1);
    }
    
    // Sync rules after adding port
    datetime_fprintf(stdout, "Syncing firewall rules...\n");
    if (manager->syncFirewallRules()) {
        datetime_fprintf(stdout, "Successfully synced firewall rules\n");
    } else {
        datetime_fprintf(stderr, "Failed to sync firewall rules\n");
    }
    
    // Display rules after adding port
    displayFirewallRules(apiToken, firewallId);
    
    // Test 2: Remove the port
    datetime_fprintf(stdout, "\n=== Test 2: Removing port %d ===\n", TEST_PORT_1);
    if (manager->removePort(TEST_PORT_1)) {
        datetime_fprintf(stdout, "Successfully removed port %d\n", TEST_PORT_1);
    } else {
        datetime_fprintf(stderr, "Failed to remove port %d\n", TEST_PORT_1);
    }
    
    // Sync rules after removing port
    datetime_fprintf(stdout, "Syncing firewall rules...\n");
    if (manager->syncFirewallRules()) {
        datetime_fprintf(stdout, "Successfully synced firewall rules\n");
    } else {
        datetime_fprintf(stderr, "Failed to sync firewall rules\n");
    }
    
    // Display rules after removing port
    displayFirewallRules(apiToken, firewallId);
    
    // Test 3: Add multiple ports
    datetime_fprintf(stdout, "\n=== Test 3: Adding multiple ports (%d, %d) ===\n", TEST_PORT_2, TEST_PORT_3);
    if (manager->addPort(TEST_PORT_2) && manager->addPort(TEST_PORT_3)) {
        datetime_fprintf(stdout, "Successfully added ports %d and %d\n", TEST_PORT_2, TEST_PORT_3);
    } else {
        datetime_fprintf(stderr, "Failed to add ports %d and %d\n", TEST_PORT_2, TEST_PORT_3);
    }
    
    // Sync rules after adding multiple ports
    datetime_fprintf(stdout, "Syncing firewall rules...\n");
    if (manager->syncFirewallRules()) {
        datetime_fprintf(stdout, "Successfully synced firewall rules\n");
    } else {
        datetime_fprintf(stderr, "Failed to sync firewall rules\n");
    }
    
    // Display rules after adding multiple ports
    displayFirewallRules(apiToken, firewallId);
    
    // Test 4: Add port ranges
    datetime_fprintf(stdout, "\n=== Test 4: Adding port ranges ===\n");
    for (uint16_t port = TEST_RANGE_START_1; port <= TEST_RANGE_END_1; port++) {
        if (manager->addPort(port)) {
            datetime_fprintf(stdout, "Added port %d\n", port);
        } else {
            datetime_fprintf(stderr, "Failed to add port %d\n", port);
        }
    }
    
    // Sync rules after adding port range
    datetime_fprintf(stdout, "Syncing firewall rules...\n");
    if (manager->syncFirewallRules()) {
        datetime_fprintf(stdout, "Successfully synced firewall rules\n");
    } else {
        datetime_fprintf(stderr, "Failed to sync firewall rules\n");
    }
    
    // Display rules after adding port range
    displayFirewallRules(apiToken, firewallId);
    
    // Test 5: Add another port range to test rule consolidation
    datetime_fprintf(stdout, "\n=== Test 5: Adding another port range to test rule consolidation ===\n");
    for (uint16_t port = TEST_RANGE_START_2; port <= TEST_RANGE_END_2; port++) {
        if (manager->addPort(port)) {
            datetime_fprintf(stdout, "Added port %d\n", port);
        } else {
            datetime_fprintf(stderr, "Failed to add port %d\n", port);
        }
    }
    
    // Sync rules after adding second port range
    datetime_fprintf(stdout, "Syncing firewall rules...\n");
    if (manager->syncFirewallRules()) {
        datetime_fprintf(stdout, "Successfully synced firewall rules\n");
    } else {
        datetime_fprintf(stderr, "Failed to sync firewall rules\n");
    }
    
    // Display rules after adding second port range
    displayFirewallRules(apiToken, firewallId);
    
    // Test 6: Cleanup - remove all ports
    datetime_fprintf(stdout, "\n=== Test 6: Cleanup - removing all ports ===\n");
    
    // Remove individual ports
    if (manager->removePort(TEST_PORT_2) && manager->removePort(TEST_PORT_3)) {
        datetime_fprintf(stdout, "Successfully removed ports %d and %d\n", TEST_PORT_2, TEST_PORT_3);
    } else {
        datetime_fprintf(stderr, "Failed to remove ports %d and %d\n", TEST_PORT_2, TEST_PORT_3);
    }
    
    // Remove port ranges
    for (uint16_t port = TEST_RANGE_START_1; port <= TEST_RANGE_END_1; port++) {
        if (manager->removePort(port)) {
            datetime_fprintf(stdout, "Removed port %d\n", port);
        } else {
            datetime_fprintf(stderr, "Failed to remove port %d\n", port);
        }
    }
    
    for (uint16_t port = TEST_RANGE_START_2; port <= TEST_RANGE_END_2; port++) {
        if (manager->removePort(port)) {
            datetime_fprintf(stdout, "Removed port %d\n", port);
        } else {
            datetime_fprintf(stderr, "Failed to remove port %d\n", port);
        }
    }
    
    // Final sync to apply all removals
    datetime_fprintf(stdout, "Final sync to apply all removals...\n");
    if (manager->syncFirewallRules()) {
        datetime_fprintf(stdout, "Successfully synced firewall rules\n");
    } else {
        datetime_fprintf(stderr, "Failed to sync firewall rules\n");
    }
    
    // Display final firewall rules
    displayFirewallRules(apiToken, firewallId);
    
    // Cleanup
    curl_global_cleanup();
    
    datetime_fprintf(stdout, "\n=== Test Summary ===\n");
    datetime_fprintf(stdout, "All tests completed for %s provider\n", cloudProvider.c_str());
    
    return 0;
}