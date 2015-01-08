# ZeroTier One Redis Database Schema

### Notes

- : is used as the key namespace separator as per de-facto Redis standard.
- A top-level record may have a :~ child containing a hash. This is the root hash and contains any simple key=value properties of the record.
- Booleans: any value other than "1" or "true" is false.
- Unless otherwise indicated *all integer values are in hexadecimal!*
- Timestamps are in milliseconds since the epoch
- IPv4 addresees: stored in standard dot notation e.g. 1.2.3.4
- IPv6 addresses: :'s are optional and addresses must be stored *without* shortening, e.g. with :0000: instead of ::. It must be possible to strip :'s from the address and get 128 bits of straight hex.
- Hexadecimal: all hex values must be lower case
- 16-digit network IDs and 10-digit addresses are left zero-padded to 16 and 10 digits respectively, as they are everywhere else in the ZT1 universe.

### Field attribute flags used in this documentation (not in database)

- ! required
- M mutable (from a user/UI point of view)
- R read-only (from a user/UI point of view)

# Networks

Network records are used by the network configuration master to issue configurations and certificates to virtual network members. These are the record types you should be interested in if you want to run your own netconf node.

### [Hash] zt1:network:\<nwid\>:~

- !R id :: must be \<nwid\>
- !M name :: network's globally unique short name, which can contain only characters valid in an e-mail address. It's the job of the code that populates this DB to ensure that this is globally unique.
- R owner :: id of user who owns this network (not used by netconf master, only for web UI and web API)
- R billingUser :: user paying for premium subscriptions (also unused by netconf-master)
- R billingUserConfirmed :: if true, billingUser has confirmed and authorized billing
- M desc :: a longer network description
- R infrastructure :: if true, network can't be deleted through API or web UI
- M private :: if true, network requires authentication
- R creationTime :: timestamp of network creation
- M etherTypes :: comma-delimited list of HEX integers indicating Ethernet types permitted on network
- M enableBroadcast :: if true, ff:ff:ff:ff:ff:ff is enabled network-wide
- M v4AssignMode :: 'none' (or null/empty/etc.), 'zt', 'dhcp'
- M v4AssignPool :: network/bits from which to assign IPs
- M v6AssignMode :: 'none' (or null/empty/etc.), 'zt', 'v6native', 'dhcp6'
- M v6AssignPool :: network/bits from which to assign IPs
- M allowPassiveBridging :: if true, allow passive bridging
- M multicastLimit :: maximum number of recipients to receive a multicast on this network
- M multicastRates :: string-encoded dictionary containing multicast groups and rates (see below)
- M encryptionMode :: encryption mode -- 0=always (default), 1=non-local only, 2=disable
- M subscriptions :: comma-delimited list of subscriptions for this network
- M ui :: arbitrary field that can be used by the UI to store stuff

Multicast rates are encoded as a dictionary. Each key is a multicast group in "MAC/ADI" format (e.g. *ff:ff:ff:ff:ff:ff/0*), and each value is a comma-delimited tuple of hex integer values: preload, max balance, and rate of accrual in bytes per second. An entry for *0* (or *0/0* or *00:00:00:00:00:00/0*) indicates the default setting for all unspecified multicast groups. Setting a rate limit like *ffffffff,ffffffff,ffffffff* as default will effectively turn off rate limits.

The encryption field allows encryption and full cryptographic authentication to be turned off in some or all cases. This improves performance for fast links such as LANs at the expense of security. Possible values are: (0) -- always encrypt (the default), (1) -- encrypt only for public IP destinations (e.g. not 10.x.x.x), (2) -- disable encryption. When encryption is disabled your traffic is vulnerable to both snooping and man-in-the-middle attacks, so most users will want to stick with the default. For same-data-center SDN-like uses cases we recommend sticking with (1). Only use (2) if you really know what you're doing.

### [Decimal Integer] zt1:network:\<nwid\>:revision

The revision number holds a decimal integer that can be incremented with the INCR Redis command. It should be changed whenever any network or network member setting changes that impacts the network configuration that is sent to users.

For private networks, the revision is used as part of the network membership certificate. *Certificates agree if their revision numbers differ by no more than one.* This has important implications. Generally speaking, you should INCR the revision *once* for most changes but *twice* when you de-authorize a member. This double increment may be performed with a time delay to allow the surviving members time to grab up to date network configurations before de-authorized members fall off the horizon.

### [Set] zt1:network:\<nwid\>:activeBridges

This is a set of ZeroTier addresses of designated active bridges on this network. It mirrors the state of the activeBridge field in the member record. Code should automatically remove entries from this set when activeBridge is false and add them when activeBridge is true.

### [Hash] zt1:network:\<nwid\>:ipAssignments

This is a hash mapping IP/netmask bits fields to 10-digit ZeroTier addresses of network members. IPv4 fields contain dots, e.g. "10.2.3.4/24" or "29.1.1.1/7". IPv6 fields contain colons. Note that IPv6 IP abbreviations must *not* be used; use \:0000\: instead of \:\: for zero parts of addresses. This is to simplify parser code and canonicalize for rapid search. All hex digits must be lower-case.

This is only used if the network's IPv4 and/or IPv6 auto-assign mode is 'zt' for ZeroTier assignment. The netconf-master will auto-populate by choosing unused IPs, and it can be edited via the API as well.

### [Hash] zt1:network:\<nwid\>:member:\<address\>:~

Each member of a network has a hash containing its configuration and authorization information.

- !R id :: must be \<address\>
- !R nwid :: must be \<nwid\>
- M authorized :: true if node is authorized and will be issued valid certificates and network configurations
- M activeBridge :: true if node is an active bridge (must mirror state of acticeBridges set)
- M name :: name of member (user-defined)
- M notes :: annotation field (user-defined)
- R authorizedBy :: user ID of user who authorized membership (unused by netconf master)
- R authorizedAt :: timestamp of authorization (unused by netconf master)
- R identity :: full identity of member (public key, etc.)
- R firstSeen :: time node was first seen
- R lastSeen :: time node was most recently seen
- R lastAt :: real Internet IP/port where node was most recently seen
- R ipAssignments :: comma-delimited list of IP address assignments (see below)
- R netconf :: most recent network configuration dictionary
- R netconfRevision :: network revision when netconf was generated (decimal integer)
- R netconfTimestamp :: timestamp from netconf dictionary
- R netconfClientTimestamp :: timestamp client most recently reported
- M ui :: string-serialized JSON blob for use by the user interface (unused by netconf-master)

The netconf field contains the most recent network configuration dictionary for this member. It is
updated whenever network configuration or member authorization is changed. It is sent to clients if
authorized is true and if netconf itself contains a valid string-serialized dictionary.

The ipAssignments field is re-generated whenever the zt1:network:\<nwid\>:ipAssignments hash is modified for this member. Both the API code and the netconf-master code must keep this in sync.

# Users (ZeroTier Networks only)

This record type is only of interest to ZeroTier Networks itself. It holds user records, billing information, subscriptions, etc. It's just documented here so all our Redis docs are in the same place.

Netconf masters do not use these records so you don't need to worry about this if you are trying to run your own.

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
