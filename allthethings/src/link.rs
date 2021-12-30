use std::mem::MaybeUninit;
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::time::Duration;

use serde::{Deserialize, Serialize};
use smol::io::{AsyncReadExt, AsyncWriteExt, BufReader, BufWriter};
use smol::lock::Mutex;
use smol::net::{SocketAddr, TcpStream};

use zerotier_core_crypto::gmac::SequentialNonceGMAC;
use zerotier_core_crypto::hash::SHA384;
use zerotier_core_crypto::kbkdf::zt_kbkdf_hmac_sha384;
use zerotier_core_crypto::p521::{P521KeyPair, P521PublicKey};
use zerotier_core_crypto::secret::Secret;

use crate::{Config, io_timeout, ms_monotonic, ms_since_epoch, varint};
use crate::protocol::*;

#[inline(always)]
fn decode_msgpack<'de, T: Deserialize<'de>>(data: &'de [u8]) -> smol::io::Result<T> {
    rmp_serde::from_read_ref(data).map_err(|_| smol::io::Error::new(smol::io::ErrorKind::InvalidData, "invalid msgpack data"))
}

struct OutputStream {
    stream: BufWriter<TcpStream>,
    gmac: Option<SequentialNonceGMAC>,
}

/// A TCP link between this node and another.
pub(crate) struct Link<'a, 'b> {
    node_secret: &'a P521KeyPair,
    config: &'b Config,
    remote_node_id: parking_lot::Mutex<Option<[u8; 48]>>,
    reader: Mutex<BufReader<TcpStream>>,
    writer: Mutex<OutputStream>,
    pub remote_addr: SocketAddr,
    pub connect_time: u64,
    pub authenticated: AtomicBool,
    keepalive_period: u64,
    last_send_time: AtomicU64,
    max_message_size: usize,
}

impl<'a, 'b> Link<'a, 'b> {
    pub fn new(stream: TcpStream, remote_addr: SocketAddr, connect_time: u64, node_secret: &'a P521KeyPair, config: &'b Config) -> Self {
        let _ = stream.set_nodelay(false);
        let max_message_size = HELLO_SIZE_MAX.max(config.max_message_size);
        Self {
            node_secret,
            config,
            remote_node_id: parking_lot::Mutex::new(None),
            reader: Mutex::new(BufReader::with_capacity((max_message_size + 16).max(16384), stream.clone())),
            writer: Mutex::new(OutputStream {
                stream: BufWriter::with_capacity(max_message_size + 16, stream),
                gmac: None
            }),
            remote_addr,
            connect_time,
            authenticated: AtomicBool::new(false),
            keepalive_period: (config.io_timeout * 1000) / 2,
            last_send_time: AtomicU64::new(ms_monotonic()),
            max_message_size
        }
    }

    /// Get the remote node ID, which is SHA384(its long-term public keys).
    /// Returns None if the remote node has not yet responded with HelloAck and been verified.
    pub fn remote_node_id(&self) -> Option<[u8; 48]> { self.remote_node_id.lock().clone() }

    /// Send message and increment outgoing GMAC nonce.
    async fn write_message(&self, timeout: Duration, message_type: u8, message: &[u8]) -> smol::io::Result<()> {
        let mut mac: [u8; 16] = unsafe { MaybeUninit::uninit().assume_init() };
        let mt = [message_type];

        let mut writer = self.writer.lock().await;

        writer.gmac.as_mut().map_or_else(|| {
            Err(smol::io::Error::new(smol::io::ErrorKind::InvalidData, "link negotiation is not complete"))
        }, |gmac| {
            gmac.init_for_next_message();
            gmac.update(&mt);
            gmac.update(message);
            gmac.finish(&mut mac);
            Ok(())
        })?;

        io_timeout(timeout, writer.stream.write_all(&mt)).await?;
        io_timeout(timeout, varint::async_write(&mut writer.stream, message.len() as u64)).await?;
        io_timeout(timeout, writer.stream.write_all(message)).await?;
        io_timeout(timeout, writer.stream.write_all(&mac)).await?;
        io_timeout(timeout, writer.stream.flush()).await
    }

    /// Serialize object with msgpack and send, increment outgoing GMAC nonce.
    async fn write_message_msgpack<T: Serialize>(&self, timeout: Duration, serialize_buf: &mut Vec<u8>, message_type: u8, message: &T) -> smol::io::Result<()> {
        serialize_buf.clear();
        rmp_serde::encode::write(serialize_buf, message).map_err(|_| smol::io::Error::new(smol::io::ErrorKind::InvalidData, "msgpack encode failure"))?;
        self.write_message(timeout, message_type, serialize_buf.as_slice()).await
    }

    /// Send a keepalive if necessary.
    pub async fn send_keepalive_if_needed(&self, now_monotonic: u64) {
        if now_monotonic.saturating_sub(self.last_send_time.load(Ordering::Relaxed)) >= self.keepalive_period && self.authenticated.load(Ordering::Relaxed) {
            self.last_send_time.store(now_monotonic, Ordering::Relaxed);
            let timeout = Duration::from_secs(1);
            let mut writer = self.writer.lock().await;
            io_timeout(timeout, writer.stream.write_all(&[MESSAGE_TYPE_KEEPALIVE])).await;
            io_timeout(timeout, writer.stream.flush()).await;
        }
    }

