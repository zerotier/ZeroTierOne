zerotier-one(8) -- ZeroTier virtual network endpoint service
============================================================

## SYNOPSIS

`zerotier-one` [-switches] [working directory]

## DESCRIPTION

**zerotier-one** is the service/daemon responsible for connecting a Unix (Linux/BSD/OSX) system to one or more ZeroTier virtual networks and presenting those networks to the system as virtual network ports. You can think of it as a peer to peer VPN client.

It's typically run by init systems like systemd (Linux) or launchd (Mac) rather than directly by the user, and it must be run as root unless you give it the `-U` switch and don't plan on actually joining networks (e.g. to run a network controller microservice only).

The **zerotier-one** service keeps its state and other files in a working directory. If this directory is not specified at launch it defaults to "/var/lib/zerotier-one" on Linux, "/Library/Application Support/ZeroTier/One" on Mac, and "/var/db/zerotier-one" on FreeBSD and other similar BSDs. The working directory should persist. It shouldn't be automatically cleaned by system cleanup daemons or stored in a volatile location. Loss of its identity.secret file results in loss of this system's unique 10-digit ZeroTier address and key.

Multiple instances of **zerotier-one** can be run on the same system as long as they are run with different primary ports (see switches) and a different working directory. But since a single service can join any number of networks, typically there's no point in doing this.

The **zerotier-one** service is controlled via a JSON API available at 127.0.0.1:<primary port> with the default primary port being 9993. Access to this API requires an authorization token normally found in the authtoken.secret file in the service's working directory. On some platforms access may be guarded by other measures such as socket peer UID/GID lookup if additional security options are enabled (this is not the default).

The first time the service is started in a fresh working directory, it generates a ZeroTier identity. On slow systems this process can take ten seconds or more due to an anti-DDOS/anti-counterfeit proof of work function used by ZeroTier in address generation. This only happens once, and once generated the result is saved in identity.secret in the working directory. This file represents and defines/claims your ZeroTier address and associated ECC-256 key pair.

## SWITCHES

 * `-h`:
   Display help.

 * `-v`:
   Display ZeroTier One version.

 * `-U`:
   Skip privilege check and allow to be run by non-privileged user. This is typically used when **zerotier-one** is built with the network controller option included. In this case the ZeroTier service might only be acting as a network controller and might never actually join networks, in which case it does not require elevated system permissions.

 * `-p<port>`:
   Specify a different primary port. If this is not given the default is 9993. If zero is given a random port is chosen each time.

 * `-d`:
   Fork and run as a daemon.

 * `-i`:
   Invoke the **zerotier-idtool** personality, in which case the binary behaves like zerotier-idtool(1). This happens automatically if the name of the binary (or a symlink to it) is zerotier-idtool.

 * `-q`:
   Invoke the **zerotier-cli** personality, in which case the binary behaves like zerotier-cli(1). This happens automatically if the name of the binary (or a symlink to it) is zerotier-cli.

## EXAMPLES

Run as daemon with OS default working directory and default port:

    $ sudo zerotier-one -d

Run as daemon with a different working directory and port:

    $ sudo zerotier-one -d -p12345 /tmp/zerotier-working-directory-test

## FILES

These are found in the service's working directory.

 * `identity.public`:
   The public portion of your ZeroTier identity, which is your 10-digit hex address and the associated public key.

 * `identity.secret`:
   Your full ZeroTier identity including its private key. This file identifies the system on the network, which means you can move a ZeroTier address around by copying this file and you should back up this file if you want to save your system's static ZeroTier address. This file must be protected, since theft of its secret key will allow anyone to impersonate your device on any network and decrypt traffic. For network controllers this file is particularly sensitive since it constitutes the private key for a certificate authority for the controller's networks.

 * `authtoken.secret`:
   The secret token used to authenticate requests to the service's local JSON API. If it does not exist it is generated from a secure random source on service start. To use, send it in the "X-ZT1-Auth" header with HTTP requests to 127.0.0.1:<primary port>.

 * `devicemap`:
   Remembers mappings of zt# interface numbers to ZeroTier networks so they'll persist across restarts. On some systems that support longer interface names that can encode the network ID (such as FreeBSD) this file may not be present.

 * `zerotier-one.pid`:
   ZeroTier's PID. This file is deleted on normal shutdown.

 * `zerotier-one.port`:
   ZeroTier's primary port, which is also where its JSON API is found at 127.0.0.1:<this port>. This file is created on startup and is read by zerotier-cli(1) to determine where it should find the control API.

 * `controller.db`:
   If the ZeroTier One service is built with the network controller enabled, this file contains the controller's SQLite3 database.

 * `controller.db.backup`:
   If the ZeroTier One service is built with the network controller enabled, it periodically backs up its controller.db database in this file (currently every 5 minutes if there have been changes). Since this file is not a currently in use SQLite3 database it's safer to back up without corruption. On new backups the file is rotated out rather than being rewritten in place.

 * `iddb.d/` (directory):
   Caches the public identity of every peer ZeroTier has spoken with in the last 60 days. This directory and its contents can be deleted, but this may result in slower connection initiations since it will require that we go out and re-fetch full identities for peers we're speaking to.

 * `networks.d` (directory):
   This caches network configurations and certificate information for networks you belong to. ZeroTier scans this directory for <network ID>.conf files on startup to recall its networks, so "touch"ing an empty <network ID>.conf file in this directory is a way of pre-configuring ZeroTier to join a specific network on startup without using the API. If the config file is empty ZeroTIer will just fetch it from the network's controller.

## COPYRIGHT

(c)2011-2016 ZeroTier, Inc. -- https://www.zerotier.com/ -- https://github.com/zerotier

## SEE ALSO

zerotier-cli(1), zerotier-idtool(1)
