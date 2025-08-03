CXX=$(shell which clang++ g++ c++ 2>/dev/null | head -n 1)

INCLUDES?=-I../ext/prometheus-cpp-lite-1.0/core/include  -I../ext/prometheus-cpp-lite-1.0/simpleapi/include

# Add libcurl dependency for provider targets
LIBS=-lcurl

# Default target - original version without cloud provider functionality
all:
	$(CXX) -O3 -fno-rtti $(INCLUDES) -std=c++11 -pthread -frtti  -o tcp-proxy tcp-proxy.cpp ../node/Metrics.cpp

# Provider target - includes new functionality
provider: tcp-proxy-enhanced

tcp-proxy-enhanced:
	$(CXX) -O3 -fno-rtti $(INCLUDES) -std=c++11 -pthread -frtti -DENABLE_CLOUD_PROVIDER -o tcp-proxy tcp-proxy.cpp cloud/provider/LinodeFirewallManager.cpp cloud/provider/FirewallManagerFactory.cpp ../node/Metrics.cpp $(LIBS)

# Test Provider target - includes new functionality
test_linode_firewall:
	$(CXX) -O3 $(INCLUDES) -std=c++11 -pthread -DENABLE_CLOUD_PROVIDER -o test_linode_firewall cloud/provider/tests/test_linode_firewall.cpp cloud/provider/LinodeFirewallManager.cpp cloud/provider/FirewallManagerFactory.cpp $(LIBS)

# Test template target
test_cloud_firewall_template:
	$(CXX) -O3 $(INCLUDES) -std=c++11 -pthread -DENABLE_CLOUD_PROVIDER -o test_cloud_firewall_template cloud/provider/tests/test_cloud_firewall_template.cpp cloud/provider/LinodeFirewallManager.cpp cloud/provider/FirewallManagerFactory.cpp $(LIBS)

# Clean all
clean:
	rm -f *.o tcp-proxy test_linode_firewall test_cloud_firewall_template *.dSYM
