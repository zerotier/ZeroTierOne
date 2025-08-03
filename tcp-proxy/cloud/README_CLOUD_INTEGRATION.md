# Cloud Firewall Manager Integration Guide

This document provides a comprehensive guide to the cloud-agnostic firewall management architecture implemented in the ZeroTier TCP Proxy. It explains how to integrate new cloud providers beyond the existing Linode implementation.

## Architecture Overview

The cloud firewall management system uses an abstract base class and factory pattern to support multiple cloud providers while maintaining a consistent interface. This allows the TCP proxy to manage firewall rules across different cloud platforms without changing the core application logic.

### Key Components

1. **CloudFirewallManager** - Abstract base class defining the interface for all cloud firewall managers
2. **LinodeFirewallManager** - Concrete implementation for Linode Cloud
3. **FirewallManagerFactory** - Factory class for creating provider-specific implementations

## File Structure

```
tcp-proxy/
├── cloud/
│   ├── README_CLOUD_INTEGRATION.md    # This documentation file
│   └── provider/
│       ├── CloudFirewallManager.hpp    # Abstract base class
│       ├── FirewallManagerFactory.hpp  # Factory class header
│       ├── FirewallManagerFactory.cpp  # Factory class implementation
│       ├── LinodeFirewallManager.hpp   # Linode-specific implementation header
│       ├── LinodeFirewallManager.cpp   # Linode-specific implementation
│       ├── tests/                      # Test files directory
│       │   ├── test_linode_firewall.cpp        # Linode test implementation
│       │   └── test_cloud_firewall_template.cpp # Template for new provider tests
│       ├── YourCloudProvider.hpp       # Your new cloud provider header
│       └── YourCloudProvider.cpp       # Your new cloud provider implementation
└── tcp-proxy.cpp                       # Main application using the factory
```

## Adding a New Cloud Provider

To add support for a new cloud provider, follow these steps:

### 1. Create Provider-Specific Implementation Files

Create two new files for your cloud provider (e.g., `AwsFirewallManager.hpp` and `AwsFirewallManager.cpp`).

#### Header File Template (`YourCloudProvider.hpp`)

```cpp
#ifndef YOUR_CLOUD_PROVIDER_FIREWALL_MANAGER_HPP
#define YOUR_CLOUD_PROVIDER_FIREWALL_MANAGER_HPP

#include <string>
#include <set>
#include "CloudFirewallManager.hpp"

class YourCloudProviderFirewallManager : public CloudFirewallManager {
private:
    // Provider-specific member variables
    std::string apiToken;
    std::string firewallId;
    uint16_t internalTcpPort; // TCP port the proxy listens on internally
    uint16_t externalTcpPort; // TCP port exposed in firewall rules (may be different from internal port)
    std::set<uint16_t> activePorts;
    
    // Private helper methods specific to your cloud provider
    bool fetchCurrentRules();
    // Add other helper methods as needed

public:
    // Constructor
    YourCloudProviderFirewallManager(
        const std::string& apiToken, 
        const std::string& firewallId,
        uint16_t internalTcpPort,
        uint16_t externalTcpPort = 0, // Default to 0, which means use internalTcpPort
        const std::string& additionalParams = "");
    
    // Implementation of abstract methods from CloudFirewallManager
    bool initialize() override;
    bool addPort(uint16_t port) override;
    bool removePort(uint16_t port) override;
    bool syncFirewallRules() override;
};

#endif // YOUR_CLOUD_PROVIDER_FIREWALL_MANAGER_HPP
```

#### Implementation File Template (`YourCloudProvider.cpp`)

