// (c) 2020-2022 ZeroTier, Inc. -- currently propritery pending actual release and licensing. See LICENSE.md.

use std::sync::atomic::{AtomicU64, Ordering};

use zerotier_core_crypto::aes::{Aes, AesGcm};
use zerotier_core_crypto::hash::{hmac_sha384, hmac_sha512, SHA384, SHA512};
use zerotier_core_crypto::p384::{P384KeyPair, P384PublicKey, P384_PUBLIC_KEY_SIZE};
use zerotier_core_crypto::random;
use zerotier_core_crypto::secret::Secret;

use parking_lot::{Mutex, RwLock};

/*

ZeroTier V2 Noise(-like?) Session Protocol

This protocol implements the Noise_IK key exchange pattern using NIST P-384 ECDH, AES-GCM,
and SHA512. So yes, Virginia, it's a FIPS-compliant Noise implementation. NIST P-384 is
not listed in official Noise documentation though, so consider it "Noise-like" if you
prefer.

See also: http://noiseprotocol.org/noise.html

Secondary hybrid exchange using Kyber512, the recently approved post-quantum KEX algorithm,
is also supported but is optional. When it is enabled the additional shared secret is
mixed into the final Noise_IK secret with HMAC/HKDF. This provides an exchange at least as
strong as the stronger of the two algorithms (ECDH and Kyber) since hashing anything with
a secret yields a secret.

Kyber theoretically provides data forward secrecy into the post-quantum era if and when it
arrives. It might also reassure those paranoid about NIST elliptic curves a little, though
we tend to accept the arguments of Koblitz and Menezes against the curves being backdoored.
These arguments are explained at the end of this post:

https://blog.cryptographyengineering.com/2015/10/22/a-riddle-wrapped-in-curve/

Kyber is used as long as both sides set the "jedi" parameter to true. It should be used
by default but can be disabled on tiny and slow devices or systems that talk to vast
numbers of endpoints and don't want the extra overhead.

Last and least, this includes an obfuscation step. AES in simple ECB mode is used to encrypt
the first block (or few blocks for key exchanges) of each packet using a hash of the
recipient's public static identity as a key. Packets are indistinguishable from random by
any observer who doesn't know the identity of the recipient, making bulk de-anonymization
or filtering via DPI more difficult. Since only someone knowing the identity of the recipient
can form a valid initial key exchange packet, it renders nodes invisible to naive scanners
as well.

*/

/// Minimum supported size for work buffers / minimum packet size.
pub const MIN_BUFFER_SIZE: usize = 1400;

/// Maximum possible value of a session ID
///
/// Session IDs are 48 bits, so this is also a bit mask that can just be ANDed to get
/// a valid session ID.
pub const SESSION_ID_MAX: u64 = 0xffffffffffff;

/// Start attempting to rekey after a key has been used to send packets this many times.
const REKEY_AFTER_USES: u64 = 1073741824;

/// Maximum random jitter to add to rekey-after usage count.
const REKEY_AFTER_USES_MAX_JITTER: u32 = 1048576;

/// Hard expiration after this many uses.
const EXPIRE_AFTER_USES: u64 = (u32::MAX - 1024) as u64;

/// Start attempting to rekey after a key has been in use for this many milliseconds.
const REKEY_AFTER_TIME_MS: i64 = 1000 * 60 * 60; // 1 hour

/// Maximum random jitter to add to rekey-after time.
const REKEY_AFTER_TIME_MS_MAX_JITTER: u32 = 1000 * 60 * 5;

const PACKET_TYPE_DATA: u8 = 0;
const PACKET_TYPE_NOP: u8 = 1;
const PACKET_TYPE_KEY_OFFER: u8 = 2; // "alice"
const PACKET_TYPE_KEY_COUNTER_OFFER: u8 = 3; // "bob"

/// Secondary (hybrid) ephemeral key disabled.
const E1_TYPE_NONE: u8 = 0;

/// Secondary (hybrid) ephemeral key is Kyber512
const E1_TYPE_KYBER512: u8 = 1;

/// Header size; header is: [4] counter | [6] destination session ID | [1] type
const HEADER_SIZE: usize = 11;

const AES_GCM_TAG_SIZE: usize = 16;
const HMAC_SIZE: usize = 48; // HMAC-SHA384
const SESSION_ID_SIZE: usize = 6;

/// Aribitrary starting value for key derivation chain
///
/// It doesn't matter very much what this is, but it's good for it to be unique.
const KEY_COMPUTATION_STARTING_SALT: [u8; 64] = [
    // echo -n 'Noise_IKpsk2_NISTP384+hybrid_AESGCM_SHA512' | shasum -a 512  | cut -d ' ' -f 1 | xxd -r -p | xxd -i
    0xc7, 0x66, 0xf3, 0x71, 0xc8, 0xbc, 0xc3, 0x19, 0xc6, 0xf0, 0x2a, 0x6e, 0x5c, 0x4b, 0x3c, 0xc0, 0x83, 0x29, 0x09, 0x09, 0x14, 0x4a, 0xf0, 0xde, 0xea, 0x3d, 0xbd, 0x00, 0x4c, 0x9e, 0x01, 0xa0, 0x6e, 0xb6, 0x9b, 0x56, 0x47, 0x97, 0x86, 0x1d, 0x4e, 0x94,
    0xc5, 0xdd, 0xde, 0x4a, 0x1c, 0xc3, 0x4e, 0xcc, 0x8b, 0x09, 0x3b, 0xb3, 0xc3, 0xb0, 0x03, 0xd7, 0xdf, 0x22, 0x49, 0x3f, 0xa5, 0x01,
];

