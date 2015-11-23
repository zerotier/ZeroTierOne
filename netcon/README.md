ZeroTier Network Containers
======

### Functional Overview:

This system exists as a dynamically-linked library, and a service/IP-stack built into ZeroTier

If you care about the technicals,

The intercept is compiled as a shared library and installed in some user-accessible directory. When you want to intercept
a user application you dynamically link the shared library to the application during runtime. When the application starts, the
intercept's global constructor is called which sets up a hidden pipe which is used to communicate remote procedure calls (RPC) to the host Netcon service running in the background.

When an RPC for a socket() is received by the Netcon service from the intercepted application, the Netcon service will ask the lwIP stack for a new PCB structure (used to represent a connection), if the system permits its allocation, it will be passed to Netcon where a PCB/socket table entry will be created. The table is used for mapping [callbacks from lwIP] and [RPCs from the intercept] to the correct connections.

Upon the first call to a intercept-overriden system call, a Unix-domain socket is opened between the Netcon service and the application's intercept. This socket provides us the ability to pass file descriptors of newly-created socketpairs to the intercept (used as the read/write buffer). More specifically, after the socketpair creation, one end is kept in a table entry in Netcon and one end is sent to the intercept.

### Building from Source (and Installing)

Build zerotier-intercept library:

    make -f make-intercept.mk

Install:

    make -f make-intercept.mk install

Build LWIP library:

    make -f make-liblwip.mk

Run automated tests (from netcon/docker-test/ directory):

    ./build.sh
    ./test.sh




### Running

To intercept a specific application (requires an already running instance of Zerotier-One with Network Containers enabled):

    zerotier-intercept my_app


### Unit Tests

To run unit tests:

1) Set up your own network, use its network id as follows:

2) Place a blank network config file in this directory (e.g. "e5cd7a9e1c5311ab.conf")
 - This will be used to inform test-specific scripts what network to use for testing

3) run build.sh
 - Builds ZeroTier-One with Network Containers enabled
 - Builds LWIP library
 - Builds intercept library
 - Copies all aformentioned files into unit test directory to be used for building docker files

4) run test.sh
 - Will execute each unit test's (test.sh) one at a time and populate _results/


### Anatomy of a unit test

A) Each unit test's test.sh will:
 - temporarily copy all built files into local directory
 - build test container
 - build monitor container
 - remove temporary files
 - run each container and perform test and monitoring specified in netcon_entrypoint.sh and monitor_entrypoint.sh

B) Results will be written to the 'netcon/docker-test/_results/' directory
 - Results will be a combination of raw and formatted dumps to files whose names reflect the test performed
 - In the event of failure, 'FAIL.' will be appended to the result file's name
  - (e.g. FAIL.my_application_1.0.2.x86_64)
 - In the event of success, 'OK.' will be appended


### Compatibility

Network Containers have been tested with the following:

	sshd                     [ WORKS as of 20151112]
	ssh                      [ WORKS as of 20151112]
	sftp                     [ WORKS as of 20151022]
	curl                     [ WORKS as of 20151021]
	apache (debug mode)      [ WORKS as of 20150810]
	apache (prefork MPM)     [ WORKS as of 20151123] (2.4.6-31.x86-64 on Centos 7), (2.4.16-1.x84-64 on F22), (2.4.17-3.x86-64 on F22)
	nginx                    [ WORKS as of 20151123] Broken on Centos 7, unreliable on Fedora 23
	nodejs                   [ WORKS as of 20151123]
	java                     [ WORKS as of 20151010]
	MongoDB                  [ WORKS as of 20151028]
	Redis-server             [ WORKS as of 20151123]

Future:

	GET many different files via HTTP (web stress)
	LARGE continuous transfer (e.g. /dev/urandom all night)
	Open and close many TCP connections constantly
	Simulate packet loss (can be done with iptables)
	Many parallel TCP transfers
	Multithreaded software (e.g. apache in thread mode)
	UDP support



### Extended Version Notes

20151028 Added MongoDB support:

	- Added logic (RPC_MAP_REQ) to check whether a given AF_LOCAL socket is mapped to anything
	inside the service instance.

20151027 Added Redis-server support:

	- Added extra logic to detect socket re-issuing and consequent service-side double mapping.
	Redis appears to try to set its initial listen socket to IPV6 only, this currently fails. As
	a result, Redis will close the socket and re-open it. The server will now test for closures
	during mapping and will eliminate any mappings to broken pipes.

20151021 Added Node.js support:

	- syscall(long number, ...) is now intercepted and re-directs the __NR_accept4 call to our intercepted accept4() function

	- accept() now returns -EAGAIN in the case that we cannot read a signal byte from the descriptor linked to the service. This
	is because the uv__server_io() function in libuv used by Node.js looks for this return value upon failure, without it we
	were observing an innfinite loop in the I/O polling code in libuv.

	- accept4() now correctly sets given flags for descriptor returned by accept()

	- setsockopt() was modified to return success on any call with the following conditions:
	level == IPPROTO_TCP || (level == SOL_SOCKET && option_name == SO_KEEPALIVE)
	This might be unnecessary or might need a better workaround

	- Careful attention should be given to how arguments are passed in the intercepted syscall() function, this differs for
	32/64-bit systems
