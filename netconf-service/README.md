# ZeroTier One Network Configuration Service Helper

This code will at the moment probably not be of much interest to end users.
It's a "service," which is a sub-process that ZeroTier One can execute that
provides additional functionaly not found in the core executable.

ZeroTier One communicates with services by sending and receiving string-serialized
Dictionary objects terminated by an empty line for each Dictionary.

The netconf service, written in node.js and making use of the zerotier-idtool
binary (see index.js) handles responding to virtual network config requests.
It only runs on netconf masters. It fetches its configuration information from
a Redis database.

The schama for that database is in redis-schema.txt. Some record types such
as users will not be of interest to outside people. Pay attention to the
network record type.

To install, copy the netconf-service folder to /var/lib/zerotier-one/services.d
(create services.d first). Then run "npm install" from the netconf-service
folder there and finally copy netconf.service to the parent services.d folder.
Make sure that shell script is executable. This is what the ZeroTier One
service will execute. It in turn sets up the node environment and runs index.js.

The utility zerotier-idtool must be present as /usr/local/bin/zerotier-idtool. This
is hard coded for the moment. To get it build ZeroTier One from source and then
"sudo cp zerotier-one /usr/local/bin/zerotier-idtool" (the binary determines its
personality from argv[0] on execution).

Note: Windows builds of ZeroTier One do not support services. This code has only
been tested on Linux but will probably work on Mac too.

*Adam Ierymenko @ ZeroTier*
