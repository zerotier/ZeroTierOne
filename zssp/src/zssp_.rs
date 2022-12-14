// (c) 2020-2022 ZeroTier, Inc. -- currently proprietary pending actual release and licensing. See LICENSE.md.

// ZSSP: ZeroTier Secure Session Protocol
// FIPS compliant Noise_IK with Jedi powers and built-in attack-resistant large payload (fragmentation) support.

use std::mem;
use std::sync::{Mutex, RwLock};

use zerotier_crypto::aes::{Aes, AesGcm};
use zerotier_crypto::hash::{hmac_sha512, SHA384, HMACSHA384};
use zerotier_crypto::p384::{P384KeyPair, P384PublicKey, P384_PUBLIC_KEY_SIZE};
use zerotier_crypto::random;
use zerotier_crypto::secret::Secret;

use zerotier_utils::gatherarray::GatherArray;
use zerotier_utils::memory;
use zerotier_utils::ringbuffermap::RingBufferMap;
use zerotier_utils::unlikely_branch;
use zerotier_utils::varint;

use crate::{constants::*, ApplicationLayer};
use crate::types::*;


// Write src into dest. If dest cannot fit src, nothing at all is written and UnexpectedBufferOverrun is returned. No other errors can be returned by this function. The dest slice is incremented so it starts at the end of where src was written.
fn safe_write_all(dest: &mut &mut [u8], src: &[u8]) -> Result<(), Error> {
    let amt = src.len();
    if dest.len() >= amt {
        let (a, b) = mem::replace(dest, &mut []).split_at_mut(amt);
        a.copy_from_slice(&src[..amt]);
        *dest = b;
        Ok(())
    } else {
        Err(Error::UnexpectedBufferOverrun)
    }
}
/// Write a variable length integer, which can consume up to 10 bytes.
#[inline(always)]
pub fn varint_safe_write(dest: &mut &mut [u8], v: u64) -> Result<(), Error> {
    let mut b = [0_u8; varint::VARINT_MAX_SIZE_BYTES];
    let i = varint::encode(&mut b, v);
    safe_write_all(dest, &b[0..i])
}



/// Shortcut to HMAC data split into two slices.
fn hmac_sha384_2(key: &[u8], a: &[u8], b: &[u8]) -> [u8; 48] {
    let mut hmac = HMACSHA384::new(key);
    hmac.update(a);
    hmac.update(b);
    hmac.finish()
}
/// HMAC-SHA512 key derivation function modeled on: https://csrc.nist.gov/publications/detail/sp/800-108/final (page 12)
/// Cryptographically this isn't really different from HMAC(key, [label]) with just one byte.
fn kbkdf512(key: &[u8], label: u8) -> Secret<64> {
    Secret(hmac_sha512(key, &[0, 0, 0, 0, b'Z', b'T', label, 0, 0, 0, 0, 0x02, 0x00]))
}
/// Get a hash of a secret key that can be used as a public fingerprint.
fn secret_fingerprint(key: &[u8]) -> [u8; 48] {
    let mut tmp = SHA384::new();
    tmp.update("fp".as_bytes());
    tmp.update(key);
    tmp.finish()
}



/// Create a new session and send an initial key offer message to the other end.
///
/// * `host` - Interface to application using ZSSP
/// * `local_session_id` - ID for this side of the session, must be locally unique
/// * `remote_s_public` - RemApplicationLayerote side's public key/identity
/// * `offer_metadata` - Arbitrary meta-data to send with key offer (empty if none)
/// * `psk` - Arbitrary pre-shared key to include as initial key material (use all zero secret if none)
/// * `associated_object` - Arbitrary object to put into session
/// * `mtu` - Physical wire maximum transmition unit
/// * `current_time` - Current monotonic time in milliseconds

pub fn start_session<'a, SendFn: FnMut(&mut [u8]), ExtractPublicP384Fn: Fn(&[u8]), Layer: ApplicationLayer>(
    host: Layer,
    send: SendFn,
    local_session_id: SessionId,
    remote_s_public: P384PublicKey,
    remote_s_public_raw: &[u8],
    offer_metadata: &[u8],
    psk: &Secret<64>,
    mtu: usize,
    current_time: i64,
) -> Result<Session<Layer::IncomingPacketBuffer>, Error> {
    if let Some(shared_ss) = host.get_local_static_keypair().agree(&remote_s_public) {
        let send_counter = Counter::new();
        let remote_s_public_hash = SHA384::hash(remote_s_public_raw);
        let header_check_cipher =
        Aes::new(kbkdf512(shared_ss.as_bytes(), KBKDF_KEY_USAGE_LABEL_HEADER_CHECK).first_n::<HEADER_CHECK_AES_KEY_SIZE>());
        //we don't want to return the offer by value up the stack because that's a big copy, instead we write the offer to a mutable option
        let offer = None;
        if send_ephemeral_offer(
            &mut send,
            send_counter.next(),
            local_session_id,
            None,
            host.get_local_static_public_raw(),
            offer_metadata,
            &remote_s_public,
            &remote_s_public_hash,
            &shared_ss,
            None,
            None,
            mtu,
            current_time,
            &mut offer,
        ).is_ok() {
            return Ok(Session {
                id: local_session_id,
                send_counter,
                psk: psk.clone(),
                shared_ss,
                header_check_cipher,
                state: RwLock::new(SessionMutableState {
                    remote_session_id: None,
                    keys: [None, None, None],
                    key_idx: 0,
                    offer: Some(offer.unwrap()),
                    last_remote_offer: i64::MIN,
                }),
                remote_s_public_hash,
                remote_s_public_raw: remote_s_public.as_bytes().clone(),
                defrag: Mutex::new(RingBufferMap::new(random::xorshift64_random() as u32)),
            });
        }
    }
    return Err(Error::InvalidParameter);
}

