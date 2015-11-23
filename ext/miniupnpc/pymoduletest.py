#! /usr/bin/python
# vim: tabstop=2 shiftwidth=2 expandtab
# MiniUPnP project
# Author : Thomas Bernard
# This Sample code is public domain.
# website : http://miniupnp.tuxfamily.org/

# import the python miniupnpc module
import miniupnpc
import sys

try:
  import argparse
  parser = argparse.ArgumentParser()
  parser.add_argument('-m', '--multicastif')
  parser.add_argument('-p', '--minissdpdsocket')
  parser.add_argument('-d', '--discoverdelay', type=int, default=200)
  parser.add_argument('-z', '--localport', type=int, default=0)
  # create the object
  u = miniupnpc.UPnP(**vars(parser.parse_args()))
except:
  print 'argparse not available'
  i = 1
  multicastif = None
  minissdpdsocket = None
  discoverdelay = 200
  localport = 0
  while i < len(sys.argv):
    print sys.argv[i]
    if sys.argv[i] == '-m' or sys.argv[i] == '--multicastif':
      multicastif = sys.argv[i+1]
    elif sys.argv[i] == '-p' or sys.argv[i] == '--minissdpdsocket':
      minissdpdsocket = sys.argv[i+1]
    elif sys.argv[i] == '-d' or sys.argv[i] == '--discoverdelay':
      discoverdelay = int(sys.argv[i+1])
    elif sys.argv[i] == '-z' or sys.argv[i] == '--localport':
      localport = int(sys.argv[i+1])
    else:
      raise Exception('invalid argument %s' % sys.argv[i])
    i += 2
  # create the object
  u = miniupnpc.UPnP(multicastif, minissdpdsocket, discoverdelay, localport)

print 'inital(default) values :'
print ' discoverdelay', u.discoverdelay
print ' lanaddr', u.lanaddr
print ' multicastif', u.multicastif
print ' minissdpdsocket', u.minissdpdsocket
#u.minissdpdsocket = '../minissdpd/minissdpd.sock'
# discovery process, it usualy takes several seconds (2 seconds or more)
print 'Discovering... delay=%ums' % u.discoverdelay
print u.discover(), 'device(s) detected'
# select an igd
try:
  u.selectigd()
except Exception, e:
  print 'Exception :', e
  sys.exit(1)
# display information about the IGD and the internet connection
print 'local ip address :', u.lanaddr
print 'external ip address :', u.externalipaddress()
print u.statusinfo(), u.connectiontype()
print 'total bytes : sent', u.totalbytesent(), 'received', u.totalbytereceived()
print 'total packets : sent', u.totalpacketsent(), 'received', u.totalpacketreceived()

#print u.addportmapping(64000, 'TCP',
#                       '192.168.1.166', 63000, 'port mapping test', '')
#print u.deleteportmapping(64000, 'TCP')

port = 0
proto = 'UDP'
# list the redirections :
i = 0
while True:
	p = u.getgenericportmapping(i)
	if p==None:
		break
	print i, p
	(port, proto, (ihost,iport), desc, c, d, e) = p
	#print port, desc
	i = i + 1

print u.getspecificportmapping(port, proto)
try:
  print u.getportmappingnumberofentries()
except Exception, e:
  print 'GetPortMappingNumberOfEntries() is not supported :', e

