zerotier-cli(1) -- control local ZeroTier virtual network service
=================================================================

## SYNOPSIS

`zerotier-cli` [-switches] <command> [arguments]

## DESCRIPTION

**zerotier-cli** provides a simple command line interface to the local JSON API of the ZeroTier virtual network endpoint service zerotier-one(8).

By default **zerotier-cli** must be run as root or with `sudo`. If you want to allow an unprivileged user to use **zerotier-cli** to control the system ZeroTier service, you can create a local copy of the ZeroTier service authorization token in the user's home directory:

    sudo cp /var/lib/zerotier-one/authtoken.secret /home/user/.zeroTierOneAuthToken
    chown user /home/user/.zeroTierOneAuthToken
    chmod 0600 /home/user/.zeroTierOneAuthToken

(The location of ZeroTier's service home may differ by platform. See zerotier-one(8).)

Note that this gives the user the power to connect or disconnect the system to or from any virtual network, which is a significant permission.

**zerotier-cli** has several command line arguments that are visible in `help` output. The two most commonly used are `-j` for raw JSON output and `-D<path>` to specify an alternative ZeroTier service working directory. Raw JSON output is easier to parse in scripts and also contains verbose details not present in the tabular output. The `-D<path>` option specifies where the service's zerotier-one.port and authtoken.secret files are located if the service is not running at the default location for your system.

## COMMANDS

 * `help`:
   Displays **zerotier-cli** help.

 * `info`:
   Shows information about this device including its 10-digit ZeroTier address and apparent connection status. Use `-j` for more verbose output.

 * `listpeers`:
   This command lists the ZeroTier VL1 (virtual layer 1, the peer to peer network) peers this service knows about and has recently (within the past 30 minutes or so) communicated with. These are not necessarily all the devices on your virtual network(s), and may also include a few devices not on any virtual network you've joined. These are typically either root servers or network controllers.

 * `listnetworks`:
   This lists the networks your system belongs to and some information about them, such as any ZeroTier-managed IP addresses you have been assigned. (IP addresses assigned manually to ZeroTier interfaces will not be listed here. Use the standard network interface commands to see these.)

 * `join`:
   To join a network just use `join` and its 16-digit hex network ID. That's it. Then use `listnetworks` to see the status. You'll either get a reply from the network controller with a certificate and other info such as IP assignments, or you'll get "access denied." In this case you'll need the administrator of this network to authorize your device by its 10-digit device ID (visible with `info`) on the network's controller.

 * `leave`:
   Leaving a network is as easy as joining it. This disconnects from the network and deletes its interface from the system. Note that peers on the network may hang around in `listpeers` for up to 30 minutes until they time out due to lack of traffic. But if they no longer share a network with you, they can't actually communicate with you in any meaningful way.

## EXAMPLES

Join "Earth," ZeroTier's big public party line network:

    $ sudo zerotier-cli join 8056c2e21c000001
    $ sudo zerotier-cli listnetworks
    ( wait until you get an Earth IP )
    $ ping earth.zerotier.net
    ( you should now be able to ping our Earth test IP )

Leave "Earth":

    $ sudo zerotier-cli leave 8056c2e21c000001

List VL1 peers:

    $ sudo zerotier-cli listpeers

## COPYRIGHT

(c)2011-2016 ZeroTier, Inc. -- https://www.zerotier.com/ -- https://github.com/zerotier

## SEE ALSO

zerotier-one(8), zerotier-idtool(1)