/// Create and send an ephemeral offer, returning the EphemeralOffer part that must be saved.
fn send_ephemeral_offer<SendFn: FnMut(&mut [u8])>(
    send: &mut SendFn,
    counter: CounterValue,
    local_session_id: SessionId,
    remote_session_id: Option<SessionId>,
    local_s_public_raw: &[u8],
    offer_metadata: &[u8],
    remote_s_public: &P384PublicKey,
    remote_s_public_hash: &[u8],
    shared_ss: &Secret<48>,
    current_shared_symmetric: Option<&SessionKey>,
    header_check_cipher: Option<&Aes>, // None to use one based on the recipient's public key for initial contact rename this
    mtu: usize,
    current_time: i64,
    ret_offer: &mut Option<EphemeralOffer>,
) -> Result<(), Error> {
    // Generate a NIST P-384 pair.
    let local_e0_keypair = P384KeyPair::generate();

    // Perform key agreement with the other side's static P-384 public key.
    let shared_e0s = local_e0_keypair.agree(remote_s_public).ok_or(Error::InvalidPacket)?;

    // Generate a Kyber1024 pair if enabled.
    let local_e1_keypair = if JEDI {
        Some(pqc_kyber::keypair(&mut random::SecureRandom::get()))
    } else {
        None
    };

    // Get ratchet key for current key if one exists.
    let (ratchet_key, ratchet_count) = if let Some(current_key) = current_shared_symmetric {
        (Some(current_key.ratchet_key.clone()), current_key.ratchet_count)
    } else {
        (None, 0)
    };

    // Random ephemeral offer ID
    let id: [u8; 16] = random::get_bytes_secure();

    // Create ephemeral offer packet (not fragmented yet).
    const PACKET_BUF_SIZE: usize = MIN_TRANSPORT_MTU * KEY_EXCHANGE_MAX_FRAGMENTS;
    let mut packet_buf = [0_u8; PACKET_BUF_SIZE];
    let mut packet_len = {
        let mut p = &mut packet_buf[HEADER_SIZE..];

        safe_write_all(&mut p, &[SESSION_PROTOCOL_VERSION])?;
        safe_write_all(&mut p, local_e0_keypair.public_key_bytes())?;

        safe_write_all(&mut p, &id)?;
        safe_write_all(&mut p, &local_session_id.0.to_le_bytes()[..SESSION_ID_SIZE])?;
        varint_safe_write(&mut p, local_s_public_raw.len() as u64)?;
        safe_write_all(&mut p, local_s_public_raw)?;
        varint_safe_write(&mut p, offer_metadata.len() as u64)?;
        safe_write_all(&mut p, offer_metadata)?;
        if let Some(e1kp) = local_e1_keypair {
            safe_write_all(&mut p, &[E1_TYPE_KYBER1024])?;
            safe_write_all(&mut p, &e1kp.public)?;
        } else {
            safe_write_all(&mut p, &[E1_TYPE_NONE])?;
        }
        if let Some(ratchet_key) = ratchet_key.as_ref() {
            safe_write_all(&mut p, &[0x01])?;
            safe_write_all(&mut p, &secret_fingerprint(ratchet_key.as_bytes())[..16])?;
        } else {
            safe_write_all(&mut p, &[0x00])?;
        }

        PACKET_BUF_SIZE - p.len()
    };

    // Create ephemeral agreement secret.
    let shared_symmetric_es = Secret(hmac_sha512(
        &hmac_sha512(&INITIAL_KEY, local_e0_keypair.public_key_bytes()),
        shared_e0s.as_bytes(),
    ));

    let remote_session_id = remote_session_id.unwrap_or(SessionId::NIL);

    create_packet_header(&mut packet_buf, packet_len, mtu, PACKET_TYPE_KEY_OFFER, remote_session_id, counter)?;

    let authenticated_header = CanonicalHeader::make(remote_session_id, PACKET_TYPE_KEY_OFFER, counter.to_u32());

    // Encrypt packet and attach AES-GCM tag.
    let gcm_tag = {
        let mut c = AesGcm::new(
            kbkdf512(shared_symmetric_es.as_bytes(), KBKDF_KEY_USAGE_LABEL_AES_GCM_ALICE_TO_BOB).first_n::<AES_KEY_SIZE>(),
            true,
        );
        c.reset_init_gcm(authenticated_header.as_bytes());
        c.crypt_in_place(&mut packet_buf[(HEADER_SIZE + 1 + P384_PUBLIC_KEY_SIZE)..packet_len]);
        c.finish_encrypt()
    };

    packet_buf[packet_len..(packet_len + AES_GCM_TAG_SIZE)].copy_from_slice(&gcm_tag);
    packet_len += AES_GCM_TAG_SIZE;

    // Mix in static secret.
    let shared_symmetric_ss = Secret(hmac_sha512(shared_symmetric_es.as_bytes(), shared_ss.as_bytes()));

    // HMAC packet using static + ephemeral key.
    let hmac = hmac_sha384_2(
        kbkdf512(shared_symmetric_ss.as_bytes(), KBKDF_KEY_USAGE_LABEL_HMAC).first_n::<48>(),
        authenticated_header.as_bytes(),
        &packet_buf[HEADER_SIZE..packet_len],
    );
    packet_buf[packet_len..(packet_len + HMAC_SIZE)].copy_from_slice(&hmac);
    packet_len += HMAC_SIZE;

    // Add secondary HMAC to verify that the caller knows the recipient's full static public identity.
    let hmac = hmac_sha384_2(remote_s_public_hash, authenticated_header.as_bytes(), &packet_buf[HEADER_SIZE..packet_len]);
    packet_buf[packet_len..(packet_len + HMAC_SIZE)].copy_from_slice(&hmac);
    packet_len += HMAC_SIZE;

    let header_check_cipher = if let Some(header_check_cipher) = header_check_cipher {
        header_check_cipher
    } else {
        &Aes::new(kbkdf512(&remote_s_public_hash, KBKDF_KEY_USAGE_LABEL_HEADER_CHECK).first_n::<HEADER_CHECK_AES_KEY_SIZE>())
    };

    send_with_fragmentation(send, &mut packet_buf[..packet_len], mtu, header_check_cipher);

    *ret_offer = Some(EphemeralOffer {
        id,
        creation_time: current_time,
        ratchet_count,
        ratchet_key,
        shared_symmetric_ss,
        local_e0_keypair,
        local_e1_keypair,
    });
    Ok(())
}

