ZeroTier One Network Virtualization Service
======

This is the actual implementation of ZeroTier One, a service providing connectivity to ZeroTier virtual networks for desktops, laptops, servers, VMs, etc. (Mobile versions for iOS and Android have their own implementations in native Java and Objective C that leverage only the ZeroTier core engine.)

### Local Configuration File

A file called `local.conf` in the ZeroTier home folder contains configuration options that apply to the local node. (It does not exist unless you create it). It can be used to set up trusted paths, blacklist physical paths, set up physical path hints for certain nodes, and define trusted upstream devices (federated roots). In a large deployment it can be deployed using a tool like Puppet, Chef, SaltStack, etc. to set a uniform configuration across systems. It's a JSON format file that can also be edited and rewritten by ZeroTier One itself, so ensure that proper JSON formatting is used. 

Settings available in `local.conf` (this is not valid JSON, and JSON does not allow comments):

```javascript
{
	"physical": { /* Settings that apply to physical L2/L3 network paths. */
		"NETWORK/bits": { /* Network e.g. 10.0.0.0/24 or fd00::/32 */
			"blacklist": true|false, /* If true, blacklist this path for all ZeroTier traffic */
			"trustedPathId": 0|!0, /* If present and nonzero, define this as a trusted path (see below) */
			"mtu": 0|!0 /* if present and non-zero, set UDP maximum payload MTU for this path */
		} /* ,... additional networks */
	},
	"virtual": { /* Settings applied to ZeroTier virtual network devices (VL1) */
		"##########": { /* 10-digit ZeroTier address */
			"try": [ "IP/port"/*,...*/ ], /* Hints on where to reach this peer if no upstreams/roots are online */
			"blacklist": [ "NETWORK/bits"/*,...*/ ] /* Blacklist a physical path for only this peer. */
		}
	},
	"settings": { /* Other global settings */
		"primaryPort": 1-65535, /* If set, override default port of 9993 and any command line port */
		"secondaryPort": 1-65535, /* If set, override default random secondary port */
		"tertiaryPort": 1-65535, /* If set, override default random tertiary port */
		"portMappingEnabled": true|false, /* If true (the default), try to use uPnP or NAT-PMP to map ports */
		"allowSecondaryPort": true|false /* false will also disable secondary port */
		"softwareUpdate": "apply"|"download"|"disable", /* Automatically apply updates, just download, or disable built-in software updates */
		"softwareUpdateChannel": "release"|"beta", /* Software update channel */
		"softwareUpdateDist": true|false, /* If true, distribute software updates (only really useful to ZeroTier, Inc. itself, default is false) */
		"interfacePrefixBlacklist": [ "XXX",... ], /* Array of interface name prefixes (e.g. eth for eth#) to blacklist for ZT traffic */
		"allowManagementFrom": [ "NETWORK/bits", ...] |null, /* If non-NULL, allow JSON/HTTP management from this IP network. Default is 127.0.0.1 only. */
		"bind": [ "ip",... ], /* If present and non-null, bind to these IPs instead of to each interface (wildcard IP allowed) */
		"allowTcpFallbackRelay": true|false, /* Allow or disallow establishment of TCP relay connections (true by default) */
		"multipathMode": 0|1|2 /* multipath mode: none (0), random (1), proportional (2) */
	}
}
```

 * **trustedPathId**: A trusted path is a physical network over which encryption and authentication are not required. This provides a performance boost but sacrifices all ZeroTier's security features when communicating over this path. Only use this if you know what you are doing and really need the performance! To set up a trusted path, all devices using it *MUST* have the *same trusted path ID* for the same network. Trusted path IDs are arbitrary positive non-zero integers. For example a group of devices on a LAN with IPs in 10.0.0.0/24 could use it as a fast trusted path if they all had the same trusted path ID of "25" defined for that network.

An example `local.conf`:

```javascript
{
	"physical": {
		"10.0.0.0/24": {
			"blacklist": true
		},
		"10.10.10.0/24": {
			"trustedPathId": 101010024
		},
	},
	"virtual": {
		"feedbeef12": {
			"role": "UPSTREAM",
			"try": [ "10.10.20.1/9993" ],
			"blacklist": [ "192.168.0.0/24" ]
		}
	},
	"settings": {
		"softwareUpdate": "apply",
		"softwareUpdateChannel": "release"
	}
}
```

### Network Virtualization Service API

The JSON API supports GET, POST/PUT, and DELETE. PUT is treated as a synonym for POST. Other methods including HEAD are not supported.

Values POSTed to the JSON API are *extremely* type sensitive. Things *must* be of the indicated type, otherwise they will be ignored or will generate an error. Anything quoted is a string so booleans and integers must lack quotes. Booleans must be *true* or *false* and nothing else. Integers cannot contain decimal points or they are floats (and vice versa). If something seems to be getting ignored or set to a strange value, or if you receive errors, check the type of all JSON fields you are submitting against the types listed below. Unrecognized fields in JSON objects are also ignored.

