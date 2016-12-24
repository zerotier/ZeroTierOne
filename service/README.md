ZeroTier One Network Virtualization Service
======

This is the common background service implementation for ZeroTier One, the VPN-like OS-level network virtualization service.

It provides a ready-made core I/O loop and a local HTTP-based JSON control bus for controlling the service. This control bus HTTP server can also serve the files in ui/ if this folder's contents are installed in the ZeroTier home folder. The ui/ implements a React-based HTML5 user interface which is then wrappered for various platforms via MacGap, Windows .NET WebControl, etc. It can also be used locally from scripts or via *curl*.

### Network Virtualization Service API

The JSON API supports GET, POST/PUT, and DELETE. PUT is treated as a synonym for POST. Other methods including HEAD are not supported.

Values POSTed to the JSON API are *extremely* type sensitive. Things *must* be of the indicated type, otherwise they will be ignored or will generate an error. Anything quoted is a string so booleans and integers must lack quotes. Booleans must be *true* or *false* and nothing else. Integers cannot contain decimal points or they are floats (and vice versa). If something seems to be getting ignored or set to a strange value, or if you receive errors, check the type of all JSON fields you are submitting against the types listed below. Unrecognized fields in JSON objects are also ignored.

API requests must be authenticated via an authentication token. ZeroTier One saves this token in the *authtoken.secret* file in its working directory. This token may be supplied via the *auth* URL parameter (e.g. '?auth=...') or via the *X-ZT1-Auth* HTTP request header. Static UI pages are the only thing the server will allow without authentication.

A *jsonp* URL argument may be supplied to request JSONP encapsulation. A JSONP response is sent as a script with its JSON response payload wrapped in a call to the function name supplied as the argument to *jsonp*.

#### /status

 * Purpose: Get running node status and addressing info
 * Methods: GET
 * Returns: { object }

<table>
<tr><td><b>Field</b></td><td><b>Type</b></td><td><b>Description</b></td><td><b>Writable</b></td></tr>
<tr><td>address</td><td>string</td><td>10-digit hexadecimal ZeroTier address of this node</td><td>no</td></tr>
<tr><td>publicIdentity</td><td>string</td><td>Full public ZeroTier identity of this node</td><td>no</td></tr>
<tr><td>online</td><td>boolean</td><td>Does this node appear to have upstream network access?</td><td>no</td></tr>
<tr><td>tcpFallbackActive</td><td>boolean</td><td>Is TCP fallback mode active?</td><td>no</td></tr>
<tr><td>versionMajor</td><td>integer</td><td>ZeroTier major version</td><td>no</td></tr>
<tr><td>versionMinor</td><td>integer</td><td>ZeroTier minor version</td><td>no</td></tr>
<tr><td>versionRev</td><td>integer</td><td>ZeroTier revision</td><td>no</td></tr>
<tr><td>version</td><td>string</td><td>Version in major.minor.rev format</td><td>no</td></tr>
<tr><td>clock</td><td>integer</td><td>Node system clock in ms since epoch</td><td>no</td></tr>
</table>

#### /config

 * Purpose: Get or set local configuration
 * Methods: GET, POST
 * Returns: { object }

No local configuration options are exposed yet.

<table>
<tr><td><b>Field</b></td><td><b>Type</b></td><td><b>Description</b></td><td><b>Writable</b></td></tr>
</table>

#### /network

 * Purpose: Get all network memberships
 * Methods: GET
 * Returns: [ {object}, ... ]

Getting /network returns an array of all networks that this node has joined. See below for network object format.

#### /network/\<network ID\>

 * Purpose: Get, join, or leave a network
 * Methods: GET, POST, DELETE
 * Returns: { object }

To join a network, POST to it. Since networks have no mandatory writable parameters, POST data is optional and may be omitted. Example: POST to /network/8056c2e21c000001 to join the public "Earth" network. To leave a network, DELETE it e.g. DELETE /network/8056c2e21c000001.

Most network settings are not writable, as they are defined by the network controller.

