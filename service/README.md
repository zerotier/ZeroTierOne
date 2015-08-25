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

### Network Controller API

If ZeroTier One was built with *ZT\_ENABLE\_NETWORK\_CONTROLLER* defined, the following API paths are available. Otherwise these paths will return 404.

#### /controller

 * Purpose: Check for controller function and return controller status
 * Methods: GET
 * Returns: { object }

<table>
<tr><td><b>Field</b></td><td><b>Type</b></td><td><b>Description</b></td><td><b>Writable</b></td></tr>
<tr><td>controller</td><td>boolean</td><td>Always 'true' if controller is running</td><td>no</td></tr>
<tr><td>apiVersion</td><td>integer</td><td>JSON API version, currently 1</td><td>no</td></tr>
<tr><td>clock</td><td>integer</td><td>Controller system clock in ms since epoch</td><td>no</td></tr>
</table>

#### /controller/network

 * Purpose: List all networks hosted by this controller
 * Methods: GET
 * Returns: [ string, ... ]

This returns an array of 16-digit hexadecimal network IDs. Unlike /network under the top-level API, it does not dump full network information for all networks as this may be quite large for a large controller.

#### /controller/network/\<network ID\>

 * Purpose: Create, configure, and delete hosted networks
 * Methods: GET, POST, DELETE
 * Returns: { object }

By making queries to this path you can create, configure, and delete networks. DELETE is final, so don't do it unless you really mean it.

It's important to understand how network IDs work. The first ten digits (most significant 40 bits) of a network ID are the ZeroTier address of the controller. This is how clients find it. The last six digits (least significant 24 bits) are arbitrary and serve to identify the network uniquely on the controller.

Thus a network's first ten digits *must* be the controller's address. If your controller is *deadbeef01*, then the networks it controls must have IDs like *deadbeef01feed02* or *deadbeef01beef03*. This API however *does not* enforce this requirement. It will allow you to add arbitrary network IDs, but they won't work since clients will never be able to find them. To create a new network with a random last six digits safely and atomically, you can POST to */controller/network/##########\_\_\_\_\_\_* where ########## is the controller's address and the underscores are as shown. This will pick a random unallocated network ID, which will be returned in the 'nwid' field of the returned JSON object.

<table>
<tr><td><b>Field</b></td><td><b>Type</b></td><td><b>Description</b></td><td><b>Writable</b></td></tr>
<tr><td>nwid</td><td>string</td><td>16-digit hex network ID</td><td>no</td></tr>
<tr><td>name</td><td>string</td><td>Short network name (max: 127 chars)</td><td>yes</td></tr>
<tr><td>private</td><td>boolean</td><td>False if public network, true for access control</td><td>yes</td></tr>
<tr><td>enableBroadcast</td><td>boolean</td><td>True to allow Ethernet broadcast (ff:ff:ff:ff:ff:ff)</td><td>yes</td></tr>
<tr><td>allowPassiveBridging</td><td>boolean</td><td>True to allow any member to bridge (experimental!)</td><td>yes</td></tr>
<tr><td>v4AssignMode</td><td>string</td><td>'none', 'zt', or 'dhcp' (see below)</td><td>yes</td></tr>
<tr><td>v6AssignMode</td><td>string</td><td>'none', 'zt', or 'dhcp' (see below)</td><td>yes</td></tr>
<tr><td>multicastLimit</td><td>integer</td><td>Maximum number of multicast recipients per multicast/broadcast address</td><td>yes</td></tr>
<tr><td>creationTime</td><td>integer</td><td>Time network was created in ms since epoch</td><td>no</td></tr>
<tr><td>revision</td><td>integer</td><td>Network config revision number</td><td>no</td></tr>
<tr><td>memberRevisionCounter</td><td>integer</td><td>Current value of network revision counter (incremented after every member add or revision)</td><td>no</td></tr>
<tr><td>clock</td><td>integer</td><td>Current clock in ms since epoch (for convenience)</td><td>no</td></tr>
<tr><td>authorizedMemberCount</td><td>integer</td><td>Number of authorized members</td><td>no</td></tr>
<tr><td>relays</td><td>[object]</td><td>Array of network-specific relay nodes (see below)</td><td>yes</td></tr>
<tr><td>ipLocalRoutes</td><td>[string]</td><td>Array of IP network/netmask entries corresponding to networks routed directly via this interface (e.g. 10.0.0.0/8 to route 10.0.0.0 via this interface)</td></tr>
<tr><td>ipAssignmentPools</td><td>[object]</td><td>Array of IP auto-assignment pools for 'zt' assignment mode</td><td>yes</td></tr>
<tr><td>rules</td><td>[object]</td><td>Array of network flow rules (see below)</td><td>yes</td></tr>
</table>

The network member list includes both authorized and unauthorized members. DELETE unauthorized members to remove them from the list. Relays, IP assignment pools, and rules are edited via direct POSTs to the network object. New values replace all previous values.

