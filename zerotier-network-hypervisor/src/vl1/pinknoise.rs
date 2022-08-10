/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2022 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::sync::atomic::{AtomicU64, Ordering};

use zerotier_core_crypto::aes::{Aes, AesGcm};
use zerotier_core_crypto::hash::{hmac_sha384, hmac_sha512, SHA512};
use zerotier_core_crypto::p384::{P384KeyPair, P384PublicKey, P384_PUBLIC_KEY_SIZE};
use zerotier_core_crypto::random;
use zerotier_core_crypto::secret::Secret;

use parking_lot::{Mutex, RwLock};

/// Minimum suggested buffer size for work and output buffer supplied to functions.
/// Supplying work buffers smaller than this will likely result in panics.
pub const MIN_BUFFER_SIZE: usize = 1400;

/// Start attempting to rekey after a key has been used to send packets this many times.
pub const REKEY_AFTER_USES: u64 = 1073741824;

/// Maximum random jitter to add to rekey-after usage count.
pub const REKEY_AFTER_USES_MAX_JITTER: u32 = 1048576;

/// Hard expiration after this many uses.
pub const EXPIRE_AFTER_USES: u64 = (u32::MAX - 1024) as u64;

/// Start attempting to rekey after a key has been in use for this many milliseconds.
pub const REKEY_AFTER_TIME_MS: i64 = 1000 * 60 * 60;

/// Maximum random jitter to add to rekey-after time.
pub const REKEY_AFTER_TIME_MS_MAX_JITTER: u32 = 1000 * 60 * 5;

const PACKET_TYPE_DATA: u8 = 0;
const PACKET_TYPE_NOP: u8 = 1;
const PACKET_TYPE_KEY_OFFER: u8 = 2;
const PACKET_TYPE_KEY_COUNTER_OFFER: u8 = 3;

const E1_TYPE_NONE: u8 = 0;
const E1_TYPE_KYBER512_90S: u8 = 1;

// [4] counter | [6] destination session ID | [1] type
const HEADER_SIZE: usize = 11;

const AES_GCM_TAG_SIZE: usize = 16;
const HMAC_SIZE: usize = 48; // HMAC-SHA384
const SESSION_ID_SIZE: usize = 6;

// on macOS: echo -n 'pinkNoise_IKpsk2_hybrid_NISTP384_AESGCM_SHA512' | shasum -a 512  | cut -d ' ' -f 1 | xxd -r -p | xxd -i
const KEY_COMPUTATION_STARTING_SALT: [u8; 64] = [
    0xa8, 0x4c, 0x50, 0x1e, 0x41, 0x84, 0x5a, 0x6e, 0x73, 0x0b, 0x39, 0xad, 0x99, 0xaa, 0x10, 0x0e, 0x79, 0x42, 0x7c, 0x52, 0xc7, 0x10, 0x91, 0xb3, 0x87, 0x96, 0xe4, 0x98, 0x76,
    0x11, 0x15, 0x42, 0xd2, 0xfc, 0x3d, 0xe6, 0x19, 0xbf, 0x36, 0xab, 0x22, 0xf1, 0x62, 0xb6, 0x92, 0x3b, 0x80, 0x26, 0x0d, 0xcb, 0x16, 0xfc, 0x25, 0x4a, 0xad, 0x9a, 0x32, 0x4f,
    0x37, 0xf8, 0x63, 0xeb, 0x10, 0x94,
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
            Self::MaxKeyLifetimeExceeded => f.write_str("MaxKeyLifetimeExceeded"),
        }
    }
}

impl std::error::Error for Error {}

impl std::fmt::Debug for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        std::fmt::Display::fmt(self, f)
    }
}

/// Obfuscator/deobfuscator for privacy and indistinguishability masking of packets on the wire.
pub struct Obfuscator(Aes);