impl CanonicalHeader {
    #[inline(always)]
    pub fn make(session_id: SessionId, packet_type: u8, counter: u32) -> Self {
        CanonicalHeader(
            (u64::from(session_id) | (packet_type as u64).wrapping_shl(48)).to_le(),
            counter.to_le(),
        )
    }

    #[inline(always)]
    pub fn as_bytes(&self) -> &[u8; 12] {
        memory::as_byte_array(self)
    }
}


/// Populate all but the header check code in the first 16 bytes of a packet or fragment.
#[inline(always)]
fn create_packet_header(
    header: &mut [u8],
    packet_len: usize,
    mtu: usize,
    packet_type: u8,
    recipient_session_id: SessionId,
    counter: CounterValue,
) -> Result<(), Error> {
    let fragment_count = ((packet_len as f32) / (mtu - HEADER_SIZE) as f32).ceil() as usize;

    debug_assert!(header.len() >= HEADER_SIZE);
    debug_assert!(mtu >= MIN_TRANSPORT_MTU);
    debug_assert!(packet_len >= MIN_PACKET_SIZE);
    debug_assert!(fragment_count > 0);
    debug_assert!(fragment_count <= MAX_FRAGMENTS);
    debug_assert!(packet_type <= 0x0f); // packet type is 4 bits

    if fragment_count <= MAX_FRAGMENTS {
        // Header indexed by bit:
        //   [0-31]    counter
        //   [32-63]   header check code (computed later)
        //   [64-111]  recipient's session ID (unique on their side)
        //   [112-115] packet type (0-15)
        //   [116-121] number of fragments (0..63 for 1..64 fragments total)
        //   [122-127] fragment number (0, 1, 2, ...)
        memory::store_raw((counter.to_u32() as u64).to_le(), header);
        memory::store_raw(
            (u64::from(recipient_session_id) | (packet_type as u64).wrapping_shl(48) | ((fragment_count - 1) as u64).wrapping_shl(52))
            .to_le(),
            &mut header[8..],
        );
        Ok(())
    } else {
        unlikely_branch();
        Err(Error::DataTooLarge)
    }
}


/// Break a packet into fragments and send them all.
fn send_with_fragmentation<SendFunction: FnMut(&mut [u8])>(
    send: &mut SendFunction,
    packet: &[u8],
    mtu: usize,
    header_check_cipher: &Aes,
) {
    let packet_len = packet.len();
    let mut fragment_start = 0;
    let mut fragment_end = packet_len.min(mtu);
    let mut header: [u8; 16] = packet[..HEADER_SIZE].try_into().unwrap();
    loop {
        let fragment = &mut packet[fragment_start..fragment_end];
        set_header_check_code(fragment, header_check_cipher);

        send(fragment);

        if fragment_end < packet_len {
            debug_assert!(header[15].wrapping_shr(2) < 63);
            header[15] += 0x04; // increment fragment number
            fragment_start = fragment_end - HEADER_SIZE;
            fragment_end = (fragment_start + mtu).min(packet_len);
            packet[fragment_start..(fragment_start + HEADER_SIZE)].copy_from_slice(&header);
        } else {
            debug_assert_eq!(fragment_end, packet_len);
            break;
        }
    }
}

/// Set 32-bit header check code, used to make fragmentation mechanism robust.
#[inline]
fn set_header_check_code(packet: &mut [u8], header_check_cipher: &Aes) {
    debug_assert!(packet.len() >= MIN_PACKET_SIZE);
    let mut check_code = 0u128.to_ne_bytes();
    header_check_cipher.encrypt_block(&packet[8..24], &mut check_code);
    packet[4..8].copy_from_slice(&check_code[..4]);
}




/// Receive, authenticate, decrypt, and process a physical wire packet.
///
/// * `host` - Interface to application using ZSSP
/// * `remote_address` - Remote physical address of source endpoint
/// * `data_buf` - Buffer to receive decrypted and authenticated object data (an error is returned if too small)
/// * `incoming_packet_buf` - Buffer containing incoming wire packet (receive() takes ownership)
/// * `mtu` - Physical wire MTU for sending packets
/// * `current_time` - Current monotonic time in milliseconds
#[inline]
pub fn receive<'a, SendFunction: FnMut(&mut [u8]), Layer: ApplicationLayer>(
    host: &Layer,
    context : &ReceiveContext<Layer::IncomingPacketBuffer>,
    remote_address: &Layer::RemoteAddress,
    mut send: SendFunction,
    data_buf: &'a mut [u8],
    incoming_packet_buf: Layer::IncomingPacketBuffer,
    mtu: usize,
    current_time: i64,
) -> Result<ReceiveResult<'a, Layer::IncomingPacketBuffer>, Error> {
    let incoming_packet = incoming_packet_buf.as_ref();
    if incoming_packet.len() < MIN_PACKET_SIZE {
        unlikely_branch();
        return Err(Error::InvalidPacket);
    }

    let counter = u32::from_le(memory::load_raw(incoming_packet));
    let packet_type_fragment_info = u16::from_le(memory::load_raw(&incoming_packet[14..16]));
    let packet_type = (packet_type_fragment_info & 0x0f) as u8;
    let fragment_count = ((packet_type_fragment_info.wrapping_shr(4) + 1) as u8) & 63;
    let fragment_no = packet_type_fragment_info.wrapping_shr(10) as u8; // & 63 not needed

    if let Some(local_session_id) = SessionId::new_from_u64(u64::from_le(memory::load_raw(&incoming_packet[8..16])) & 0xffffffffffffu64)
    {
        if let Some(session) = host.session_lookup(local_session_id) {
            if verify_header_check_code(incoming_packet, &session.header_check_cipher) {
                let canonical_header = CanonicalHeader::make(local_session_id, packet_type, counter);
                if fragment_count > 1 {
                    if fragment_count <= (MAX_FRAGMENTS as u8) && fragment_no < fragment_count {
                        let mut defrag = session.defrag.lock().unwrap();
                        let fragment_gather_array = defrag.get_or_create_mut(&counter, || GatherArray::new(fragment_count));
                        if let Some(assembled_packet) = fragment_gather_array.add(fragment_no, incoming_packet_buf) {
                            drop(defrag); // release lock
                            return receive_complete(
                                host,
                                context,
                                remote_address,
                                &mut send,
                                data_buf,
                                canonical_header.as_bytes(),
                                assembled_packet.as_ref(),
                                packet_type,
                                Some(session),
                                mtu,
                                current_time,
                            );
                        }
                    } else {
                        unlikely_branch();
                        return Err(Error::InvalidPacket);
                    }
                } else {
                    return receive_complete(
                        host,
                        context,
                        remote_address,
                        &mut send,
                        data_buf,
                        canonical_header.as_bytes(),
                        &[incoming_packet_buf],
                        packet_type,
                        Some(session),
                        mtu,
                        current_time,
                    );
                }
            } else {
                unlikely_branch();
                return Err(Error::FailedAuthentication);
            }
        } else {
            unlikely_branch();
            return Err(Error::UnknownLocalSessionId(local_session_id));
        }
    } else {
        unlikely_branch(); // we want data receive to be the priority branch, this is only occasionally used

        if verify_header_check_code(incoming_packet, &context.incoming_init_header_check_cipher) {
            let canonical_header = CanonicalHeader::make(SessionId::NIL, packet_type, counter);
            if fragment_count > 1 {
                let mut defrag = context.initial_offer_defrag.lock().unwrap();
                let fragment_gather_array = defrag.get_or_create_mut(&counter, || GatherArray::new(fragment_count));
                if let Some(assembled_packet) = fragment_gather_array.add(fragment_no, incoming_packet_buf) {
                    drop(defrag); // release lock
                    return receive_complete(
                        host,
                        context,
                        remote_address,
                        &mut send,
                        data_buf,
                        canonical_header.as_bytes(),
                        assembled_packet.as_ref(),
                        packet_type,
                        None,
                        mtu,
                        current_time,
                    );
                }
            } else {
                return receive_complete(
                    host,
                    context,
                    remote_address,
                    &mut send,
                    data_buf,
                    canonical_header.as_bytes(),
                    &[incoming_packet_buf],
                    packet_type,
                    None,
                    mtu,
                    current_time,
                );
            }
        } else {
            unlikely_branch();
            return Err(Error::FailedAuthentication);
        }
    };

    return Ok(ReceiveResult::Ok);
}

