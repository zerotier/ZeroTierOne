/**
 * FirewallManagerFactory_example.hpp
 * 
 * Example of how to extend the FirewallManagerFactory to support multiple cloud providers
 */

#ifndef FIREWALL_MANAGER_FACTORY_EXAMPLE_HPP
#define FIREWALL_MANAGER_FACTORY_EXAMPLE_HPP

#include <memory>
#include <string>
#include "../CloudFirewallManager.hpp"
#include "../LinodeFirewallManager.hpp"
#include "../AwsFirewallManager.hpp"  // Example AWS implementation
// #include "../AzureFirewallManager.hpp"  // Future Azure implementation
// #include "../GcpFirewallManager.hpp"  // Future GCP implementation

class FirewallManagerFactory {
public:
    /**
     * Create a firewall manager for the specified cloud provider
     * 
     * @param provider Cloud provider name (e.g., "linode", "aws", "azure", "gcp")
     * @param apiToken API token or access key for the cloud provider
     * @param firewallId Firewall ID or security group ID
     * @param additionalParams Additional provider-specific parameters as JSON string
     * @return Unique pointer to a CloudFirewallManager implementation or nullptr if provider is not supported
     */
    static std::unique_ptr<CloudFirewallManager> createFirewallManager(
        const std::string& provider,
        const std::string& apiToken,
        const std::string& firewallId,
        const std::string& additionalParams = "{}");
};

#endif // FIREWALL_MANAGER_FACTORY_EXAMPLE_HPP