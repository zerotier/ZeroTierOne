/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::mem::MaybeUninit;
use std::sync::Arc;
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::time::Duration;

use serde::{Deserialize, Serialize};
use smol::io::{AsyncReadExt, AsyncWriteExt, BufReader, BufWriter};
use smol::lock::Mutex;
use smol::net::TcpStream;

use zerotier_core_crypto::gmac::GMACStream;
use zerotier_core_crypto::hash::SHA384;
use zerotier_core_crypto::kbkdf::zt_kbkdf_hmac_sha384;
use zerotier_core_crypto::p521::{P521KeyPair, P521PublicKey};
use zerotier_core_crypto::secret::Secret;

use crate::{Config, IDENTITY_HASH_SIZE, io_timeout, Store, varint};
use crate::protocol::*;

struct Output {
    stream: BufWriter<TcpStream>,
    gmac: Option<GMACStream>,
}

pub(crate) struct Link<'e, S: Store + 'static> {
    pub connect_time: u64,
    io_timeout: Duration,
    node_secret: &'e P521KeyPair,
    config: &'e Config,
    store: &'e S,
    remote_node_id: parking_lot::Mutex<Option<[u8; 48]>>,
    reader: Mutex<BufReader<TcpStream>>,
    writer: Mutex<Output>,
    keepalive_period: u64,
    last_send_time: AtomicU64,
    max_message_size: usize,
    authenticated: AtomicBool,
}

#[inline(always)]
fn decode_msgpack<'de, T: Deserialize<'de>>(data: &'de [u8]) -> smol::io::Result<T> {
    rmp_serde::from_read_ref(data).map_err(|_| smol::io::Error::new(smol::io::ErrorKind::InvalidData, "invalid msgpack data"))
}

#[inline(always)]
fn next_id_hash_in_slice(bytes: &[u8]) -> smol::io::Result<&[u8; IDENTITY_HASH_SIZE]> {
    if bytes.len() >= IDENTITY_HASH_SIZE {
        Ok(unsafe { &*bytes.as_ptr().cast::<[u8; IDENTITY_HASH_SIZE]>() })
    } else {
        Err(smol::io::Error::new(smol::io::ErrorKind::InvalidData, "invalid identity hash"))
    }
}

