Network Controller Microservice
======

Every ZeroTier virtual network has a *network controller* responsible for admitting members to the network, issuing certificates, and issuing default configuration information.

This is our reference controller implementation and is the same one we use to power our own hosted services at [my.zerotier.com](https://my.zerotier.com/). As of ZeroTier One version 1.2.0 this code is included in normal builds for desktop, laptop, and server (Linux, etc.) targets.

Controller data is stored in JSON format under `controller.d` in the ZeroTier working directory. It can be copied, rsync'd, placed in `git`, etc. The files under `controller.d` should not be modified in place while the controller is running or data loss may result, and if they are edited directly take care not to save corrupt JSON since that can also lead to data loss when the controller is restarted. Going through the API is strongly preferred to directly modifying these files.

See the API section below for information about controlling the controller.

### Scalability and Reliability

Controllers can in theory host up to 2^24 networks and serve many millions of devices (or more), but we recommend spreading large numbers of networks across many controllers for load balancing and fault tolerance reasons. Since the controller uses the filesystem as its data store we recommend fast filesystems and fast SSD drives for heavily loaded controllers.

Since ZeroTier nodes are mobile and do not need static IPs, implementing high availability fail-over for controllers is easy. Just replicate their working directories from master to backup and have something automatically fire up the backup if the master goes down. Modern orchestration tools like Nomad and Kubernetes can be of help here.

### Dockerizing Controllers

ZeroTier network controllers can easily be run in Docker or other container systems. Since containers do not need to actually join networks, extra privilege options like "--device=/dev/net/tun --privileged" are not needed. You'll just need to map the local JSON API port of the running controller and allow it to access the Internet (over UDP/9993 at a minimum) so things can reach and query it.

### PostgreSQL Database Implementation

The default controller stores its data in the filesystem in `controller.d` under ZeroTier's home folder. There's an alternative implementation that stores data in PostgreSQL that can be built with `make central-controller`. Right now this is only guaranteed to build and run on Centos 7 Linux with PostgreSQL 10 installed via the [PostgreSQL Yum Repository](https://www.postgresql.org/download/linux/redhat/) and is designed for use with [ZeroTier Central](https://my.zerotier.com/). You're welcome to use it but we don't "officially" support it for end-user use and it could change at any time.

### Upgrading from Older (1.1.14 or earlier) Versions

Older versions of this code used a SQLite database instead of in-filesystem JSON. A migration utility called `migrate-sqlite` is included here and *must* be used to migrate this data to the new format. If the controller is started with an old `controller.db` in its working directory it will terminate after printing an error to *stderr*. This is done to prevent "surprises" for those running DIY controllers using the old code.

The migration tool is written in nodeJS and can be used like this:

    cd migrate-sqlite
    npm install
    node migrate.js </path/to/controller.db> </path/to/controller.d>

### Network Controller API

The controller API is hosted via the same JSON API endpoint that ZeroTier One uses for local control (usually at 127.0.0.1 port 9993). All controller options are routed under the `/controller` base path.

The controller microservice itself does not implement any fine-grained access control. Access control is via the ZeroTier control interface itself and `authtoken.secret`. This can be sent as the `X-ZT1-Auth` HTTP header field or appended to the URL as `?auth=<token>`. Take care when doing the latter that request URLs are not being logged.

While networks with any valid ID can be added to the controller's database, it will only actually work to control networks whose first 10 hex digits correspond with the network controller's ZeroTier ID. See [section 2.2.1 of the ZeroTier manual](https://zerotier.com/manual.shtml#2_2_1).

The controller JSON API is *very* sensitive about types. Integers must be integers and strings strings, etc. Incorrect types may be ignored, set to default values, or set to undefined values.

#### `/controller`

 * Purpose: Check for controller function and return controller status
 * Methods: GET
 * Returns: { object }

| Field              | Type        | Description                                       | Writable |
| ------------------ | ----------- | ------------------------------------------------- | -------- |
| controller         | boolean     | Always 'true'                                     | no       |
| apiVersion         | integer     | Controller API version, currently 3               | no       |
| clock              | integer     | Current clock on controller, ms since epoch       | no       |

#### `/controller/network`

 * Purpose: List all networks hosted by this controller
 * Methods: GET
 * Returns: [ string, ... ]

This returns an array of 16-digit hexadecimal network IDs.

#### `/controller/network/<network ID>`

 * Purpose: Create, configure, and delete hosted networks
 * Methods: GET, POST, DELETE
 * Returns: { object }

By making queries to this path you can create, configure, and delete networks. DELETE is final, so don't do it unless you really mean it.

When POSTing new networks take care that their IDs are not in use, otherwise you may overwrite an existing one. To create a new network with a random unused ID, POST to `/controller/network/##########______`. The #'s are the controller's 10-digit ZeroTier address and they're followed by six underscores. Check the `nwid` field of the returned JSON object for your network's newly allocated ID. Subsequent POSTs to this network must refer to its actual path.

Example:

`curl -X POST --header "X-ZT1-Auth: secret" -d '{"name":"my network"}' http://localhost:9993/controller/network/305f406058______`

**Network object format:**

| Field                 | Type          | Description                                       | Writable |
| --------------------- | ------------- | ------------------------------------------------- | -------- |
| id                    | string        | 16-digit network ID                               | no       |
| nwid                  | string        | 16-digit network ID (legacy)                      | no       |
| objtype               | string        | Always "network"                                  | no       |
| name                  | string        | A short name for this network                     | YES      |
| creationTime          | integer       | Time network record was created (ms since epoch)  | no       |
| private               | boolean       | Is access control enabled?                        | YES      |
| enableBroadcast       | boolean       | Ethernet ff:ff:ff:ff:ff:ff allowed?               | YES      |
| v4AssignMode          | object        | IPv4 management and assign options (see below)    | YES      |
| v6AssignMode          | object        | IPv6 management and assign options (see below)    | YES      |
| mtu                   | integer       | Network MTU (default: 2800)                       | YES      |
| multicastLimit        | integer       | Maximum recipients for a multicast packet         | YES      |
| revision              | integer       | Network config revision counter                   | no       |
| routes                | array[object] | Managed IPv4 and IPv6 routes; see below           | YES      |
| ipAssignmentPools     | array[object] | IP auto-assign ranges; see below                  | YES      |
| rules                 | array[object] | Traffic rules; see below                          | YES      |
| capabilities          | array[object] | Array of capability objects (see below)           | YES      |
| tags                  | array[object] | Array of tag objects (see below)                  | YES      |
| remoteTraceTarget     | string        | 10-digit ZeroTier ID of remote trace target       | YES      |
| remoteTraceLevel      | integer       | Remote trace verbosity level                      | YES      |

 * Networks without rules won't carry any traffic. If you don't specify any on network creation an "accept anything" rule set will automatically be added.
 * Managed IP address assignments and IP assignment pools that do not fall within a route configured in `routes` are ignored and won't be used or sent to members.
 * The default for `private` is `true` and this is probably what you want. Turning `private` off means *anyone* can join your network with only its 16-digit network ID. It's also impossible to de-authorize a member as these networks don't issue or enforce certificates. Such "party line" networks are used for decentralized app backplanes, gaming, and testing but are otherwise not common.
 * Changing the MTU can be disruptive and on some operating systems may require a leave/rejoin of the network or a restart of the ZeroTier service.

**Auto-Assign Modes:**

Auto assign modes (`v4AssignMode` and `v6AssignMode`) contain objects that map assignment modes to booleans.

For IPv4 the only valid setting is `zt` which, if true, causes IPv4 addresses to be auto-assigned from `ipAssignmentPools` to members that do not have an IPv4 assignment. Note that active bridges are exempt and will not get auto-assigned IPs since this can interfere with bridging. (You can still manually assign one if you want.)

IPv6 includes this option and two others: `6plane` and `rfc4193`. These assign private IPv6 addresses to each member based on a deterministic assignment scheme that allows members to emulate IPv6 NDP to skip multicast for better performance and scalability. The `rfc4193` mode gives every member a /128 on a /88 network, while `6plane` gives every member a /80 within a /40 network but uses NDP emulation to route *all* IPs under that /80 to its owner. The `6plane` mode is great for use cases like Docker since it allows every member to assign IPv6 addresses within its /80 that just work instantly and globally across the network.

**IP assignment pool object format:**

| Field                 | Type          | Description                                       |
| --------------------- | ------------- | ------------------------------------------------- |
| ipRangeStart          | string        | Starting IP address in range                      |
| ipRangeEnd            | string        | Ending IP address in range (inclusive)            |

Pools are only used if auto-assignment is on for the given address type (IPv4 or IPv6) and if the entire range falls within a managed route.

IPv6 ranges work just like IPv4 ranges and look like this:

    {
        "ipRangeStart": "fd00:feed:feed:beef:0000:0000:0000:0000",
        "ipRangeEnd": "fd00:feed:feed:beef:ffff:ffff:ffff:ffff"
    }

(You can POST a shortened-form IPv6 address but the API will always report back un-shortened canonical form addresses.)

That defines a range within network `fd00:feed:feed:beef::/64` that contains up to 2^64 addresses. If an IPv6 range is large enough, the controller will assign addresses by placing each member's device ID into the address in a manner similar to the RFC4193 and 6PLANE modes. Otherwise it will assign addresses at random.

**Managed Route object format:**

| Field                 | Type          | Description                                       |
| --------------------- | ------------- | ------------------------------------------------- |
| target                | string        | Subnet in CIDR notation                           |
| via                   | string/null   | Next hop router IP address                        |

Managed Route objects look like this:

      {
        "target": "10.147.20.0/24"
      }

or 

      {
        "target": "192.168.168.0/24",
        "via": "10.147.20.1"
      }

**Rule object format:**

Each rule is actually a sequence of zero or more `MATCH_` entries in the rule array followed by an `ACTION_` entry that describes what to do if all the preceding entries match. An `ACTION_` without any preceding `MATCH_` entries is always taken, so setting a single `ACTION_ACCEPT` rule yields a network that allows all traffic. If no rules are present the default action is `ACTION_DROP`.

Rules are evaluated in the order in which they appear in the array. There is currently a limit of 256 entries per network. Capabilities should be used if a larger and more complex rule set is needed since they allow rules to be grouped by purpose and only shipped to members that need them.

Each rule table entry has two common fields.

| Field                 | Type          | Description                                       |
| --------------------- | ------------- | ------------------------------------------------- |
| type                  | string        | Entry type (all caps, case sensitive)             |
| not                   | boolean       | If true, MATCHes match if they don't match        |

The following fields may or may not be present depending on rule type:

| Field                 | Type          | Description                                       |
| --------------------- | ------------- | ------------------------------------------------- |
| zt                    | string        | 10-digit hex ZeroTier address                     |
| etherType             | integer       | Ethernet frame type                               |
| mac                   | string        | Hex MAC address (with or without :'s)             |
| ip                    | string        | IPv4 or IPv6 address                              |
| ipTos                 | integer       | IP type of service                                |
| ipProtocol            | integer       | IP protocol (e.g. TCP)                            |
| start                 | integer       | Start of an integer range (e.g. port range)       |
| end                   | integer       | End of an integer range (inclusive)               |
| id                    | integer       | Tag ID                                            |
| value                 | integer       | Tag value or comparison value                     |
| mask                  | integer       | Bit mask (for characteristics flags)              |

The entry types and their additional fields are:

| Entry type                      | Description                                                       | Fields         |
| ------------------------------- | ----------------------------------------------------------------- | -------------- |
| `ACTION_DROP`                   | Drop any packets matching this rule                               | (none)         |
| `ACTION_ACCEPT`                 | Accept any packets matching this rule                             | (none)         |
| `ACTION_TEE`                    | Send a copy of this packet to a node (rule parsing continues)     | `zt`           |
| `ACTION_REDIRECT`               | Redirect this packet to another node                              | `zt`           |
| `ACTION_DEBUG_LOG`              | Output debug info on match (if built with rules engine debug)     | (none)         |
| `MATCH_SOURCE_ZEROTIER_ADDRESS` | Match VL1 ZeroTier address of packet sender.                      | `zt`           |
| `MATCH_DEST_ZEROTIER_ADDRESS`   | Match VL1 ZeroTier address of recipient                           | `zt`           |
| `MATCH_ETHERTYPE`               | Match Ethernet frame type                                         | `etherType`    |
| `MATCH_MAC_SOURCE`              | Match source Ethernet MAC address                                 | `mac`          |
| `MATCH_MAC_DEST`                | Match destination Ethernet MAC address                            | `mac`          |
| `MATCH_IPV4_SOURCE`             | Match source IPv4 address                                         | `ip`           |
| `MATCH_IPV4_DEST`               | Match destination IPv4 address                                    | `ip`           |
| `MATCH_IPV6_SOURCE`             | Match source IPv6 address                                         | `ip`           |
| `MATCH_IPV6_DEST`               | Match destination IPv6 address                                    | `ip`           |
| `MATCH_IP_TOS`                  | Match IP TOS field                                                | `ipTos`        |
| `MATCH_IP_PROTOCOL`             | Match IP protocol field                                           | `ipProtocol`   |
| `MATCH_IP_SOURCE_PORT_RANGE`    | Match a source IP port range                                      | `start`,`end`  |
| `MATCH_IP_DEST_PORT_RANGE`      | Match a destination IP port range                                 | `start`,`end`  |
| `MATCH_CHARACTERISTICS`         | Match on characteristics flags                                    | `mask`,`value` |
| `MATCH_FRAME_SIZE_RANGE`        | Match a range of Ethernet frame sizes                             | `start`,`end`  |
| `MATCH_TAGS_SAMENESS`           | Match if both sides' tags differ by no more than value            | `id`,`value`   |
| `MATCH_TAGS_BITWISE_AND`        | Match if both sides' tags AND to value                            | `id`,`value`   |
| `MATCH_TAGS_BITWISE_OR`         | Match if both sides' tags OR to value                             | `id`,`value`   |
| `MATCH_TAGS_BITWISE_XOR`        | Match if both sides' tags XOR to value                            | `id`,`value`   |

Important notes about rules engine behavior:

 * IPv4 and IPv6 IP address rules do not match for frames that are not IPv4 or IPv6 respectively.
 * `ACTION_DEBUG_LOG` is a no-op on nodes not built with `ZT_RULES_ENGINE_DEBUGGING` enabled (see Network.cpp). If that is enabled nodes will dump a trace of rule evaluation results to *stdout* when this action is encountered but will otherwise keep evaluating rules. This is used for basic "smoke testing" of the rules engine.
 * Multicast packets and packets destined for bridged devices treated a little differently. They are matched more than once. They are matched at the point of send with a NULL ZeroTier destination address, meaning that `MATCH_DEST_ZEROTIER_ADDRESS` is useless. That's because the true VL1 destination is not yet known. Then they are matched again for each true VL1 destination. On these later subsequent matches TEE actions are ignored and REDIRECT rules are interpreted as DROPs. This prevents multiple TEE or REDIRECT packets from being sent to third party devices.
 * Rules in capabilities are always matched as if the current device is the sender (inbound == false). A capability specifies sender side rules that can be enforced on both sides.

#### `/controller/network/<network ID>/member`

 * Purpose: Get a set of all members on this network
 * Methods: GET
 * Returns: { object }

This returns a JSON object containing all member IDs as keys and their `memberRevisionCounter` values as values.

#### `/controller/network/<network ID>/member/<address>`

 * Purpose: Create, authorize, or remove a network member
 * Methods: GET, POST, DELETE
 * Returns: { object }

| Field                 | Type          | Description                                       | Writable |
| --------------------- | ------------- | ------------------------------------------------- | -------- |
| id                    | string        | Member's 10-digit ZeroTier address                | no       |
| address               | string        | Member's 10-digit ZeroTier address                | no       |
| nwid                  | string        | 16-digit network ID                               | no       |
| authorized            | boolean       | Is member authorized? (for private networks)      | YES      |
| activeBridge          | boolean       | Member is able to bridge to other Ethernet nets   | YES      |
| identity              | string        | Member's public ZeroTier identity (if known)      | no       |
| ipAssignments         | array[string] | Managed IP address assignments                    | YES      |
| revision              | integer       | Member revision counter                           | no       |
| vMajor                | integer       | Most recently known major version                 | no       |
| vMinor                | integer       | Most recently known minor version                 | no       |
| vRev                  | integer       | Most recently known revision                      | no       |
| vProto                | integer       | Most recently known protocl version               | no       |

Note that managed IP assignments are only used if they fall within a managed route. Otherwise they are ignored.

