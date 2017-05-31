VSDM: Very Simple Distributed Map
======

VSDM is a super-minimal replicated in-memory associative container. Its advantages are small code size, small footprint, simplicity, and lack of dependencies.

VSDM uses a rumor mill replication algorithm that provides fast best-effort replication. If connectivity is stable this results in eventual consistency, but data loss or regression can occur under split brain conditions. This class is not recommended for data that is intolerant of loss or regression. Its ideal use case is a distributed cache for small data objects or a distributed database for ephemeral data.

Transport is via TCP and can optionally be encrypted if one specifies a cryptor class (see below). The transport protocol does not implement any features for versioning or backward compatibility and changes to key/value type, cryptor, or other relevant parameters will render it incompatible. Again this is designed for simple app-embedded use cases. Use something more feature complete if you need to support different versions across the network.

Each node maintains a 64-bit monotonically increasing revision counter that starts at zero. When a node connects to another node it sends this revision counter and the other party will send all updates with revision numbers greater than or equal to it. When a node receives an update it replaces the entry it has if the revision counter is higher and also sets its own revision counter to the new counter if it is higher. It then re-transmits the update if a replace event occurred (if the new update was newer). This is the "rumor mill" part: each node retransmits all changes to all other connected nodes (excluding the source).

VSDM nodes can be connected according to any arbitrary connectivity graph. A more fully connected graph trades increased bandwidth consumption (due to redundant messages) for decreased likelihood of data loss or split brain conditions.

The VSDM class is thread safe. It launches a single background thread to handle network I/O and periodic cleanup. Deleted keys are purged from memory after a period of time to allow time for propagation and possible re-propagation.

## Template parameters

VSDM supports a number of template parameters for customization. The only required parameters are K and V, the key and value types. The default serializer allows these to be one of the *uintX_t* stdint numeric types or *std::string*. Specify a different serializer for anything else.

Here are the other parameters after K and V (in order):

 * **L**: Maximum message length, which also limits the max size of the combined key and value for a given entry. This imposes a sanity limit to prevent memory exhaustion. Default is 131072. Absolute max is UINT32_MAX - 4 (4294967291).
 * **W**: Watcher function type. Default is `vsdm_watcher_noop` which does nothing. The watcher function (or function object) is passed into the constructor and receives notifications of remotely initiated changes to the map's contents. (Local changes via set() or del() do not trigger the watcher). The watcher must have the following methods:
   * `void add(uint64_t remoteNodeId,const K &key,const V &value,uint64_t revision)`
   * `void update(uint64_t remoteNodeId,const K &key,const V &value,uint64_t revision)`
   * `void del(uint64_t remoteNodeId,const K &key)`
 * **S**: Serializer class, which must contain *static* methods for serializing and deserializing keys and values. The following must be present for both key and value types:
   * `unsigned long objectSize(const [K|V] &)`
   * `const char *objectData(const [K|V] &)`
   * `bool objectDeserialize(const char *,unsigned long,[K|V] &)` (false return means object was invalid and causes disconnect)
 * **C**: Cryptor type to encrypt transport, default is `vsdm_cryptor_noop` (no encryption). It is also passed into the constructor for internal initialization. This must implement a static method `static unsigned long overhead()` that returns the *constant* per-message overhead for things like IV and MAC, and two methods to encrypt and decrypt the payload in place. The vsdm class will add space for overhead at the *end* of the message. Encrypt/decrypt mathods have these signatures:
   * `void encrypt(void *,unsigned long)`
   * `bool decrypt(void *,unsigned long)` (false return means invalid MAC and causes disconnect)
 * **M**: Map type for underlying storage. Default is `std::unordered_map` with default STL hashers. Substitute a different container if you need to deal with non-hashable keys or need a sorted map. Containers supporting duplicate keys should not be used as the replication algorithm will not function properly.

## License

(c)2017 ZeroTier, Inc. (MIT license)

Written by [Adam Ierymenko](https://github.com/adamierymenko)