const KBKDF_KEY_USAGE_LABEL_HMAC: u8 = b'h';
const KBKDF_KEY_USAGE_LABEL_AES_GCM_ALICE_TO_BOB: u8 = b'a';
const KBKDF_KEY_USAGE_LABEL_AES_GCM_BOB_TO_ALICE: u8 = b'b';

pub enum Error {
    /// The packet was addressed to an unrecognized local session
    UnknownLocalSessionId(u64),

    /// Packet was not well formed
    InvalidPacket,

    /// An invalid paramter was supplied to the function
    InvalidParameter,

    /// Packet failed one or more authentication checks
    FailedAuthentication,

    /// The supplied authenticator function rejected a new session
    NewSessionRejected,

    /// A packet was received that is out of sequence, like a counter offer to no offer.
    OutOfSequence,

    /// Rekeying failed and session secret has reached its maximum usage count
    MaxKeyLifetimeExceeded,
}

impl std::fmt::Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::UnknownLocalSessionId(_) => f.write_str("UnknownLocalSessionId"),
            Self::InvalidPacket => f.write_str("InvalidPacket"),
            Self::InvalidParameter => f.write_str("InvalidParameter"),
            Self::FailedAuthentication => f.write_str("FailedAuthentication"),
            Self::NewSessionRejected => f.write_str("NewSessionRejected"),
            Self::OutOfSequence => f.write_str("OutOfSequence"),
            Self::MaxKeyLifetimeExceeded => f.write_str("MaxKeyLifetimeExceeded"),
        }
    }
}

impl std::error::Error for Error {}

impl std::fmt::Debug for Error {
    #[inline(always)]
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        std::fmt::Display::fmt(self, f)
    }
}

/// Obfuscator/deobfuscator for privacy and indistinguishability masking of packets on the wire.
///
/// This is used to ECB encrypt the first block or for KEX packets the first few blocks using
/// the recipient's public static key as a key. That way a third party must know the identity
/// of the recipient to even see that this is ZeroTier traffic or trivial things like header
/// info, and bulk DPI becomes harder because you now have to do AES decrypts.
pub struct Obfuscator(Aes);

impl Obfuscator {
    /// Create a new obfuscator for sending packets TO the provided static public identity.
    pub fn new(recipient_static_public: &[u8]) -> Self {
        Self(Aes::new(&SHA512::hash(recipient_static_public)[..32]))
    }
}

