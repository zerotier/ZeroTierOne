Simple Dockerfile Example
======

This is a simple Docker example using ZeroTier One in normal tun/tap mode. It uses a Dockerfile to build an image containing ZeroTier One and a main.sh that launches it with an identity supplied via the Docker environment via the ZEROTIER\_IDENTITY\_SECRET and ZEROTIER\_NETWORK variables. The Dockerfile assumes that the zerotier-one binary is in the build folder.

This is not a very secure way to load an identity secret, but it's useful for testing since it allows you to repeatedly launch Docker containers with the same identity. For production we'd recommend using something like Hashicorp Vault, or modifying main.sh to leave identities unspecified and allow the container to generate a new identity at runtime. Then you could script approval of containers using the controller API, approving them as they launch, etc. (We are working on better ways of doing mass provisioning.)

To use in normal tun/tap mode with Docker, containers must be run with the options "--device=/dev/net/tun --privileged". The main.sh script supplied here will complain and exit if these options are not present (no /dev/net/tun device).
