Network Containers (beta)
======

ZeroTier Network Containers offers a microkernel-like networking paradigm for containerized applications and application-specific virtual networking.

Network Containers couples the ZeroTier core Ethernet virtualization engine with a user-space TCP/IP stack and a library that intercepts calls to the Posix network API. This allows servers and applications to be used without modification or recompilation. It can be used to run services on virtual networks without elevated privileges, special configuration of the physical host, kernel support, or any other application specific configuration. It's ideal for use with [Docker](http://http://www.docker.com), [LXC](https://linuxcontainers.org), or [Rkt](https://coreos.com/rkt/docs/latest/) to build containerized microservices that automatically connect to a virtual network when deployed. It can also be used on a plain un-containerized Linux system to run applications on virtual networks without elevated privileges or system modification.

[More discussion can be found in our original blog announcement.](https://www.zerotier.com/blog/?p=490)

Network Containers is currently in **beta** and is suitable for testing and experimentation. Only Linux is supported. Future updates will focus on compatibility, full stack support, and improved performance, and may also port to other OSes.

# Limitations and Compatibility

The beta version of Network Containers **only supports TCP over IPv4**. There is no IPv6 support and no support for UDP or ICMP (or RAW sockets). That means network-containerizing *ping* won't work, nor will UDP-based apps like VoIP servers, DNS servers, or P2P apps.

The virtual TCP/IP stack will respond to *incoming* ICMP ECHO requests, which means that you can ping it from another host on the same ZeroTier virtual network. This is useful for testing.

**Network Containers are currently all or nothing.** If engaged, the intercept library intercepts all network I/O calls and redirects them through the new path. A network-containerized application cannot communicate over the regular network connection of its host or container or with anything else except other hosts on its ZeroTier virtual LAN. Support for optional "fall-through" to the host IP stack for outgoing connections outside the virtual network and for gateway routes within the virtual network is planned. (It will be optional since in some cases total network isolation might be considered a nice security feature.)

#### Compatibility Test Results

The following applications have been tested and confirmed to work for the beta release:

Fedora 23: 
    
    httpstub.c
    nginx 1.8.0
    http 2.4.16, 2.4.17
    darkhttpd 1.11
    python 2.7.10 (python -m SimpleHTTPServer)
    python 3.4.3 (python -m http.server)
    redis 3.0.4
    node 6.0.0-pre
    sshd

CentOS 7:

    httpstub.c
    nginx 1.6.3
    httpd 2.4.6 (debug mode -X)
    darkhttpd 1.11
    node 4.2.2
    redis 2.8.19
    sshd

Ubuntu 14.04.3:
    
    httpstub.c
    nginx 1.4.6
    python 2.7.6 (python -m SimpleHTTPServer)
    python 3.4.0 (python -m http.server)
    node 5.2.0
    redis 2.8.4
    sshd

It is *likely* to work with other things but there are no guarantees.

# Building Network Containers

Network Containers are currently only for Linux. To build the network container host, IP stack, and intercept library, from the base of the ZeroTier One tree run:

    make clean
    make netcon

This will build a binary called *zerotier-netcon-service* and a library called *libzerotierintercept.so*. It will also build the IP stack as *netcon/liblwip.so*.

The *zerotier-netcon-service* binary is almost the same as a regular ZeroTier One build except instead of creating virtual network ports using Linux's */dev/net/tun* interface, it creates instances of a user-space TCP/IP stack for each virtual network and provides RPC access to this stack via a Unix domain socket. The latter is a library that can be loaded with the Linux *LD\_PRELOAD* environment variable or by placement into */etc/ld.so.preload* on a Linux system or container. Additional magic involving nameless Unix domain socket pairs and interprocess socket handoff is used to emulate TCP sockets with extremely low overhead and in a way that's compatible with select, poll, epoll, and other I/O event mechanisms.

The intercept library does nothing unless the *ZT\_NC\_NETWORK* environment variable is set. If on program launch (or fork) it detects the presence of this environment variable, it will attempt to connect to a running *zerotier-netcon-service* at the specified Unix domain socket path.

Unlike *zerotier-one*, *zerotier-netcon-service* does not need to be run with root privileges and will not modify the host's network configuration in any way. It can be run alongside *zerotier-one* on the same host with no ill effect, though this can be confusing since you'll have to remember the difference between "real" host interfaces (tun/tap) and network containerized endpoints. The latter are completely unknown to the kernel and will not show up in *ifconfig*.

# Starting the Network Containers Service

You don't need Docker or any other container engine to try Network Containers. A simple test can be performed in user space (no root) in your own home directory.

First, build the netcon service and intercept library as described above. Then create a directory to act as a temporary ZeroTier home for your test netcon service instance. You'll need to move the *liblwip.so* binary that was built with *make netcon* into there, since the service must be able to find it there and load it.

    mkdir /tmp/netcon-test-home
    cp -f ./netcon/liblwip.so /tmp/netcon-test-home

Now you can run the service (no sudo needed, and *-d* tells it to run in the background):

    ./zerotier-netcon-service -d -p8000 /tmp/netcon-test-home

As with ZeroTier One in its normal incarnation, you'll need to join a network for anything interesting to happen:

    ./zerotier-cli -D/tmp/netcon-test-home join 8056c2e21c000001

If you don't want to use [Earth](https://www.zerotier.com/public.shtml) for this test, replace 8056c2e21c000001 with a different network ID. The *-D* option tells *zerotier-cli* not to look in /var/lib/zerotier-one for information about a running instance of the ZeroTier system service but instead to look in */tmp/netcon-test-home*.

Now type:

    ./zerotier-cli -D/tmp/netcon-test-home listnetworks

Try it a few times until you see that you've successfully joined the network and have an IP address. Instead of a *zt#* device, a path to a Unix domain socket will be listed for the network's port.

Now you will want to have ZeroTier One (the normal *zerotier-one* build, not network containers) running somewhere else, such as on another Linux system or VM. Technically you could run it on the *same* Linux system and it wouldn't matter at all, but many people find this intensely confusing until they grasp just what exactly is happening here.

On the other Linux system, join the same network if you haven't already (8056c2e21c000001 if you're using Earth) and wait until you have an IP address. Then try pinging the IP address your netcon instance received. You should see ping replies.