#[allow(unused)]
pub enum ReceiveResult<'a, O> {
    /// Packet is valid and contained a data payload.
    OkData(&'a [u8]),

    /// Packet is valid and the provided reply should be sent back.
    OkSendReply(&'a [u8]),

    /// Packet is valid and a new session was created, also includes a reply to be sent back.
    OkNewSession(Session<O>, &'a [u8]),

    /// Packet is valid, no action needs to be taken.
    Ok,

    /// Packet appears valid but was ignored as a duplicate.
    Duplicate,

    /// Packet apperas valid but was ignored for another reason.
    Ignored,
}

pub struct Session<O> {
    pub id: u64,
    outgoing_packet_counter: Counter,
    remote_s_public_hash: [u8; 48],
    psk: Secret<64>,
    ss: Secret<48>,
    outgoing_obfuscator: Obfuscator,
    offer: Mutex<Option<Box<EphemeralOffer>>>,
    state: RwLock<State>,
    pub associated_object: O,
    remote_s_public_p384: [u8; P384_PUBLIC_KEY_SIZE],
}

struct State {
    remote_session_id: u64,
    keys: [Option<SessionKey>; 2], // current, next
}

/// Create a new session and return this plus an outgoing packet to send to the other end.
#[allow(unused)]
pub fn initiate<'a, O, const MAX_PACKET_SIZE: usize, const STATIC_PUBLIC_SIZE: usize>(
    buffer: &'a mut [u8; MAX_PACKET_SIZE],
    local_session_id: u64,
    local_s_public: &[u8; STATIC_PUBLIC_SIZE],
    local_s_keypair_p384: &P384KeyPair,
    remote_s_public: &[u8; STATIC_PUBLIC_SIZE],
    remote_s_public_p384: &P384PublicKey,
    psk: &Secret<64>,
    associated_object: O,
    jedi: bool,
) -> Result<(Session<O>, &'a [u8]), Error> {
    debug_assert!(MAX_PACKET_SIZE >= MIN_BUFFER_SIZE);
    assert!(local_session_id > 0 && local_session_id <= SESSION_ID_MAX);
    let counter = Counter::new();
    if let Some(ss) = local_s_keypair_p384.agree(remote_s_public_p384) {
        let outgoing_obfuscator = Obfuscator::new(remote_s_public);
        if let Some((offer, psize)) = EphemeralOffer::create_alice_offer(buffer, counter.next(), local_session_id, 0, local_s_public, remote_s_public_p384, &ss, &outgoing_obfuscator, jedi) {
            return Ok((
                Session::<O> {
                    id: local_session_id,
                    outgoing_packet_counter: counter,
                    remote_s_public_hash: SHA384::hash(remote_s_public),
                    psk: psk.clone(),
                    ss,
                    outgoing_obfuscator,
                    offer: Mutex::new(Some(offer)),
                    state: RwLock::new(State { remote_session_id: 0, keys: [None, None] }),
                    associated_object,
                    remote_s_public_p384: remote_s_public_p384.as_bytes().clone(),
                },
                &buffer[..psize],
            ));
        }
    }
    return Err(Error::InvalidParameter);
}

/// Receive a packet from the network and take the appropriate action.
///
/// Check ReceiveResult to see if it includes data or a reply packet.
#[allow(unused)]
pub fn receive<
    'a,
    ExtractP384PublicKeyFunction: FnOnce(&[u8; STATIC_PUBLIC_SIZE]) -> Option<P384PublicKey>,
    SessionLookupFunction: FnOnce(u64) -> Option<S>,
    NewSessionAuthenticatorFunction: FnOnce(&[u8; STATIC_PUBLIC_SIZE]) -> Option<(u64, Secret<64>, O)>,
    S: std::ops::Deref<Target = Session<O>>,
    O,
    const MAX_PACKET_SIZE: usize,
    const STATIC_PUBLIC_SIZE: usize,
>(
    incoming_packet: &[u8],
    buffer: &'a mut [u8; MAX_PACKET_SIZE],
    local_s_keypair_p384: &P384KeyPair,
    incoming_obfuscator: &Obfuscator,
    extract_p384_static_public: ExtractP384PublicKeyFunction,
    session_lookup: SessionLookupFunction,
    new_session_auth: NewSessionAuthenticatorFunction,
    current_time: i64,
    jedi: bool,
) -> Result<ReceiveResult<'a, O>, Error> {
    debug_assert!(MAX_PACKET_SIZE >= MIN_BUFFER_SIZE);

    if incoming_packet.len() > MAX_PACKET_SIZE || incoming_packet.len() <= 16 {
        unlikely_branch();
        return Err(Error::InvalidPacket);
    }

    incoming_obfuscator.0.decrypt_block(&incoming_packet[0..16], &mut buffer[0..16]);
    let local_session_id = u64::from_le_bytes(buffer[2..10].try_into().unwrap()).wrapping_shr(16);
    let packet_type = buffer[10];

    if packet_type <= PACKET_TYPE_NOP {
        if let Some(session) = session_lookup(local_session_id) {
            let state = session.state.read();
            for ki in 0..2 {
                if let Some(key) = state.keys[ki].as_ref() {
                    let mut c = key.get_receive_cipher();
                    c.init(&get_aes_gcm_nonce(buffer));
                    c.crypt_in_place(&mut buffer[HEADER_SIZE..16]);
                    let data_len = incoming_packet.len() - AES_GCM_TAG_SIZE;
                    c.crypt(&incoming_packet[16..data_len], &mut buffer[16..data_len]);
                    let tag = c.finish();
                    key.return_receive_cipher(c);

                    if tag.eq(&incoming_packet[data_len..]) {
                        // If this is the "next" key, a valid packet using it indicates that it should become the current key.
                        if ki == 1 {
                            unlikely_branch();
                            drop(state);
                            let mut state = session.state.write();
                            state.keys[0] = state.keys[1].take();
                        }

                        if packet_type == PACKET_TYPE_DATA {
                            return Ok(ReceiveResult::OkData(&buffer[HEADER_SIZE..data_len]));
                        } else {
                            unlikely_branch();
                            return Ok(ReceiveResult::Ok);
                        }
                    }
                }
            }
            return Err(Error::FailedAuthentication);
        } else {
            unlikely_branch();
            return Err(Error::UnknownLocalSessionId(local_session_id));
        }
    } else {
        unlikely_branch();

        let session = if local_session_id == 0 {
            None
        } else {
            let s = session_lookup(local_session_id);
            if s.is_none() {
                return Err(Error::UnknownLocalSessionId(local_session_id));
            }
            s
        };

        if incoming_packet.len() > (HEADER_SIZE + P384_PUBLIC_KEY_SIZE + AES_GCM_TAG_SIZE + HMAC_SIZE) {
            incoming_obfuscator.0.decrypt_block(&incoming_packet[16..32], &mut buffer[16..32]);
            incoming_obfuscator.0.decrypt_block(&incoming_packet[32..48], &mut buffer[32..48]);
            incoming_obfuscator.0.decrypt_block(&incoming_packet[48..64], &mut buffer[48..64]);
            buffer[64..incoming_packet.len()].copy_from_slice(&incoming_packet[64..]);
        } else {
            return Err(Error::InvalidPacket);
        }
        let payload_end = incoming_packet.len() - (AES_GCM_TAG_SIZE + HMAC_SIZE);

        match packet_type {
            PACKET_TYPE_KEY_OFFER => {
                // alice (remote) -> bob (local)

                let (alice_e0_public, e0s) = P384PublicKey::from_bytes(&buffer[HEADER_SIZE..HEADER_SIZE + P384_PUBLIC_KEY_SIZE]).and_then(|pk| local_s_keypair_p384.agree(&pk).map(move |s| (pk, s))).ok_or(Error::FailedAuthentication)?;

                let key = Secret(hmac_sha512(&hmac_sha512(&KEY_COMPUTATION_STARTING_SALT, alice_e0_public.as_bytes()), e0s.as_bytes()));

                let mut c = AesGcm::new(kbkdf512(key.as_bytes(), KBKDF_KEY_USAGE_LABEL_AES_GCM_ALICE_TO_BOB).first_n::<32>(), false);
                c.init(&get_aes_gcm_nonce(buffer));
                c.crypt_in_place(&mut buffer[(HEADER_SIZE + P384_PUBLIC_KEY_SIZE)..payload_end]);
                if !c.finish().eq(&buffer[payload_end..(payload_end + AES_GCM_TAG_SIZE)]) {
                    return Err(Error::FailedAuthentication);
                }

                let (alice_session_id, alice_s_public, alice_e1_public) = parse_KEY_OFFER_after_header(&buffer[(HEADER_SIZE + P384_PUBLIC_KEY_SIZE)..payload_end])?;

                if let Some(session) = session.as_ref() {
                    // If we already have a session for this session ID, make sure this is the same node calling.
                    if !session.remote_s_public_hash.eq(&SHA384::hash(&alice_s_public)) {
                        return Err(Error::FailedAuthentication);
                    }
                }

                let alice_s_public_p384 = extract_p384_static_public(&alice_s_public).ok_or(Error::InvalidPacket)?;
                let ss = local_s_keypair_p384.agree(&alice_s_public_p384).ok_or(Error::FailedAuthentication)?;

                let key = Secret(hmac_sha512(key.as_bytes(), ss.as_bytes()));

                if !hmac_sha384(kbkdf512(key.as_bytes(), KBKDF_KEY_USAGE_LABEL_HMAC).first_n::<48>(), &buffer[..(payload_end + AES_GCM_TAG_SIZE)]).eq(&buffer[(payload_end + AES_GCM_TAG_SIZE)..(payload_end + AES_GCM_TAG_SIZE + HMAC_SIZE)]) {
                    return Err(Error::FailedAuthentication);
                }

                // Alice's offer has been verified and her current key state reconstructed.

                let bob_e0_keypair = P384KeyPair::generate();
                let e0e0 = bob_e0_keypair.agree(&alice_e0_public).ok_or(Error::FailedAuthentication)?;
                let se0 = bob_e0_keypair.agree(&alice_s_public_p384).ok_or(Error::FailedAuthentication)?;

                let new_session = if let Some(session) = session.as_ref() {
                    None
                } else {
                    if let Some((local_session_id, psk, associated_object)) = new_session_auth(&alice_s_public) {
                        Some(Session::<O> {
                            id: local_session_id,
                            outgoing_packet_counter: Counter::new(),
                            remote_s_public_hash: SHA384::hash(&alice_s_public),
                            psk,
                            ss,
                            outgoing_obfuscator: Obfuscator::new(&alice_s_public),
                            offer: Mutex::new(None),
                            state: RwLock::new(State { remote_session_id: alice_session_id, keys: [None, None] }),
                            associated_object,
                            remote_s_public_p384: alice_s_public_p384.as_bytes().clone(),
                        })
                    } else {
                        return Err(Error::NewSessionRejected);
                    }
                };
                let session_ref = session;
                let session = session_ref.as_ref().map_or_else(|| new_session.as_ref().unwrap(), |s| &*s);

                // FIPS note: the order of HMAC parameters are flipped here from the usual Noise HMAC(key, X). That's because
                // NIST/FIPS allows HKDF with HMAC(salt, key) and salt is allowed to be anything. This way if the PSK is not
                // FIPS compliant the compliance of the entire key derivation is not invalidated. Both inputs are secrets of
                // fixed size so this shouldn't matter cryptographically.
                let key = Secret(hmac_sha512(session.psk.as_bytes(), &hmac_sha512(&hmac_sha512(&hmac_sha512(key.as_bytes(), bob_e0_keypair.public_key_bytes()), e0e0.as_bytes()), se0.as_bytes())));

                // At this point we've completed Noise_IK key derivation with NIST P-384 ECDH, but see final step below...

                let (bob_e1_public, e1e1) = if jedi && alice_e1_public.is_some() {
                    if let Ok((bob_e1_public, e1e1)) = pqc_kyber::encapsulate(alice_e1_public.as_ref().unwrap(), &mut random::SecureRandom::default()) {
                        (Some(bob_e1_public), Secret(e1e1))
                    } else {
                        return Err(Error::FailedAuthentication);
                    }
                } else {
                    (None, Secret::default()) // use all zero Kyber secret if disabled
                };

                let counter = session.outgoing_packet_counter.next();
                let mut reply_size = assemble_KEY_COUNTER_OFFER(buffer, counter, alice_session_id, bob_e0_keypair.public_key(), session.id, bob_e1_public.as_ref());

                let mut c = AesGcm::new(kbkdf512(key.as_bytes(), KBKDF_KEY_USAGE_LABEL_AES_GCM_BOB_TO_ALICE).first_n::<32>(), true);
                c.init(&get_aes_gcm_nonce(buffer));
                c.crypt_in_place(&mut buffer[(HEADER_SIZE + P384_PUBLIC_KEY_SIZE)..reply_size]);
                buffer[reply_size..(reply_size + AES_GCM_TAG_SIZE)].copy_from_slice(&c.finish());
                reply_size += AES_GCM_TAG_SIZE;

                // Normal Noise_IK is done, but we have one more step: mix in the Kyber shared secret (or all zeroes if Kyber is
                // disabled). We have to wait until this point because Kyber's keys are encrypted and can't be decrypted until
                // the P-384 exchange is done. We also flip the HMAC parameter order here for the same reason we do in the previous
                // key derivation step.
                let key = Secret(hmac_sha512(e1e1.as_bytes(), key.as_bytes()));

                let hmac = hmac_sha384(kbkdf512(key.as_bytes(), KBKDF_KEY_USAGE_LABEL_HMAC).first_n::<48>(), &buffer[..reply_size]);
                buffer[reply_size..reply_size + HMAC_SIZE].copy_from_slice(&hmac);
                reply_size += HMAC_SIZE;

                let mut state = session.state.write();
                state.remote_session_id = alice_session_id;
                state.keys[1].replace(SessionKey::new(key, Role::Bob, current_time, counter, jedi));
                drop(state);

                // Bob now has final key state for this exchange. Yay! Now reply to Alice so she can construct it.

                session.outgoing_obfuscator.0.encrypt_block_in_place(&mut buffer[0..16]);
                session.outgoing_obfuscator.0.encrypt_block_in_place(&mut buffer[16..32]);
                session.outgoing_obfuscator.0.encrypt_block_in_place(&mut buffer[32..48]);
                session.outgoing_obfuscator.0.encrypt_block_in_place(&mut buffer[48..64]);

                return new_session.map_or_else(|| Ok(ReceiveResult::OkSendReply(&buffer[..reply_size])), |ns| Ok(ReceiveResult::OkNewSession(ns, &buffer[..reply_size])));
            }

            PACKET_TYPE_KEY_COUNTER_OFFER => {
                // bob (remote) -> alice (local)

                if let Some(session) = session {
                    let mut session_offer = session.offer.lock();
                    if let Some(offer) = session_offer.as_ref() {
                        let (bob_e0_public, e0e0) = P384PublicKey::from_bytes(&buffer[HEADER_SIZE..(HEADER_SIZE + P384_PUBLIC_KEY_SIZE)]).and_then(|pk| offer.alice_e0_keypair.agree(&pk).map(move |s| (pk, s))).ok_or(Error::FailedAuthentication)?;
                        let se0 = local_s_keypair_p384.agree(&bob_e0_public).ok_or(Error::FailedAuthentication)?;

                        let key = Secret(hmac_sha512(session.psk.as_bytes(), &hmac_sha512(&hmac_sha512(&hmac_sha512(offer.key.as_bytes(), bob_e0_public.as_bytes()), e0e0.as_bytes()), se0.as_bytes())));

                        let mut c = AesGcm::new(kbkdf512(key.as_bytes(), KBKDF_KEY_USAGE_LABEL_AES_GCM_BOB_TO_ALICE).first_n::<32>(), false);
                        c.init(&get_aes_gcm_nonce(buffer));
                        c.crypt_in_place(&mut buffer[(HEADER_SIZE + P384_PUBLIC_KEY_SIZE)..payload_end]);
                        if !c.finish().eq(&buffer[payload_end..(payload_end + AES_GCM_TAG_SIZE)]) {
                            return Err(Error::FailedAuthentication);
                        }

                        // Alice has now completed Noise_IK for P-384, now for the hybrid part.

                        let (bob_session_id, bob_e1_public) = parse_KEY_COUNTER_OFFER_after_header(&buffer[(HEADER_SIZE + P384_PUBLIC_KEY_SIZE)..payload_end])?;

                        let e1e1 = if jedi && bob_e1_public.is_some() && offer.alice_e1_keypair.is_some() {
                            if let Ok(e1e1) = pqc_kyber::decapsulate(bob_e1_public.as_ref().unwrap(), &offer.alice_e1_keypair.as_ref().unwrap().secret) {
                                Secret(e1e1)
                            } else {
                                return Err(Error::FailedAuthentication);
                            }
                        } else {
                            Secret::default()
                        };

                        let key = Secret(hmac_sha512(e1e1.as_bytes(), key.as_bytes()));

                        if !hmac_sha384(kbkdf512(key.as_bytes(), KBKDF_KEY_USAGE_LABEL_HMAC).first_n::<48>(), &buffer[..(payload_end + AES_GCM_TAG_SIZE)]).eq(&buffer[(payload_end + AES_GCM_TAG_SIZE)..(payload_end + AES_GCM_TAG_SIZE + HMAC_SIZE)]) {
                            return Err(Error::FailedAuthentication);
                        }

                        // Alice has now completed and validated the full hybrid exchange. If this is the first exchange send
                        // a NOP back to Bob to acknowledge that the session is open and can now be used. Otherwise just queue
                        // this up as the next key to be promoted to current when Bob uses it.

                        let _ = session_offer.take();
                        drop(session_offer);

                        let mut state = session.state.write();
                        state.remote_session_id = bob_session_id;
                        if state.keys[0].is_some() {
                            let _ = state.keys[1].replace(SessionKey::new(key, Role::Alice, current_time, session.outgoing_packet_counter.current(), jedi));
                            return Ok(ReceiveResult::Ok);
                        } else {
                            let counter = session.outgoing_packet_counter.next();
                            let key = SessionKey::new(key, Role::Alice, current_time, counter, jedi);

                            let dummy_data_len = (random::next_u32_secure() % (MAX_PACKET_SIZE - (HEADER_SIZE + AES_GCM_TAG_SIZE)) as u32) as usize;
                            let mut dummy_data = [0_u8; MAX_PACKET_SIZE];
                            random::fill_bytes_secure(&mut dummy_data[..dummy_data_len]);
                            let nop_len = assemble_and_armor_DATA(buffer, &dummy_data[..dummy_data_len], PACKET_TYPE_NOP, bob_session_id, counter, &key, &session.outgoing_obfuscator)?;

                            let _ = state.keys[0].replace(key);
                            let _ = state.keys[1].take();

                            return Ok(ReceiveResult::OkSendReply(&buffer[..nop_len]));
                        }
                    } else {
                        return Err(Error::OutOfSequence);
                    }
                } else {
                    return Err(Error::OutOfSequence);
                }
            }

            _ => return Err(Error::InvalidPacket),
        }
    }
}

