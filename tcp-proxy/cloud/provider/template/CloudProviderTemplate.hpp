/**
 * CloudProviderTemplate.hpp
 * 
 * Template for implementing a new cloud provider's firewall manager
 */

#ifndef CLOUD_PROVIDER_FIREWALL_MANAGER_HPP
#define CLOUD_PROVIDER_FIREWALL_MANAGER_HPP

#include <string>
#include <set>
#include "../CloudFirewallManager.hpp"

class CloudProviderFirewallManager : public CloudFirewallManager {
private:
    // Authentication credentials
    std::string apiToken;
    
    // Firewall identifier
    std::string firewallId;
    
    // Set of currently active ports
    std::set<uint16_t> activePorts;
    
    // Provider-specific helper methods
    bool fetchCurrentRules();
    bool makeApiRequest(const std::string& endpoint, 
                       const std::string& method, 
                       const std::string& data, 
                       std::string& response);

public:
    // Constructor
    CloudProviderFirewallManager(const std::string& apiToken, const std::string& firewallId);
    
    // Destructor
    virtual ~CloudProviderFirewallManager() = default;
    
    // Implementation of abstract methods from CloudFirewallManager
    bool initialize() override;
    bool addPort(uint16_t port) override;
    bool removePort(uint16_t port) override;
    bool syncFirewallRules() override;
};

#endif // CLOUD_PROVIDER_FIREWALL_MANAGER_HPP