API requests must be authenticated via an authentication token. ZeroTier One saves this token in the *authtoken.secret* file in its working directory. This token may be supplied via the *auth* URL parameter (e.g. '?auth=...') or via the *X-ZT1-Auth* HTTP request header. Static UI pages are the only thing the server will allow without authentication.

A *jsonp* URL argument may be supplied to request JSONP encapsulation. A JSONP response is sent as a script with its JSON response payload wrapped in a call to the function name supplied as the argument to *jsonp*.

#### /status

 * Purpose: Get running node status and addressing info
 * Methods: GET
 * Returns: { object }

| Field                 | Type          | Description                                       | Writable |
| --------------------- | ------------- | ------------------------------------------------- | -------- |
| address               | string        | 10-digit hex ZeroTier address of this node        | no       |
| publicIdentity        | string        | This node's ZeroTier identity.public              | no       |
| worldId               | integer       | ZeroTier world ID (never changes except for test) | no       |
| worldTimestamp        | integer       | Timestamp of most recent world definition         | no       |
| online                | boolean       | If true at least one upstream peer is reachable   | no       |
| tcpFallbackActive     | boolean       | If true we are using slow TCP fallback            | no       |
| relayPolicy           | string        | Relay policy: ALWAYS, TRUSTED, or NEVER           | no       |
| versionMajor          | integer       | Software major version                            | no       |
| versionMinor          | integer       | Software minor version                            | no       |
| versionRev            | integer       | Software revision                                 | no       |
| version               | string        | major.minor.revision                              | no       |
| clock                 | integer       | Current system clock at node (ms since epoch)     | no       |

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

| Field                 | Type          | Description                                       | Writable |
| --------------------- | ------------- | ------------------------------------------------- | -------- |
| id                    | string        | 16-digit hex network ID                           | no       |
| nwid                  | string        | 16-digit hex network ID (legacy field)            | no       |
| mac                   | string        | MAC address of network device for this network    | no       |
| name                  | string        | Short name of this network (from controller)      | no       |
| status                | string        | Network status (OK, ACCESS_DENIED, etc.)          | no       |
| type                  | string        | Network type (PUBLIC or PRIVATE)                  | no       |
| mtu                   | integer       | Ethernet MTU                                      | no       |
| dhcp                  | boolean       | If true, DHCP should be used to get IP info       | no       |
| bridge                | boolean       | If true, this device can bridge others            | no       |
| broadcastEnabled      | boolean       | If true ff:ff:ff:ff:ff:ff broadcasts work         | no       |
| portError             | integer       | Error code returned by underlying tap driver      | no       |
| netconfRevision       | integer       | Network configuration revision ID                 | no       |
| assignedAddresses     | [string]      | Array of ZeroTier-assigned IP addresses (/bits)   | no       |
| routes                | [object]      | Array of ZeroTier-assigned routes (see below)     | no       |
| portDeviceName        | string        | Name of virtual network device (if any)           | no       |
| allowManaged          | boolean       | Allow IP and route management                     | yes      |
| allowGlobal           | boolean       | Allow IPs and routes that overlap with global IPs | yes      |
| allowDefault          | boolean       | Allow overriding of system default route          | yes      |

Route objects:

| Field                 | Type          | Description                                       | Writable |
| --------------------- | ------------- | ------------------------------------------------- | -------- |
| target                | string        | Target network / netmask bits                     | no       |
| via                   | string        | Gateway IP address (next hop) or null for LAN     | no       |
| flags                 | integer       | Flags, currently always 0                         | no       |
| metric                | integer       | Route metric (not currently used)                 | no       |

#### /peer

 * Purpose: Get all peers
 * Methods: GET
 * Returns: [ {object}, ... ]

Getting /peer returns an array of peer objects for all current peers. See below for peer object format.

#### /peer/\<address\>

 * Purpose: Get or set information about a peer
 * Methods: GET, POST
 * Returns: { object }

| Field                 | Type          | Description                                       | Writable |
| --------------------- | ------------- | ------------------------------------------------- | -------- |
| address               | string        | 10-digit hex ZeroTier address of peer             | no       |
| versionMajor          | integer       | Major version of remote (if known)                | no       |
| versionMinor          | integer       | Minor version of remote (if known)                | no       |
| versionRev            | integer       | Software revision of remote (if known)            | no       |
| version               | string        | major.minor.revision                              | no       |
| latency               | integer       | Latency in milliseconds if known                  | no       |
| role                  | string        | LEAF, UPSTREAM, ROOT or PLANET                    | no       |
| paths                 | [object]      | Currently active physical paths (see below)       | no       |

Path objects:

| Field                 | Type          | Description                                       | Writable |
| --------------------- | ------------- | ------------------------------------------------- | -------- |
| address               | string        | Physical socket address e.g. IP/port              | no       |
| lastSend              | integer       | Time of last send through this path               | no       |
| lastReceive           | integer       | Time of last receive through this path            | no       |
| active                | boolean       | Is this path in use?                              | no       |
| expired               | boolean       | Is this path expired?                             | no       |
| preferred             | boolean       | Is this a current preferred path?                 | no       |
| trustedPathId         | integer       | If nonzero this is a trusted path (unencrypted)   | no       |