Networks must have rules. If there are no rules, the default action is 'deny'. As also documented in the Rule object definition below, rules currently only support etherType and allow/deny. Thus to make a functioning network, add etherType allow entries for IPV4/ARP and/or IPv6. Alternately you can add a null allow entry to allow all traffic, causing the network to behave like a normal pass-through switch.

**Relay object format:**

Relay objects define network-specific preferred relay nodes. Traffic to peers on this network will preferentially use these relays if they are available, and otherwise will fall back to the global rootserver infrastructure.

<table>
<tr><td><b>Field</b></td><td><b>Type</b></td><td><b>Description</b></td></tr>
<tr><td>address</td><td>string</td><td>10-digit ZeroTier address of relay node</td></tr>
<tr><td>phyAddress</td><td>string</td><td>Fixed path address in IP/port format e.g. 192.168.1.1/9993</td></tr>
</table>

**IP assignment pool object format:**

IP assignment pools are only used if they are within a network specified in ipLocalRoutes.

<table>
<tr><td><b>Field</b></td><td><b>Type</b></td><td><b>Description</b></td></tr>
<tr><td>ipRangeStart</td><td>string</td><td>Start of IP assignment range</td></tr>
<tr><td>ipRangeEnd</td><td>string</td><td>End of IP assignment range</td></tr>
</table>

**Rule object format:**

 * **Note**: at the moment, <u>only rules specifying allowed Ethernet types are used</u>. The database supports a richer rule set, but this is not implemented yet in the client. <u>Other types of rules will have no effect</u> (yet).

Rules are matched in order of ruleNo. If no rules match, the default action is 'drop'. To allow all traffic, create a single rule with all *null* fields and an action of 'accept'.

Rule object fields can be *null*, in which case they are omitted from the object. A null field indicates "no match on this criteria."

IP related fields apply only to Ethernet frames of type IPv4 or IPV6. Otherwise they are ignored.

<table>
<tr><td><b>Field</b></td><td><b>Type</b></td><td><b>Description</b></td></tr>
<tr><td>ruleNo</td><td>integer</td><td>User-defined rule ID and sort order</td></tr>
<tr><td>nodeId</td><td>string</td><td>10-digit hex ZeroTier address of node if this rule is local to only one member</td></tr>
<tr><td>sourcePort</td><td>string</td><td>10-digit hex ZeroTier address of source port on virtual switch (source device address)</td></tr>
<tr><td>destPort</td><td>string</td><td>10-digit hex ZeroTier address of destination port on virtual switch (destination device address)</td></tr>
<tr><td>vlanId</td><td>integer</td><td>Ethernet VLAN ID</td></tr>
<tr><td>vlanPcp</td><td>integer</td><td>Ethernet VLAN priority code point (PCP) ID</td></tr>
<tr><td>etherType</td><td>integer</td><td>Ethernet frame type</td></tr>
<tr><td>macSource</td><td>string</td><td>Ethernet source MAC address</td></tr>
<tr><td>macDest</td><td>string</td><td>Ethernet destination MAC address</td></tr>
<tr><td>ipSource</td><td>string</td><td>Source IP address</td></tr>
<tr><td>ipDest</td><td>string</td><td>Destination IP address</td></tr>
<tr><td>ipTos</td><td>integer</td><td>IP TOS field</td></tr>
<tr><td>ipProtocol</td><td>integer</td><td>IP protocol</td></tr>
<tr><td>ipSourcePort</td><td>integer</td><td>IP source port</td></tr>
<tr><td>ipDestPort</td><td>integer</td><td>IP destination port</td></tr>
<tr><td>action</td><td>string</td><td>Rule action: accept, drop, etc.</td></tr>
</table>

#### /controller/network/\<network ID\>/member/\<address\>

 * Purpose: Create, authorize, or remove a network member
 * Methods: GET, POST, DELETE
 * Returns: { object }

<table>
<tr><td><b>Field</b></td><td><b>Type</b></td><td><b>Description</b></td><td><b>Writable</b></td></tr>
<tr><td>nwid</td><td>string</td><td>16-digit hex network ID</td><td>no</td></tr>
<tr><td>clock</td><td>integer</td><td>Current clock in ms since epoch (for convenience)</td><td>no</td></tr>
<tr><td>address</td><td>string</td><td>10-digit hex ZeroTier address</td><td>no</td></tr>
<tr><td>authorized</td><td>boolean</td><td>Is member authorized?</td><td>yes</td></tr>
<tr><td>activeBridge</td><td>boolean</td><td>This member is an active network bridge</td><td>yes</td></tr>
<tr><td>identity</td><td>string</td><td>Full ZeroTier identity of member</td><td>no</td></tr>
<tr><td>ipAssignments</td><td>[string]</td><td>Array of IP/bits IP assignments</td><td>yes</td></tr>
<tr><td>memberRevision</td><td>integer</td><td>Member revision counter value from network at time of last revision or member creation</td><td>no</td></tr>
</table>