Back on the host that's running *zerotier-netcon-service*, type *ip addr list* or *ifconfig* (ifconfig is technically deprecated so some Linux systems might not have it). Notice that the IP address of the network containers endpoint is not listed and no network device is listed for it either. That's because as far as the Linux kernel is concerned it doesn't exist.

What are you pinging? What is happening here?

The *zerotier-netcon-service* binary has joined a *virtual* network and is running a *virtual* TCP/IP stack entirely in user space. As far as your system is concerned it's just another program exchanging UDP packets with a few other hosts on the Internet and nothing out of the ordinary is happening at all. That's why you never had to type *sudo*. It didn't change anything on the host.

Now you can run an application inside your network container.

    export LD_PRELOAD=`pwd`/libzerotierintercept.so
    export ZT_NC_NETWORK=/tmp/netcon-test-home/nc_8056c2e21c000001
    node netcon/httpserver.js

Also note that the "pwd" in LD_PRELOAD assumes you are in the ZeroTier source root and have built netcon there. If not, substitute the full path to *libzerotierintercept.so*. If you want to remove those environment variables later, use "unset LD_PRELOAD" and "unset ZT_NC_NETWORK".

If you don't have node.js installed, an alternative test using python would be:
    
    python -m SimpleHTTPServer 80

If you are running Python 3, use "-m http.server".

If all went well a small static HTTP server is now serving up the current directory, but only inside the network container. Going to port 80 on your machine won't work. To reach it, go to the other system where you joined the same network with a conventional ZeroTier instance and try:

    curl http://NETCON.INSTANCE.IP/

Replace *NETCON.INSTANCE.IP* with the IP address that *zerotier-netcon-service* was assigned on the virtual network. (This is the same IP you pinged in your first test.) If everything works, you should get back a copy of ZeroTier One's main README.md file.

# Installing in a Docker container (or any other container engine)

If it's not immediately obvious, installation into a Docker container is easy. Just install *zerotier-netcon-service*, *libzerotierintercept.so*, and *liblwip.so* into the container at an appropriate locations. We suggest putting it all in */var/lib/zerotier-one* since this is the default ZeroTier home and will eliminate the need to supply a path to any of ZeroTier's services or utilities. Then, in your Docker container entry point script launch the service with *-d* to run it in the background, set the appropriate environment variables as described above, and launch your container's main application.