struct Counter(AtomicU64);

impl Counter {
    fn new() -> Self {
        Self(AtomicU64::new(0))
    }

    #[inline(always)]
    fn current(&self) -> CounterValue {
        CounterValue(self.0.load(Ordering::SeqCst))
    }

    #[inline(always)]
    fn next(&self) -> CounterValue {
        CounterValue(self.0.fetch_add(1, Ordering::SeqCst))
    }
}

/// A value of the outgoing packet counter.
///
/// The counter is internally 64-bit so we can more easily track usage limits without
/// confusing modular difference stuff. The counter as seen externally and placed in
/// packets is the least significant 32 bits.
#[repr(transparent)]
#[derive(Copy, Clone)]
struct CounterValue(u64);

impl CounterValue {
    #[inline(always)]
    pub fn to_bytes(&self) -> [u8; 4] {
        (self.0 as u32).to_le_bytes()
    }
}

struct KeyLifetime {
    rekey_at_or_after_counter: u64,
    hard_expire_at_counter: u64,
    rekey_at_or_after_timestamp: i64,
}

impl KeyLifetime {
    fn new(current_counter: CounterValue, current_time: i64) -> Self {
        Self {
            rekey_at_or_after_counter: current_counter.0 + REKEY_AFTER_USES + (random::next_u32_secure() % REKEY_AFTER_USES_MAX_JITTER) as u64,
            hard_expire_at_counter: current_counter.0 + EXPIRE_AFTER_USES,
            rekey_at_or_after_timestamp: current_time + REKEY_AFTER_TIME_MS + (random::next_u32_secure() % REKEY_AFTER_TIME_MS_MAX_JITTER) as i64,
        }
    }

