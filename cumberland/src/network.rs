use std::hash::Hash;

/// An interface to a physical network such as TCP/IP or ZeroTier.
pub trait Network {
    /// An endpoint address on the network to which messages can be sent.
    type Address: Hash + Clone;

    /// The maximum message size that can be handled by this Network.
    /// Note that the underlying transport must be able to handle sizes of at least 4096.
    const MAX_MESSAGE_SIZE: usize;

    /// Attempt to send a message to an address.
    ///
    /// The semantics required are similar to UDP in that delivery need not be guaranteed.
    /// A return value of false indicates an obvious error such as invalid address.
    fn send(&self, to: &Address, data: &[u8]) -> bool;

    /// Receive the next incoming message.
    ///
    /// This should block until the next message is available. A return of None indicates
    /// that the instance is shutting down.
    fn receive(&self) -> Option<(Address, Vec<u8>)>;
}