The only bit of complexity is configuring which virtual network to join. ZeroTier's service automatically joins networks that have *.conf* files in *ZTHOME/networks.d* even if the *.conf* file is empty. So one way of doing this very easily is to add the following commands to your Dockerfile or container entry point script:

    mkdir -p /var/lib/zerotier-one/networks.d
    touch /var/lib/zerotier-one/networks.d/8056c2e21c000001.conf

Replace 8056c2e21c000001 with the network ID of the network you want your container to automatically join. It's also a good idea in your container's entry point script to add a small loop to wait until the container's instance of ZeroTier generates an identity and comes online. This could be something like:

    /var/lib/zerotier-one/zerotier-netcon-service -d
    while [ ! -f /var/lib/zerotier-one/identity.secret ]; do
      sleep 0.1
    done
    # zerotier-netcon-service is now running and has generated an identity

(Be sure you don't bundle the identity into the container, otherwise every container will try to be the same device and they will "fight" over the device's address.)

Now each new instance of your container will automatically join the specified network on startup. Authorizing the container on a private network still requires a manual authorization step either via the ZeroTier Central web UI or the API. We're working on some ideas to automate this via bearer token auth or similar since doing this manually or with scripts for large deployments is tedious.

# Docker-based Unit Tests

Each unit test will temporarily copy all required ZeroTier binaries into its local directory, then build the *netcon_dockerfile* and *monitor_dockerfile*. Once built, each container will be run and perform tests and monitoring specified in *netcon_entrypoint.sh* and *monitor_entrypoint.sh*

Results will be written to the *netcon/docker-test/_results/* directory which is a common shared volume between all containers involved in the test and will be a combination of raw and formatted dumps to files whose names reflect the test performed. In the event of failure, *FAIL.* will be prepended to the result file's name (e.g. *FAIL.my_application_1.0.2.x86_64*), likewise in the event of success, *OK.* will be prepended.

To run unit tests:

1) Set up your own network at [https://my.zerotier.com/](https://my.zerotier.com/). For our example we'll just use the Earth network (8056c2e21c000001). Use its network id as follows:

2) Generate two pairs of identity keys. Each public/private pair will be used by the *netcon* and *monitor* containers:

    mkdir -p /tmp/netcon_first
    cp -f ./netcon/liblwip.so /tmp/netcon_first
    ./zerotier-netcon-service -d -p8100 /tmp/netcon_first
    ./zerotier-cli -D/tmp/netcon_first join 8056c2e21c000001
    kill `cat /tmp/netcon_first/zerotier-one.pid`

    mkdir -p /tmp/netcon_second
    cp -f ./netcon/liblwip.so /tmp/netcon_second
    ./zerotier-netcon-service -d -p8101 /tmp/netcon_second
    ./zerotier-cli -D/tmp/netcon_second join 8056c2e21c000001
    kill `cat /tmp/netcon_second/zerotier-one.pid`

3) Copy the identity files to your *docker-test* directory. Names will be altered during copy step so the dockerfiles know which identities to use for each image/container:

    cp /tmp/netcon_first/identity.public ./netcon/docker-test/netcon_identity.public
    cp /tmp/netcon_first/identity.secret ./netcon/docker-test/netcon_identity.secret

    cp /tmp/netcon_second/identity.public ./netcon/docker-test/monitor_identity.public
    cp /tmp/netcon_second/identity.secret ./netcon/docker-test/monitor_identity.secret


4) Place a blank network config file in the *netcon/docker-test* directory (e.g. "8056c2e21c000001.conf")
 - This will be used to inform test-specific scripts what network to use for testing

After you've created your network and placed its blank config file in *netcon/docker-test* run the following to perform unit tests for httpd:

    ./build.sh httpd
    ./test.sh httpd

It's useful to note that the keyword *httpd* in this example is merely a substring for a test name, this means that if we replaced it with *x86_64* or *fc23*, it would run all unit tests for *x86_64* systems or *Fedora 23* respectively.