    #[inline(always)]
    fn should_rekey(&self, counter: CounterValue, current_time: i64) -> bool {
        counter.0 >= self.rekey_at_or_after_counter || current_time >= self.rekey_at_or_after_timestamp
    }

    #[inline(always)]
    fn expired(&self, counter: CounterValue) -> bool {
        counter.0 < self.hard_expire_at_counter
    }
}

/// Ephemeral offer sent with KEY_OFFER and rememebered so state can be reconstructed on COUNTER_OFFER.
#[allow(unused)]
struct EphemeralOffer {
    key: Secret<64>, // key "under construction"
    alice_e0_keypair: P384KeyPair,
    alice_e1_keypair: Option<pqc_kyber::Keypair>,
}

impl EphemeralOffer {
    fn create_alice_offer<const MAX_PACKET_SIZE: usize, const STATIC_PUBLIC_SIZE: usize>(
        buffer: &mut [u8; MAX_PACKET_SIZE],
        counter: CounterValue,
        alice_session_id: u64,
        bob_session_id: u64,
        alice_s_public: &[u8; STATIC_PUBLIC_SIZE],
        bob_s_public_p384: &P384PublicKey,
        ss: &Secret<48>,
        outgoing_obfuscator: &Obfuscator, // bobfuscator?
        jedi: bool,
    ) -> Option<(Box<EphemeralOffer>, usize)> {
        debug_assert!(MAX_PACKET_SIZE >= MIN_BUFFER_SIZE);

        let alice_e0_keypair = P384KeyPair::generate();
        let e0s = alice_e0_keypair.agree(bob_s_public_p384)?;
        let alice_e1_keypair = if jedi {
            Some(pqc_kyber::keypair(&mut random::SecureRandom::get()))
        } else {
            None
        };

        let key = Secret(hmac_sha512(&hmac_sha512(&KEY_COMPUTATION_STARTING_SALT, alice_e0_keypair.public_key_bytes()), e0s.as_bytes()));

        let mut packet_size = assemble_KEY_OFFER(buffer, counter, bob_session_id, alice_e0_keypair.public_key(), alice_session_id, alice_s_public, alice_e1_keypair.as_ref().map(|s| &s.public));

        let mut c = AesGcm::new(kbkdf512(key.as_bytes(), KBKDF_KEY_USAGE_LABEL_AES_GCM_ALICE_TO_BOB).first_n::<32>(), true);
        c.init(&get_aes_gcm_nonce(buffer));
        c.crypt_in_place(&mut buffer[HEADER_SIZE + P384_PUBLIC_KEY_SIZE..packet_size]);
        buffer[packet_size..packet_size + AES_GCM_TAG_SIZE].copy_from_slice(&c.finish());
        packet_size += AES_GCM_TAG_SIZE;

        let key = Secret(hmac_sha512(key.as_bytes(), ss.as_bytes()));

        let hmac = hmac_sha384(kbkdf512(key.as_bytes(), KBKDF_KEY_USAGE_LABEL_HMAC).first_n::<48>(), &buffer[..packet_size]);
        buffer[packet_size..packet_size + HMAC_SIZE].copy_from_slice(&hmac);
        packet_size += HMAC_SIZE;

        outgoing_obfuscator.0.encrypt_block_in_place(&mut buffer[0..16]);
        outgoing_obfuscator.0.encrypt_block_in_place(&mut buffer[16..32]);
        outgoing_obfuscator.0.encrypt_block_in_place(&mut buffer[32..48]);
        outgoing_obfuscator.0.encrypt_block_in_place(&mut buffer[48..64]);

        Some((Box::new(EphemeralOffer { key, alice_e0_keypair, alice_e1_keypair }), packet_size + AES_GCM_TAG_SIZE + HMAC_SIZE))
    }
}

