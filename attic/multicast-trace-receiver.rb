#!/usr/bin/ruby

#
# This can be used with the debug build option ZT_TRACE_MULTICAST to trace
# a multicast cascade.
#
# Define ZT_TRACE_MULTICAST to the IP/port where this script will be listening.
# The default port here is 6060, so an example would be to add:
#
# -DZT_TRACE_MULTICAST=\"10.0.0.1/6060\"
#
# ... to DEFS in the Makefile. Then build and run ZeroTier One on a testnet and
# the box defined as the trace endpoint will get spammed with UDP packets
# containing trace information for multicast propagation. This script then dumps
# these trace packets to stdout. Look at the code in PacketDecoder.cpp to see
# what this information entails.
#

require 'socket'

s = UDPSocket.new
s.bind('0.0.0.0',6060)

loop {
	m = s.recvfrom(4096)[0].chomp
	puts m if m.length > 0
}
