/*
 * Copyright (c)2021 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */

pub mod error;
pub mod ext;

extern crate base64;
extern crate bytes;
extern crate openidconnect;
extern crate time;
extern crate url;

use crate::error::ZeroIDCError;

use bytes::Bytes;
use jwt::{Token};
use openidconnect::core::{CoreClient, CoreProviderMetadata, CoreResponseType};
use openidconnect::reqwest::http_client;
use openidconnect::{AccessToken, AccessTokenHash, AuthorizationCode, AuthenticationFlow, ClientId, CsrfToken, IssuerUrl, Nonce, OAuth2TokenResponse, PkceCodeChallenge, PkceCodeVerifier, RedirectUrl, RefreshToken, Scope, TokenResponse};
use serde::{Deserialize, Serialize};
use std::str::from_utf8;
use std::sync::{Arc, Mutex};
use std::thread::{sleep, spawn, JoinHandle};
use std::time::{SystemTime, UNIX_EPOCH, Duration};
use time::{OffsetDateTime, format_description};


use url::Url;

pub struct ZeroIDC {
    inner: Arc<Mutex<Inner>>,
}

struct Inner {
    running: bool,
    auth_endpoint: String,
    oidc_thread: Option<JoinHandle<()>>,
    oidc_client: Option<openidconnect::core::CoreClient>,
    access_token: Option<AccessToken>,
    refresh_token: Option<RefreshToken>,
    exp_time: u64,
    kick: bool,

    url: Option<Url>,
    csrf_token: Option<CsrfToken>,
    nonce: Option<Nonce>,
    pkce_verifier: Option<PkceCodeVerifier>,
}

impl Inner {
    #[inline]
    fn as_opt(&mut self) -> Option<&mut Inner> {
        Some(self)
    }
}

#[derive(Debug, Serialize, Deserialize)]
struct Exp {
    exp: u64
}

fn csrf_func(csrf_token: String) -> Box<dyn Fn() -> CsrfToken> {
    return Box::new(move || CsrfToken::new(csrf_token.to_string()));
}

fn nonce_func(nonce: String) -> Box<dyn Fn() -> Nonce> {
    return Box::new(move || Nonce::new(nonce.to_string()));
}

#[cfg(debug_assertions)]
fn systemtime_strftime<T>(dt: T, format: &str) -> String
   where T: Into<OffsetDateTime>
{
    let f = format_description::parse(format);
    match f {
        Ok(f) => {
            match dt.into().format(&f) {
                Ok(s) => s,
                Err(_e) => "".to_string(),
            }
        },
        Err(_e) => {
            "".to_string()
        },
    }
}

impl ZeroIDC {
    pub fn new(
        issuer: &str,
        client_id: &str,
        auth_ep: &str,
        local_web_port: u16,
    ) -> Result<ZeroIDC, ZeroIDCError> {
        let idc = ZeroIDC {
            inner: Arc::new(Mutex::new(Inner {
                running: false,
                auth_endpoint: auth_ep.to_string(),
                oidc_thread: None,
                oidc_client: None,
                access_token: None,
                refresh_token: None,
                exp_time: 0,
                kick: false,

                url: None,
                csrf_token: None,
                nonce: None,
                pkce_verifier: None, 
            })),
        };

        let iss = IssuerUrl::new(issuer.to_string())?;

        let provider_meta = CoreProviderMetadata::discover(&iss, http_client)?;

        let r = format!("http://localhost:{}/sso", local_web_port);
        let redir_url = Url::parse(&r)?;

        let redirect = RedirectUrl::new(redir_url.to_string())?;

        (*idc.inner.lock().unwrap()).oidc_client = Some(
            CoreClient::from_provider_metadata(
                provider_meta,
                ClientId::new(client_id.to_string()),
                None,
            )
            .set_redirect_uri(redirect),
        );

        Ok(idc)
    }

    fn kick_refresh_thread(&mut self) {
        let local = Arc::clone(&self.inner);
        (*local.lock().unwrap()).kick = true;
    }

