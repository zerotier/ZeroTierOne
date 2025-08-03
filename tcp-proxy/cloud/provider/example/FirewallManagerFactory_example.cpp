/**
 * FirewallManagerFactory_example.cpp
 * 
 * Example implementation of FirewallManagerFactory with support for multiple cloud providers
 */

#include "FirewallManagerFactory_example.hpp"
#include "../../../ext/nlohmann/json.hpp"
#include <iostream>

// For JSON parsing
using json = nlohmann::json;

// External function for logging (defined in tcp-proxy.cpp)
extern void datetime_fprintf(FILE *stream, const char *fmt, ...);

std::unique_ptr<CloudFirewallManager> FirewallManagerFactory::createFirewallManager(
    const std::string& provider,
    const std::string& apiToken,
    const std::string& firewallId,
    const std::string& additionalParams) {
    
    datetime_fprintf(stdout, "Creating firewall manager for provider: %s\n", provider.c_str());
    
    // Parse additional parameters if provided
    json params;
    try {
        if (!additionalParams.empty()) {
            params = json::parse(additionalParams);
        }
    } catch (json::parse_error& e) {
        datetime_fprintf(stderr, "Failed to parse additional parameters: %s\n", e.what());
        return nullptr;
    }
    
    // Create the appropriate firewall manager based on the provider
    if (provider == "linode") {
        return std::unique_ptr<CloudFirewallManager>(new LinodeFirewallManager(apiToken, firewallId));
    } 
    else if (provider == "aws") {
        // Extract AWS-specific parameters
        std::string secretKey;
        std::string region = "us-east-1";  // Default region
        
        try {
            if (params.contains("secretKey")) {
                secretKey = params["secretKey"].get<std::string>();
            } else {
                datetime_fprintf(stderr, "AWS requires secretKey in additionalParams\n");
                return nullptr;
            }
            
            if (params.contains("region")) {
                region = params["region"].get<std::string>();
            }
        } catch (json::exception& e) {
            datetime_fprintf(stderr, "Error parsing AWS parameters: %s\n", e.what());
            return nullptr;
        }
        
        return std::unique_ptr<CloudFirewallManager>(
            new AwsFirewallManager(apiToken, secretKey, firewallId, region));
    }
    // Example for Azure implementation
    else if (provider == "azure") {
        // Extract Azure-specific parameters
        std::string tenantId;
        std::string subscriptionId;
        std::string resourceGroup;
        
        try {
            if (params.contains("tenantId") && params.contains("subscriptionId") && 
                params.contains("resourceGroup")) {
                
                tenantId = params["tenantId"].get<std::string>();
                subscriptionId = params["subscriptionId"].get<std::string>();
                resourceGroup = params["resourceGroup"].get<std::string>();
            } else {
                datetime_fprintf(stderr, "Azure requires tenantId, subscriptionId, and resourceGroup in additionalParams\n");
                return nullptr;
            }
        } catch (json::exception& e) {
            datetime_fprintf(stderr, "Error parsing Azure parameters: %s\n", e.what());
            return nullptr;
        }
        
        // Uncomment when Azure implementation is available
        // return std::unique_ptr<CloudFirewallManager>(
        //     new AzureFirewallManager(apiToken, tenantId, subscriptionId, resourceGroup, firewallId));
        
        datetime_fprintf(stderr, "Azure provider not yet implemented\n");
        return nullptr;
    }
    // Example for GCP implementation
    else if (provider == "gcp") {
        // Extract GCP-specific parameters
        std::string projectId;
        std::string network = "default";
        
        try {
            if (params.contains("projectId")) {
                projectId = params["projectId"].get<std::string>();
            } else {
                datetime_fprintf(stderr, "GCP requires projectId in additionalParams\n");
                return nullptr;
            }
            
            if (params.contains("network")) {
                network = params["network"].get<std::string>();
            }
        } catch (json::exception& e) {
            datetime_fprintf(stderr, "Error parsing GCP parameters: %s\n", e.what());
            return nullptr;
        }
        
        // Uncomment when GCP implementation is available
        // return std::unique_ptr<CloudFirewallManager>(
        //     new GcpFirewallManager(apiToken, projectId, network, firewallId));
        
        datetime_fprintf(stderr, "GCP provider not yet implemented\n");
        return nullptr;
    }
    else {
        datetime_fprintf(stderr, "Unsupported cloud provider: %s\n", provider.c_str());
        return nullptr;
    }
}