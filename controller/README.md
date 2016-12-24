Network Controller Microservice
======

ZeroTier's 16-digit network IDs are really just a concatenation of the 10-digit ZeroTier address of a network controller followed by a 6-digit (24-bit) network number on that controller. Fans of software defined networking will recognize this as a variation of the familiar [separation of data plane and control plane](http://sdntutorials.com/difference-between-control-plane-and-data-plane/) SDN design pattern.

This code implements the *node/NetworkController.hpp* interface and provides a SQLite3-backed network controller microservice. Including it in the build allows ZeroTier One to act as a controller and create/manage networks.

This is the same code we use to run [my.zerotier.com](https://my.zerotier.com/), which is a web UI and API that runs in front of a pool of controllers.

### Building

On Linux, Mac, or BSD you can create a controller-enabled build with:

    make ZT_ENABLE_NETWORK_CONTROLLER=1

You will need the development headers and libraries for SQLite3 installed.

### Running

After building and installing (`make install`) a controller-enabled build of ZeroTier One, start it and try:

    sudo zerotier-cli /controller

You should see something like:

    {
        "controller": true,
        "apiVersion": 2,
        "clock": 1468002975497,
        "instanceId": "8ab354604debe1da27ee627c9ef94a48"
    }

When started, a controller-enabled build of ZeroTier One will automatically create and initialize a `controller.db` file in its home folder. This is where all the controller's data and persistent state lives. If you're upgrading an old controller it will upgrade its database schema automatically on first launch. Make a backup of the old controller's database first since you can't go backward.

Controllers periodically make backups of their database as `controller.db.backup`. This is done so that this file can be more easily copied/rsync'ed to other systems without worrying about corruption. SQLite3 supports multiple processes accessing the same database file, so `sqlite3 /path/to/controller.db .dump` also works but can be slow on a busy controller.

Controllers can in theory host up to 2^24 networks and serve many millions of devices (or more), but we recommend running multiple controllers for a lot of networks to spread load and be more fault tolerant.

### Dockerizing Controllers

ZeroTier network controllers can easily be run in Docker or other container systems. Since containers do not need to actually join networks, extra privilege options like "--device=/dev/net/tun --privileged" are not needed. You'll just need to map the local JSON API port of the running controller and allow it to access the Internet (over UDP/9993 at a minimum) so things can reach and query it.

### Implementing High Availability Fail-Over

ZeroTier network controllers are not single points of failure for networks-- in the sense that if a controller goes down *existing* members of a network can continue to communicate. But new members (or those that have been offline for a while) can't join, existing members can't be de-authorized, and other changes to the network's configuration can't be made. This means that short "glitches" in controller availability are not a major problem but long periods of unavailability can be.

Because controllers are just regular ZeroTier nodes and controller queries are in-band, controllers can trivially be moved without worrying about changes to underlying physical IPs. This makes high-availability fail-over very easy to implement.

Just set up two cloud hosts, preferably in different data centers (e.g. two different AWS regions or Digital Ocean SF and NYC). Now set up the hot spare controller to constantly mirror `controller.db.backup` from its active sibling.

If the active controller goes down, rename `controller.db.backup` to `controller.db` on the hot spare and start the ZeroTier One service there. The spare will take over and has now become the active controller. If the original active node comes back, it should take on the role of spare and should not start its service. Instead it should start mirroring the active controller's backup and wait until it is needed.

The details of actually implementing this kind of HA fail-over on Linux or other OSes are beyond the scope of these docs and there are many ways to do it. Docker orchestration tools like Kubernetes can also be used to accomplish this if you've dockerized your controller.

### Network Controller API

The controller API is hosted via the same JSON API endpoint that ZeroTier One uses for local control (usually at 127.0.0.1 port 9993). All controller options are routed under the `/controller` base path.

The controller microservice does not implement any fine-grained access control (authentication is via authtoken.secret just like the regular JSON API) or other complex mangement features. It just takes network and network member configurations and reponds to controller queries. We have an enterprise product called [ZeroTier Central](https://my.zerotier.com/) that we host as a service (and that companies can license to self-host) that does this.

All working network IDs on a controller must begin with the controller's ZeroTier address. The API will *allow* "foreign" networks to be added but the controller will have no way of doing anything with them.

Also note that the API is *very* sensitive about types. Integers must be integers and strings strings, etc. Incorrectly typed and unrecognized fields are just ignored.

#### `/controller`

 * Purpose: Check for controller function and return controller status
 * Methods: GET
 * Returns: { object }

| Field              | Type        | Description                                       | Writable |
| ------------------ | ----------- | ------------------------------------------------- | -------- |
| controller         | boolean     | Always 'true'                                     | no       |
| apiVersion         | integer     | Controller API version, currently 2               | no       |
| clock              | integer     | Current clock on controller, ms since epoch       | no       |
| instanceId         | string      | A random ID generated on first controller DB init | no       |

The instance ID can be used to check whether a controller's database has been reset or otherwise switched.

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

| Field                 | Type          | Description                                       | Writable |
| --------------------- | ------------- | ------------------------------------------------- | -------- |
| nwid                  | string        | 16-digit network ID                               | no       |
| controllerInstanceId  | string        | Controller database instance ID                   | no       |
| clock                 | integer       | Current clock, ms since epoch                     | no       |
| name                  | string        | A short name for this network                     | YES      |
| private               | boolean       | Is access control enabled?                        | YES      |
| enableBroadcast       | boolean       | Ethernet ff:ff:ff:ff:ff:ff allowed?               | YES      |
| allowPassiveBridging  | boolean       | Allow any member to bridge (very experimental)    | YES      |
| v4AssignMode          | string        | If 'zt', auto-assign IPv4 from pool(s)            | YES      |
| v6AssignMode          | string        | IPv6 address auto-assign modes; see below         | YES      |
| multicastLimit        | integer       | Maximum recipients for a multicast packet         | YES      |
| creationTime          | integer       | Time network was first created                    | no       |
| revision              | integer       | Network config revision counter                   | no       |
| memberRevisionCounter | integer       | Network member revision counter                   | no       |
| authorizedMemberCount | integer       | Number of authorized members (for private nets)   | no       |
| relays                | array[object] | Alternative relays; see below                     | YES      |
| routes                | array[object] | Managed IPv4 and IPv6 routes; see below           | YES      |
| ipAssignmentPools     | array[object] | IP auto-assign ranges; see below                  | YES      |
| rules                 | array[object] | Traffic rules; see below                          | YES      |

(The `ipLocalRoutes` field appeared in older versions but is no longer present. Routes will now show up in `routes`.)

Two important things to know about networks:

 - Networks without rules won't carry any traffic. See below for an example with rules to permit IPv4 and IPv6.
 - Managed IP address assignments and IP assignment pools that do not fall within a route configured in `routes` are ignored and won't be used or sent to members.
 - The default for `private` is `true` and this is probably what you want. Turning `private` off means *anyone* can join your network with only its 16-digit network ID. It's also impossible to de-authorize a member as these networks don't issue or enforce certificates. Such "party line" networks are used for decentralized app backplanes, gaming, and testing but are not common in ordinary use.

**IPv6 Auto-Assign Modes:**

This field is (for legacy reasons) a comma-delimited list of strings. These can be `rfc4193`, `6plane`, and `zt`. RFC4193 and 6PLANE are special addressing modes that deterministically assign IPv6 addresses based on the network ID and the ZeroTier address of each member. The `zt` mode enables IPv6 auto-assignment from arbitrary IPv6 IP ranges configured in `ipAssignmentPools`.

**Relay object format:**

Relay objects define network-specific preferred relay nodes. Traffic to peers on this network will preferentially use these relays if they are available, and otherwise will fall back to the global rootserver infrastructure.

| Field                 | Type          | Description                                       | Writable |
| --------------------- | ------------- | ------------------------------------------------- | -------- |
| address               | string        | 10-digit ZeroTier address of relay                | YES      |
| phyAddress            | string        | Optional IP/port suggestion for finding relay     | YES      |

**IP assignment pool object format:**

| Field                 | Type          | Description                                       | Writable |
| --------------------- | ------------- | ------------------------------------------------- | -------- |
| ipRangeStart          | string        | Starting IP address in range                      | YES      |
| ipRangeEnd            | string        | Ending IP address in range (inclusive)            | YES      |

Pools are only used if auto-assignment is on for the given address type (IPv4 or IPv6) and if the entire range falls within a managed route.

IPv6 ranges work just like IPv4 ranges and look like this:

    {
        "ipRangeStart": "fd00:feed:feed:beef:0000:0000:0000:0000",
        "ipRangeEnd": "fd00:feed:feed:beef:ffff:ffff:ffff:ffff"
    }

(You can POST a shortened-form IPv6 address but the API will always report back un-shortened canonical form addresses.)

That defines a range within network `fd00:feed:feed:beef::/64` that contains up to 2^64 addresses. If an IPv6 range is large enough, the controller will assign addresses by placing each member's device ID into the address in a manner similar to the RFC4193 and 6PLANE modes. Otherwise it will assign addresses at random.

**Rule object format:**

Rules are matched in order of ruleNo. If no rules match, the default action is `drop`. To allow all traffic, create a single rule with all *null* fields and an action of `accept`.

In the future there will be many, many more types of rules. As of today only filtering by Ethernet packet type is supported.

| Field                 | Type          | Description                                       | Writable |
| --------------------- | ------------- | ------------------------------------------------- | -------- |
| ruleNo                | integer       | Rule sorting key                                  | YES      |
| etherType             | integer       | Ethernet frame type (e.g. 34525 for IPv6)         | YES      |
| action                | string        | Currently either `allow` or `drop`                | YES      |

**An Example: The Configuration for Earth**

Here is an example of a correctly configured ZeroTier network with IPv4 auto-assigned addresses from 28.0.0.0/7 (a "de-facto private" space) and RFC4193 IPv6 addressing. Users might recognize this as *Earth*, our public "global LAN party" that's used for demos and testing and occasionally gaming.

For your own networks you'll probably want to change `private` to `true` unless you like company. These rules on the other hand probably are what you want. These allow IPv4, IPv4 ARP, and IPv6 Ethernet frames. To allow only IPv4 omit the one for Ethernet type 34525 (IPv6).

    {
        "nwid": "8056c2e21c000001",
        "controllerInstanceId": "8ab354604debe1da27ee627c9ef94a48",
        "clock": 1468004857100,
        "name": "earth.zerotier.net",
        "private": false,
        "enableBroadcast": false,
        "allowPassiveBridging": false,
        "v4AssignMode": "zt",
        "v6AssignMode": "rfc4193",
        "multicastLimit": 64,
        "creationTime": 1442292573165,
        "revision": 234,
        "memberRevisionCounter": 3326,
        "authorizedMemberCount": 2873,
        "relays": [],
        "routes": [
            {"target":"28.0.0.0/7","via":null,"flags":0,"metric":0}],
        "ipAssignmentPools": [
            {"ipRangeStart":"28.0.0.1","ipRangeEnd":"29.255.255.254"}],
        "rules": [
        {
            "ruleNo": 20,
            "etherType": 2048,
            "action": "accept"
        },{
            "ruleNo": 21,
            "etherType": 2054,
            "action": "accept"
        },{
            "ruleNo": 30,
            "etherType": 34525,
            "action": "accept"
        }]
    }

#### `/controller/network/<network ID>/member`

 * Purpose: Get a set of all members on this network
 * Methods: GET
 * Returns: { object }

This returns a JSON object containing all member IDs as keys and their `memberRevisionCounter` values as values.

#### `/controller/network/<network ID>/active`

 * Purpose: Get a set of all active members on this network
 * Methods: GET
 * Returns: { object }

This returns an object containing all currently online members and the most recent `recentLog` entries for their last request.

#### `/controller/network/<network ID>/member/<address>`

 * Purpose: Create, authorize, or remove a network member
 * Methods: GET, POST, DELETE
 * Returns: { object }

| Field                 | Type          | Description                                       | Writable |
| --------------------- | ------------- | ------------------------------------------------- | -------- |
| nwid                  | string        | 16-digit network ID                               | no       |
| clock                 | integer       | Current clock, ms since epoch                     | no       |
| address               | string        | Member's 10-digit ZeroTier address                | no       |
| authorized            | boolean       | Is member authorized? (for private networks)      | YES      |
| activeBridge          | boolean       | Member is able to bridge to other Ethernet nets   | YES      |
| identity              | string        | Member's public ZeroTier identity (if known)      | no       |
| ipAssignments         | array[string] | Managed IP address assignments                    | YES      |
| memberRevision        | integer       | Member revision counter                           | no       |
| recentLog             | array[object] | Recent member activity log; see below             | no       |

Note that managed IP assignments are only used if they fall within a managed route. Otherwise they are ignored.

**Recent log object format:**

| Field                 | Type          | Description                                       |
| --------------------- | ------------- | ------------------------------------------------- |
| ts                    | integer       | Time of request, ms since epoch                   |
| authorized            | boolean       | Was member authorized?                            |
| clientMajorVersion    | integer       | Client major version or -1 if unknown             |
| clientMinorVersion    | integer       | Client minor version or -1 if unknown             |
| clientRevision        | integer       | Client revision or -1 if unknown                  |
| fromAddr              | string        | Physical address if known                         |

The controller can only know a member's `fromAddr` if it's able to establish a direct path to it. Members behind very restrictive firewalls may not have this information since the controller will be receiving the member's requests by way of a relay. ZeroTier does not back-trace IP paths as packets are relayed since this would add a lot of protocol overhead.
