Running ZeroTier One
======

This guide is for those building and running from source. See BUILDING.md
first.

The wiki at GitHub contains several pages that are probably also of interest:
  https://github.com/zerotier/ZeroTierOne/wiki

### MacOS

On Mac, the default ZeroTier home is:

/Library/Application Support/ZeroTier/One

ZeroTier ships with a kernel extension for its own tap device, which it
stores in the above directory. To install this, type:

sudo make install-mac-tap

This will create the ZeroTier One home above if it does not exist and install
the kext there. Note that the kext must be owned by root:wheel. The make
rule for install-mac-tap takes care of that.

Next, simply run the binary. It must be run as root to open the tap device.
If run with no options, it will use the default home directory above.

sudo ./zerotier-one &

### LINUX

On Linux, the default ZeroTier home is:

/var/lib/zerotier-one

Just type:

sudo mkdir /var/lib/zerotier-one
sudo ./zerotier-one &

Your system must have the Linux tun/tap driver available (tun). All tested
distributions so far ship with this driver as a module that will load
automatically.

UDP port 9993 must be open in your local firewall for this to work properly.
How to do this varies by Linux distribution.

 - Opening port 9993 on Ubuntu

Follow the Ubuntu documentation about UFW https://help.ubuntu.com/community/UFW

Check if your UFW is active.

sudo ufw status verbose

If it is active, open UDP port 9993

sudo ufw allow 9993/udp

You should now be able to ping and browse earth.zerotier.net

### FreeBSD

FreeBSD is identical to Linux except that the default home is
/var/db/zerotier-one instead of /var/lib.

### WINDOWS

Run zerotier-one.exe -h for help. There's a command to install the current
binary as a service to run it that way, and another option to run it from
the Windows console.

### Once you're up and running...

To use the command line interface, see this guide:
  https://github.com/zerotier/ZeroTierOne/wiki/Command-Line-Interface

If you want to test by joining the Earth network, try:
  sudo ./zerotier-cli join 8056c2e21c000001

An interface called 'zt####' should appear and should get an IP address in
the 28.0.0.0/7 range (28.* or 29.*) within a few seconds or so. Then try
pinging earth.zerotier.net or navigating to http://earth.zerotier.net/ in
a web browser.