impl Obfuscator {
    /// Create a new obfuscator for sending packets TO the provided static public identity.
    pub fn new(recipient_static_public: &[u8]) -> Self {
        Self(Aes::new(&SHA512::hash(recipient_static_public)[..16]))
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
    pub local_session_id: u64,
    remote_session_id: AtomicU64,
    outgoing_packet_counter: Counter,
    psk: Secret<64>,
    ss: Secret<48>,
    remote_s_public_p384: [u8; P384_PUBLIC_KEY_SIZE],
    outgoing_obfuscator: Obfuscator,
    offer: Mutex<Option<Box<EphemeralOffer>>>,
    keys: RwLock<[Option<SessionKey>; 2]>, // current, next
    pub associated_object: O,
}

impl<O> Session<O> {
    /// Create a new session and return this plus an outgoing packet to send to the other end.
    #[allow(unused)]
    pub fn new<'a, const MAX_PACKET_SIZE: usize, const STATIC_PUBLIC_SIZE: usize>(
        buffer: &'a mut [u8; MAX_PACKET_SIZE],
        local_session_id: u64,
        local_s_public: &[u8; STATIC_PUBLIC_SIZE],
        local_s_keypair_p384: &P384KeyPair,
        remote_s_public: &[u8; STATIC_PUBLIC_SIZE],
        remote_s_public_p384: &P384PublicKey,
        psk: &Secret<64>,
        associated_object: O,
        jedi: bool,
    ) -> Result<(Self, &'a [u8]), Error> {
        debug_assert!(MAX_PACKET_SIZE >= MIN_BUFFER_SIZE);
        if local_session_id > 0 && local_session_id <= 0xffffffffffff {
            let counter = Counter::new();
            if let Some(ss) = local_s_keypair_p384.agree(remote_s_public_p384) {
                let outgoing_obfuscator = Obfuscator::new(remote_s_public);
                if let Some((offer, psize)) =
                    EphemeralOffer::create_alice_offer(buffer, counter.next(), local_session_id, 0, local_s_public, remote_s_public_p384, &ss, &outgoing_obfuscator, jedi)
                {
                    return Ok((
                        Self {
                            local_session_id,
                            remote_session_id: AtomicU64::new(0),
                            outgoing_packet_counter: counter,
                            psk: psk.clone(),
                            ss,
                            remote_s_public_p384: remote_s_public_p384.as_bytes().clone(),
                            outgoing_obfuscator,
                            offer: Mutex::new(Some(offer)),
                            keys: RwLock::new([None, None]),
                            associated_object,
                        },
                        &buffer[..psize],
                    ));
                }
            }
        }
        return Err(Error::InvalidParameter);
    }
}

/// Receive a packet from the network and take the appropriate action.
///
/// Check ReceiveResult to see if it includes data or a reply packet.
#[allow(unused)]
pub fn receive<
    'a,
    SessionLookupFunction: FnOnce(u64) -> Option<S>,
    NewSessionAuthenticatorFunction: FnOnce(&[u8; STATIC_PUBLIC_SIZE]) -> Option<(u64, [u8; P384_PUBLIC_KEY_SIZE], Secret<64>, O)>,
    S: std::ops::Deref<Target = Session<O>>,
    O,
    const MAX_PACKET_SIZE: usize,
    const STATIC_PUBLIC_SIZE: usize,
