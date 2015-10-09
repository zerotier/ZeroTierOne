API Examples
======

This folder contains examples that can be posted with curl or another http query utility to a local instance.

To test querying with curl:

    curl -H 'X-ZT1-Auth:AUTHTOKEN' http://127.0.0.1:9993/status

To create a public network on a local controller (service must be built with "make ZT\_ENABLE\_NETWORK\_CONTROLLER=1"):

    curl -H 'X-ZT1-Auth:AUTHTOKEN' -X POST -d @public.json http://127.0.0.1:9993/controller/network/################

Replace AUTHTOKEN with the contents of this instance's authtoken.secret file and ################ with a valid network ID. Its first 10 hex digits must be the ZeroTier address of the controller itself, while the last 6 hex digits can be anything. Also be sure to change the port if you have this instance listening somewhere other than 9993.

After POSTing you can double check the network config with:

    curl -H 'X-ZT1-Auth:AUTHTOKEN' http://127.0.0.1:9993/controller/network/################

Once this network is created (and if your controller is online, etc.) you can then join this network from any device anywhere in the world and it will receive a valid network configuration.
