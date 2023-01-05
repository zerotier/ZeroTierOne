#[allow(unused_imports)]
#[cfg(test)]
mod tests {
    use std::collections::LinkedList;
    use std::sync::{Arc, Mutex};
    use zerotier_crypto::hash::SHA384;
    use zerotier_crypto::p384::{P384KeyPair, P384PublicKey};
    use zerotier_crypto::random;
    use zerotier_crypto::secret::Secret;
    use zerotier_utils::hex;

    use crate::counter::CounterWindow;
    use crate::*;
    use constants::*;

    struct TestHost {
        local_s: P384KeyPair,
        local_s_hash: [u8; 48],
        psk: Secret<64>,
        session: Mutex<Option<Arc<Session<TestHost>>>>,
        session_id_counter: Mutex<u64>,
        queue: Mutex<LinkedList<Vec<u8>>>,
        key_id: Mutex<[u8; 16]>,
        this_name: &'static str,
        other_name: &'static str,
    }

    impl TestHost {
        fn new(psk: Secret<64>, this_name: &'static str, other_name: &'static str) -> Self {
            let local_s = P384KeyPair::generate();
            let local_s_hash = SHA384::hash(local_s.public_key_bytes());
            Self {
                local_s,
                local_s_hash,
                psk,
                session: Mutex::new(None),
                session_id_counter: Mutex::new(1),
                queue: Mutex::new(LinkedList::new()),
                key_id: Mutex::new([0; 16]),
                this_name,
                other_name,
            }
        }
    }

    impl ApplicationLayer for TestHost {
        type Data = u32;
        type SessionRef<'a> = Arc<Session<TestHost>>;
        type IncomingPacketBuffer = Vec<u8>;
        type RemoteAddress = u32;

        const REKEY_RATE_LIMIT_MS: i64 = 0;

        fn get_local_s_public_blob(&self) -> &[u8] {
            self.local_s.public_key_bytes()
        }

        fn get_local_s_public_blob_hash(&self) -> &[u8; 48] {
            &self.local_s_hash
        }

        fn get_local_s_keypair(&self) -> &P384KeyPair {
            &self.local_s
        }

        fn extract_s_public_from_raw(static_public: &[u8]) -> Option<P384PublicKey> {
            P384PublicKey::from_bytes(static_public)
        }

        fn lookup_session<'a>(&self, local_session_id: SessionId) -> Option<Self::SessionRef<'a>> {
            self.session.lock().unwrap().as_ref().and_then(|s| {
                if s.id == local_session_id {
                    Some(s.clone())
                } else {
                    None
                }
            })
        }

        fn check_new_session(&self, _: &ReceiveContext<Self>, _: &Self::RemoteAddress) -> bool {
            true
        }

        fn accept_new_session(&self, _: &ReceiveContext<Self>, _: &u32, _: &[u8], _: &[u8]) -> Option<(SessionId, Secret<64>, Self::Data)> {
            loop {
                let mut new_id = self.session_id_counter.lock().unwrap();
                *new_id += 1;
                return Some((SessionId::new_from_u64(*new_id).unwrap(), self.psk.clone(), 0));
            }
        }
    }