impl<'e, S: Store + 'static> Link<'e, S> {
    pub fn new(stream: TcpStream, node_secret: &'e P521KeyPair, config: &'e Config, store: &'e S) -> Self {
        let _ = stream.set_nodelay(false);
        let max_message_size = HELLO_SIZE_MAX.max(config.max_message_size);
        let now_monotonic = store.monotonic_clock();
        Self {
            connect_time: now_monotonic,
            io_timeout: Duration::from_secs(config.io_timeout),
            node_secret,
            config,
            store,
            remote_node_id: parking_lot::Mutex::new(None),
            reader: Mutex::new(BufReader::with_capacity(65536, stream.clone())),
            writer: Mutex::new(Output {
                stream: BufWriter::with_capacity(max_message_size + 16, stream),
                gmac: None
            }),
            keepalive_period: (config.io_timeout * 1000) / 2,
            last_send_time: AtomicU64::new(now_monotonic),
            max_message_size,
            authenticated: AtomicBool::new(false),
        }
    }

    /// Get the remote node ID, which is SHA384(its long-term public keys).
    /// Returns None if the remote node has not yet responded with HelloAck and been verified.
    pub fn remote_node_id(&self) -> Option<[u8; 48]> { self.remote_node_id.lock().clone() }

    pub(crate) async fn do_periodic_tasks(&self, now_monotonic: u64) -> smol::io::Result<()> {
        if now_monotonic.saturating_sub(self.last_send_time.load(Ordering::Relaxed)) >= self.keepalive_period && self.authenticated.load(Ordering::Relaxed) {
            let timeout = Duration::from_secs(1);
            let mut writer = self.writer.lock().await;
            io_timeout(timeout, writer.stream.write_all(&[MESSAGE_TYPE_KEEPALIVE])).await?;
            io_timeout(timeout, writer.stream.flush()).await?;
            self.last_send_time.store(now_monotonic, Ordering::Relaxed);
        }
        Ok(())
    }

    async fn write_message(&self, message_type: u8, message: &[&[u8]]) -> smol::io::Result<()> {
        let mut writer = self.writer.lock().await;
        if writer.gmac.is_some() {
            let mut mac: [u8; 16] = unsafe { MaybeUninit::uninit().assume_init() };
            let mt = [message_type];

            let gmac = writer.gmac.as_mut().unwrap();
            gmac.init_for_next_message();
            gmac.update(&mt);
            let mut total_length = 0_usize;
            for m in message.iter() {
                total_length += (*m).len();
                gmac.update(*m);
            }
            gmac.finish(&mut mac);

            io_timeout(self.io_timeout, writer.stream.write_all(&mt)).await?;
            io_timeout(self.io_timeout, varint::async_write(&mut writer.stream, total_length as u64)).await?;
            for m in message.iter() {
                io_timeout(self.io_timeout, writer.stream.write_all(*m)).await?;
            }
            io_timeout(self.io_timeout, writer.stream.write_all(&mac)).await?;
            io_timeout(self.io_timeout, writer.stream.flush()).await
        } else {
            Err(smol::io::Error::new(smol::io::ErrorKind::InvalidData, "link negotiation is not complete"))
        }
    }

    async fn write_message_msgpack<T: Serialize>(&self, serialize_buf: &mut Vec<u8>, message_type: u8, message: &T) -> smol::io::Result<()> {
        serialize_buf.clear();
        rmp_serde::encode::write(serialize_buf, message).map_err(|_| smol::io::Error::new(smol::io::ErrorKind::InvalidData, "msgpack encode failure"))?;
        self.write_message(message_type, &[serialize_buf.as_slice()]).await
    }

    pub(crate) async fn io_main(&self) -> smol::io::Result<()> {
        // Reader is held here for the duration of the link's I/O loop.
        let mut reader_mg = self.reader.lock().await;
        let reader = &mut *reader_mg;

        let mut read_buf: Vec<u8> = Vec::new();
        read_buf.resize(self.max_message_size, 0);
        let mut tmp_buf: Vec<u8> = Vec::with_capacity(4096);

        // (1) Send Hello and save the nonce and the hash of the raw Hello message for later HelloAck HMAC check.
        let mut outgoing_nonce = [0_u8; HELLO_NONCE_SIZE];
        zerotier_core_crypto::random::fill_bytes_secure(&mut outgoing_nonce);
        let ephemeral_secret = P521KeyPair::generate(true).unwrap();
        let sent_hello_hash = {
            tmp_buf.clear();
            let _ = rmp_serde::encode::write(&mut tmp_buf, &Hello {
                protocol_version: PROTOCOL_VERSION,
                flags: 0,
                clock: self.store.clock(),
                domain: self.config.domain.as_str(),
                nonce: &outgoing_nonce,
                p521_ecdh_ephemeral_key: ephemeral_secret.public_key_bytes(),
                p521_ecdh_node_key: self.node_secret.public_key_bytes(),
            }).unwrap();

            let mut writer = self.writer.lock().await;
            io_timeout(self.io_timeout, varint::async_write(&mut writer.stream, tmp_buf.len() as u64)).await?;
            io_timeout(self.io_timeout, writer.stream.write_all(tmp_buf.as_slice())).await?;
            io_timeout(self.io_timeout, writer.stream.flush()).await?;
            drop(writer);

            SHA384::hash(tmp_buf.as_slice())
        };

        self.last_send_time.store(self.store.monotonic_clock(), Ordering::Relaxed);

        // (2) Read other side's HELLO and send ACK. Also do key agreement, initialize GMAC, etc.
        let message_size = io_timeout(self.io_timeout, varint::async_read(reader)).await? as usize;
        if message_size > HELLO_SIZE_MAX {
            return Err(smol::io::Error::new(smol::io::ErrorKind::InvalidData, "message too large"));
        }
        let (mut gmac_receive, ack_key, remote_node_id) = {
            let hello_buf = &mut read_buf.as_mut_slice()[0..message_size];
            io_timeout(self.io_timeout, reader.read_exact(hello_buf)).await?;
            let received_hello_hash = SHA384::hash(hello_buf); // for ACK generation
            let hello: Hello = decode_msgpack(hello_buf)?;

            if hello.nonce.len() < HELLO_NONCE_SIZE || hello.protocol_version != PROTOCOL_VERSION {
                return Err(smol::io::Error::new(smol::io::ErrorKind::InvalidData, "invalid HELLO parameters"));
            }

            let remote_node_public_key = P521PublicKey::from_bytes(hello.p521_ecdh_node_key);
            let remote_ephemeral_public_key = P521PublicKey::from_bytes(hello.p521_ecdh_ephemeral_key);
            if remote_node_public_key.is_none() || remote_ephemeral_public_key.is_none() {
                return Err(smol::io::Error::new(smol::io::ErrorKind::InvalidData, "invalid public key in HELLO"));
            }
            let node_shared_key = self.node_secret.agree(&remote_node_public_key.unwrap());
            let ephemeral_shared_key = ephemeral_secret.agree(&remote_ephemeral_public_key.unwrap());
            if node_shared_key.is_none() || ephemeral_shared_key.is_none() {
                return Err(smol::io::Error::new(smol::io::ErrorKind::InvalidData, "key agreement failed"));
            }
            let shared_base_key = Secret(SHA384::hmac(&SHA384::hash(ephemeral_shared_key.unwrap().as_bytes()), node_shared_key.unwrap().as_bytes()));

            let shared_gmac_base_key = zt_kbkdf_hmac_sha384(shared_base_key.as_bytes(), KBKDF_LABEL_GMAC, 0, 0);
            let gmac_receive_key = Secret(SHA384::hmac(&hello.nonce[0..48], &shared_gmac_base_key.0));
            let gmac_send_key = Secret(SHA384::hmac(&outgoing_nonce[0..48], &shared_gmac_base_key.0));
            let gmac_receive = GMACStream::new(&gmac_receive_key.0[0..32], &hello.nonce[48..64]);
            self.writer.lock().await.gmac.replace(GMACStream::new(&gmac_send_key.0[0..32], &outgoing_nonce[48..64]));

            let shared_ack_key = zt_kbkdf_hmac_sha384(shared_base_key.as_bytes(), KBKDF_LABEL_HELLO_ACK_HMAC, 0, 0);
            let ack_hmac = SHA384::hmac(shared_ack_key.as_bytes(), &received_hello_hash);
            self.write_message_msgpack(&mut tmp_buf, MESSAGE_TYPE_HELLO_ACK, &HelloAck {
                ack: &ack_hmac,
                clock_echo: hello.clock
            }).await?;

            (gmac_receive, shared_ack_key, SHA384::hash(hello.p521_ecdh_node_key))
        };

        self.last_send_time.store(self.store.monotonic_clock(), Ordering::Relaxed);

        // Done with ephemeral secret key, so forget it.
        drop(ephemeral_secret);

        // (3) Start primary I/O loop and initially listen for HelloAck to confirm the other side's node identity.
        let mut received_mac_buf = [0_u8; 16];
        let mut expected_mac_buf = [0_u8; 16];
        let mut message_type_buf = [0_u8; 1];
        let mut authenticated = false;
        loop {
            io_timeout(self.io_timeout, reader.read_exact(&mut message_type_buf)).await?;
            if message_type_buf[0] != MESSAGE_TYPE_KEEPALIVE {
                let message_size = io_timeout(self.io_timeout, varint::async_read(reader)).await? as usize;
                if message_size > self.max_message_size {
                    return Err(smol::io::Error::new(smol::io::ErrorKind::InvalidData, "message too large"));
                }
                let message_buf = &mut read_buf.as_mut_slice()[0..message_size];
                io_timeout(self.io_timeout, reader.read_exact(message_buf)).await?;
                io_timeout(self.io_timeout, reader.read_exact(&mut received_mac_buf)).await?;

                gmac_receive.init_for_next_message();
                gmac_receive.update(&message_type_buf);
                gmac_receive.update(message_buf);
                gmac_receive.finish(&mut expected_mac_buf);
                if !received_mac_buf.eq(&expected_mac_buf) {
                    return Err(smol::io::Error::new(smol::io::ErrorKind::InvalidData, "message authentication failed"));
                }

                if authenticated {
                    match message_type_buf[0] {
                        MESSAGE_TYPE_OBJECTS => self.do_objects(message_buf).await?,
                        MESSAGE_TYPE_HAVE_OBJECTS => self.do_have_objects(&mut tmp_buf, message_buf).await?,
                        MESSAGE_TYPE_WANT_OBJECTS => self.do_want_objects(message_buf).await?,
                        MESSAGE_TYPE_STATE => self.do_sync_request(decode_msgpack(message_buf)?).await?,
                        MESSAGE_TYPE_IBLT_SYNC_DIGEST => self.do_iblt_sync_digest(decode_msgpack(message_buf)?).await?,
                        _ => {},
                    }
                } else {
                    if message_type_buf[0] == MESSAGE_TYPE_HELLO_ACK {
                        let hello_ack: HelloAck = decode_msgpack(message_buf)?;
                        let expected_ack_hmac = SHA384::hmac(ack_key.as_bytes(), &sent_hello_hash);
                        if !hello_ack.ack.eq(&expected_ack_hmac) {
                            return Err(smol::io::Error::new(smol::io::ErrorKind::InvalidData, "session authentication failed"));
                        }

                        authenticated = true;
                        let _ = self.remote_node_id.lock().replace(remote_node_id.clone());
                        self.authenticated.store(true, Ordering::Relaxed);
                    } else {
                        return Err(smol::io::Error::new(smol::io::ErrorKind::InvalidData, "message received before session authenticated"));
                    }
                }
            }
        }
    }

    async fn do_objects(&self, mut msg: &[u8]) -> smol::io::Result<()> {
        while !msg.is_empty() {
            let obj_size = varint::async_read(&mut msg).await? as usize;
            if obj_size >= msg.len() {
                return Err(smol::io::Error::new(smol::io::ErrorKind::InvalidData, "object incomplete"));
            }
            let obj = &msg[0..obj_size];
            msg = &msg[obj_size..];
            match self.store.put(&SHA384::hash(obj), obj) {
                crate::StorePutResult::Invalid => {
                    return Err(smol::io::Error::new(smol::io::ErrorKind::InvalidData, "invalid object in stream"));
                },
                _ => {}
            }
        }
        Ok(())
    }

    async fn do_have_objects(&self, tmp_buf: &mut Vec<u8>, mut msg: &[u8]) -> smol::io::Result<()> {
        tmp_buf.clear();
        varint::write(tmp_buf, self.store.clock());
        let empty_tmp_buf_size = tmp_buf.len();

        while !msg.is_empty() {
            let id_hash = next_id_hash_in_slice(msg)?;
            msg = &msg[IDENTITY_HASH_SIZE..];
            if !self.store.have(id_hash) {
                let _ = tmp_buf.write_all(id_hash);
            }
        }

        if tmp_buf.len() != empty_tmp_buf_size {
            self.write_message(MESSAGE_TYPE_WANT_OBJECTS, &[tmp_buf.as_slice()]).await
        } else {
            Ok(())
        }
    }

    async fn do_want_objects(&self, mut msg: &[u8]) -> smol::io::Result<()> {
        let ref_time = varint::read(&mut msg);
        if !ref_time.is_err() {
            return Err(smol::io::Error::new(smol::io::ErrorKind::InvalidData, "object incomplete"));
        }
        let ref_time = ref_time.unwrap().0;

        let mut objects: Vec<S::Object> = Vec::with_capacity((msg.len() / IDENTITY_HASH_SIZE) + 1);
        while !msg.is_empty() {
            let id_hash = next_id_hash_in_slice(msg)?;
            msg = &msg[IDENTITY_HASH_SIZE..];
            let _ = self.store.get(ref_time, id_hash).map(|obj| objects.push(obj));
        }

        if !objects.is_empty() {
            let mut sizes: Vec<varint::Encoded> = Vec::with_capacity(objects.len());
            let mut slices: Vec<&[u8]> = Vec::with_capacity(objects.len() * 2);
            for o in objects.iter() {
                sizes.push(varint::Encoded::from(o.as_ref().len() as u64));
            }
            for i in 0..objects.len() {
                slices.push(sizes.get(i).unwrap().as_ref());
                slices.push(objects.get(i).unwrap().as_ref());
            }
            self.write_message(MESSAGE_TYPE_OBJECTS, slices.as_slice()).await
        } else {
            Ok(())
        }
    }

    async fn do_sync_request(&self, sr: State<'_>) -> smol::io::Result<()> {
        Ok(())
    }

    async fn do_iblt_sync_digest(&self, sd: IBLTSyncDigest<'_>) -> smol::io::Result<()> {
        Ok(())
    }
}