```cpp
#include "YourCloudProviderFirewallManager.hpp"
#include <curl/curl.h>
#include <iostream>
#include <sstream>

// External function for logging (defined in tcp-proxy.cpp)
extern void datetime_fprintf(FILE *stream, const char *fmt, ...);

// Constructor
YourCloudProviderFirewallManager::YourCloudProviderFirewallManager(
    const std::string& apiToken, 
    const std::string& firewallId,
    uint16_t internalTcpPort,
    uint16_t externalTcpPort = 0, // Default to 0, which means use internalTcpPort
    const std::string& additionalParams = "")
    : apiToken(apiToken), firewallId(firewallId),
      internalTcpPort(internalTcpPort),
      externalTcpPort(externalTcpPort ? externalTcpPort : internalTcpPort) {
    // Initialization code
}

// Initialize the firewall manager
bool YourCloudProviderFirewallManager::initialize() {
    // Implementation specific to your cloud provider
    // Verify credentials, check firewall existence, etc.
    return true; // Return success/failure
}

// Add a port to the managed set
bool YourCloudProviderFirewallManager::addPort(uint16_t port) {
    // Add port to the set of active ports
    activePorts.insert(port);
    return true;
}

// Remove a port from the managed set
bool YourCloudProviderFirewallManager::removePort(uint16_t port) {
    // Remove port from the set of active ports
    activePorts.erase(port);
    return true;
}

// Synchronize the current set of ports with the cloud provider
bool YourCloudProviderFirewallManager::syncFirewallRules() {
    // Implementation specific to your cloud provider
    // This is where you'll make API calls to update firewall rules
    return true;
}

// Implement any private helper methods
bool YourCloudProviderFirewallManager::fetchCurrentRules() {
    // Implementation specific to your cloud provider
    return true;
}
```

### 2. Update the FirewallManagerFactory

Modify `FirewallManagerFactory.hpp` and `FirewallManagerFactory.cpp` to include your new provider:

#### Update FirewallManagerFactory.hpp

```cpp
// Add include for your new provider
#include "YourCloudProviderFirewallManager.hpp"
```

#### Update FirewallManagerFactory.cpp

```cpp
std::unique_ptr<CloudFirewallManager> FirewallManagerFactory::createFirewallManager(
    const std::string& provider,
    const std::string& apiToken,
    const std::string& firewallId,
    uint16_t internalTcpPort,
    uint16_t externalTcpPort = 0, // Default to 0, which means use internalTcpPort
    const std::string& additionalParams = "") {
    
    if (provider == "linode") {
        return std::unique_ptr<CloudFirewallManager>(new LinodeFirewallManager(apiToken, firewallId, internalTcpPort, externalTcpPort));
    } else if (provider == "yourprovider") {
        // Add your provider here
        return std::unique_ptr<CloudFirewallManager>(new YourCloudProviderFirewallManager(apiToken, firewallId, internalTcpPort, externalTcpPort, additionalParams));
    } else {
        // Unknown provider
        return nullptr;
    }
}
```

### 3. Update the Makefile

Add your new source files to the Makefile:

```makefile
tcp-proxy: tcp-proxy.cpp LinodeFirewallManager.cpp YourCloudProviderFirewallManager.cpp FirewallManagerFactory.cpp ../node/Metrics.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o tcp-proxy tcp-proxy.cpp LinodeFirewallManager.cpp YourCloudProviderFirewallManager.cpp FirewallManagerFactory.cpp ../node/Metrics.cpp -lcurl

test_linode_firewall: test_linode_firewall.cpp LinodeFirewallManager.cpp YourCloudProviderFirewallManager.cpp FirewallManagerFactory.cpp
	$(CXX) $(TEST_CXXFLAGS) $(INCLUDES) -o test_linode_firewall test_linode_firewall.cpp LinodeFirewallManager.cpp YourCloudProviderFirewallManager.cpp FirewallManagerFactory.cpp -lcurl
```

### 4. Update Configuration Documentation

Update `local.conf.cloud_examples` to include your provider's configuration options. The existing file already contains examples for different cloud providers, and you should add your provider's specific configuration following the same pattern:

```json
{
  "// Example: YourProvider configuration":"",
  "// Replace the above cloud settings with these for YourProvider":"",
  "// tcpPort": 8443,
  "// externalTcpPort": 443,
  "// cloudProvider": "yourprovider",
  "// cloudApiToken": "your_provider_api_token",
  "// cloudFirewallId": "your_firewall_id",
  "// cloudAdditionalParams": {
  "//   "specificParam1": "value1",
  "//   "specificParam2": "value2"
  "// }
}
```

