# CloudFirewallManager Interface

This document provides a detailed explanation of the `CloudFirewallManager` abstract base class interface. Understanding this interface is crucial for implementing support for new cloud providers in the ZeroTier TCP Proxy's cloud-agnostic firewall management system.

## Interface Overview

The `CloudFirewallManager` abstract base class defines a common interface that all cloud provider implementations must follow. This ensures that the TCP proxy can interact with different cloud providers in a consistent manner.

## Required Methods

Each cloud provider implementation must override the following methods:

### `bool initialize()`

**Purpose:** Initialize the firewall manager and verify that it can communicate with the cloud provider's API.

**Implementation Requirements:**
- Verify that the provided API credentials are valid
- Check that the specified firewall/security group exists
- Establish any necessary connections or sessions with the cloud provider's API
- Set up any required state for subsequent operations

**Return Value:**
- `true` if initialization was successful
- `false` if there was an error (invalid credentials, non-existent firewall, etc.)

**Example Implementation:**
```cpp
bool YourCloudProviderFirewallManager::initialize() {
    // Log initialization attempt
    datetime_fprintf(stdout, "Initializing Your Cloud Provider firewall manager...\n");
    
    // Make an API call to verify credentials and firewall existence
    std::string response;
    if (!makeApiRequest("/firewalls/" + firewallId, "GET", "", response)) {
        datetime_fprintf(stderr, "Failed to verify firewall existence\n");
        return false;
    }
    
    // Parse response to verify firewall exists
    try {
        json j = json::parse(response);
        // Verify the response indicates the firewall exists
        
        datetime_fprintf(stdout, "Successfully initialized Your Cloud Provider firewall manager\n");
        return true;
    } catch (json::parse_error& e) {
        datetime_fprintf(stderr, "JSON parse error: %s\n", e.what());
        return false;
    }
}
```

### `bool addPort(uint16_t port)`

**Purpose:** Add a UDP port to the set of ports that should be allowed through the firewall.

**Implementation Requirements:**
- Add the port to an internal data structure (e.g., a set of active ports)
- Do NOT make API calls to update the firewall rules immediately
- Changes should be applied when `syncFirewallRules()` is called

**Parameters:**
- `port`: The UDP port number to add (0-65535)

**Return Value:**
- `true` if the port was successfully added to the managed set
- `false` if there was an error

**Example Implementation:**
```cpp
bool YourCloudProviderFirewallManager::addPort(uint16_t port) {
    datetime_fprintf(stdout, "Adding port %d to managed set\n", port);
    activePorts.insert(port);
    return true;
}
```

### `bool removePort(uint16_t port)`

**Purpose:** Remove a UDP port from the set of ports that should be allowed through the firewall.

**Implementation Requirements:**
- Remove the port from the internal data structure
- Do NOT make API calls to update the firewall rules immediately
- Changes should be applied when `syncFirewallRules()` is called

**Parameters:**
- `port`: The UDP port number to remove (0-65535)

**Return Value:**
- `true` if the port was successfully removed from the managed set
- `false` if there was an error

**Example Implementation:**
```cpp
bool YourCloudProviderFirewallManager::removePort(uint16_t port) {
    datetime_fprintf(stdout, "Removing port %d from managed set\n", port);
    activePorts.erase(port);
    return true;
}
```

### `bool syncFirewallRules()`

**Purpose:** Synchronize the current set of managed ports with the cloud provider's firewall rules.

**Implementation Requirements:**
- Make API calls to update the firewall rules based on the current set of active ports
- Handle any provider-specific rule formatting or limitations
- Implement efficient rule updates (e.g., consolidate adjacent ports into ranges)
- Handle API rate limiting and errors

**Return Value:**
- `true` if the firewall rules were successfully synchronized
- `false` if there was an error

**Example Implementation:**
```cpp
bool YourCloudProviderFirewallManager::syncFirewallRules() {
    datetime_fprintf(stdout, "Synchronizing firewall rules with Your Cloud Provider...\n");
    
    // Fetch current rules to avoid unnecessary updates
    if (!fetchCurrentRules()) {
        datetime_fprintf(stderr, "Failed to fetch current firewall rules\n");
        return false;
    }
    
    // Prepare the update payload based on the provider's API requirements
    json payload;
    
    // Convert individual ports to ranges where possible
    std::vector<std::pair<uint16_t, uint16_t>> ranges = consolidatePortRanges(activePorts);
    
    // Add each range to the payload
    json rules = json::array();
    for (const auto& range : ranges) {
        json rule;
        rule["protocol"] = "udp";
        if (range.first == range.second) {
            rule["port"] = range.first;
        } else {
            rule["portRange"] = {
                {"from", range.first},
                {"to", range.second}
            };
        }
        rule["description"] = "ZeroTier UDP Port";
        rules.push_back(rule);
    }
    
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
```

## Helper Methods

In addition to the required methods, you will likely need to implement several helper methods specific to your cloud provider:

### API Request Handling

Implement a method to handle API requests to the cloud provider:

```cpp
bool YourCloudProviderFirewallManager::makeApiRequest(
    const std::string& endpoint, 
    const std::string& method, 
    const std::string& data, 
    std::string& response) {
    
    // Implementation using libcurl or another HTTP client library
}
```

### Port Range Consolidation

Implement a method to consolidate adjacent ports into ranges to minimize the number of firewall rules:

```cpp
std::vector<std::pair<uint16_t, uint16_t>> YourCloudProviderFirewallManager::consolidatePortRanges(
    const std::set<uint16_t>& ports) {
    
    std::vector<std::pair<uint16_t, uint16_t>> ranges;
    
    // Implementation to convert individual ports to ranges
    
    return ranges;
}
```

## Best Practices

1. **Error Handling**: Implement robust error handling for API calls and other operations.

2. **Logging**: Use the `datetime_fprintf` function for consistent logging.

3. **Rate Limiting**: Be aware of the cloud provider's API rate limits and implement appropriate backoff strategies.

4. **Idempotency**: Ensure operations are idempotent where possible to avoid duplicate rules.

5. **Rule Limits**: Be aware of provider-specific limits on rule complexity or count.

6. **Authentication**: Handle authentication securely and refresh tokens if necessary.

7. **Cleanup**: Properly clean up resources in the destructor.

## Testing

Thoroughly test your implementation with various scenarios:

1. Adding and removing individual ports
2. Adding and removing port ranges
3. Handling API errors and rate limiting
4. Recovering from network interruptions
5. Handling edge cases (e.g., maximum number of rules)

## Example Implementation

Refer to the `LinodeFirewallManager` implementation as a reference for how to implement the interface for your cloud provider.