    fn start(&mut self) {
        let local = Arc::clone(&self.inner);

        if !(*local.lock().unwrap()).running {
            let inner_local = Arc::clone(&self.inner);
            (*local.lock().unwrap()).oidc_thread = Some(spawn(move || {
                (*inner_local.lock().unwrap()).running = true;
                let mut running = true;

                // Keep a copy of the initial nonce used to get the tokens
                // Will be needed later when verifying the responses from refresh tokens
                let nonce = (*inner_local.lock().unwrap()).nonce.clone();

                while running {
                    let exp = UNIX_EPOCH + Duration::from_secs((*inner_local.lock().unwrap()).exp_time);
                    let now = SystemTime::now();

                    #[cfg(debug_assertions)] {
                        println!("refresh token thread tick, now: {}, exp: {}", systemtime_strftime(now, "[year]-[month]-[day] [hour]:[minute]:[second]"), systemtime_strftime(exp, "[year]-[month]-[day] [hour]:[minute]:[second]"));
                    }
                    let refresh_token = (*inner_local.lock().unwrap()).refresh_token.clone();
                    if let Some(refresh_token) =  refresh_token {
                        let should_kick = (*inner_local.lock().unwrap()).kick;
                        if now >= (exp - Duration::from_secs(30)) || should_kick {
                            if should_kick {
                                #[cfg(debug_assertions)] {
                                    println!("refresh thread kicked");
                                }
                                (*inner_local.lock().unwrap()).kick = false;
                            }

                            let token_response = (*inner_local.lock().unwrap()).oidc_client.as_ref().map(|c| {
                                let res = c.exchange_refresh_token(&refresh_token)
                                    .request(http_client);
                                
                                res
                            });
                            
                            if let Some(res) = token_response {
                                match res {
                                    Ok(res) => {

                                        let n = match nonce.clone() {
                                            Some(n) => n,
                                            None => {
                                                println!("err: no nonce");
                                                continue;
                                            }
                                        };
                                        
                                        let id = match res.id_token() {
                                            Some(t) => t,
                                            None => {
                                                println!("err: no id_token");
                                                continue;
                                            }
                                        };
            
                                        // verify & validate claims
                                        let verified = (*inner_local.lock().unwrap()).oidc_client.as_ref().map(|c| {
                                            let claims = match id.claims(&c.id_token_verifier(), &n) {
                                                Ok(c) => c,
                                                Err(e) => {
                                                    println!("claims err: {}", e);
                                                    return false;
                                                }
                                            };
                
                                            let signing_algo = match id.signing_alg() {
                                                Ok(s) => s,
                                                Err(e) => {
                                                    println!("alg err: {}", e);
                                                    return false;
                                                }
                                            };
                
                                            if let Some(expected_hash) = claims.access_token_hash() {
                                                let actual_hash = match AccessTokenHash::from_token(res.access_token(), &signing_algo) {
                                                    Ok(h) => h,
                                                    Err(e) => {
                                                        println!("Error hashing access token: {}", e);
                                                        return false;
                                                    }
                                                };
                
                                                if actual_hash != *expected_hash {
                                                    println!("token hash error");
                                                    return false;
                                                }
                                            }
                                            return true;
                                        });
                                        
                                        let v = match verified {
                                            Some(verified) => {
                                                if !verified {
                                                    println!("not verified.");
                                                    (*inner_local.lock().unwrap()).running = false;
                                                    false
                                                } else {
                                                    true
                                                }
                                            },
                                            None => {
                                                println!("no verification performed?");
                                                (*inner_local.lock().unwrap()).running = false;
                                                false
                                            }
                                        };
                                        
                                        if v {
                                            match res.id_token() {
                                                Some(id_token) => {
                                                    let params = [("id_token", id_token.to_string()),("state", "refresh".to_string())];
                                                    #[cfg(debug_assertions)] {
                                                        println!("New ID token: {}", id_token.to_string());
                                                    }
                                                    let client = reqwest::blocking::Client::new();
                                                    let r = client.post((*inner_local.lock().unwrap()).auth_endpoint.clone())
                                                        .form(&params)
                                                        .send();

                                                    match r {
                                                        Ok(r) => {
                                                            if r.status().is_success() {
                                                                #[cfg(debug_assertions)] {
                                                                    println!("hit url: {}", r.url().as_str());
                                                                    println!("status: {}", r.status());
                                                                }

                                                                let access_token = res.access_token();
                                                                let at = access_token.secret();

                                                                let t: Result<Token<jwt::Header, jwt::Claims, jwt::Unverified<'_>>, jwt::Error>= Token::parse_unverified(at);
                                                                
                                                                if let Ok(t) = t {
                                                                    let claims = t.claims().registered.clone();
                                                                    match claims.expiration {
                                                                        Some(exp) => {
                                                                            (*inner_local.lock().unwrap()).exp_time = exp;
                                                                        },
                                                                        None => {
                                                                            panic!("expiration is None.  This shouldn't happen")
                                                                        }
                                                                    }
                                                                }  

                                                                (*inner_local.lock().unwrap()).access_token = Some(access_token.clone());
                                                                if let Some(t) = res.refresh_token() {
                                                                    // println!("New Refresh Token: {}", t.secret());
                                                                    (*inner_local.lock().unwrap()).refresh_token = Some(t.clone());
                                                                }
                                                                #[cfg(debug_assertions)] {
                                                                    println!("Central post succeeded");
                                                                }
                                                            } else {
                                                                println!("Central post failed: {}", r.status().to_string());
                                                                println!("hit url: {}", r.url().as_str());
                                                                println!("Status: {}", r.status());
                                                                (*inner_local.lock().unwrap()).exp_time = 0;
                                                                (*inner_local.lock().unwrap()).running = false;
                                                            }
                                                        },
                                                        Err(e) => {
                                                            
                                                            println!("Central post failed: {}", e.to_string());
                                                            println!("hit url: {}", e.url().unwrap().as_str());
                                                            println!("Status: {}", e.status().unwrap());
                                                            (*inner_local.lock().unwrap()).exp_time = 0;
                                                            (*inner_local.lock().unwrap()).running = false;
                                                        }
                                                    }
                                                },
                                                None => {
                                                    println!("no id token?!?");
                                                }
                                            }
                                        } else {
                                            println!("claims not verified");
                                        }
                                    },
                                    Err(e) => {
                                        println!("token error: {}", e);
                                    }
                                }
                            } else {
                                println!("token response??");
                            }
                        } else {
                            #[cfg(debug_assertions)]
                            println!("waiting to refresh");
                        }
                    } else {
                        println!("no refresh token?");
                    }

                    sleep(Duration::from_secs(1));
                    running = (*inner_local.lock().unwrap()).running;
                }