enum Role {
    Alice,
    Bob,
}

#[allow(unused)]
struct SessionKey {
    lifetime: KeyLifetime,
    receive_key: Secret<32>,
    send_key: Secret<32>,
    receive_cipher_pool: Mutex<Vec<Box<AesGcm>>>,
    send_cipher_pool: Mutex<Vec<Box<AesGcm>>>,
    role: Role,
    jedi: bool, // true if kyber was used
}

impl SessionKey {
    /// Create a new symmetric shared session key and set its key expiration times, etc.
    fn new(key: Secret<64>, role: Role, current_time: i64, current_counter: CounterValue, jedi: bool) -> Self {
        let a2b: Secret<32> = kbkdf512(key.as_bytes(), KBKDF_KEY_USAGE_LABEL_AES_GCM_ALICE_TO_BOB).first_n_clone();
        let b2a: Secret<32> = kbkdf512(key.as_bytes(), KBKDF_KEY_USAGE_LABEL_AES_GCM_BOB_TO_ALICE).first_n_clone();
        let (receive_key, send_key) = match role {
            Role::Alice => (b2a, a2b),
            Role::Bob => (a2b, b2a),
        };
        Self {
            lifetime: KeyLifetime::new(current_counter, current_time),
            receive_key,
            send_key,
            receive_cipher_pool: Mutex::new(Vec::with_capacity(2)),
            send_cipher_pool: Mutex::new(Vec::with_capacity(2)),
            role,
            jedi,
        }
    }

