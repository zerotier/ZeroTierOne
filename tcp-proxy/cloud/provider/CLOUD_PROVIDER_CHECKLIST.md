# Cloud Provider Integration Checklist

This checklist guides you through the process of adding support for a new cloud provider to the ZeroTier TCP Proxy's cloud-agnostic firewall management system.

## Prerequisites

- [ ] Understand the cloud provider's firewall/security group API
- [ ] Obtain necessary API credentials for testing
- [ ] Create a test firewall/security group in the cloud provider's console

## Implementation Steps

### 1. Create Provider-Specific Header File

- [ ] Create `YourCloudProviderFirewallManager.hpp` file
- [ ] Define class that inherits from `CloudFirewallManager`
- [ ] Declare required member variables (API credentials, firewall ID, etc.)
- [ ] Declare constructor and destructor
- [ ] Declare override methods for the abstract base class:
  - [ ] `bool initialize()`
  - [ ] `bool addPort(uint16_t port)`
  - [ ] `bool removePort(uint16_t port)`
  - [ ] `bool syncFirewallRules()`
- [ ] Declare any provider-specific helper methods

### 2. Create Provider-Specific Implementation File

- [ ] Create `YourCloudProviderFirewallManager.cpp` file
- [ ] Implement constructor to initialize member variables
- [ ] Implement `initialize()` method to verify API credentials and firewall existence
- [ ] Implement `addPort()` method to add a port to the managed set
- [ ] Implement `removePort()` method to remove a port from the managed set
- [ ] Implement `syncFirewallRules()` method to update the cloud firewall rules
- [ ] Implement any provider-specific helper methods
- [ ] Add proper error handling and logging

### 3. Update FirewallManagerFactory

- [ ] Add include for your new provider's header file in `FirewallManagerFactory.hpp`
- [ ] Update `createFirewallManager()` method in `FirewallManagerFactory.cpp` to handle your provider
- [ ] Add parsing for any provider-specific parameters

### 4. Update Makefile

- [ ] Add your new source files to the compilation targets

### 5. Create Tests

- [ ] Create a test file based on `test_cloud_firewall_template.cpp`
- [ ] Customize the test file for your provider's specific requirements
- [ ] Add the test to the Makefile

### 6. Update Documentation

- [ ] Add configuration examples for your provider to `local.conf.example`
- [ ] Update `README_CLOUD_INTEGRATION.md` with provider-specific details

## Testing

- [ ] Test basic functionality:
  - [ ] Initialize the firewall manager
  - [ ] Add a single port
  - [ ] Remove a single port
  - [ ] Add multiple ports
  - [ ] Add port ranges
  - [ ] Remove port ranges
  - [ ] Sync firewall rules

- [ ] Test edge cases:
  - [ ] Invalid credentials
  - [ ] Non-existent firewall ID
  - [ ] API rate limiting
  - [ ] Network connectivity issues
  - [ ] Maximum number of rules/ports

## Final Review

- [ ] Code follows project style guidelines
- [ ] All methods have proper error handling
- [ ] Logging is consistent with the rest of the codebase
- [ ] Documentation is complete and accurate
- [ ] Tests pass for all functionality

## Submission

- [ ] Create a pull request with your changes
- [ ] Include test results and documentation updates
- [ ] Address any review comments