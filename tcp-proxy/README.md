TCP Proxy Server
======

This is the TCP proxy server we run for TCP tunneling from peers behind difficult NATs. Regular users won't have much use for this.

The server now includes integration with Cloud Firewall to dynamically manage UDP ports as clients connect and disconnect. For more information, see the [Cloud Firewall Integration](cloud/README.md) documentation. Configuration examples for different cloud providers can be found in `conf/local.conf.cloud_examples`.

## How to run your own
Currently you must build it and distribute it to your server manually. 

To reduce latency, the tcp-relay should be as close as possible to the nodes it is serving. A datacenter in the same city or the LAN would be ideal.


### Build

#### Standard Build
```bash
cd tcp-proxy
make
```
This builds the standard TCP proxy without cloud firewall integration.

#### Build with Cloud Firewall Support
```bash
cd tcp-proxy
make provider
```
This builds the TCP proxy with cloud firewall integration enabled. See the [Cloud Firewall Integration](cloud/README.md) documentation for more details.

### Point your node at it
 The default tcp relay is at `204.80.128.1/443` -an anycast address.

#### Option 1 - local.conf configuration
See [Service docs](https://github.com/zerotier/ZeroTierOne/blob/e0acccc3c918b59678033e585b31eb000c68fdf2/service/README.md) for more info on local.conf
`{ "settings": { "tcpFallbackRelay": "198.51.100.123/443", "forceTcpRelay": true  } }`


In this example, `forceTcpRelay` is enabled. This is helpful for testing or if you know you'll need tcp relay. It takes a few minutes for zerotier-one to realize it needs to relay otherwise. 



#### Option 2 - redirect 204.80.128.1 to your own IP

If you are the admin of the network that is blocking ZeroTier UDP, you can transparently redirect 204.80.128.1 to one of your IP addresses. Users won't need to edit their local client configuration.

Configuring this in your Enterprise Firewall is left as an exercise to the reader.

Here is an iptables example for illustrative purposes:

``` shell
-A PREROUTING -p tcp -d 204.80.128.1 --dport 443 -j DNAT --to-destination 198.51.100.123
-A POSTROUTING -p tcp -d 198.51.100.123 --dport 443 -j SNAT --to-source 204.80.128.1
```