<table>
<tr><td><b>Field</b></td><td><b>Type</b></td><td><b>Description</b></td><td><b>Writable</b></td></tr>
<tr><td>nwid</td><td>string</td><td>16-digit hex network ID</td><td>no</td></tr>
<tr><td>mac</td><td>string</td><td>Ethernet MAC address of virtual network port</td><td>no</td></tr>
<tr><td>name</td><td>string</td><td>Network short name as configured on network controller</td><td>no</td></tr>
<tr><td>status</td><td>string</td><td>Network status: OK, ACCESS_DENIED, PORT_ERROR, etc.</td><td>no</td></tr>
<tr><td>type</td><td>string</td><td>Network type, currently PUBLIC or PRIVATE</td><td>no</td></tr>
<tr><td>mtu</td><td>integer</td><td>Ethernet MTU</td><td>no</td></tr>
<tr><td>dhcp</td><td>boolean</td><td>If true, DHCP may be used to obtain an IP address</td><td>no</td></tr>
<tr><td>bridge</td><td>boolean</td><td>If true, this node may bridge in other Ethernet devices</td><td>no</td></tr>
<tr><td>broadcastEnabled</td><td>boolean</td><td>Is Ethernet broadcast (ff:ff:ff:ff:ff:ff) allowed?</td><td>no</td></tr>
<tr><td>portError</td><td>integer</td><td>Error code (if any) returned by underlying OS "tap" driver</td><td>no</td></tr>
<tr><td>netconfRevision</td><td>integer</td><td>Network configuration revision ID</td><td>no</td></tr>
<tr><td>multicastSubscriptions</td><td>[string]</td><td>Multicast memberships as array of MAC/ADI tuples</td><td>no</td></tr>
<tr><td>assignedAddresses</td><td>[string]</td><td>ZeroTier-managed IP address assignments as array of IP/netmask bits tuples</td><td>no</td></tr>
<tr><td>portDeviceName</td><td>string</td><td>OS-specific network device name (if available)</td><td>no</td></tr>
</table>

#### /peer

 * Purpose: Get all peers
 * Methods: GET
 * Returns: [ {object}, ... ]

Getting /peer returns an array of peer objects for all current peers. See below for peer object format.

#### /peer/\<address\>

 * Purpose: Get information about a peer
 * Methods: GET
 * Returns: { object }

<table>
<tr><td><b>Field</b></td><td><b>Type</b></td><td><b>Description</b></td><td><b>Writable</b></td></tr>
<tr><td>address</td><td>string</td><td>10-digit hex ZeroTier address</td><td>no</td></tr>
<tr><td>lastUnicastFrame</td><td>integer</td><td>Time of last unicast frame in ms since epoch</td><td>no</td></tr>
<tr><td>lastMulticastFrame</td><td>integer</td><td>Time of last multicast frame in ms since epoch</td><td>no</td></tr>
<tr><td>versionMajor</td><td>integer</td><td>Major version of remote if known</td><td>no</td></tr>
<tr><td>versionMinor</td><td>integer</td><td>Minor version of remote if known</td><td>no</td></tr>
<tr><td>versionRev</td><td>integer</td><td>Revision of remote if known</td><td>no</td></tr>
<tr><td>version</td><td>string</td><td>Version in major.minor.rev format</td><td>no</td></tr>
<tr><td>latency</td><td>integer</td><td>Latency in milliseconds if known</td><td>no</td></tr>
<tr><td>role</td><td>string</td><td>LEAF, HUB, or ROOTSERVER</td><td>no</td></tr>
<tr><td>paths</td><td>[object]</td><td>Array of path objects (see below)</td><td>no</td></tr>
</table>

Path objects describe direct physical paths to peer. If no path objects are listed, peer is only reachable via indirect relay fallback. Path object format is:

<table>
<tr><td><b>Field</b></td><td><b>Type</b></td><td><b>Description</b></td><td><b>Writable</b></td></tr>
<tr><td>address</td><td>string</td><td>Physical socket address e.g. IP/port for UDP</td><td>no</td></tr>
<tr><td>lastSend</td><td>integer</td><td>Last send via this path in ms since epoch</td><td>no</td></tr>
<tr><td>lastReceive</td><td>integer</td><td>Last receive via this path in ms since epoch</td><td>no</td></tr>
<tr><td>fixed</td><td>boolean</td><td>If true, this is a statically-defined "fixed" path</td><td>no</td></tr>
<tr><td>preferred</td><td>boolean</td><td>If true, this is the current preferred path</td><td>no</td></tr>
</table>
