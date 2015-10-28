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