    /// Launched as an async task for each new link.
    pub async fn io_main(&self) -> smol::io::Result<()> {
        // Reader is held here for the duration of the link's I/O loop.
        let mut reader_mg = self.reader.lock().await;
        let reader = &mut *reader_mg;

        let mut read_buf: Vec<u8> = Vec::new();
        read_buf.resize(self.max_message_size, 0);
        let mut serialize_buf: Vec<u8> = Vec::with_capacity(4096);
        let timeout = Duration::from_secs(self.config.io_timeout);

        // (1) Send Hello and save the nonce and the hash of the raw Hello message for later HelloAck HMAC check.
        let mut gmac_send_nonce_initial = [0_u8; 16];
        zerotier_core_crypto::random::fill_bytes_secure(&mut gmac_send_nonce_initial);
        let ephemeral_secret = P521KeyPair::generate(true).unwrap();
        let sent_hello_hash = {
            serialize_buf.clear();
            let _ = rmp_serde::encode::write(&mut serialize_buf, &Hello {
                protocol_version: PROTOCOL_VERSION,
                flags: 0,
                clock: ms_since_epoch(),
                domain: self.config.domain.as_str(),
                nonce: &gmac_send_nonce_initial,
                p521_ecdh_ephemeral_key: ephemeral_secret.public_key_bytes(),
                p521_ecdh_node_key: self.node_secret.public_key_bytes(),
            }).unwrap();

            let mut writer = self.writer.lock().await;
            io_timeout(timeout, varint::async_write(&mut writer.stream, serialize_buf.len() as u64)).await?;
            io_timeout(timeout, writer.stream.write_all(serialize_buf.as_slice())).await?;
            io_timeout(timeout, writer.stream.flush()).await?;
            drop(writer);

            SHA384::hash(serialize_buf.as_slice())
        };

        // (2) Read other side's HELLO and send ACK. Also do key agreement, initialize GMAC, etc.
        let message_size = io_timeout(timeout, varint::async_read(reader)).await? as usize;
        if message_size > HELLO_SIZE_MAX {
            return Err(smol::io::Error::new(smol::io::ErrorKind::InvalidData, "message too large"));
        }
        let (mut gmac_receive, ack_key, remote_node_id) = {
            let hello_buf = &mut read_buf.as_mut_slice()[0..message_size];
            io_timeout(timeout, reader.read_exact(hello_buf)).await?;
            let received_hello_hash = SHA384::hash(hello_buf); // for ACK generation
            let hello: Hello = decode_msgpack(hello_buf)?;

            if hello.nonce.len() < 16 || hello.protocol_version != PROTOCOL_VERSION {
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
            let shared_key = Secret(SHA384::hmac(&SHA384::hash(ephemeral_shared_key.unwrap().as_bytes()), node_shared_key.unwrap().as_bytes()));

            let gmac_key = zt_kbkdf_hmac_sha384(shared_key.as_bytes(), KBKDF_LABEL_GMAC, 0, 0);
            let ack_key = zt_kbkdf_hmac_sha384(shared_key.as_bytes(), KBKDF_LABEL_HELLO_ACK_HMAC, 0, 0);

            let gmac_receive = SequentialNonceGMAC::new(&gmac_key.0[0..32], &hello.nonce[0..16]);
            self.writer.lock().await.gmac.replace(SequentialNonceGMAC::new(&gmac_key.0[0..32], &gmac_send_nonce_initial));

            let ack_hmac = SHA384::hmac(ack_key.as_bytes(), &received_hello_hash);
            self.write_message_msgpack(timeout, &mut serialize_buf, MESSAGE_TYPE_HELLO_ACK, &HelloAck {
                ack: &ack_hmac,
                clock_echo: hello.clock
            }).await?;

            (gmac_receive, ack_key, SHA384::hash(hello.p521_ecdh_node_key))
        };

        self.last_send_time.store(ms_monotonic(), Ordering::Relaxed);

        // Done with ephemeral secret key, so forget it.
        drop(ephemeral_secret);

        // (3) Start primary I/O loop and initially listen for HelloAck to confirm the other side's node identity.
        let mut received_mac_buf = [0_u8; 16];
        let mut expected_mac_buf = [0_u8; 16];
        let mut message_type_buf = [0_u8; 1];
        let mut authenticated = false;
        loop {
            io_timeout(timeout, reader.read_exact(&mut message_type_buf)).await?;

            // NOP is a single byte keepalive, so skip. Otherwise handle actual messages.
            if message_type_buf[0] != MESSAGE_TYPE_KEEPALIVE {
                let message_size = io_timeout(timeout, varint::async_read(reader)).await? as usize;
                if message_size > self.max_message_size {
                    return Err(smol::io::Error::new(smol::io::ErrorKind::InvalidData, "message too large"));
                }
                let message_buf = &mut read_buf.as_mut_slice()[0..message_size];
                io_timeout(timeout, reader.read_exact(message_buf)).await?;
                io_timeout(timeout, reader.read_exact(&mut received_mac_buf)).await?;

                gmac_receive.init_for_next_message();
                gmac_receive.update(&message_type_buf);
                gmac_receive.update(message_buf);
                gmac_receive.finish(&mut expected_mac_buf);
                if !received_mac_buf.eq(&expected_mac_buf) {
                    return Err(smol::io::Error::new(smol::io::ErrorKind::InvalidData, "message authentication failed"));
                }

                if authenticated {
                    match message_type_buf[0] {
                        MESSAGE_TYPE_HELLO_ACK => {
                            // Multiple HelloAck messages don't make sense.
                        },
                        MESSAGE_TYPE_OBJECTS => {},
                        MESSAGE_TYPE_HAVE_OBJECTS => {},
                        MESSAGE_TYPE_WANT_OBJECTS => {},
                        MESSAGE_TYPE_IBLT_SYNC_REQUEST => {},
                        MESSAGE_TYPE_IBLT_SYNC_DIGEST => {},
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
}
