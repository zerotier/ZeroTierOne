# ZeroTier One in a container!

**NOTE:** _Most of this information pertains to the docker image only. For more information about ZeroTier, check out the repository_: [here](https://github.com/zerotier/ZeroTierOne) or the [commercial website](https://www.zerotier.com).

[ZeroTier](https://www.zerotier.com) is a smart programmable Ethernet switch for planet Earth. It allows all networked devices, VMs, containers, and applications to communicate as if they all reside in the same physical data center or cloud region.

This is accomplished by combining a cryptographically addressed and secure peer to peer network (termed VL1) with an Ethernet emulation layer somewhat similar to VXLAN (termed VL2). Our VL2 Ethernet virtualization layer includes advanced enterprise SDN features like fine grained access control rules for network micro-segmentation and security monitoring.

All ZeroTier traffic is encrypted end-to-end using secret keys that only you control. Most traffic flows peer to peer, though we offer free (but slow) relaying for users who cannot establish peer to peer connections.

The goals and design principles of ZeroTier are inspired by among other things the original [Google BeyondCorp](https://static.googleusercontent.com/media/research.google.com/en//pubs/archive/43231.pdf) paper and the [Jericho Forum](https://en.wikipedia.org/wiki/Jericho_Forum) with its notion of "deperimeterization."

Visit [ZeroTier's site](https://www.zerotier.com/) for more information and [pre-built binary packages](https://www.zerotier.com/download/). Apps for Android and iOS are available for free in the Google Play and Apple app stores.

ZeroTier is licensed under the [BSL version 1.1](https://mariadb.com/bsl11/). See [LICENSE.txt](LICENSE.txt) and the [ZeroTier pricing page](https://www.zerotier.com/pricing) for details. ZeroTier is free to use internally in businesses and academic institutions and for non-commercial purposes. Certain types of commercial use such as building closed-source apps and devices based on ZeroTier or offering ZeroTier network controllers and network management as a SaaS service require a commercial license.

A small amount of third party code is also included in ZeroTier and is not subject to our BSL license. See [AUTHORS.md](AUTHORS.md) for a list of third party code, where it is included, and the licenses that apply to it. All of the third party code in ZeroTier is liberally licensed (MIT, BSD, Apache, public domain, etc.).

## Building the docker image

Due to the network being a substrate for most applications and not an application unto itself, it makes sense that many people would want to build their own image based on our formula.

The image is based on `debian:buster`.

The `Dockerfile.release` file contains build instructions for building the described image in the rest of the README. The build is multi-arch and multi-release capable.

These build arguments power the build:

- `PACKAGE_BASEURL`: The base URL of the package repository to fetch from. (default: `https://download.zerotier.com/debian/buster/pool/main/z/zerotier-one/`)
- `ARCH`: The architecture of the package, in debian format. Must match your image arch. (default: `amd64`)
- `VERSION`: **REQUIRED** the version of ZeroTier to fetch.

You can build this image like so:

```
docker build -f Dockerfile.release -t mybuild --build-arg VERSION=1.6.5 .
```

## Using the docker image

The `entrypoint.sh` in the docker image is a little different; zerotier will be spawned in the background and the "main process" is actually just a sleeping shell script. This allows `zerotier-one` to gracefully terminate in some situations largely unique to docker.

The `zerotier/zerotier` image requires the `CAP_NET_ADMIN` capability and the `/dev/net/tun` device must be forwarded to it.

To join a network, simply supply it on the command-line; you can supply multiple networks.

```
docker run --name myzerotier --rm --cap-add NET_ADMIN --device /dev/net/tun zerotier/zerotier:latest abcdefdeadbeef00
```

Once joining all the networks you have provided, it will sleep until terminated. Note that in ZeroTier, joining a network does not necessarily mean you have an IP or can do anything, really. You will want to probe the control socket:

```
docker exec myzerotier zerotier-cli listnetworks
```

To ensure you have a network available before trying to listen on it. Without pre-configuring the identity, this usually means going to the central admin panel and clicking the checkmark against your zerotier identity.

### Environment Variables

You can control a few settings including the identity used and the authtoken used to interact with the control socket (which you can forward and access through `localhost:9993`).

- `ZEROTIER_API_SECRET`: replaces the `authtoken.secret` before booting and allows you to manage the control socket's authentication key.
- `ZEROTIER_IDENTITY_PUBLIC`: the `identity.public` file for zerotier-one. Use `zerotier-idtool` to generate one of these for you.
- `ZEROTIER_IDENTITY_SECRET`: the `identity.secret` file for zerotier-one. Use `zerotier-idtool` to generate one of these for you.

### Tips

- Forwarding port `<dockerip>:9993` to somewhere outside is probably a good idea for highly trafficked services.
- Forwarding `localhost:9993` to a control network where you can drive it remotely might be a good idea, just be sure to set your authtoken properly through environment variables.
- Pre-generating your identities could be much simpler to do via our [terraform plugin](https://github.com/zerotier/terraform-provider-zerotier)