/// Verify 32-bit header check code.
#[inline]
fn verify_header_check_code(packet: &[u8], header_check_cipher: &Aes) -> bool {
    debug_assert!(packet.len() >= MIN_PACKET_SIZE);
    let mut header_mac = 0u128.to_ne_bytes();
    header_check_cipher.encrypt_block(&packet[8..24], &mut header_mac);
    memory::load_raw::<u32>(&packet[4..8]) == memory::load_raw::<u32>(&header_mac)
}


fn receive_complete<'a, SendFunction: FnMut(&mut [u8]), Layer: ApplicationLayer>(
    host: &Layer,
    context: &ReceiveContext<Layer::IncomingPacketBuffer>,
    remote_address: &Layer::RemoteAddress,
    send: &mut SendFunction,
    data_buf: &'a mut [u8],
    canonical_header_bytes: &[u8; 12],
    fragments: &[Layer::IncomingPacketBuffer],
    packet_type: u8,
    session: Option<Layer::SessionRef>,
    mtu: usize,
    current_time: i64,
) -> Result<ReceiveResult<'a, Layer::IncomingPacketBuffer>, Error> {
    debug_assert!(fragments.len() >= 1);

    // The first 'if' below should capture both DATA and NOP but not other types. Sanity check this.
    debug_assert_eq!(PACKET_TYPE_DATA, 0);
    debug_assert_eq!(PACKET_TYPE_NOP, 1);

    if packet_type <= PACKET_TYPE_NOP {
        if let Some(session) = session {
            let state = session.state.read().unwrap();
            for p in 0..KEY_HISTORY_SIZE {
                let key_ptr = (state.key_idx + p) % KEY_HISTORY_SIZE;
                if let Some(key) = state.keys[key_ptr].as_ref() {
                    let mut c = key.get_receive_cipher();
                    c.reset_init_gcm(canonical_header_bytes);

                    let mut data_len = 0;

                    // Decrypt fragments 0..N-1 where N is the number of fragments.
                    for f in fragments[..(fragments.len() - 1)].iter() {
                        let f = f.as_ref();
                        debug_assert!(f.len() >= HEADER_SIZE);
                        let current_frag_data_start = data_len;
                        data_len += f.len() - HEADER_SIZE;
                        if data_len > data_buf.len() {
                            unlikely_branch();
                            key.return_receive_cipher(c);
                            return Err(Error::DataBufferTooSmall);
                        }
                        c.crypt(&f[HEADER_SIZE..], &mut data_buf[current_frag_data_start..data_len]);
                    }

                    // Decrypt final fragment (or only fragment if not fragmented)
                    let current_frag_data_start = data_len;
                    let last_fragment = fragments.last().unwrap().as_ref();
                    if last_fragment.len() < (HEADER_SIZE + AES_GCM_TAG_SIZE) {
                        unlikely_branch();
                        return Err(Error::InvalidPacket);
                    }
                    data_len += last_fragment.len() - (HEADER_SIZE + AES_GCM_TAG_SIZE);
                    if data_len > data_buf.len() {
                        unlikely_branch();
                        key.return_receive_cipher(c);
                        return Err(Error::DataBufferTooSmall);
                    }
                    c.crypt(
                        &last_fragment[HEADER_SIZE..(last_fragment.len() - AES_GCM_TAG_SIZE)],
                        &mut data_buf[current_frag_data_start..data_len],
                    );

                    let aead_authentication_ok = c.finish_decrypt(&last_fragment[(last_fragment.len() - AES_GCM_TAG_SIZE)..]);
                    key.return_receive_cipher(c);

                    if aead_authentication_ok {
                        // Select this key as the new default if it's newer than the current key.
                        if p > 0
                        && state.keys[state.key_ptr]
                        .as_ref()
                        .map_or(true, |old| old.establish_counter < key.establish_counter)
                        {
                            drop(state);
                            let mut state = session.state.write().unwrap();
                            state.key_ptr = key_ptr;
                            for i in 0..KEY_HISTORY_SIZE {
                                if i != key_ptr {
                                    if let Some(old_key) = state.keys[key_ptr].as_ref() {
                                        // Release pooled cipher memory from old keys.
                                        old_key.receive_cipher_pool.lock().unwrap().clear();
                                        old_key.send_cipher_pool.lock().unwrap().clear();
                                    }
                                }
                            }
                        }

                        if packet_type == PACKET_TYPE_DATA {
                            return Ok(ReceiveResult::OkData(&mut data_buf[..data_len]));
                        } else {
                            unlikely_branch();
                            return Ok(ReceiveResult::Ok);
                        }
                    }
                }
            }

            // If no known key authenticated the packet, decryption has failed.
            return Err(Error::FailedAuthentication);
        } else {
            unlikely_branch();
            return Err(Error::SessionNotEstablished);
        }
    } else {
        unlikely_branch();

        // To greatly simplify logic handling key exchange packets, assemble these first.
        // Handling KEX packets isn't the fast path so the extra copying isn't significant.
        const KEX_BUF_LEN: usize = MIN_TRANSPORT_MTU * KEY_EXCHANGE_MAX_FRAGMENTS;
        let mut kex_packet = [0_u8; KEX_BUF_LEN];
        let mut kex_packet_len = 0;
        for i in 0..fragments.len() {
            let mut ff = fragments[i].as_ref();
            debug_assert!(ff.len() >= MIN_PACKET_SIZE);
            if i > 0 {
                ff = &ff[HEADER_SIZE..];
            }
            let j = kex_packet_len + ff.len();
            if j > KEX_BUF_LEN {
                return Err(Error::InvalidPacket);
            }
            kex_packet[kex_packet_len..j].copy_from_slice(ff);
            kex_packet_len = j;
        }
        let kex_packet_saved_ciphertext = kex_packet.clone(); // save for HMAC check later

        // Key exchange packets begin (after header) with the session protocol version.
        if kex_packet[HEADER_SIZE] != SESSION_PROTOCOL_VERSION {
            return Err(Error::UnknownProtocolVersion);
        }

        match packet_type {
            PACKET_TYPE_KEY_OFFER => {
                // alice (remote) -> bob (local)

                if kex_packet_len < (HEADER_SIZE + 1 + P384_PUBLIC_KEY_SIZE + AES_GCM_TAG_SIZE + HMAC_SIZE + HMAC_SIZE) {
                    return Err(Error::InvalidPacket);
                }
                let payload_end = kex_packet_len - (AES_GCM_TAG_SIZE + HMAC_SIZE + HMAC_SIZE);
                let aes_gcm_tag_end = kex_packet_len - (HMAC_SIZE + HMAC_SIZE);
                let hmac1_end = kex_packet_len - HMAC_SIZE;

                // Check the second HMAC first, which proves that the sender knows the recipient's full static identity.
                if !hmac_sha384_2(
                    host.get_local_s_public_hash(),
                    canonical_header_bytes,
                    &kex_packet[HEADER_SIZE..hmac1_end],
                )
                .eq(&kex_packet[hmac1_end..kex_packet_len])
                {
                    return Err(Error::FailedAuthentication);
                }

                // Check rate limits.
                if let Some(session) = session.as_ref() {
                    if (current_time - session.state.read().unwrap().last_remote_offer) < Layer::REKEY_RATE_LIMIT_MS {
                        return Err(Error::RateLimited);
                    }
                } else {
                    if !host.check_new_session_attempt(context, remote_address) {
                        return Err(Error::RateLimited);
                    }
                }

                // Key agreement: alice (remote) ephemeral NIST P-384 <> local static NIST P-384
                let (alice_e0_public, e0s) =
                P384PublicKey::from_bytes(&kex_packet[(HEADER_SIZE + 1)..(HEADER_SIZE + 1 + P384_PUBLIC_KEY_SIZE)])
                .and_then(|pk| host.get_local_static_keypair().agree(&pk).map(move |s| (pk, s)))
                .ok_or(Error::FailedAuthentication)?;

                // Initial key derivation from starting point, mixing in alice's ephemeral public and the e0s.
                let mut key = Secret(hmac_sha512(&hmac_sha512(&INITIAL_KEY, alice_e0_public.as_bytes()), e0s.as_bytes()));

                // Decrypt the encrypted part of the packet payload and authenticate the above key exchange via AES-GCM auth.
                let mut c = AesGcm::new(
                    kbkdf512(key.as_bytes(), KBKDF_KEY_USAGE_LABEL_AES_GCM_ALICE_TO_BOB).first_n::<AES_KEY_SIZE>(),
                    false,
                );
                c.reset_init_gcm(canonical_header_bytes);
                c.crypt_in_place(&mut kex_packet[(HEADER_SIZE + 1 + P384_PUBLIC_KEY_SIZE)..payload_end]);
                if !c.finish_decrypt(&kex_packet[payload_end..aes_gcm_tag_end]) {
                    return Err(Error::FailedAuthentication);
                }

                // Parse payload and get alice's session ID, alice's public blob, metadata, and (if present) Alice's Kyber1024 public.
                let (offer_id, alice_session_id, alice_s_public, alice_metadata, alice_e1_public, alice_ratchet_key_fingerprint) =
                parse_key_offer_after_header(&kex_packet[(HEADER_SIZE + 1 + P384_PUBLIC_KEY_SIZE)..kex_packet_len], packet_type)?;

                // We either have a session, in which case they should have supplied a ratchet key fingerprint, or
                // we don't and they should not have supplied one.
                if session.is_some() != alice_ratchet_key_fingerprint.is_some() {
                    return Err(Error::FailedAuthentication);
                }

                // Extract alice's static NIST P-384 public key from her public blob.
                let alice_s_public_p384 = Layer::extract_p384_static(alice_s_public).ok_or(Error::InvalidPacket)?;

                // Key agreement: both sides' static P-384 keys.
                let ss = host
                .get_local_s_keypair_p384()
                .agree(&alice_s_public_p384)
                .ok_or(Error::FailedAuthentication)?;

                // Mix result of 'ss' agreement into master key.
                key = Secret(hmac_sha512(key.as_bytes(), ss.as_bytes()));

                // Authenticate entire packet with HMAC-SHA384, verifying alice's identity via 'ss' secret that was
                // just mixed into the key.
                if !hmac_sha384_2(
                    kbkdf512(key.as_bytes(), KBKDF_KEY_USAGE_LABEL_HMAC).first_n::<48>(),
                    canonical_header_bytes,
                    &kex_packet_saved_ciphertext[HEADER_SIZE..aes_gcm_tag_end],
                )
                .eq(&kex_packet[aes_gcm_tag_end..hmac1_end])
                {
                    return Err(Error::FailedAuthentication);
                }

                // Alice's offer has been verified and her current key state reconstructed.

                // Perform checks and match ratchet key if there's an existing session, or gate (via host) and
                // then create new sessions.
                let (new_session, ratchet_key, ratchet_count) = if let Some(session) = session.as_ref() {
                    // Existing session identity must match the one in this offer.
                    if !session.remote_s_public_hash.eq(&SHA384::hash(&alice_s_public)) {
                        return Err(Error::FailedAuthentication);
                    }

                    // Match ratchet key fingerprint and fail if no match, which likely indicates an old offer packet.
                    let alice_ratchet_key_fingerprint = alice_ratchet_key_fingerprint.as_ref().unwrap();
                    let mut ratchet_key = None;
                    let mut ratchet_count = 0;
                    let state = session.state.read().unwrap();
                    for k in state.keys.iter() {
                        if let Some(k) = k.as_ref() {
                            if secret_fingerprint(k.ratchet_key.as_bytes())[..16].eq(alice_ratchet_key_fingerprint) {
                                ratchet_key = Some(k.ratchet_key.clone());
                                ratchet_count = k.ratchet_count;
                                break;
                            }
                        }
                    }
                    if ratchet_key.is_none() {
                        return Ok(ReceiveResult::Ignored); // old packet?
                    }

                    (None, ratchet_key, ratchet_count)
                } else {
                    if let Some((new_session_id, psk, associated_object)) =
                    host.accept_new_session(context, remote_address, alice_s_public, alice_metadata)
                    {
                        let header_check_cipher = Aes::new(
                            kbkdf512(ss.as_bytes(), KBKDF_KEY_USAGE_LABEL_HEADER_CHECK).first_n::<HEADER_CHECK_AES_KEY_SIZE>(),
                        );
                        (
                            Some(Session {
                                id: new_session_id,
                                send_counter: Counter::new(),
                                psk,
                                shared_ss: ss,
                                header_check_cipher,
                                state: RwLock::new(SessionMutableState {
                                    remote_session_id: Some(alice_session_id),
                                    keys: [None, None, None],
                                    key_idx: 0,
                                    offer: None,
                                    last_remote_offer: current_time,
                                }),
                                remote_s_public_hash: SHA384::hash(&alice_s_public),
                                remote_s_public_raw: alice_s_public_p384.as_bytes().clone(),
                                defrag: Mutex::new(RingBufferMap::new(random::xorshift64_random() as u32)),
                            }),
                            None,
                            0,
                        )
                    } else {
                        return Err(Error::NewSessionRejected);
                    }
                };

                // Set 'session' to a reference to either the existing or the new session.
                let existing_session = session;
                let session = existing_session.as_ref().map_or_else(|| new_session.as_ref().unwrap(), |s| &*s);

                // Generate our ephemeral NIST P-384 key pair.
                let bob_e0_keypair = P384KeyPair::generate();

                // Key agreement: both sides' ephemeral P-384 public keys.
                let e0e0 = bob_e0_keypair.agree(&alice_e0_public).ok_or(Error::FailedAuthentication)?;

                // Key agreement: bob (local) static NIST P-384, alice (remote) ephemeral P-384.
                let se0 = bob_e0_keypair.agree(&alice_s_public_p384).ok_or(Error::FailedAuthentication)?;

                // Mix in the psk, the key to this point, our ephemeral public, e0e0, and se0, completing Noise_IK.
                //
                // FIPS note: the order of HMAC parameters are flipped here from the usual Noise HMAC(key, X). That's because
                // NIST/FIPS allows HKDF with HMAC(salt, key) and salt is allowed to be anything. This way if the PSK is not
                // FIPS compliant the compliance of the entire key derivation is not invalidated. Both inputs are secrets of
                // fixed size so this shouldn't matter cryptographically.
                key = Secret(hmac_sha512(
                    session.psk.as_bytes(),
                    &hmac_sha512(
                        &hmac_sha512(&hmac_sha512(key.as_bytes(), bob_e0_keypair.public_key_bytes()), e0e0.as_bytes()),
                        se0.as_bytes(),
                    ),
                ));

                // At this point we've completed Noise_IK key derivation with NIST P-384 ECDH, but now for hybrid and ratcheting...

                // Generate a Kyber encapsulated ciphertext if Kyber is enabled and the other side sent us a public key.
                let (bob_e1_public, e1e1) = if JEDI && alice_e1_public.len() > 0 {
                    if let Ok((bob_e1_public, e1e1)) = pqc_kyber::encapsulate(alice_e1_public, &mut random::SecureRandom::default()) {
                        (Some(bob_e1_public), Some(Secret(e1e1)))
                    } else {
                        return Err(Error::FailedAuthentication);
                    }
                } else {
                    (None, None)
                };

                // Create reply packet.
                let mut reply_buf = [0_u8; KEX_BUF_LEN];
                let reply_counter = session.send_counter.next();
                let mut reply_len = {
                    let mut rp = &mut reply_buf[HEADER_SIZE..];

                    rp.write_all(&[SESSION_PROTOCOL_VERSION])?;
                    rp.write_all(bob_e0_keypair.public_key_bytes())?;

                    rp.write_all(&offer_id)?;
                    rp.write_all(&session.id.0.to_le_bytes()[..SESSION_ID_SIZE])?;
                    varint::write(&mut rp, 0)?; // they don't need our static public; they have it
                    varint::write(&mut rp, 0)?; // no meta-data in counter-offers (could be used in the future)
                    if let Some(bob_e1_public) = bob_e1_public.as_ref() {
                        rp.write_all(&[E1_TYPE_KYBER1024])?;
                        rp.write_all(bob_e1_public)?;
                    } else {
                        rp.write_all(&[E1_TYPE_NONE])?;
                    }
                    if ratchet_key.is_some() {
                        rp.write_all(&[0x01])?;
                        rp.write_all(alice_ratchet_key_fingerprint.as_ref().unwrap())?;
                    } else {
                        rp.write_all(&[0x00])?;
                    }

                    KEX_BUF_LEN - rp.len()
                };
                create_packet_header(
                    &mut reply_buf,
                    reply_len,
                    mtu,
                    PACKET_TYPE_KEY_COUNTER_OFFER,
                    alice_session_id.into(),
                    reply_counter,
                )?;
                let reply_canonical_header =
                CanonicalHeader::make(alice_session_id.into(), PACKET_TYPE_KEY_COUNTER_OFFER, reply_counter.to_u32());

                // Encrypt reply packet using final Noise_IK key BEFORE mixing hybrid or ratcheting, since the other side
                // must decrypt before doing these things.
                let mut c = AesGcm::new(
                    kbkdf512(key.as_bytes(), KBKDF_KEY_USAGE_LABEL_AES_GCM_BOB_TO_ALICE).first_n::<AES_KEY_SIZE>(),
                    true,
                );
                c.reset_init_gcm(reply_canonical_header.as_bytes());
                c.crypt_in_place(&mut reply_buf[(HEADER_SIZE + 1 + P384_PUBLIC_KEY_SIZE)..reply_len]);
                let c = c.finish_encrypt();
                reply_buf[reply_len..(reply_len + AES_GCM_TAG_SIZE)].copy_from_slice(&c);
                reply_len += AES_GCM_TAG_SIZE;

                // Mix ratchet key from previous session key (if any) and Kyber1024 hybrid shared key (if any).
                if let Some(ratchet_key) = ratchet_key {
                    key = Secret(hmac_sha512(ratchet_key.as_bytes(), key.as_bytes()));
                }
                if let Some(e1e1) = e1e1.as_ref() {
                    key = Secret(hmac_sha512(e1e1.as_bytes(), key.as_bytes()));
                }

                // Authenticate packet using HMAC-SHA384 with final key. Note that while the final key now has the Kyber secret
                // mixed in, this doesn't constitute session authentication with Kyber because there's no static Kyber key
                // associated with the remote identity. An attacker who can break NIST P-384 (and has the psk) could MITM the
                // Kyber exchange, but you'd need a not-yet-existing quantum computer for that.
                let hmac = hmac_sha384_2(
                    kbkdf512(key.as_bytes(), KBKDF_KEY_USAGE_LABEL_HMAC).first_n::<48>(),
                    reply_canonical_header.as_bytes(),
                    &reply_buf[HEADER_SIZE..reply_len],
                );
                reply_buf[reply_len..(reply_len + HMAC_SIZE)].copy_from_slice(&hmac);
                reply_len += HMAC_SIZE;

                let key = SessionKey::new(key, Role::Bob, current_time, reply_counter, ratchet_count + 1, e1e1.is_some());

                let mut state = session.state.write().unwrap();
                let _ = state.remote_session_id.replace(alice_session_id);
                let next_key_ptr = (state.key_ptr + 1) % KEY_HISTORY_SIZE;
                let _ = state.keys[next_key_ptr].replace(key);
                drop(state);

                // Bob now has final key state for this exchange. Yay! Now reply to Alice so she can construct it.

                send_with_fragmentation(send, &mut reply_buf[..reply_len], mtu, &session.header_check_cipher);

                if new_session.is_some() {
                    return Ok(ReceiveResult::OkNewSession(new_session.unwrap()));
                } else {
                    return Ok(ReceiveResult::Ok);
                }
            }

            PACKET_TYPE_KEY_COUNTER_OFFER => {
                // bob (remote) -> alice (local)

                if kex_packet_len < (HEADER_SIZE + 1 + P384_PUBLIC_KEY_SIZE + AES_GCM_TAG_SIZE + HMAC_SIZE) {
                    return Err(Error::InvalidPacket);
                }
                let payload_end = kex_packet_len - (AES_GCM_TAG_SIZE + HMAC_SIZE);
                let aes_gcm_tag_end = kex_packet_len - HMAC_SIZE;

                if let Some(session) = session {
                    let state = session.state.read().unwrap();
                    if let Some(offer) = state.offer.as_ref() {
                        let (bob_e0_public, e0e0) =
                        P384PublicKey::from_bytes(&kex_packet[(HEADER_SIZE + 1)..(HEADER_SIZE + 1 + P384_PUBLIC_KEY_SIZE)])
                        .and_then(|pk| offer.alice_e0_keypair.agree(&pk).map(move |s| (pk, s)))
                        .ok_or(Error::FailedAuthentication)?;
                        let se0 = host
                        .get_local_s_keypair_p384()
                        .agree(&bob_e0_public)
                        .ok_or(Error::FailedAuthentication)?;

                        let mut key = Secret(hmac_sha512(
                            session.psk.as_bytes(),
                            &hmac_sha512(
                                &hmac_sha512(&hmac_sha512(offer.key.as_bytes(), bob_e0_public.as_bytes()), e0e0.as_bytes()),
                                se0.as_bytes(),
                            ),
                        ));

                        let mut c = AesGcm::new(
                            kbkdf512(key.as_bytes(), KBKDF_KEY_USAGE_LABEL_AES_GCM_BOB_TO_ALICE).first_n::<AES_KEY_SIZE>(),
                            false,
                        );
                        c.reset_init_gcm(canonical_header_bytes);
                        c.crypt_in_place(&mut kex_packet[(HEADER_SIZE + 1 + P384_PUBLIC_KEY_SIZE)..payload_end]);
                        if !c.finish_decrypt(&kex_packet[payload_end..aes_gcm_tag_end]) {
                            return Err(Error::FailedAuthentication);
                        }

                        // Alice has now completed Noise_IK with NIST P-384 and verified with GCM auth, but now for hybrid...

                        let (offer_id, bob_session_id, _, _, bob_e1_public, bob_ratchet_key_id) = parse_key_offer_after_header(
                            &kex_packet[(HEADER_SIZE + 1 + P384_PUBLIC_KEY_SIZE)..kex_packet_len],
                            packet_type,
                        )?;

                        if !offer.id.eq(&offer_id) {
                            return Ok(ReceiveResult::Ignored);
                        }

                        let e1e1 = if JEDI && bob_e1_public.len() > 0 && offer.alice_e1_keypair.is_some() {
                            if let Ok(e1e1) = pqc_kyber::decapsulate(bob_e1_public, &offer.alice_e1_keypair.as_ref().unwrap().secret) {
                                Some(Secret(e1e1))
                            } else {
                                return Err(Error::FailedAuthentication);
                            }
                        } else {
                            None
                        };

                        let mut ratchet_count = 0;
                        if bob_ratchet_key_id.is_some() && offer.ratchet_key.is_some() {
                            key = Secret(hmac_sha512(offer.ratchet_key.as_ref().unwrap().as_bytes(), key.as_bytes()));
                            ratchet_count = offer.ratchet_count;
                        }
                        if let Some(e1e1) = e1e1.as_ref() {
                            key = Secret(hmac_sha512(e1e1.as_bytes(), key.as_bytes()));
                        }

                        if !hmac_sha384_2(
                            kbkdf512(key.as_bytes(), KBKDF_KEY_USAGE_LABEL_HMAC).first_n::<48>(),
                            canonical_header_bytes,
                            &kex_packet_saved_ciphertext[HEADER_SIZE..aes_gcm_tag_end],
                        )
                        .eq(&kex_packet[aes_gcm_tag_end..kex_packet_len])
                        {
                            return Err(Error::FailedAuthentication);
                        }

                        // Alice has now completed and validated the full hybrid exchange.

                        let counter = session.send_counter.next();
                        let key = SessionKey::new(key, Role::Alice, current_time, counter, ratchet_count + 1, e1e1.is_some());

                        let mut reply_buf = [0_u8; HEADER_SIZE + AES_GCM_TAG_SIZE];
                        create_packet_header(
                            &mut reply_buf,
                            HEADER_SIZE + AES_GCM_TAG_SIZE,
                            mtu,
                            PACKET_TYPE_NOP,
                            bob_session_id.into(),
                            counter,
                        )?;

                        let mut c = key.get_send_cipher(counter)?;
                        c.reset_init_gcm(CanonicalHeader::make(bob_session_id.into(), PACKET_TYPE_NOP, counter.to_u32()).as_bytes());
                        reply_buf[HEADER_SIZE..].copy_from_slice(&c.finish_encrypt());
                        key.return_send_cipher(c);

                        set_header_check_code(&mut reply_buf, &session.header_check_cipher);
                        send(&mut reply_buf);

                        drop(state);
                        let mut state = session.state.write().unwrap();
                        let _ = state.remote_session_id.replace(bob_session_id);
                        let next_key_ptr = (state.key_ptr + 1) % KEY_HISTORY_SIZE;
                        let _ = state.keys[next_key_ptr].replace(key);
                        let _ = state.offer.take();

                        return Ok(ReceiveResult::Ok);
                    }
                }

                // Just ignore counter-offers that are out of place. They probably indicate that this side
                // restarted and needs to establish a new session.
                return Ok(ReceiveResult::Ignored);
            }

            _ => return Err(Error::InvalidPacket),
        }
    }
}


