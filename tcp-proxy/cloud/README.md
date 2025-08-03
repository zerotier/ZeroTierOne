# Cloud Firewall Integration

## Build Instructions

To build the TCP proxy with cloud firewall integration support, use the following command from the `tcp-proxy` directory:

```bash
make provider
```

This will compile the TCP proxy with cloud provider functionality enabled by:
- Defining the `ENABLE_CLOUD_PROVIDER` preprocessor flag
- Including the necessary cloud provider implementation files
- Linking against required libraries (like libcurl for API requests)

The resulting binary will be named `tcp-proxy` but will include all cloud firewall management capabilities.

### Testing Cloud Provider Implementations

For testing specific cloud provider implementations, you can use:

```bash
# Test Linode firewall implementation
make test_linode_firewall

# Test cloud firewall template (useful when developing new providers)
make test_cloud_firewall_template
```

These test targets compile standalone executables that can be used to verify your cloud provider implementation works correctly without running the full TCP proxy.

### Cleaning Build Artifacts

To clean all build artifacts:

```bash
make clean
```

### Architecture Note

The build system uses a modular approach that allows adding new cloud providers without modifying the core TCP proxy code. When you build with `make provider`, the system compiles in the cloud-agnostic firewall management architecture described in [README_CLOUD_INTEGRATION.md](README_CLOUD_INTEGRATION.md).

## Linode Firewall Integration

The TCP proxy server can automatically manage cloud firewall rules to open and close UDP ports as clients connect and disconnect. This is useful when running the proxy on a cloud instance with a firewall. Currently, Linode Cloud Firewall is supported, with an extensible architecture to add support for other cloud providers.

### Configuration

To enable cloud firewall integration, you can use either the legacy Linode-specific configuration or the new generic cloud provider configuration in your `local.conf` file:

#### Option 1: Legacy Linode Configuration (Backward Compatible)

```json
{
  "settings": {
    "tcpPort": 443,
    "linodeApiToken": "your_linode_api_token_here",
    "linodeFirewallId": "12345"
  }
}
```

You need to provide:
1. `linodeApiToken`: A Linode API token with `firewall:read_write` permissions
2. `linodeFirewallId`: The ID of the Linode Firewall to manage

#### Option 2: Generic Cloud Provider Configuration

```json
{
  "settings": {
    "tcpPort": 443,
    "cloudProvider": "linode",
    "cloudApiToken": "your_cloud_api_token_here",
    "cloudFirewallId": "12345"
  }
}
```

You need to provide:
1. `cloudProvider`: The cloud provider name (currently supported: "linode")
2. `cloudApiToken`: A cloud provider API token with appropriate permissions
3. `cloudFirewallId`: The ID of the cloud firewall to manage

A sample configuration file with examples for different cloud providers is provided in `conf/local.conf.cloud_examples`.

### How It Works

When a client connects to the TCP proxy, the server:
1. Assigns a UDP port for the client
2. Adds that UDP port to the cloud firewall rules (with provider-specific implementation details)
3. When the client disconnects, removes the UDP port from the firewall
4. Handles provider-specific limitations (e.g., for Linode, automatically splits ports across multiple rules when necessary to comply with Linode's 15-piece limit per rule)

The server also periodically syncs with the cloud firewall every 15 minutes to ensure rules are up to date.

### Preserving Existing Rules

The cloud firewall integration is designed to work alongside your existing firewall rules:

- All existing non-ZeroTier rules (like TCP, ICMP, etc.) are preserved
- Provider-specific rules are created as needed (e.g., for Linode, if no UDP rule with the "ZeroTier-UDP-Ports" label exists, one will be created when the first client connects)
- If a TCP 443 rule doesn't exist, one will be added automatically
- Inbound and outbound policies from your existing configuration are preserved
- The system automatically handles provider-specific limitations (e.g., for Linode, maximum 15 port pieces per rule, where a single port counts as 1 piece and a port range counts as 2 pieces)

### Extending to Other Cloud Providers

The TCP proxy server uses an extensible architecture that makes it easy to add support for other cloud providers:

1. The `CloudFirewallManager` abstract base class defines the interface for all cloud firewall managers
2. Provider-specific implementations (like `LinodeFirewallManager`) inherit from this base class
3. The `FirewallManagerFactory` creates the appropriate firewall manager based on the configuration

To add support for a new cloud provider:
1. Create a new class that inherits from `CloudFirewallManager`
2. Implement the required methods for the specific cloud provider's API
3. Add the new provider to the `FirewallManagerFactory`

For more detailed information on implementing support for additional cloud providers, see the [Cloud Firewall Manager Integration Guide](README_CLOUD_INTEGRATION.md).