                println!("thread done!")
            }));
        }
    }

    pub fn stop(&mut self) {
        let local = self.inner.clone();
        if (*local.lock().unwrap()).running {
            if let Some(u) = (*local.lock().unwrap()).oidc_thread.take() {
                u.join().expect("join failed");
            }
        }
    }

    pub fn is_running(&mut self) -> bool {
        let local = Arc::clone(&self.inner);

        if (*local.lock().unwrap()).running {
            true
        } else {
            false
        }
    }

    pub fn get_exp_time(&mut self) -> u64 {
        return (*self.inner.lock().unwrap()).exp_time;
    }

    pub fn set_nonce_and_csrf(&mut self, csrf_token: String, nonce: String) {
        let local = Arc::clone(&self.inner);
        (*local.lock().expect("can't lock inner")).as_opt().map(|i| {
            if i.running {
                println!("refresh thread running. not setting new nonce or csrf");
                return
            }

            let need_verifier = match i.pkce_verifier {
                None => true,
                _ => false,
            };

            let csrf_diff = if let Some(csrf) = i.csrf_token.clone() {
                if *csrf.secret() != csrf_token {
                    true    
                } else {
                    false
                }
            } else {
                false
            };

            let nonce_diff = if let Some(n) = i.nonce.clone() {
                if *n.secret() != nonce {
                    true
                } else {
                    false
                }
            } else {
                false
            };

            if need_verifier || csrf_diff || nonce_diff {
                let (pkce_challenge, pkce_verifier) = PkceCodeChallenge::new_random_sha256();
                let r = i.oidc_client.as_ref().map(|c| {
                    let (auth_url, csrf_token, nonce) = c
                    .authorize_url(
                        AuthenticationFlow::<CoreResponseType>::AuthorizationCode,
                        csrf_func(csrf_token),
                        nonce_func(nonce),
                    )
                    .add_scope(Scope::new("profile".to_string()))
                    .add_scope(Scope::new("email".to_string()))
                    .add_scope(Scope::new("offline_access".to_string()))
                    .add_scope(Scope::new("openid".to_string()))
                    .set_pkce_challenge(pkce_challenge)
                    .url();

                    (auth_url, csrf_token, nonce)
                });

                if let Some(r) = r {
                    i.url = Some(r.0);
                    i.csrf_token = Some(r.1);
                    i.nonce = Some(r.2);
                    i.pkce_verifier = Some(pkce_verifier);
                }
            }
        });
    }

    pub fn auth_url(&self) -> String {
        let url = (*self.inner.lock().expect("can't lock inner")).as_opt().map(|i| {
            match i.url.clone() {
                Some(u) => u.to_string(),
                _ => "".to_string(),
            }
        });

        match url {
            Some(url) => url.to_string(),
            None => "".to_string(),
        }
    }

    pub fn do_token_exchange(&mut self, code: &str) -> String {
        let local = Arc::clone(&self.inner);
        let mut should_start = false;
        let res = (*local.lock().unwrap()).as_opt().map(|i| {
            if let Some(verifier) = i.pkce_verifier.take() {
                let token_response = i.oidc_client.as_ref().map(|c| {
                    let r = c.exchange_code(AuthorizationCode::new(code.to_string()))
                        .set_pkce_verifier(verifier)
                        .request(http_client);

                    // validate the token hashes
                    match r {
                        Ok(res) =>{
                            let n = match i.nonce.clone() {
                                Some(n) => n,
                                None => {
                                    return None;
                                }
                            };
                            
                            let id = match res.id_token() {
                                Some(t) => t,
                                None => {
                                    return None;
                                }
                            };

                            let claims = match id.claims(&c.id_token_verifier(), &n) {
                                Ok(c) => c,
                                Err(_e) => {
                                    return None;
                                }
                            };

                            let signing_algo = match id.signing_alg() {
                                Ok(s) => s,
                                Err(_) => {
                                    return None;
                                }
                            };

                            if let Some(expected_hash) = claims.access_token_hash() {
                                let actual_hash = match AccessTokenHash::from_token(res.access_token(), &signing_algo) {
                                    Ok(h) => h,
                                    Err(e) => {
                                        println!("Error hashing access token: {}", e);
                                        return None;
                                    }
                                };

                                if actual_hash != *expected_hash {
                                    println!("token hash error");
                                    return None;
                                }
                            }
                            Some(res)
                        },
                        Err(_e) => {
                            #[cfg(debug_assertions)] {
                                println!("token response error: {}", _e.to_string());
                            }

                            return None;
                        },
                    }
                });
                
                if let Some(Some(tok)) = token_response {
                    let id_token = tok.id_token().unwrap();
                    #[cfg(debug_assertions)] {
                        println!("ID token: {}", id_token.to_string());
                    }

                    let mut split = "".to_string();
                    match i.csrf_token.clone() {
                        Some(csrf_token) => {
                            split = csrf_token.secret().to_owned();
                        },
                        _ => (),
                    }

                    let split = split.split("_").collect::<Vec<&str>>();
                    
                    if split.len() == 2 {
                        let params = [("id_token", id_token.to_string()),("state", split[0].to_string())];
                        let client = reqwest::blocking::Client::new();
                        let res = client.post(i.auth_endpoint.clone())
                            .form(&params)
                            .send();

                        match res {
                            Ok(res) => {
                                #[cfg(debug_assertions)] {
                                    println!("hit url: {}", res.url().as_str());
                                    println!("Status: {}", res.status());
                                }

                                let at = tok.access_token().secret();

                                // see previous note about this function's use
                                let t: Result<Token<jwt::Header, jwt::Claims, jwt::Unverified<'_>>, jwt::Error>= Token::parse_unverified(at);
                                                                
                                if let Ok(t) = t {
                                    let claims = t.claims().registered.clone();
                                    match claims.expiration {
                                        Some(exp) => {
                                            i.exp_time = exp;
                                        },
                                        None => {
                                            panic!("expiration is None.  This shouldn't happen")
                                        }
                                    }
                                } 

                                i.access_token = Some(tok.access_token().clone());
                                if let Some(t) = tok.refresh_token() {
                                    i.refresh_token = Some(t.clone());
                                    should_start = true;
                                }
                                #[cfg(debug_assertions)] {
                                    let access_token = tok.access_token();
                                    println!("Access Token: {}", access_token.secret());

                                    let refresh_token = tok.refresh_token();
                                    println!("Refresh Token: {}", refresh_token.unwrap().secret());
                                }
                        
                                let bytes = match res.bytes() {
                                    Ok(bytes) => bytes,
                                    Err(_) => Bytes::from(""),
                                };

                                let bytes = match from_utf8(bytes.as_ref()) {
                                    Ok(bytes) => bytes.to_string(),
                                    Err(_) => "".to_string(),
                                };

                                return bytes;
                            },
                            Err(res) => {
                                println!("hit url: {}", res.url().unwrap().as_str());
                                println!("Status: {}", res.status().unwrap());
                                println!("Post error: {}", res.to_string());
                                i.exp_time = 0;
                            }
                        }

                        
                    } else {
                        println!("invalid split length?!?");
                    }
                }
            }
            "".to_string()
        });
        if should_start {
            self.start();
        }
        return match res {
            Some(res) => res,
            _ => "".to_string(),
        };
    }
}