>(
    incoming_packet: &[u8],
    buffer: &'a mut [u8; MAX_PACKET_SIZE],
    local_s_keypair_p384: &P384KeyPair,
    incoming_obfuscator: &Obfuscator,
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
            let keys = session.keys.read();
            for ki in 0..2 {
                if let Some(key) = keys[ki].as_ref() {
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
                            drop(keys);
                            let mut keys = session.keys.write();
                            keys[0] = keys[1].take();
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

        if incoming_packet.len() >= (HEADER_SIZE + P384_PUBLIC_KEY_SIZE + AES_GCM_TAG_SIZE + HMAC_SIZE) {
            incoming_obfuscator.0.decrypt_block(&incoming_packet[16..32], &mut buffer[16..32]);
            incoming_obfuscator.0.decrypt_block(&incoming_packet[32..48], &mut buffer[32..48]);
            incoming_obfuscator.0.decrypt_block(&incoming_packet[48..64], &mut buffer[48..64]);
            buffer[64..incoming_packet.len()].copy_from_slice(&incoming_packet[64..]);
        } else {
            return Err(Error::InvalidPacket);
        }

        match packet_type {
            PACKET_TYPE_KEY_OFFER => {
                // alice (remote) -> bob (local)

                let (alice_e0_public, e0s) = P384PublicKey::from_bytes(&buffer[HEADER_SIZE..HEADER_SIZE + P384_PUBLIC_KEY_SIZE])
                    .and_then(|pk| local_s_keypair_p384.agree(&pk).map(move |s| (pk, s)))
                    .ok_or(Error::FailedAuthentication)?;

                let key = Secret(hmac_sha512(&hmac_sha512(&KEY_COMPUTATION_STARTING_SALT, alice_e0_public.as_bytes()), e0s.as_bytes()));

                let mut c = AesGcm::new(kbkdf512(key.as_bytes(), KBKDF_KEY_USAGE_LABEL_AES_GCM_BOB_TO_ALICE).first_n::<32>(), false);
                c.init(&get_aes_gcm_nonce(buffer));
                c.crypt_in_place(&mut buffer[(HEADER_SIZE + P384_PUBLIC_KEY_SIZE)..incoming_packet.len() - (AES_GCM_TAG_SIZE + HMAC_SIZE)]);
                if !c.finish().eq(&buffer[incoming_packet.len() - (AES_GCM_TAG_SIZE + HMAC_SIZE)..incoming_packet.len() - HMAC_SIZE]) {
                    return Err(Error::FailedAuthentication);
                }
                drop(c);

                let (alice_session_id, alice_s_public, alice_e1_public) = parse_KEY_OFFER_after_header(buffer)?;

                let new_session = if let Some(session) = session.as_ref() {
                    None
                } else {
                    if let Some((local_session_id, remote_s_public_p384, psk, associated_object)) = new_session_auth(&alice_s_public) {
                        if let Some(ss) = P384PublicKey::from_bytes(&remote_s_public_p384).and_then(|pk| local_s_keypair_p384.agree(&pk)) {
                            Some(Session::<O> {
                                local_session_id, // Bob's session ID
                                remote_session_id: AtomicU64::new(alice_session_id),
                                outgoing_packet_counter: Counter::new(),
                                psk,
                                ss,
                                remote_s_public_p384, // Bob's P-384 static public key
                                outgoing_obfuscator: Obfuscator::new(&alice_s_public),
                                offer: Mutex::new(None),
                                keys: RwLock::new([None, None]),
                                associated_object,
                            })
                        } else {
                            return Err(Error::FailedAuthentication);
                        }
                    } else {
                        return Err(Error::NewSessionRejected);
                    }
                };
                let session = session.as_ref().map_or_else(|| new_session.as_ref().unwrap(), |s| &*s);

                let key = Secret(hmac_sha512(key.as_bytes(), session.ss.as_bytes()));

                if !hmac_sha384(kbkdf512(key.as_bytes(), KBKDF_KEY_USAGE_LABEL_HMAC).first_n::<48>(), &buffer[..incoming_packet.len() - HMAC_SIZE])
                    .eq(&buffer[incoming_packet.len() - HMAC_SIZE..incoming_packet.len()])
                {
                    return Err(Error::FailedAuthentication);
                }

                // Alice's offer has been verified and her key state reconstructed.

                session.remote_session_id.store(alice_session_id, Ordering::Relaxed);
                let counter = session.outgoing_packet_counter.next();

                let bob_e0_keypair = P384KeyPair::generate();
                let e0e0 = bob_e0_keypair.agree(&alice_e0_public).ok_or(Error::FailedAuthentication)?;
                let se0 = P384PublicKey::from_bytes(&session.remote_s_public_p384).and_then(|pk| bob_e0_keypair.agree(&pk)).ok_or(Error::FailedAuthentication)?;

                let (bob_e1_public, e1e1) = if jedi && alice_e1_public.is_some() {
                    if let Ok((bob_e1_public, e1e1)) = pqc_kyber::encapsulate(alice_e1_public.as_ref().unwrap(), &mut random::SecureRandom::default()) {
                        (Some(bob_e1_public), Secret(e1e1))
                    } else {
                        return Err(Error::FailedAuthentication);
                    }
                } else {
                    (None, Secret::default()) // use all zero Kyber secret if disabled
                };

                // FIPS note: the order of HMAC parameters are flipped here from the usual Noise HMAC(key, X). That's because
                // NIST/FIPS allows HKDF with HMAC(salt, key) and salt is allowed to be anything. This way if the PSK is not
                // FIPS compliant the compliance of the entire key derivation is not invalidated. It can just be considered a
                // salt. Since both inputs are fixed size secrets nobody else can control this shouldn't be cryptographically
                // meaningful.
                let key = Secret(hmac_sha512(
                    session.psk.as_bytes(),
                    &hmac_sha512(&hmac_sha512(&hmac_sha512(key.as_bytes(), bob_e0_keypair.public_key_bytes()), e0e0.as_bytes()), se0.as_bytes()),
                ));

                // At this point we've completed standard Noise_IK key derivation, but see the extra step after AES-GCM below...

                let mut counter_offer_size =
                    assemble_KEY_COUNTER_OFFER(buffer, counter, alice_session_id, bob_e0_keypair.public_key(), session.local_session_id, bob_e1_public.as_ref());

                let mut c = AesGcm::new(kbkdf512(key.as_bytes(), KBKDF_KEY_USAGE_LABEL_AES_GCM_BOB_TO_ALICE).first_n::<32>(), true);
                c.init(&get_aes_gcm_nonce(buffer));
                c.crypt_in_place(&mut buffer[HEADER_SIZE + P384_PUBLIC_KEY_SIZE..counter_offer_size]);
                buffer[counter_offer_size..counter_offer_size + AES_GCM_TAG_SIZE].copy_from_slice(&c.finish());
                counter_offer_size += AES_GCM_TAG_SIZE;

                // NOTE: this is the only major departure from standard Noise_IK: we mix the optional Kyber key
                // AFTER mixing the PSK. The Kyber key could have been mixed with the PSK but that would create a
                // chicken-or-egg problem due to the fact that we encrypt Kyber. How would Alice decrypt it? The
                // extra HMAC below authenticates the entire exchange including Kyber.
                let key = Secret(hmac_sha512(e1e1.as_bytes(), key.as_bytes()));

                let hmac = hmac_sha384(kbkdf512(key.as_bytes(), KBKDF_KEY_USAGE_LABEL_HMAC).first_n::<48>(), &buffer[..counter_offer_size]);
                buffer[counter_offer_size..counter_offer_size + HMAC_SIZE].copy_from_slice(&hmac);
                counter_offer_size += HMAC_SIZE;

                let _ = session.keys.write()[1].replace(SessionKey::new(key, Role::Bob, current_time, counter, jedi));

                // Bob now has final key state for this exchange. Yay! Now reply to Alice so she can construct it.

                session.outgoing_obfuscator.0.encrypt_block_in_place(&mut buffer[0..16]);
                session.outgoing_obfuscator.0.encrypt_block_in_place(&mut buffer[16..32]);
                session.outgoing_obfuscator.0.encrypt_block_in_place(&mut buffer[32..48]);
                session.outgoing_obfuscator.0.encrypt_block_in_place(&mut buffer[48..64]);

                return new_session
                    .map_or_else(|| Ok(ReceiveResult::OkSendReply(&buffer[..counter_offer_size])), |ns| Ok(ReceiveResult::OkNewSession(ns, &buffer[..counter_offer_size])));
            }

            PACKET_TYPE_KEY_COUNTER_OFFER => {
                // bob (remote) -> alice (local)

                if let Some(session) = session {
                } else {
                    return Err(Error::InvalidPacket);
                }

                /*
                let (remote_e0_public, local_static_remote_e0) = decrypt_aead_key_exchange::<BS, STATIC_PUBLIC_SIZE>(incoming_packet, buffer, remote_to_local_obfuscator, local_static_p384_secret)?;
                if let Some(session) = session {
                    let (remote_session_id, in_re_offer_id, remote_e1_public) = parse_KEY_COUNTER_OFFER_after_header(buffer)?;
                    if let Some(last_offer) = session.offer.lock().take() {
                        if last_offer.id.eq(&in_re_offer_id) {
                            if let Some(ee0) = last_offer.local_e0_secret.agree(&remote_e0_public) {
                                let local_e1_remote_e1 = last_offer.local_e1_secret.and_then(|e1| {
                                    if let Some(remote_e1_public) = remote_e1_public {
                                        if let Ok(ee1) = pqc_kyber::decapsulate(&remote_e1_public, &e1.secret) {
                                            Some(Secret(ee1))
                                        } else {
                                            None
                                        }
                                    } else {
                                        None
                                    }
                                });

                                let counter = session.outgoing_packet_counter.next();

                                let new_shared_secret = SessionKey::new(
                                    &last_offer.local_e0_remote_static,
                                    &local_static_remote_e0,
                                    &session.static_secret,
                                    ee0,
                                    local_e1_remote_e1,
                                    current_time,
                                    counter,
                                    false,
                                );
                                let new_secret_key = new_shared_secret.key.clone();

                                let mut session_shared_secret = session.shared_secret.write();
                                session_shared_secret.previous = session_shared_secret.current.replace(new_shared_secret);
                                let _ = session_shared_secret.next.take();
                                drop(session_shared_secret); // release lock

                                let mut reply: packed::KeyCounterOfferAck = packed::zeroed();
                                reply.t = PACKET_TYPE_KEY_COUNTER_OFFER_ACK;
                                reply.counter = counter.to_bytes();
                                reply.to_session_id.copy_from_slice(&remote_session_id.to_le_bytes()[..6]);
                                let mut hmac = HMACSHA384::new(zt_kbkdf_hmac_sha384(&new_secret_key.as_bytes()[..48], KBKDF_KEY_USAGE_LABEL_HMAC).as_bytes());
                                hmac.update(&packed::as_bytes(&reply)[..HEADER_SIZE]);
                                hmac.update(new_secret_key.as_bytes());
                                reply.hmac_shared_secret = hmac.finish();
                                reply.hmac_static_key = hmac_sha384(
                                    zt_kbkdf_hmac_sha384(&session.static_secret.0[..48], KBKDF_KEY_USAGE_LABEL_HMAC).as_bytes(),
                                    &packed::as_bytes(&reply)[..size_of::<packed::KeyCounterOfferAck>() - 48],
                                );

                                buffer[..size_of::<packed::KeyCounterOfferAck>()].copy_from_slice(&packed::as_bytes(&reply)[..size_of::<packed::KeyCounterOfferAck>()]);
                                session.local_to_remote_obfuscator.0.encrypt_block_in_place(&mut buffer[0..16]);

                                return Ok(ReceiveResult::OkSendReply(&buffer[..HEADER_SIZE + HMAC_SIZE + HMAC_SIZE]));
                            } else {
                                return Err(Error::InvalidPacket);
                            }
                        }
                    }
                }

                return Ok(ReceiveResult::Ignored);
                                */
                todo!()
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
            rekey_at_or_after_counter: current_counter.0 + REKEY_AFTER_USES + (random::next_u32_secure() % REKEY_AFTER_TIME_MS_MAX_JITTER) as u64,
            hard_expire_at_counter: current_counter.0 + EXPIRE_AFTER_USES,
            rekey_at_or_after_timestamp: current_time + REKEY_AFTER_TIME_MS + (random::next_u32_secure() % REKEY_AFTER_TIME_MS_MAX_JITTER) as i64,
        }
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
        let alice_e1_keypair = if jedi { Some(pqc_kyber::keypair(&mut random::SecureRandom::get())) } else { None };

        let key = Secret(hmac_sha512(&hmac_sha512(&KEY_COMPUTATION_STARTING_SALT, alice_e0_keypair.public_key_bytes()), e0s.as_bytes()));

        let mut packet_size =
            assemble_KEY_OFFER(buffer, counter, bob_session_id, alice_e0_keypair.public_key(), alice_session_id, alice_s_public, alice_e1_keypair.as_ref().map(|s| &s.public));

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
        b[0] = E1_TYPE_KYBER512_90S;
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
                let alice_e1_public = if e1_type == E1_TYPE_KYBER512_90S {
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
        b[0] = E1_TYPE_KYBER512_90S;
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
            let bob_e1_public = if e1_type == E1_TYPE_KYBER512_90S {
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
    (b[0] as u64)
        | (b[1] as u64).wrapping_shl(8)
        | (b[2] as u64).wrapping_shl(16)
        | (b[3] as u64).wrapping_shl(24)
        | (b[4] as u64).wrapping_shl(32)
        | (b[5] as u64).wrapping_shl(40)
}

#[cold]
#[inline(never)]
extern "C" fn unlikely_branch() {}