    #[inline(always)]
    fn get_send_cipher(&self, counter: CounterValue) -> Result<Box<AesGcm>, Error> {
        if !self.lifetime.expired(counter) {
            Ok(self.send_cipher_pool.lock().pop().unwrap_or_else(|| Box::new(AesGcm::new(self.send_key.as_bytes(), true))))
        } else {
            unlikely_branch();
            Err(Error::MaxKeyLifetimeExceeded)
        }
    }

    #[inline(always)]
    fn return_send_cipher(&self, c: Box<AesGcm>) {
        self.send_cipher_pool.lock().push(c);
    }

    #[inline(always)]
    fn get_receive_cipher(&self) -> Box<AesGcm> {
        self.receive_cipher_pool.lock().pop().unwrap_or_else(|| Box::new(AesGcm::new(self.receive_key.as_bytes(), false)))
    }

    #[inline(always)]
    fn return_receive_cipher(&self, c: Box<AesGcm>) {
        self.receive_cipher_pool.lock().push(c);
    }
}

#[inline(always)]
#[allow(non_snake_case)]
fn assemble_and_armor_DATA<const MAX_PACKET_SIZE: usize>(buffer: &mut [u8; MAX_PACKET_SIZE], data: &[u8], packet_type: u8, remote_session_id: u64, counter: CounterValue, key: &SessionKey, outgoing_obfuscator: &Obfuscator) -> Result<usize, Error> {
    buffer[0..4].copy_from_slice(&counter.to_bytes());
    buffer[4..10].copy_from_slice(&remote_session_id.to_le_bytes()[..SESSION_ID_SIZE]);
    debug_assert!(packet_type == PACKET_TYPE_DATA || packet_type == PACKET_TYPE_NOP);
    buffer[10] = packet_type;

    let payload_end = HEADER_SIZE + data.len();
    let tag_end = payload_end + AES_GCM_TAG_SIZE;

    let mut c = key.get_send_cipher(counter)?;
    buffer[11..16].fill(0);
    c.init(&buffer[..16]);
    c.crypt(data, &mut buffer[HEADER_SIZE..payload_end]);
    buffer[payload_end..tag_end].copy_from_slice(&c.finish());
    key.return_send_cipher(c);

    outgoing_obfuscator.0.encrypt_block_in_place(&mut buffer[..16]);

    Ok(tag_end)
}

#[allow(non_snake_case)]
fn assemble_KEY_OFFER<const MAX_PACKET_SIZE: usize, const STATIC_PUBLIC_SIZE: usize>(
    buffer: &mut [u8; MAX_PACKET_SIZE],
    counter: CounterValue,
    bob_session_id: u64,
    alice_e0_public: &P384PublicKey,
    alice_session_id: u64,
    alice_s_public: &[u8; STATIC_PUBLIC_SIZE],
    alice_e1_public: Option<&[u8; pqc_kyber::KYBER_PUBLICKEYBYTES]>,
) -> usize {
    buffer[0..4].copy_from_slice(&counter.to_bytes());
    buffer[4..10].copy_from_slice(&bob_session_id.to_le_bytes()[..SESSION_ID_SIZE]);
    buffer[10] = PACKET_TYPE_KEY_OFFER;
    let mut b = &mut buffer[HEADER_SIZE..];

    b[..P384_PUBLIC_KEY_SIZE].copy_from_slice(alice_e0_public.as_bytes());
    b = &mut b[P384_PUBLIC_KEY_SIZE..];

    b[..SESSION_ID_SIZE].copy_from_slice(&alice_session_id.to_le_bytes()[..SESSION_ID_SIZE]);
    b = &mut b[SESSION_ID_SIZE..];

    b[..STATIC_PUBLIC_SIZE].copy_from_slice(alice_s_public);
    b = &mut b[STATIC_PUBLIC_SIZE..];

    if let Some(k) = alice_e1_public {
        b[0] = E1_TYPE_KYBER512;
        b[1..1 + pqc_kyber::KYBER_PUBLICKEYBYTES].copy_from_slice(k);
        b = &mut b[1 + pqc_kyber::KYBER_PUBLICKEYBYTES..];
    } else {
        b[0] = E1_TYPE_NONE;
        b = &mut b[1..];
    }

    b[0] = 0;
    b[1] = 0; // reserved for future use
    b = &mut b[2..];

    let random_padding_len = (random::next_u32_secure() as usize) % (MAX_PACKET_SIZE - (b.len() + AES_GCM_TAG_SIZE + HMAC_SIZE));
    random::fill_bytes_secure(&mut b[..random_padding_len]);
    b = &mut b[random_padding_len..];

    b.len()
}

