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

use crate::error::*;

use bytes::Bytes;
use jwt::Token;
use openidconnect::core::{CoreClient, CoreProviderMetadata, CoreResponseType};
use openidconnect::reqwest::http_client;
use openidconnect::{
    AccessToken, AccessTokenHash, AuthenticationFlow, AuthorizationCode, ClientId, CsrfToken,
    IssuerUrl, Nonce, OAuth2TokenResponse, PkceCodeChallenge, PkceCodeVerifier, RedirectUrl,
    RefreshToken, Scope, TokenResponse,
};
use std::error::Error;
use std::str::from_utf8;
use std::sync::{Arc, Mutex};
use std::thread::{sleep, spawn, JoinHandle};
use std::time::{Duration, SystemTime, UNIX_EPOCH};
use time::{format_description, OffsetDateTime};

use url::Url;

#[cfg(any(
    all(target_os = "linux", target_arch = "x86"),
    all(target_os = "linux", target_arch = "x86_64"),
    all(target_os = "linux", target_arch = "aarch64"),
    target_os = "windows",
    target_os = "macos",
))]
pub struct ZeroIDC {
    inner: Arc<Mutex<Inner>>,
}

#[cfg(any(
    all(target_os = "linux", target_arch = "x86"),
    all(target_os = "linux", target_arch = "x86_64"),
    all(target_os = "linux", target_arch = "aarch64"),
    target_os = "windows",
    target_os = "macos",
))]
struct Inner {
    running: bool,
    issuer: String,
    auth_endpoint: String,
    provider: String,
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

fn csrf_func(csrf_token: String) -> Box<dyn Fn() -> CsrfToken> {
    Box::new(move || CsrfToken::new(csrf_token.to_string()))
}

fn nonce_func(nonce: String) -> Box<dyn Fn() -> Nonce> {
    Box::new(move || Nonce::new(nonce.to_string()))
}

#[cfg(debug_assertions)]
fn systemtime_strftime<T>(dt: T, format: &str) -> String
where
    T: Into<OffsetDateTime>,
{
    let f = format_description::parse(format);
    match f {
        Ok(f) => match dt.into().format(&f) {
            Ok(s) => s,
            Err(_e) => "".to_string(),
        },
        Err(_e) => "".to_string(),
    }
}

#[cfg(any(
    all(target_os = "linux", target_arch = "x86"),
    all(target_os = "linux", target_arch = "x86_64"),
    all(target_os = "linux", target_arch = "aarch64"),
    target_os = "windows",
    target_os = "macos",
))]
impl ZeroIDC {
    pub fn new(
        issuer: &str,
        client_id: &str,
        provider: &str,
        auth_ep: &str,
        local_web_port: u16,
    ) -> Result<ZeroIDC, ZeroIDCError> {
        let idc = ZeroIDC {
            inner: Arc::new(Mutex::new(Inner {
                running: false,
                issuer: issuer.to_string(),
                provider: provider.to_string(),
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

        println!(
            "issuer: {}, client_id: {}, auth_endopint: {}, local_web_port: {}",
            issuer, client_id, auth_ep, local_web_port
        );
        let iss = IssuerUrl::new(issuer.to_string())?;

        let provider_meta = CoreProviderMetadata::discover(&iss, http_client)?;

        let r = format!("http://localhost:{}/sso", local_web_port);
        let redir_url = Url::parse(&r)?;

        let redirect = RedirectUrl::new(redir_url.to_string())?;

        idc.inner.lock().unwrap().oidc_client = Some(
            CoreClient::from_provider_metadata(
                provider_meta,
                ClientId::new(client_id.to_string()),
                None,
            )
            .set_redirect_uri(redirect)
            .set_auth_type(openidconnect::AuthType::RequestBody),
        );

        Ok(idc)
    }

    fn kick_refresh_thread(&mut self) {
        let local = Arc::clone(&self.inner);
        local.lock().unwrap().kick = true;
    }

    fn start(&mut self) {
        let local = Arc::clone(&self.inner);

        if !local.lock().unwrap().running {
            let inner_local = Arc::clone(&self.inner);
            local.lock().unwrap().oidc_thread = Some(spawn(move || {
                inner_local.lock().unwrap().running = true;
                let mut running = true;

                // Keep a copy of the initial nonce used to get the tokens
                // Will be needed later when verifying the responses from refresh tokens
                let nonce = inner_local.lock().unwrap().nonce.clone();

                while running {
                    let exp =
                        UNIX_EPOCH + Duration::from_secs(inner_local.lock().unwrap().exp_time);
                    let now = SystemTime::now();

                    #[cfg(debug_assertions)]
                    {
                        println!(
                            "refresh token thread tick, now: {}, exp: {}",
                            systemtime_strftime(
                                now,
                                "[year]-[month]-[day] [hour]:[minute]:[second]"
                            ),
                            systemtime_strftime(
                                exp,
                                "[year]-[month]-[day] [hour]:[minute]:[second]"
                            )
                        );
                    }
                    let refresh_token = inner_local.lock().unwrap().refresh_token.clone();

                    if let Some(refresh_token) = refresh_token {
                        let should_kick = inner_local.lock().unwrap().kick;
                        if now >= (exp - Duration::from_secs(30)) || should_kick {
                            if should_kick {
                                #[cfg(debug_assertions)]
                                {
                                    println!("refresh thread kicked");
                                }
                                inner_local.lock().unwrap().kick = false;
                            }

                            #[cfg(debug_assertions)]
                            {
                                println!("Refresh Token: {}", refresh_token.secret());
                            }

                            let token_response =
                                inner_local.lock().unwrap().oidc_client.as_ref().map(|c| {
                                    let res = c
                                        .exchange_refresh_token(&refresh_token)
                                        .request(http_client);

                                    res
                                });

                            if let Some(res) = token_response {
                                match res {
                                    Ok(res) => {
                                        match res.id_token() {
                                            Some(id_token) => {
                                                let n = match nonce.clone() {
                                                    Some(n) => n.secret().to_string(),
                                                    None => "".to_string(),
                                                };

                                                let params = [
                                                    ("id_token", id_token.to_string()),
                                                    ("state", "refresh".to_string()),
                                                    ("extra_nonce", n),
                                                ];
                                                #[cfg(debug_assertions)]
                                                {
                                                    println!(
                                                        "New ID token: {}",
                                                        id_token.to_string()
                                                    );
                                                }
                                                let client = reqwest::blocking::Client::new();
                                                let r = client
                                                    .post(
                                                        inner_local
                                                            .lock()
                                                            .unwrap()
                                                            .auth_endpoint
                                                            .clone(),
                                                    )
                                                    .form(&params)
                                                    .send();

                                                match r {
                                                    Ok(r) => {
                                                        if r.status().is_success() {
                                                            #[cfg(debug_assertions)]
                                                            {
                                                                println!(
                                                                    "hit url: {}",
                                                                    r.url().as_str()
                                                                );
                                                                println!("status: {}", r.status());
                                                            }

                                                            let access_token = res.access_token();
                                                            let idt = &id_token.to_string();

                                                            let t: Result<
                                                                Token<
                                                                    jwt::Header,
                                                                    jwt::Claims,
                                                                    jwt::Unverified<'_>,
                                                                >,
                                                                jwt::Error,
                                                            > = Token::parse_unverified(idt);

                                                            if let Ok(t) = t {
                                                                let claims =
                                                                    t.claims().registered.clone();
                                                                match claims.expiration {
                                                                    Some(exp) => {
                                                                        println!("exp: {}", exp);
                                                                        inner_local
                                                                            .lock()
                                                                            .unwrap()
                                                                            .exp_time = exp;
                                                                    }
                                                                    None => {
                                                                        panic!("expiration is None.  This shouldn't happen")
                                                                    }
                                                                }
                                                            } else {
                                                                panic!("error parsing claims");
                                                            }

                                                            inner_local
                                                                .lock()
                                                                .unwrap()
                                                                .access_token =
                                                                Some(access_token.clone());
                                                            if let Some(t) = res.refresh_token() {
                                                                // println!("New Refresh Token: {}", t.secret());
                                                                inner_local
                                                                    .lock()
                                                                    .unwrap()
                                                                    .refresh_token =
                                                                    Some(t.clone());
                                                            }
                                                            #[cfg(debug_assertions)]
                                                            {
                                                                println!("Central post succeeded");
                                                            }
                                                        } else {
                                                            println!(
                                                                "Central post failed: {}",
                                                                r.status()
                                                            );
                                                            println!(
                                                                "hit url: {}",
                                                                r.url().as_str()
                                                            );
                                                            println!("Status: {}", r.status());
                                                            if let Ok(body) = r.bytes() {
                                                                if let Ok(body) =
                                                                    std::str::from_utf8(&body)
                                                                {
                                                                    println!("Body: {}", body);
                                                                }
                                                            }

                                                            inner_local.lock().unwrap().exp_time =
                                                                0;
                                                            inner_local.lock().unwrap().running =
                                                                false;
                                                        }
                                                    }
                                                    Err(e) => {
                                                        println!("Central post failed: {}", e);
                                                        println!(
                                                            "hit url: {}",
                                                            e.url().unwrap().as_str()
                                                        );
                                                        println!("Status: {}", e.status().unwrap());
                                                        inner_local.lock().unwrap().exp_time = 0;
                                                        inner_local.lock().unwrap().running = false;
                                                    }
                                                }
                                            }
                                            None => {
                                                println!("no id token?!?");
                                                inner_local.lock().unwrap().exp_time = 0;
                                                inner_local.lock().unwrap().running = false;
                                            }
                                        }
                                    }
                                    Err(e) => {
                                        println!("token error: {}", e);
                                        inner_local.lock().unwrap().exp_time = 0;
                                        inner_local.lock().unwrap().running = false;
                                    }
                                }
                            } else {
                                println!("token response??");
                                inner_local.lock().unwrap().exp_time = 0;
                                inner_local.lock().unwrap().running = false;
                            }
                        } else {
                            #[cfg(debug_assertions)]
                            println!("waiting to refresh");
                        }
                    } else {
                        println!("no refresh token?");
                        inner_local.lock().unwrap().exp_time = 0;
                        inner_local.lock().unwrap().running = false;
                    }

                    sleep(Duration::from_secs(1));
                    {
                        running = inner_local.lock().unwrap().running;
                    }
                }
                // end run loop

                println!("thread done!");
                inner_local.lock().unwrap().running = false;
                println!("set idc thread running flag to false");
            }));
        }
    }

    pub fn stop(&mut self) {
        let local = self.inner.clone();
        if self.is_running() {
            local.lock().unwrap().running = false;
        }
    }

    pub fn is_running(&mut self) -> bool {
        let local = Arc::clone(&self.inner);
        let running = local.lock().unwrap().running;

        running
    }

    pub fn get_exp_time(&mut self) -> u64 {
        return self.inner.lock().unwrap().exp_time;
    }

    pub fn set_nonce_and_csrf(&mut self, csrf_token: String, nonce: String) {
        let local = Arc::clone(&self.inner);
        (*local.lock().expect("can't lock inner"))
            .as_opt()
            .map(|i| {
                if i.running {
                    println!("refresh thread running. not setting new nonce or csrf");
                    return;
                }

                let need_verifier = matches!(i.pkce_verifier, None);

                let csrf_diff = if let Some(csrf) = i.csrf_token.clone() {
                    *csrf.secret() != csrf_token
                } else {
                    false
                };

                let nonce_diff = if let Some(n) = i.nonce.clone() {
                    *n.secret() != nonce
                } else {
                    false
                };

                if need_verifier || csrf_diff || nonce_diff {
                    let (pkce_challenge, pkce_verifier) = PkceCodeChallenge::new_random_sha256();
                    let r = i.oidc_client.as_ref().map(|c| {
                        let mut auth_builder = c
                            .authorize_url(
                                AuthenticationFlow::<CoreResponseType>::AuthorizationCode,
                                csrf_func(csrf_token),
                                nonce_func(nonce),
                            )
                            .set_pkce_challenge(pkce_challenge);
                        match i.provider.as_str() {
                            "auth0" => {
                                auth_builder = auth_builder
                                    .add_scope(Scope::new("profile".to_string()))
                                    .add_scope(Scope::new("email".to_string()))
                                    .add_scope(Scope::new("offline_access".to_string()));
                            }
                            "okta" => {
                                auth_builder = auth_builder
                                    .add_scope(Scope::new("profile".to_string()))
                                    .add_scope(Scope::new("email".to_string()))
                                    .add_scope(Scope::new("groups".to_string()))
                                    .add_scope(Scope::new("offline_access".to_string()));
                            }
                            "keycloak" => {
                                auth_builder = auth_builder
                                    .add_scope(Scope::new("profile".to_string()))
                                    .add_scope(Scope::new("email".to_string()));
                            }
                            "onelogin" => {
                                auth_builder = auth_builder
                                    .add_scope(Scope::new("profile".to_string()))
                                    .add_scope(Scope::new("email".to_string()))
                                    .add_scope(Scope::new("groups".to_string()))
                            }
                            "default" => {
                                auth_builder = auth_builder
                                    .add_scope(Scope::new("profile".to_string()))
                                    .add_scope(Scope::new("email".to_string()))
                                    .add_scope(Scope::new("offline_access".to_string()));
                            }
                            _ => {
                                auth_builder = auth_builder
                                    .add_scope(Scope::new("profile".to_string()))
                                    .add_scope(Scope::new("email".to_string()))
                                    .add_scope(Scope::new("offline_access".to_string()));
                            }
                        }

                        auth_builder.url()
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
        let url = (*self.inner.lock().expect("can't lock inner"))
            .as_opt()
            .map(|i| match i.url.clone() {
                Some(u) => u.to_string(),
                _ => "".to_string(),
            });

        match url {
            Some(url) => url,
            None => "".to_string(),
        }
    }

    pub fn do_token_exchange(&mut self, code: &str) -> Result<String, SSOExchangeError> {
        let local = Arc::clone(&self.inner);
        let mut should_start = false;
        let res = (*local.lock().unwrap()).as_opt().map(|i| {
            if let Some(verifier) = i.pkce_verifier.take() {
                let token_response = i.oidc_client.as_ref().map(|c| {
                    println!("auth code: {}", code);

                    let r = c
                        .exchange_code(AuthorizationCode::new(code.to_string()))
                        .set_pkce_verifier(verifier)
                        .request(http_client);

                    // validate the token hashes
                    match r {
                        Ok(res) => {
                            let n = match i.nonce.clone() {
                                Some(n) => n,
                                None => {
                                    println!("no nonce");
                                    i.running = false;
                                    return None;
                                }
                            };

                            let id = match res.id_token() {
                                Some(t) => t,
                                None => {
                                    println!("no id token");
                                    i.running = false;
                                    return None;
                                }
                            };

                            let claims = match id.claims(&c.id_token_verifier(), &n) {
                                Ok(c) => c,
                                Err(_e) => {
                                    println!("no claims");
                                    i.running = false;
                                    return None;
                                }
                            };

                            let signing_algo = match id.signing_alg() {
                                Ok(s) => s,
                                Err(_) => {
                                    println!("no signing algorithm");
                                    i.running = false;
                                    return None;
                                }
                            };

                            if let Some(expected_hash) = claims.access_token_hash() {
                                let actual_hash = match AccessTokenHash::from_token(
                                    res.access_token(),
                                    &signing_algo,
                                ) {
                                    Ok(h) => h,
                                    Err(e) => {
                                        println!("Error hashing access token: {}", e);
                                        i.running = false;
                                        return None;
                                    }
                                };

                                if actual_hash != *expected_hash {
                                    println!("token hash error");
                                    i.running = false;
                                    return None;
                                }
                            }
                            Some(res)
                        }
                        Err(e) => {
                            println!("token response error: {:?}", e.to_string());
                            println!("\t {:?}", e.source());
                            i.running = false;
                            None
                        }
                    }
                });

                if let Some(Some(tok)) = token_response {
                    let id_token = tok.id_token().unwrap();
                    #[cfg(debug_assertions)]
                    {
                        println!("ID token: {}", id_token.to_string());
                    }

                    let mut split = "".to_string();
                    if let Some(tok) = i.csrf_token.clone() {
                        split = tok.secret().to_owned();
                    }

                    let split = split.split('_').collect::<Vec<&str>>();

                    if split.len() == 2 {
                        let params = [
                            ("id_token", id_token.to_string()),
                            ("state", split[0].to_string()),
                        ];
                        let client = reqwest::blocking::Client::new();
                        let res = client.post(i.auth_endpoint.clone()).form(&params).send();

                        match res {
                            Ok(res) => {
                                if res.status() == 200 {
                                    #[cfg(debug_assertions)]
                                    {
                                        println!("hit url: {}", res.url().as_str());
                                        println!("Status: {}", res.status());
                                    }

                                    let idt = &id_token.to_string();

                                    let t: Result<
                                        Token<jwt::Header, jwt::Claims, jwt::Unverified<'_>>,
                                        jwt::Error,
                                    > = Token::parse_unverified(idt);

                                    if let Ok(t) = t {
                                        let claims = t.claims().registered.clone();
                                        match claims.expiration {
                                            Some(exp) => {
                                                i.exp_time = exp;
                                                println!("Set exp time to: {:?}", i.exp_time);
                                            }
                                            None => {
                                                panic!("expiration is None.  This shouldn't happen");
                                            }
                                        }
                                    } else {
                                        panic!("Failed to parse token");
                                    }

                                    i.access_token = Some(tok.access_token().clone());
                                    if let Some(t) = tok.refresh_token() {
                                        i.refresh_token = Some(t.clone());
                                        should_start = true;
                                    }
                                    #[cfg(debug_assertions)]
                                    {
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

                                    Ok(bytes)
                                } else if res.status() == 402 {
                                    i.running = false;
                                    Err(SSOExchangeError::new(
                                        "additional license seats required. Please contact your network administrator.".to_string(),
                                    ))
                                } else {
                                    i.running = false;
                                    Err(SSOExchangeError::new(
                                        "error from central endpoint".to_string(),
                                    ))
                                }
                            }
                            Err(res) => {
                                println!("error result: {}", res);
                                println!("hit url: {}", i.auth_endpoint.clone());
                                println!("Post error: {}", res);
                                i.exp_time = 0;
                                i.running = false;
                                Err(SSOExchangeError::new(
                                    "error from central endpoint".to_string(),
                                ))
                            }
                        }
                    } else {
                        i.running = false;
                        Err(SSOExchangeError::new(
                            "error splitting state token".to_string(),
                        ))
                    }
                } else {
                    i.running = false;
                    Err(SSOExchangeError::new("invalid token response".to_string()))
                }
            } else {
                i.running = false;
                Err(SSOExchangeError::new("invalid pkce verifier".to_string()))
            }
        });
        if should_start {
            self.start();
        }
        match res {
            Some(res) => res,
            _ => Err(SSOExchangeError::new("invalid result".to_string())),
        }
    }
}
