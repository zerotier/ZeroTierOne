ZeroTier Network Containers 'service' and 'intercept'
======
ZeroTier Network Containers is a blisteringly-fast and easy-to-use networking integration for Docker containers.


### Functional Overview:

This system exists as a dynamically-linked library, and a stand-alone background service.

The intercept is compiled as a shared library and installed in some user-accessible directory. When you want to intercept
a user application you dynamically link the shared library to the application during runtime. When the application starts, the 
intercept's global constructor is called which sets up a hidden pipe which is used to communicate remote procedure calls (RPC) to the host Netcon service running in the background.

When an RPC for a socket() is received by the Netcon service from the harnessed application, the Netcon service will ask the lwIP stack for a new PCB structure (used to represent a connection), if the system permits its allocation, it will be passed to Netcon where a PCB/socket table entry will be created. The table is used for mapping [callbacks from lwIP] and [RPCs from the intercept] to the correct connections.

Upon the first call to a intercept-overriden system call, a Unix-domain socket is opened between the Netcon service and the application's intercept. This socket provides us the ability to pass file descriptors of newly-created socketpairs to the intercept (used as the read/write buffer). More specifically, after the socketpair creation, one end is kept in a table entry in Netcon and one end is sent to the intercept.

### Reading and Writing from application
When the application reads or writes data to a socket, the intercept will override the call and read/write from one end of the socketpair created for that specific connection. The Netcon service is continually polling the file descriptors for the opposite end of the socketpair. When an available event is detected (such as the presence of data on the buffer), Netcon will consult the routing table and determine which PCB should receive this new data. A call is then made to lwIP's tcp_write(). 

### How Netcon receives data
When data is received by lwIP, a callback in the Netcon service is called which then consults the internal routing table and maps the data to the correct socketpair. From here, it's up to the harnessed application to either poll() the opposite end of the socketpair, or read from it. No notification is sent to the intercept.



### Building from Source (and Installing)

Build library:

    make lib

Build service:

    make service

Build test applications (linked with library):

    make tests

Install:

    make install



### Running

To start a service and automatically intercept an application:

    ./service my_app

Alternatively, to intercept a specific application (requires an already running service):

    intercept ./my_app

To start the Network Containers service:

    ./service

To monitor lwIP network I/O:

    tcpdump -l -n -i tap0

Show what dynamic libraries are set for inclusion:

    intercept show

Start example server (with harness):

    intercept ./test_tx.o 

Start example client (with intercept) to communicate with server:

    intercept ./test_rx.o 127.0.0.1



### Testing

To run a simple RX/TX test:

    make service
    make lib
    make install
    make tests
    
    ./service
    ./multitransfer.sh




### Compatibility

Network Containers have been tested with the following:

sshd			[ WORKS as of 20151022] Long ~15-20s delay for client during connect
ssh			[ WORKS as of 20151022]
sftp			[ WORKS as of 20151022]
curl			[ WORKS as of 20151021] Sometimes "tcp_input: pcb->next != pcb (before cache)" is seen
apache (debug mode)	[ WORKS as of 20150810]
apache (prefork MPM)	[ WORKS as of 20151021]
nginx			[ WORKS as of 20151022]
nodejs			[ WORKS as of 20151021]
java			[ WORKS as of 20151010]
tomcat			[ WORKS as of 2015xxxx]
thttpd			[ WORKS as of 2015xxxx]
vsftpd			[BROKEN as of 20151021] Server sends 500 when 220 is expected
mysql			[BROKEN as of 20151021]
postresql		[BROKEN as of 20151021]
MongoDB			[BROKEN as of 20151021]
Redis-server		[ WORKS as of 20151027]
pure-ftpd		[BROKEN as of 20151021] Socket operation on non-socket

To Test:

	GET many different files via HTTP (web stress)
	LARGE continuous transfer (e.g. /dev/urandom all night)
	Open and close many TCP connections constantly
	Simulate packet loss (can be done with iptables)
	Many parallel TCP transfers
	Multithreaded software (e.g. apache in thread mode)



### Extended Version Notes

20151027 Added Redis-server support
Notes:
 - Added extra logic to detect socket re-issuing and consequent service-side double mapping.
   Redis appears to try to set its initial listen socket to IPV6 only, this currently fails. As 
   a result, Redis will close the socket and re-open it. The server will now test for closures
   during mapping and will eliminate any mappings to broken pipes.


20151021 Added Node.js support
Notes:
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




