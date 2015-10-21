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
