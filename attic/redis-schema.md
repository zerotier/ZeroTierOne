# ZeroTier One Redis Database Schema

This is the Redis database schema used for ZeroTier One network configuration masters.

### Notes

- A top-level record may have a :~ child containing a hash. This is the root hash and contains any simple key=value properties of the record.
- Booleans: true is 1, all other values are false (unless otherwise indicated)
- With the exception of network IDs and ZeroTier addresses and unless otherwise indicated, all integers are in ASCII decimal
- 16-digit network IDs and 10-digit ZeroTier addresses must be exactly 16 and 10 digits by being left-zero padded as they are elsewhere.
- Timestamps are in milliseconds since the epoch
- IPv4 addresees are stored in standard dot notation e.g. 1.2.3.4
- IPv6 addresses must be stored *without* shortening, e.g. with :0000: instead of ::. It must be possible to strip :'s from the address and get 128 bits of straight hex.
- All hexadecimal numbers should be lower case

### Field attribute flags used in this documentation (not in database)

- **!** required
- **M** mutable via user-facing API
- **R** read-only via user-facing API
- **+** used by network configuration master and possibly API (for optional fields)
- **-** not used by network configuration master, API or UI only (for optional fields)
- **~** cache field used by network configuration master (for optional fields)

### Schema Versioning

The *zt1:schema* value contains an integer database schema version. If it is not present it is assumed to be equal to zero. Implementations should check this and auto-upgrade and/or refuse to use an old database version. This value should only be changed on significant, incompatible changes.

Current database version is **2**.

# Networks

Network records are used by the network configuration master to issue configurations and certificates to virtual network members. These are the record types you should be interested in if you want to run your own netconf node.

### [Hash] zt1:network:\<nwid\>:~

- !R id :: must be \<nwid\>
- !M name :: network's globally unique short name, which can contain only characters valid in an e-mail address. It's the job of the code that populates this DB to ensure that this is globally unique.
- +M private :: network requires authentication -- unlike other booleans this defaults to true unless value is exactly '0'
- +M etherTypes :: comma-delimited list of HEX integers indicating Ethernet types permitted on network
- +M enableBroadcast :: if true, ff:ff:ff:ff:ff:ff is enabled network-wide
- +M v4AssignMode :: 'none' (or null/empty/etc.), 'zt', 'dhcp'
- +M v4AssignPool :: network/bits from which to assign IPs
- +M v6AssignMode :: 'none' (or null/empty/etc.), 'zt', 'v6native', 'dhcp6'
- +M v6AssignPool :: network/bits from which to assign IPs
- +M allowPassiveBridging :: if true, allow passive bridging
- +M multicastLimit :: maximum number of recipients to receive a multicast on this network
- +M multicastRates :: dictionary containing multicast rate limit settings
- +M desc :: a longer network description
- -R creationTime :: timestamp of network creation
- -R owner :: id of user who owns this network
- -R billingUser :: user paying for premium subscriptions
- -R billingUserConfirmed :: if true, billingUser has confirmed and authorized billing
- -R infrastructure :: if true, network can't be deleted through API or web UI
- -M subscriptions :: comma-delimited list of billing subscriptions for this network
- -M ui :: arbitrary field that can be used by the UI to store stuff

Multicast rates are encoded as a dictionary. Each key is a multicast group in "MAC/ADI" format (e.g. *ff:ff:ff:ff:ff:ff/0*), and each value is a comma-delimited tuple of hex integer values: preload, max balance, and rate of accrual in bytes per second. An entry for *0* (or *0/0* or *00:00:00:00:00:00/0*) indicates the default setting for all unspecified multicast groups. Setting a rate limit like *ffffffff,ffffffff,ffffffff* as default will effectively turn off rate limits.

### [Decimal Integer] zt1:network:\<nwid\>:revision

The revision number holds a decimal integer that can be incremented with the INCR Redis command. It should be changed whenever any network or network member setting changes that impacts the network configuration that is sent to users.

For private networks, the revision is used as part of the network membership certificate. *Certificates agree if their revision numbers differ by no more than one.* This has important implications. Generally speaking, you should INCR the revision *once* for most changes but *twice* when you de-authorize a member. This double increment may be performed with a time delay to allow the surviving members time to grab up to date network configurations before de-authorized members fall off the horizon.

### [Hash] zt1:network:\<nwid\>:ipAssignments

This is a hash mapping IP/netmask bits fields to 10-digit ZeroTier addresses of network members. IPv4 fields contain dots, e.g. "10.2.3.4/24" or "29.1.1.1/7". IPv6 fields contain colons. Note that IPv6 IP abbreviations must *not* be used; use \:0000\: instead of \:\: for zero parts of addresses. This is to simplify parser code and canonicalize for rapid search. All hex digits must be lower-case.

This is only used if the network's IPv4 and/or IPv6 auto-assign mode is 'zt' for ZeroTier assignment. The netconf-master will auto-populate by choosing unused IPs, and it can be edited via the API as well.

### [Set] zt1:network:\<nwid\>:members

This set contains all members of this network.

### [Hash] zt1:network:\<nwid\>:member:\<address\>:~

Each member of a network has a hash containing its configuration and authorization information.

- !R id :: must be \<address\>
- !R nwid :: must be \<nwid\>
- +M authorized :: true if node is authorized and will be issued valid certificates and network configurations
- +R identity :: full identity of member (public key, etc.)
- +R firstSeen :: time node was first seen
- +R lastSeen :: time node was most recently seen
- +R lastAt :: real Internet IP/port where node was most recently seen
- +R ipAssignments :: comma-delimited list of IP address assignments (see below)
- ~R netconf :: most recent network configuration dictionary (caching)
- ~R netconfRevision :: network revision when netconf was generated
- ~R netconfTimestamp :: timestamp from netconf dictionary
- ~R netconfClientTimestamp :: timestamp client most recently reported
- -M name :: name of member (user-defined)
- -M notes :: annotation field (user-defined)
- -R authorizedBy :: user ID of user who authorized membership
- -R authorizedAt :: timestamp of authorization
- -M ui :: arbitrary field that can be used by the UI to store stuff

The netconf field contains the most recent network configuration dictionary for this member. It is updated whenever network configuration or member authorization is changed. It is sent to clients if authorized is true and if netconf itself contains a valid string-serialized dictionary.

The ipAssignments field is re-generated whenever the zt1:network:\<nwid\>:ipAssignments hash is modified for this member. Both the API code and the netconf-master code must keep this in sync.

### [Set] zt1:network:\<nwid\>:activeBridges

This set contains all members of this network designated as active bridges.

# Users

This record type holds user records, billing information, subscriptions, etc. It's just documented here so all our Redis docs are in the same place. Users outside of ZeroTier, Inc. itself do not need any of this as it's not used by the netconf master.

### [Hash] zt1:user:\<auth\>:\<authUserId\>:~

- !R id :: must be auth:authUserId -- this is the full key for referencing a user
- !R auth :: authentication type e.g. 'google' or 'local'
- !R authUserId :: user ID under auth schema, like an e-mail address or a Google profile ID.
- M email :: user's email address
- R confirmed :: is e-mail confirmed?
- R lastLogin :: timestamp of last login
- R creationTime: :: timestamp of account creation
- M displayName :: usually First Last, defaults to e-mail address for 'local' auth and whatever the OpenID API says for third party auth such as Google.
- M defaultCard :: ID of default credit card (actual card objects are stored by Stripe, not in this database)
- M ui :: arbitrary field that can be used by the UI to store stuff
- R stripeCustomerId :: customer ID for Stripe credit card service if the user has cards on file (we don't store cards, we let Stripe do that)
