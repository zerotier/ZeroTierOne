/**
 * AwsFirewallManager.hpp
 * 
 * Example implementation of CloudFirewallManager for AWS
 * This is a sample to demonstrate how to implement a specific cloud provider
 */

#ifndef AWS_FIREWALL_MANAGER_HPP
#define AWS_FIREWALL_MANAGER_HPP

#include <string>
#include <set>
#include "CloudFirewallManager.hpp"

/**
 * AwsFirewallManager
 * 
 * Implementation of CloudFirewallManager for AWS Security Groups
 */
class AwsFirewallManager : public CloudFirewallManager {
private:
    // AWS credentials
    std::string accessKey;
    std::string secretKey;
    
    // AWS Security Group ID
    std::string securityGroupId;
    
    // AWS Region
    std::string region;
    
    // Set of currently active ports
    std::set<uint16_t> activePorts;
    
    // Helper methods
    bool fetchCurrentRules();
    bool makeAwsApiRequest(const std::string& action, 
                          const std::string& params, 
                          std::string& response);

public:
    /**
     * Constructor
     * 
     * @param accessKey AWS access key
     * @param secretKey AWS secret key
     * @param securityGroupId AWS security group ID
     * @param region AWS region
     */
    AwsFirewallManager(const std::string& accessKey, 
                      const std::string& secretKey, 
                      const std::string& securityGroupId,
                      const std::string& region);
    
    // Destructor
    virtual ~AwsFirewallManager() = default;
    
    // Implementation of abstract methods from CloudFirewallManager
    bool initialize() override;
    bool addPort(uint16_t port) override;
    bool removePort(uint16_t port) override;
    bool syncFirewallRules() override;
};

#endif // AWS_FIREWALL_MANAGER_HPP