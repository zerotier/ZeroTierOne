HTTP one-to-all test
======

*This is really internal use code. You're free to test it out but expect to do some editing/tweaking to make it work. We used this to run some massive scale tests of our new geo-cluster-based root server infrastructure prior to taking it live.*

Before using this code you will want to edit agent.js to change SERVER_HOST to the IP address of where you will run server.js. This should typically be an open Internet IP, since this makes reporting not dependent upon the thing being tested. Also note that this thing does no security of any kind. It's designed for one-off tests run over a short period of time, not to be anything that runs permanently. You will also want to edit the Dockerfile if you want to build containers and change the network ID to the network you want to run tests over.

This code can be deployed across a large number of VMs or containers to test and benchmark HTTP traffic within a virtual network at scale. The agent acts as a server and can query other agents, while the server collects agent data and tells agents about each other. It's designed to use RFC4193-based ZeroTier IPv6 addresses within the cluster, which allows the easy provisioning of a large cluster without IP conflicts.

The Dockerfile builds an image that launches the agent. The image must be "docker run" with "--device=/dev/net/tun --privileged" to permit it to open a tun/tap device within the container. (Unfortunately CAP_NET_ADMIN may not work due to a bug in Docker and/or Linux.) You can run a bunch with a command like:

    for ((n=0;n<10;n++)); do docker run --device=/dev/net/tun --privileged -d zerotier/http-test; done