#[allow(non_snake_case)]
fn parse_KEY_OFFER_after_header<const STATIC_PUBLIC_SIZE: usize>(mut b: &[u8]) -> Result<(u64, [u8; STATIC_PUBLIC_SIZE], Option<[u8; pqc_kyber::KYBER_PUBLICKEYBYTES]>), Error> {
    if b.len() >= SESSION_ID_SIZE {
        let alice_session_id = u48_from_le_bytes(b);
        b = &b[SESSION_ID_SIZE..];
        if b.len() >= STATIC_PUBLIC_SIZE {
            let alice_s_public: [u8; STATIC_PUBLIC_SIZE] = b[..STATIC_PUBLIC_SIZE].try_into().unwrap();
            b = &b[STATIC_PUBLIC_SIZE..];
            if b.len() >= 1 {
                let e1_type = b[0];
                b = &b[1..];
                let alice_e1_public = if e1_type == E1_TYPE_KYBER512 {
                    if b.len() >= pqc_kyber::KYBER_PUBLICKEYBYTES {
                        let k: [u8; pqc_kyber::KYBER_PUBLICKEYBYTES] = b[..pqc_kyber::KYBER_PUBLICKEYBYTES].try_into().unwrap();
                        b = &b[pqc_kyber::KYBER_PUBLICKEYBYTES..];
                        Some(k)
                    } else {
                        return Err(Error::InvalidPacket);
                    }
                } else {
                    None
                };
                if b.len() >= 2 {
                    return Ok((alice_session_id, alice_s_public, alice_e1_public));
                }
            }
        }
    }
    return Err(Error::InvalidPacket);
}

#[allow(non_snake_case)]
fn assemble_KEY_COUNTER_OFFER<const MAX_PACKET_SIZE: usize>(
    buffer: &mut [u8; MAX_PACKET_SIZE],
    counter: CounterValue,
    alice_session_id: u64,
    bob_e0_public: &P384PublicKey,
    bob_session_id: u64,
    bob_e1_public: Option<&[u8; pqc_kyber::KYBER_CIPHERTEXTBYTES]>,
) -> usize {
    buffer[0..4].copy_from_slice(&counter.to_bytes());
    buffer[4..10].copy_from_slice(&alice_session_id.to_le_bytes()[..SESSION_ID_SIZE]);
    buffer[10] = PACKET_TYPE_KEY_COUNTER_OFFER;
    let mut b = &mut buffer[HEADER_SIZE..];

    b[..P384_PUBLIC_KEY_SIZE].copy_from_slice(bob_e0_public.as_bytes());
    b = &mut b[P384_PUBLIC_KEY_SIZE..];

    b[..SESSION_ID_SIZE].copy_from_slice(&bob_session_id.to_le_bytes()[..SESSION_ID_SIZE]);
    b = &mut b[SESSION_ID_SIZE..];

    if let Some(k) = bob_e1_public {
        b[0] = E1_TYPE_KYBER512;
        b[1..1 + pqc_kyber::KYBER_CIPHERTEXTBYTES].copy_from_slice(k);
        b = &mut b[1 + pqc_kyber::KYBER_CIPHERTEXTBYTES..];
    } else {
        b[0] = E1_TYPE_NONE;
        b = &mut b[1..];
    }

    b[0] = 0;
    b[1] = 0; // reserved for future use
    b = &mut b[2..];

    let random_padding_len = (random::next_u32_secure() as usize) % (MAX_PACKET_SIZE - (b.len() + AES_GCM_TAG_SIZE + HMAC_SIZE));
    random::fill_bytes_secure(&mut b[..random_padding_len]);
    b = &mut b[random_padding_len..];

    b.len()
}

#[allow(non_snake_case)]
fn parse_KEY_COUNTER_OFFER_after_header(mut b: &[u8]) -> Result<(u64, Option<[u8; pqc_kyber::KYBER_CIPHERTEXTBYTES]>), Error> {
    if b.len() >= SESSION_ID_SIZE {
        let bob_session_id = u48_from_le_bytes(b);
        b = &b[SESSION_ID_SIZE..];
        if b.len() >= 1 {
            let e1_type = b[0];
            b = &b[1..];
            let bob_e1_public = if e1_type == E1_TYPE_KYBER512 {
                if b.len() >= pqc_kyber::KYBER_CIPHERTEXTBYTES {
                    let k: [u8; pqc_kyber::KYBER_CIPHERTEXTBYTES] = b[..pqc_kyber::KYBER_CIPHERTEXTBYTES].try_into().unwrap();
                    b = &b[pqc_kyber::KYBER_CIPHERTEXTBYTES..];
                    Some(k)
                } else {
                    return Err(Error::InvalidPacket);
                }
            } else {
                None
            };
            if b.len() >= 1 && b[0] == 0 {
                return Ok((bob_session_id, bob_e1_public));
            }
        }
    }
    return Err(Error::InvalidPacket);
}

fn kbkdf512(key: &[u8], label: u8) -> Secret<64> {
    Secret(hmac_sha512(key, &[0, 0, 0, 0, b'Z', b'T', label, 0, 0, 0, 0, 0x02, 0x00]))
}

#[inline(always)]
fn get_aes_gcm_nonce(deobfuscated_packet: &[u8]) -> [u8; 16] {
    let mut tmp = 0_u128.to_ne_bytes();
    tmp[..HEADER_SIZE].copy_from_slice(deobfuscated_packet);
    tmp
}

#[inline(always)]
fn u48_from_le_bytes(b: &[u8]) -> u64 {
    (b[0] as u64) | (b[1] as u64).wrapping_shl(8) | (b[2] as u64).wrapping_shl(16) | (b[3] as u64).wrapping_shl(24) | (b[4] as u64).wrapping_shl(32) | (b[5] as u64).wrapping_shl(40)
}

#[cold]
#[inline(never)]
extern "C" fn unlikely_branch() {}
