pub mod ext;

extern crate base64;
extern crate openidconnect;
extern crate time;
extern crate url;

use std::time::{SystemTime, UNIX_EPOCH, Duration};
use time::{OffsetDateTime, format_description};
use std::sync::{Arc, Mutex};
use std::thread::{sleep, spawn, JoinHandle};
use serde::{Deserialize, Serialize};
use openidconnect::core::{CoreClient, CoreProviderMetadata, CoreResponseType};
use openidconnect::reqwest::http_client;
use openidconnect::{AccessToken, AuthorizationCode, AuthenticationFlow, ClientId, CsrfToken, IssuerUrl, Nonce, OAuth2TokenResponse, PkceCodeChallenge, PkceCodeVerifier, RedirectUrl, RefreshToken, Scope, TokenResponse};
use jsonwebtoken::{dangerous_insecure_decode};

use url::Url;

pub struct ZeroIDC {
    inner: Arc<Mutex<Inner>>,
}

struct Inner {
    running: bool,
    network_id: String,
    auth_endpoint: String,
    oidc_thread: Option<JoinHandle<()>>,
    oidc_client: Option<openidconnect::core::CoreClient>,
    access_token: Option<AccessToken>,
    refresh_token: Option<RefreshToken>,
    exp_time: u64,
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

pub struct AuthInfo {
    url: Url,
    csrf_token: CsrfToken,
    nonce: Nonce,
    pkce_verifier: Option<PkceCodeVerifier>,
}

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
    fn new(
        network_id: &str,
        issuer: &str,
        client_id: &str,
        auth_ep: &str,
        local_web_port: u16,
    ) -> Result<ZeroIDC, String> {
        let idc = ZeroIDC {
            inner: Arc::new(Mutex::new(Inner {
                running: false,
                network_id: network_id.to_string(),
                auth_endpoint: auth_ep.to_string(),
                oidc_thread: None,
                oidc_client: None,
                access_token: None,
                refresh_token: None,
                exp_time: 0,
            })),
        };

        let iss = match IssuerUrl::new(issuer.to_string()) {
            Ok(i) => i,
            Err(e) => {
                println!("Error generating Issuer URL");
                return Err(e.to_string());
            }

        };

        let provider_meta = match CoreProviderMetadata::discover(&iss, http_client) {
            Ok(m) => m,
            Err(e) => {
                println!("Error discovering provider metadata");
                return Err(e.to_string());
            },
        };

        let r = format!("http://localhost:{}/sso", local_web_port);
        let redir_url = match Url::parse(&r) {
            Ok(s) => s,
            Err(e) => {
                println!("Error generating redirect URL");
                return Err(e.to_string());
            }
        };

        let redirect = match RedirectUrl::new(redir_url.to_string()) {
            Ok(s) => s,
            Err(e) => {
                println!("Error generating RedirectURL instance from string: {}", redir_url.to_string());
                return Err(e.to_string());
            }
        };

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

    fn start(&mut self) {
        let local = Arc::clone(&self.inner);

        if !(*local.lock().unwrap()).running {
            let inner_local = Arc::clone(&self.inner);
            (*local.lock().unwrap()).oidc_thread = Some(spawn(move || {
                (*inner_local.lock().unwrap()).running = true;
                let mut running = true;

                while running {
                    let exp = UNIX_EPOCH + Duration::from_secs((*inner_local.lock().unwrap()).exp_time);
                    let now = SystemTime::now();

                    println!("refresh token thread tick, now: {}, exp: {}", systemtime_strftime(now, "[year]-[month]-[day] [hour]:[minute]:[second]"), systemtime_strftime(exp, "[year]-[month]-[day] [hour]:[minute]:[second]"));
                    let refresh_token = (*inner_local.lock().unwrap()).refresh_token.clone();
                    if let Some(refresh_token) =  refresh_token {
                        if now >= (exp - Duration::from_secs(15)) {
                            let token_response = (*inner_local.lock().unwrap()).oidc_client.as_ref().map(|c| {
                                let res = c.exchange_refresh_token(&refresh_token)
                                    .request(http_client);
                                
                                res
                            });

                            if let Some(res) = token_response {
                                match res {
                                    Ok(res) => {
                                        match res.id_token() {
                                            Some(id_token) => {

                                                let params = [("id_token", id_token.to_string()),("state", "refresh".to_string())];
                                                let client = reqwest::blocking::Client::new();
                                                let r = client.post((*inner_local.lock().unwrap()).auth_endpoint.clone())
                                                    .form(&params)
                                                    .send();

                                                match r {
                                                    Ok(r) => {
                                                        if r.status().is_success() {
                                                            println!("hit url: {}", r.url().as_str());
                                                            println!("status: {}", r.status());


                                                            let access_token = res.access_token();
                                                            let at = access_token.secret();
                                                            let exp = dangerous_insecure_decode::<Exp>(&at);
                                                            
                                                            if let Ok(e) = exp {
                                                                (*inner_local.lock().unwrap()).exp_time = e.claims.exp
                                                            }

                                                            (*inner_local.lock().unwrap()).access_token = Some(access_token.clone());
                                                            if let Some(t) = res.refresh_token() {
                                                                println!("New Refresh Token: {}", t.secret());
                                                                (*inner_local.lock().unwrap()).refresh_token = Some(t.clone());
                                                            }
                                                            println!("Central post succeeded");
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
                                                        // (*inner_local.lock().unwrap()).exp_time = 0;
                                                        (*inner_local.lock().unwrap()).running = false;
                                                    }
                                                }
                                            },
                                            None => {
                                                println!("No id token???");
                                            }
                                        }
                                    },
                                    Err(e) => {
                                        println!("Error posting refresh token: {}", e)
                                    }
                                }
                            }  
                        } else {
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

    fn stop(&mut self) {
        let local = self.inner.clone();
        if (*local.lock().unwrap()).running {
            if let Some(u) = (*local.lock().unwrap()).oidc_thread.take() {
                u.join().expect("join failed");
            }
        }
    }

    fn is_running(&mut self) -> bool {
        let local = Arc::clone(&self.inner);

        if (*local.lock().unwrap()).running {
            true
        } else {
            false
        }
    }

    fn get_network_id(&mut self) -> String {
        return (*self.inner.lock().unwrap()).network_id.clone();
    }

    fn get_exp_time(&mut self) -> u64 {
        return (*self.inner.lock().unwrap()).exp_time;
    }

    fn do_token_exchange(&mut self, auth_info: &mut AuthInfo, code: &str) {
        if let Some(verifier) = auth_info.pkce_verifier.take() {
            let token_response = (*self.inner.lock().unwrap()).oidc_client.as_ref().map(|c| {
                let r = c.exchange_code(AuthorizationCode::new(code.to_string()))
                    .set_pkce_verifier(verifier)
                    .request(http_client);
                match r {
                    Ok(res) =>{
                         return Some(res);
                    },
                    Err(e) => {
                        println!("token response error: {}", e.to_string());
                        
                        return None;
                    },
                }
            });
            // TODO: do stuff with token response
            if let Some(Some(tok)) = token_response {
                let id_token = tok.id_token().unwrap();
                println!("ID token: {}", id_token.to_string());

                let split = auth_info.csrf_token.secret().split("_");
                let split = split.collect::<Vec<&str>>();
                
                let params = [("id_token", id_token.to_string()),("state", split[0].to_string())];
                let client = reqwest::blocking::Client::new();
                let res = client.post((*self.inner.lock().unwrap()).auth_endpoint.clone())
                    .form(&params)
                    .send();

                match res {
                    Ok(res) => {
                        println!("hit url: {}", res.url().as_str());
                        println!("Status: {}", res.status());

                        let at = tok.access_token().secret();
                        let exp = dangerous_insecure_decode::<Exp>(&at);
                        if let Ok(e) = exp {
                            (*self.inner.lock().unwrap()).exp_time = e.claims.exp
                        }

                        (*self.inner.lock().unwrap()).access_token = Some(tok.access_token().clone());
                        if let Some(t) = tok.refresh_token() {
                            (*self.inner.lock().unwrap()).refresh_token = Some(t.clone());
                            self.start();
                        }
                    },
                    Err(res) => {
                        println!("hit url: {}", res.url().unwrap().as_str());
                        println!("Status: {}", res.status().unwrap());
                        println!("Post error: {}", res.to_string());
                        (*self.inner.lock().unwrap()).exp_time = 0;
                    }
                }

                let access_token = tok.access_token();
                println!("Access Token: {}", access_token.secret());

                let refresh_token = tok.refresh_token();
                println!("Refresh Token: {}", refresh_token.unwrap().secret());
            }
        } else {
            println!("No pkce verifier!  Can't exchange tokens!!!");
        }
    }

    fn get_auth_info(&mut self, csrf_token: String, nonce: String) -> Option<AuthInfo> {
        let (pkce_challenge, pkce_verifier) = PkceCodeChallenge::new_random_sha256();
        let network_id = self.get_network_id();

        let r = (*self.inner.lock().unwrap()).oidc_client.as_ref().map(|c| {
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
                .add_extra_param("network_id", network_id)
                .url();

            // println!("URL: {}", auth_url);

            return AuthInfo {
                url: auth_url,
                pkce_verifier: Some(pkce_verifier),
                csrf_token,
                nonce,
            };
        });

        r
    }
}