Note that `local.conf.example` should remain focused on basic TCP proxy configuration, while cloud-specific examples belong in `local.conf.cloud_examples`.

### 5. Create Provider-Specific Tests (Optional)

You should create a test file for your provider based on the template in the tests directory:

1. Copy the template file as a starting point:
```bash
cp tcp-proxy/cloud/provider/tests/test_cloud_firewall_template.cpp tcp-proxy/cloud/provider/tests/test_yourprovider_firewall.cpp
```

2. Modify the test file to include your provider-specific header and test cases

3. Add a build target to the Makefile:
```makefile
test_yourprovider_firewall:
	$(CXX) -O3 $(INCLUDES) -std=c++11 -pthread -o test_yourprovider_firewall cloud/provider/tests/test_yourprovider_firewall.cpp cloud/provider/YourProviderFirewallManager.cpp cloud/provider/FirewallManagerFactory.cpp $(LIBS)
```

4. Run your tests:
```bash
make test_yourprovider_firewall
./test_yourprovider_firewall
```

## Implementation Guidelines

### API Interaction

Most cloud providers offer REST APIs for firewall management. Use libcurl (already included in the project) for making HTTP requests. Follow these general steps:

1. **Authentication**: Implement proper authentication using the provided API token
2. **Error Handling**: Implement robust error handling for API responses
3. **Rate Limiting**: Be mindful of API rate limits and implement appropriate backoff strategies
4. **Idempotency**: Ensure operations are idempotent where possible

### Firewall Rule Management

The core functionality revolves around managing UDP ports in firewall rules:

1. **Rule Naming**: Use a consistent naming convention (e.g., "ZeroTier-UDP-Ports")
2. **Port Handling**: Efficiently manage port ranges to minimize API calls
3. **Rule Limits**: Be aware of provider-specific limits on rule complexity

### Testing

Thoroughly test your implementation:

1. **Basic Functionality**: Test adding/removing ports and syncing rules
2. **Edge Cases**: Test behavior with many ports, rapid changes, etc.
3. **Error Conditions**: Test behavior when API calls fail

## Common Challenges

### Provider-Specific Rule Formats

Each cloud provider has its own format for firewall rules. You'll need to adapt the generic port management to your provider's specific rule format.

### Rule Consolidation

Some providers have limits on the number of rules or rule complexity. Implement strategies to consolidate rules when possible (e.g., combining adjacent ports into ranges).

### API Limitations

Be aware of API rate limits, authentication requirements, and other provider-specific limitations.

## Example: AWS Implementation Outline

Here's a brief outline of what an AWS implementation might look like:

```cpp
// AwsFirewallManager.hpp
class AwsFirewallManager : public CloudFirewallManager {
private:
    std::string accessKey;
    std::string secretKey;
    std::string securityGroupId;
    std::string region;
    std::set<uint16_t> activePorts;
    
    // AWS-specific helper methods
    bool authorizeSecurityGroupIngress(uint16_t port);
    bool revokeSecurityGroupIngress(uint16_t port);
    
public:
    // Constructor with AWS-specific parameters
    AwsFirewallManager(const std::string& accessKey, const std::string& secretKey, 
                      const std::string& securityGroupId, const std::string& region);
    
    // Implementation of abstract methods
    bool initialize() override;
    bool addPort(uint16_t port) override;
    bool removePort(uint16_t port) override;
    bool syncFirewallRules() override;
};
```

You would then need to adapt the factory to handle the different parameter requirements for AWS.

## Conclusion

By following this guide, you can extend the ZeroTier TCP Proxy's cloud firewall management to support additional providers. The abstract interface ensures that the core application logic remains unchanged while allowing for provider-specific implementations.

Remember to thoroughly test your implementation and contribute back to the project if possible!