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

	sshd                     [ WORKS as of 20151112 ]
	ssh                      [ WORKS as of 20151112 ]
	sftp                     [ WORKS as of 20151022 ]
	curl                     [ WORKS as of 20151021 ]
	apache (debug mode)      [ WORKS as of 20150810 ]
	apache (prefork MPM)     [ WORKS as of 20151123 ] (2.4.6-31.x86-64 on Centos 7), (2.4.16-1.x84-64 on F22), (2.4.17-3.x86-64 on F22)
	nginx                    [ MARGINAL as of 20151123 ] Broken on Centos 7, unreliable on Fedora 23
	nodejs                   [ WORKS as of 20151123 ]
	java                     [ WORKS as of 20151010 ]
	MongoDB                  [ WORKS as of 20151028 ]
	Redis-server             [ WORKS as of 20151123 ]

It is *likely* to work with other things but there are no guarantees. UDP, ICMP/RAW, and IPv6 support are planned for the near future.

# Building Network Containers

Network Containers are currently only for Linux. To build the network container host, IP stack, and intercept library, from the base of the ZeroTier One tree run:

    make netcon

This will build a binary called *zerotier-netcon-service* and a library called *libzerotierintercept.so*. It will also build the IP stack as *netcon/liblwip.so*.

The *zerotier-netcon-service* binary is almost the same as a regular ZeroTier One build except instead of creating virtual network ports using Linux's */dev/net/tun* interface, it creates instances of a user-space TCP/IP stack for each virtual network and provides RPC access to this stack via a Unix domain socket called */tmp/.ztnc_##NETWORK_ID##*. The latter is a library that can be loaded with the Linux *LD\_PRELOAD* environment variable or by placement into */etc/ld.so.preload* on a Linux system or container.

The intercept library does nothing unless the *ZT\_NC\_NWID* environment variable is set. If on program launch (or fork) it detects the presence of this environment variable, it will attempt to connect to a running *zerotier-netcon-service* at the aforementioned Unix domain socket location and will intercept calls to the Posix sockets API and redirect network traffic through the virtual network.

Unlike *zerotier-one*, *zerotier-netcon-service* does not need to be run with root privileges and will not modify the host's network configuration in any way.

# Starting the Network Containers Service

You don't need Docker or any other container engine to try Network Containers. A simple test can be performed in user space in your own home directory.

First, build the netcon service and intercept library as describe above. Then create a directory to act as a temporary ZeroTier home for your test netcon service instance. You'll need to move the liblwip.so binary that was built with *make netcon* into there, since the service must be able to find it there and load it.

    mkdir /tmp/netcon-test-home
    cp -f ./netcon/liblwip.so /tmp/netcon-test-home

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

Now you can run a containerized application. Open another terminal window (since you might not want these environment variables to stick elsewhere) on the same machine the netcon service is running on and install something like *darkhttpd* (a simple http server) to act as a test app:

On Debian and Ubuntu:

    sudo apt-get install darkhttpd

Or for CentOS/EPEL or Fedora:

    sudo yum install darkhttpd

Now try:

    export LD_PRELOAD=/path/to/ZeroTierOne/libzerotierintercept.so
		export ZT_NC_NWID=8056c2e21c000001
		darkhttpd . --port 8080

Going to port 8080 on your machine won't work. Darkhttpd is listening, but only inside the network container. To reach it, go to the other system where you joined the same network with a conventional ZeroTier instance and try:

    curl http://NETCON.INSTANCE.IP:8080/README.md

Replace *NETCON.INSTANCE.IP* with the IP address that *zerotier-netcon-service* was assigned on the virtual network. (This is the same IP you pinged in your first test.) If everything works, you should get back a copy of ZeroTier One's main README.md file.

# Installing in a Docker Container (or any other container engine)

If it's not immediately obvious, installation into a Docker container is easy. Just install *zerotier-netcon-service*, *libzerotierintercept.so*, and *liblwip.so* into the container at an appropriate location. We suggest putting it all in */var/lib/zerotier-one* since this is the default ZeroTier home and will eliminate the need to supply a path to any of ZeroTier's services or utilities. Then, in your Docker container entry point script launch the service with *-d* to run it in the background, set the appropriate environment variables as described above, and launch your container's main application.

The only bit of complexity is configuring which virtual network to join. ZeroTier's service automatically joins networks that have *.conf* files in *ZTHOME/networks.d* even if the *.conf* file is empty. So one way of doing this very easily is to add the following commands to your Dockerfile or container entry point script:

    mkdir -p /var/lib/zerotier-one/networks.d
    touch /var/lib/zerotier-one/networks.d/8056c2e21c000001.conf

Replace 8056c2e21c000001 with the network ID of the network you want your container to automaticlaly join.

Now your container will automatically join the specified network on startup. Authorizing the container on a private network still requires a manual authorization step either via the ZeroTier Central web UI or the API. We're working on some ideas to automate this via bearer token auth or similar since doing this manually or with scripts for large deployments is tedious. We'll have something in this area by the time Network Containers itself is ready to be pronounced no-longer-beta.
