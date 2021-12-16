use smol::net::SocketAddr;

/// Result code from the put() method in Database.
pub enum PutObjectResult {
    /// Datum stored successfully.
    Ok,
    /// Datum is one we already have.
    Duplicate,
    /// Value is invalid. (this may result in dropping connections to peers, etc.)
    Invalid,
}

/// Trait that must be implemented for the data store that is to be replicated.
///
/// Each datum is identified by an identity hash, which is a cryptographic hash of HASH_SIZE
/// bytes of its value. The implementation assumes that's what it is, but the hash function
/// is not specified in this library.
pub trait Store: Sync + Send {
    /// The size in bytes of the identity hash.
    const HASH_SIZE: usize;

    /// The maximum size of the objects supported by this store (and thus replication domain).
    const MAX_OBJECT_SIZE: usize;

    /// Object type returned by get(), must implement AsRef<[u8]>.
    type GetOutput: AsRef<[u8]>;

    /// Compute a hash of a data object using the hash associated with this store.
    /// This returns the identity hash which can then be used as a key with get(), put(), etc.
    fn hash(&self, object: &[u8]) -> [u8; Self::HASH_SIZE];

    /// Get the total size of this data set in objects.
    async fn total_size(&self) -> u64;

    /// Get an object from the database, returning None if it is not found or there is an error.
    async fn get(&self, identity_hash: &[u8; Self::HASH_SIZE]) -> Option<Self::GetOutput>;

    /// Store an entry in the database.
    async fn put(&self, identity_hash: &[u8; Self::HASH_SIZE], object: &[u8]) -> PutObjectResult;

    /// Count the number of identity hash keys in this range (inclusive) of identity hashes.
    /// This may return None if an error occurs, but should return 0 if the set is empty.
    async fn count(&self, start: &[u8; Self::HASH_SIZE], end: &[u8; Self::HASH_SIZE]) -> Option<u64>;

    /// Called when a connection to a remote node was successful.
    /// This is always called on successful outbound connect.
    async fn save_remote_endpoint(&self, to_address: &SocketAddr);

    /// Get a remote endpoint to try.
    /// This can return endpoints in any order and is used to try to establish outbound links.
    async fn get_remote_endpoint(&self) -> Option<SocketAddr>;

    /*
    /// Execute a function for every hash/value in a range.
    /// Iteration stops if the supplied function returns false.
    async fn for_each_entry<F, FF>(&self, start: &[u8; Self::HASH_SIZE], end: &[u8; Self::HASH_SIZE], function: F)
        where
            F: Fn(&[u8; Self::HASH_SIZE], &[u8]) -> FF,
            FF: Future<Output=bool>;

    /// Execute a function for every hash in a range.
    /// Iteration stops if the supplied function returns false.
    async fn for_each_hash_key<F, FF>(&self, start: &[u8; Self::HASH_SIZE], end: &[u8; Self::HASH_SIZE], function: F)
        where
            F: Fn(&[u8; Self::HASH_SIZE], &[u8]) -> FF,
            FF: Future<Output=bool>;
    */
}
