Network Containers have been tested with the following:

sshd			[ WORKS as of 20151010] Long ~15-20s delay for client during connect
ssh			[ WORKS as of 20151010]
curl			[ WORKS as of 20151021]
apache (debug mode)	[ WORKS as of 2015xxxx]
apache (prefork MPM)	[ WORKS as of 20151021]
nginx			[ WORKS as of 2015xxxx]
nodejs			[ WORKS as of 20151021]
java			[ WORKS as of 2015xxxx]
tomcat			[ WORKS as of 2015xxxx]
thttpd			[ WORKS as of 2015xxxx]
vsftpd			[BROKEN as of 20151021] Server sends 500 when 220 is expected
mysql			[BROKEN as of 20151021]
postresql		[BROKEN as of 20151021]
MongoDB			[BROKEN as of 20151021]
pure-ftpd		[BROKEN as of 20151021] Socket operation on non-socket

To Test:

	GET many different files via HTTP (web stress)
	LARGE continuous transfer (e.g. /dev/urandom all night)
	Open and close many TCP connections constantly
	Simulate packet loss (can be done with iptables)
	Many parallel TCP transfers
	Multithreaded software (e.g. apache in thread mode)


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