/// Parse KEY_OFFER and KEY_COUNTER_OFFER starting after the unencrypted public key part.
fn parse_key_offer_after_header(
    incoming_packet: &[u8],
    packet_type: u8,
) -> Result<([u8; 16], SessionId, &[u8], &[u8], &[u8], Option<[u8; 16]>), Error> {
    let mut p = &incoming_packet[..];
    let mut offer_id = [0_u8; 16];
    p.read_exact(&mut offer_id)?;
    let alice_session_id = SessionId::new_from_reader(&mut p)?;
    if alice_session_id.is_none() {
        return Err(Error::InvalidPacket);
    }
    let alice_session_id = alice_session_id.unwrap();
    let alice_s_public_len = varint::read(&mut p)?.0;
    if (p.len() as u64) < alice_s_public_len {
        return Err(Error::InvalidPacket);
    }
    let alice_s_public = &p[..(alice_s_public_len as usize)];
    p = &p[(alice_s_public_len as usize)..];
    let alice_metadata_len = varint::read(&mut p)?.0;
    if (p.len() as u64) < alice_metadata_len {
        return Err(Error::InvalidPacket);
    }
    let alice_metadata = &p[..(alice_metadata_len as usize)];
    p = &p[(alice_metadata_len as usize)..];
    if p.is_empty() {
        return Err(Error::InvalidPacket);
    }
    let alice_e1_public = match p[0] {
        E1_TYPE_KYBER1024 => {
            if packet_type == PACKET_TYPE_KEY_OFFER {
                if p.len() < (pqc_kyber::KYBER_PUBLICKEYBYTES + 1) {
                    return Err(Error::InvalidPacket);
                }
                let e1p = &p[1..(pqc_kyber::KYBER_PUBLICKEYBYTES + 1)];
                p = &p[(pqc_kyber::KYBER_PUBLICKEYBYTES + 1)..];
                e1p
            } else {
                if p.len() < (pqc_kyber::KYBER_CIPHERTEXTBYTES + 1) {
                    return Err(Error::InvalidPacket);
                }
                let e1p = &p[1..(pqc_kyber::KYBER_CIPHERTEXTBYTES + 1)];
                p = &p[(pqc_kyber::KYBER_CIPHERTEXTBYTES + 1)..];
                e1p
            }
        }
        _ => &[],
    };
    if p.is_empty() {
        return Err(Error::InvalidPacket);
    }
    let alice_ratchet_key_fingerprint = if p[0] == 0x01 {
        if p.len() < 16 {
            return Err(Error::InvalidPacket);
        }
        Some(p[1..17].try_into().unwrap())
    } else {
        None
    };
    Ok((
        offer_id,
        alice_session_id,
        alice_s_public,
        alice_metadata,
        alice_e1_public,
        alice_ratchet_key_fingerprint,
    ))
}