    #[allow(unused_variables)]
    #[test]
    fn establish_session() {
        let mut data_buf = [0_u8; (1280 - 32) * MAX_FRAGMENTS];
        let mut mtu_buffer = [0_u8; 1280];
        let mut psk: Secret<64> = Secret::default();
        random::fill_bytes_secure(&mut psk.0);

        let alice_host = TestHost::new(psk.clone(), "alice", "bob");
        let bob_host = TestHost::new(psk.clone(), "bob", "alice");
        let alice_rc: ReceiveContext<TestHost> = ReceiveContext::new(&alice_host);
        let bob_rc: ReceiveContext<TestHost> = ReceiveContext::new(&bob_host);

        //println!("zssp: size of session (bytes): {}", std::mem::size_of::<Session<Box<TestHost>>>());

        let _ = alice_host.session.lock().unwrap().insert(Arc::new(
            Session::start_new(
                &alice_host,
                |data| bob_host.queue.lock().unwrap().push_front(data.to_vec()),
                SessionId::new_random(),
                bob_host.local_s.public_key_bytes(),
                &[],
                &psk,
                1,
                mtu_buffer.len(),
                1,
            )
            .unwrap(),
        ));

        let mut ts = 0;
        for test_loop in 0..256 {
            for host in [&alice_host, &bob_host] {
                let send_to_other = |data: &mut [u8]| {
                    if std::ptr::eq(host, &alice_host) {
                        bob_host.queue.lock().unwrap().push_front(data.to_vec());
                    } else {
                        alice_host.queue.lock().unwrap().push_front(data.to_vec());
                    }
                };

                let rc = if std::ptr::eq(host, &alice_host) {
                    &alice_rc
                } else {
                    &bob_rc
                };

                loop {
                    if let Some(qi) = host.queue.lock().unwrap().pop_back() {
                        let qi_len = qi.len();
                        ts += 1;
                        let r = rc.receive(host, &0, send_to_other, &mut data_buf, qi, mtu_buffer.len(), ts);
                        if r.is_ok() {
                            let r = r.unwrap();
                            match r {
                                ReceiveResult::Ok => {
                                    //println!("zssp: {} => {} ({}): Ok", host.other_name, host.this_name, qi_len);
                                }
                                ReceiveResult::OkData(data) => {
                                    //println!("zssp: {} => {} ({}): OkData length=={}", host.other_name, host.this_name, qi_len, data.len());
                                    assert!(!data.iter().any(|x| *x != 0x12));
                                }
                                ReceiveResult::OkNewSession(new_session) => {
                                    println!(
                                        "zssp: {} => {} ({}): OkNewSession ({})",
                                        host.other_name,
                                        host.this_name,
                                        qi_len,
                                        u64::from(new_session.id)
                                    );
                                    let mut hs = host.session.lock().unwrap();
                                    assert!(hs.is_none());
                                    let _ = hs.insert(Arc::new(new_session));
                                }
                                ReceiveResult::Ignored => {
                                    println!("zssp: {} => {} ({}): Ignored", host.other_name, host.this_name, qi_len);
                                }
                            }
                        } else {
                            println!(
                                "zssp: {} => {} ({}): error: {}",
                                host.other_name,
                                host.this_name,
                                qi_len,
                                r.err().unwrap().to_string()
                            );
                            panic!();
                        }
                    } else {
                        break;
                    }
                }

                data_buf.fill(0x12);
                if let Some(session) = host.session.lock().unwrap().as_ref().cloned() {
                    if session.established() {
                        {
                            let mut key_id = host.key_id.lock().unwrap();
                            let security_info = session.status().unwrap();
                            if !security_info.0.eq(key_id.as_ref()) {
                                *key_id = security_info.0;
                                println!(
                                    "zssp: new key at {}: fingerprint {} ratchet {} kyber {}",
                                    host.this_name,
                                    hex::to_string(key_id.as_ref()),
                                    security_info.2,
                                    security_info.3
                                );
                            }
                        }
                        for _ in 0..4 {
                            assert!(session
                                .send(
                                    send_to_other,
                                    &mut mtu_buffer,
                                    &data_buf[..((random::xorshift64_random() as usize) % data_buf.len())]
                                )
                                .is_ok());
                        }
                        if (test_loop % 8) == 0 && test_loop >= 8 {
                            session.service(host, send_to_other, &[], mtu_buffer.len(), test_loop as i64, true);
                        }
                    }
                }
            }
        }
    }


    #[inline(always)]
    pub fn xorshift64(x: &mut u64) -> u32 {
        *x ^= x.wrapping_shl(13);
        *x ^= x.wrapping_shr(7);
        *x ^= x.wrapping_shl(17);
        *x as u32
    }
    #[test]
    fn counter_window() {
        let mut rng = 844632;
        let mut counter = 1u32;
        let mut history = Vec::new();

        let w = CounterWindow::new();
        for _i in 0..1000000 {
            let p = xorshift64(&mut rng) as f32/(u32::MAX as f32 + 1.0);
            let c;
            if p < 0.5 {
                let r = xorshift64(&mut rng);
                c = counter + (r%(COUNTER_MAX_ALLOWED_OOO - 1) as u32 + 1);
            } else if p < 0.8 {
                counter = counter + (1);
                c = counter;
            } else if p < 0.9 {
                if history.len() > 0 {
                    let idx = xorshift64(&mut rng) as usize%history.len();
                    let c = history[idx];
                    assert!(!w.message_authenticated(c));
                }
                continue;
            } else if p < 0.999 {
                c = xorshift64(&mut rng);
                w.message_received(c);
                continue;
            } else {
                w.reset_for_new_key_offer();
                counter = 1u32;
                history = Vec::new();
                continue;
            }
            if history.contains(&c) {
                assert!(!w.message_authenticated(c));
            } else {
                assert!(w.message_authenticated(c));
                history.push(c);
            }
        }
    }
}
