Network Containers (beta)
======

ZeroTier Network Containers offers a microkernel-like networking paradigm for containerized applications and application-specific virtual networking.

Network Containers couples the ZeroTier core Ethernet virtualization engine with a user-space TCP/IP stack and a library that intercepts calls to the Posix network API. Our intercept library implements full binary compatibility with the standard network API, permitting servers and applications to be used without modification or recompilation.

It can be used to run services on virtual networks without requiring the creation of kernel-mode virtual network ports or modification of system network settings and without special privileges. It's ideal for containerized microservices that are designed exclusively for use on an isolated virtual network and that are to be deployed on commodity container hosting infrastructure. With Network Containers such services can be deployed without special permissions and connected to arbitrary virtual networks without configuration changes to the host node.

Network Containers is ideal for use with [Docker](http://http://www.docker.com), [LXC](https://linuxcontainers.org), or [Rkt](https://coreos.com/rkt/docs/latest/), allowing connectivity to a virtual network to be built into and deployed with containers without host awareness or configuration. It can also be used without containers to network-containerize applications on an ordinary VM or bare metal host. It works entirely at the library/application level and requires no special kernel extensions.

Our long term goal with network containers is to facilitate the total commoditization of the container host by allowing virtual networking without elevated privileges or host configuration. We think this will help ease the path toward commodity multi-tenant container hosting and total application portability across hosts, data centers, and cloud providers.

Network Containers are currently in **beta** and are suitable for testing, experimentation, and prototyping. There are still some issues with compatibility with some applications, as documented in the compatibility matrix below. There's also some remaining work to be done on performance and overall stability before this will be ready for production use.

# Limitations and Compatibility

The current version of Network Containers **only supports TCP over IPv4**. There is no IPv6 support and no support for UDP or ICMP (or RAW sockets). That means network-containerizing *ping* won't work, nor will UDP-based apps like VoIP servers, DNS servers, or P2P apps.

The virtual TCP/IP stack will respond to *incoming* ICMP ECHO requests, which means that you can ping it from another host on the same ZeroTier virtual network. This is useful for testing.

**Network Containers are currently all or nothing.** If engaged, the intercept library intercepts all network I/O calls and redirects them through the new path. A network-containerized application cannot communicate over the regular network connection of its host or container or with anything else except other hosts on its ZeroTier virtual LAN. Support for optional "fall-through" to the host IP stack for outgoing connections outside the virtual network and for gateway routes within the virtual network is also planned for the near future.

#### Compatibility Test Results

	sshd (debug mode -d)     [ WORKS  as of 20151208 ] Fedora 22/23, Centos 7, Ubuntu 14.04
	apache (debug mode -X)   [ WORKS  as of 20151208 ] 2.4.6 on Centos 7, 2.4.16 and 2.4.17 on Fedora 22/23
	nginx                    [ WORKS  as of 20151208 ] 1.8.0 on both Fedora 22/23 and Ubuntu 14.04
	nodejs                   [ WORKS  as of 20151208 ] 0.10.36 Fedora 22/23 (disabled, see note in accept() in netcon/Intercept.c)
	redis-server             [ WORKS  as of 20151208 ] 3.0.4 on Fedora 22/23

It is *likely* to work with other things but there are no guarantees. UDP, ICMP/RAW, and IPv6 support are planned for the near future.

# Building Network Containers

Network Containers are currently only for Linux. To build the network container host and intercept library, from the base of the ZeroTier One tree type:

    make netcon
    make install-intercept

This will build a binary called *zerotier-netcon-service* and a library called *libzerotierintercept.so*. The former is the same as a regular ZeroTier One build except instead of creating virtual network ports using Linux's */dev/net/tun* interface, it instead creates instances of a user-space TCP/IP stack for each virtual network and provides RPC access to this stack via a Unix domain socket called */tmp/.ztnc_##NETWORK_ID##*. The latter is a library that can be loaded with the Linux *LD\_PRELOAD* environment variable or by placement into */etc/ld.so.preload* on a Linux system or container.

The intercept library does nothing unless the *ZT\_NC\_NWID* environment variable is set. If on program launch (or fork) it detects the presence of this environment variable, it will attempt to connect to a running *zerotier-netcon-service* at the aforementioned Unix domain socket location and will intercept calls to the Posix sockets API and redirect network traffic through the virtual network.

Unlike *zerotier-one*, *zerotier-netcon-service* does not need to be run with root privileges and will not modify the host's network configuration in any way.

# Starting the Network Containers Service

You don't need Docker or any other container engine to try Network Containers. A simple test can be performed in user space in your own home directory.

First, build the netcon service and intercept library as describe above. Then create a directory to act as a temporary ZeroTier home for your test netcon service instance.

    mkdir /tmp/netcon-test-home

Now you can run the service (no sudo needed):

    ./zerotier-netcon-service -d /tmp/netcon-test-home

As with ZeroTier One in its normal incarnation, you'll need to join a network:

    ./zerotier-cli -D/tmp/netcon-test-home join 8056c2e21c000001

(If you don't want to use [Earth](https://www.zerotier.com/public.shtml) for this test, replace its network ID with one of your own.)

Note the *-D* option. This tells *zerotier-cli* not to look in /var/lib/zerotier-one for information about a running instance of the ZeroTier system service but instead to look in /tmp/netcon-test-home. That's because *even if you do happen to be running ZeroTier on your local machine, what you are doing now has no impact on it and does not involve it in any way.* So if you have *zerotier-one* running, forget about it. It doesn't matter for this test.

Now type:

    ./zerotier-cli -D/tmp/netcon-test-home listnetworks

Try it a few times until you see that you've successfully joined the network and have an IP address.

You'll also want to have ZeroTier One (the normal build, not network containers) running somewhere else, such as on another Linux system or VM. Technically you could run it on the *same* Linux system and it wouldn't matter at all, but many people find this intensely confusing until they grasp just what exactly is happening here.

On the other Linux system, join the same network if you haven't already (8056c2e21c000001 if you're using Earth) and wait until you have an IP address. Then try pinging the IP address your netcon instance received. You should see ping replies.

Back on the host that's running *zerotier-netcon-service*, type *ip list all* or *ifconfig* (ifconfig is technically deprecated so some Linux systems might not have it). Notice that the IP address of the network containers endpoint is not listed and no network device is listed for it either. That's because as far as the Linux kernel is concerned it doesn't exist.

What are you pinging? What is happening here?

The *zerotier-netcon-service* binary has joined a *virtual* network and is running a *virtual* TCP/IP stack entirely in user space. As far as your system is concerned it's just another program exchanging UDP packets with a few other hosts on the Internet and nothing out of the ordinary is happening at all. That's why you never had to type *sudo*. It didn't change anything on the host.

Now you can run a containerized application. Open another terminal window (since you might not want these environment variables to stick elsewhere) on the same machine the netcon service is running on and install something like *httpd* (a simple http server) to act as a test app:

On Debian and Ubuntu:

    sudo apt-get install apache2

Or for CentOS/EPEL or Fedora:

    sudo yum install httpd

Now try:

    export LD_PRELOAD=/path/to/ZeroTierOne/libzerotierintercept.so
	export ZT_NC_NWID=8056c2e21c000001
	zerotier-intercept httpd -X


Going to port 80 on your machine won't work. Httpd is listening, but only inside the network container. To reach it, go to the other system where you joined the same network with a conventional ZeroTier instance and try:

    curl http://NETCON.INSTANCE.IP:80/

Replace *NETCON.INSTANCE.IP* with the IP address that *zerotier-netcon-service* was assigned on the virtual network. (This is the same IP you pinged in your first test.) If everything works, you should get back a copy of ZeroTier One's main README.md file.

# Unit Tests

Each unit test will temporarily copy all required ZeroTier binaries into its local directory, then build the *netcon_dockerfile* and *monitor_dockerfile*. Once built, each container will be run and perform tests and monitoring specified in *netcon_entrypoint.sh* and *monitor_entrypoint.sh*

Results will be written to the *netcon/docker-test/_results/* directory which is a common shared volume between all containers involved in the test and will be a combination of raw and formatted dumps to files whose names reflect the test performed. In the event of failure, *FAIL.* will be prepended to the result file's name (e.g. *FAIL.my_application_1.0.2.x86_64*), likewise in the event of success, *OK.* will be prepended.

To run unit tests:

1) Set up your own network, use its network id as follows:

2) Place a blank network config file in the *netcon/docker-test* directory (e.g. "e5cd7a9e1c5311ab.conf")
 - This will be used to inform test-specific scripts what network to use for testing

After you've created your network and placed its blank config file in *netcon/docker-test* run the following to perform unit tests for httpd:

	./build.sh httpd
	./test.sh httpd

It's useful to note that the keyword *httpd* in this example is merely a substring for a test name, this means that if we replaced it with *x86_64* or *fc23*, it would run all unit tests for *x86_64* systems or *Fedora 23* respectively.
