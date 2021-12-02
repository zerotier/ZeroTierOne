pub mod ext;

extern crate base64;
extern crate openidconnect;
extern crate url;

use std::sync::{Arc, Mutex};
use std::thread::{sleep, spawn, JoinHandle};
use std::time::Duration;

use openidconnect::core::{CoreClient, CoreProviderMetadata, CoreResponseType};
use openidconnect::reqwest::http_client;
use openidconnect::{AuthenticationFlow, PkceCodeVerifier, TokenResponse, OAuth2TokenResponse};
use openidconnect::{AuthorizationCode, ClientId, CsrfToken, IssuerUrl, Nonce, PkceCodeChallenge, RedirectUrl, RequestTokenError, Scope};

use url::Url;
use std::borrow::BorrowMut;

pub struct ZeroIDC {
    inner: Arc<Mutex<Inner>>,
}

struct Inner {
    running: bool,
    network_id: String,
    auth_endpoint: String,
    oidc_thread: Option<JoinHandle<()>>,
    oidc_client: Option<openidconnect::core::CoreClient>,
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

                while (*inner_local.lock().unwrap()).running {
                    println!("tick");
                    sleep(Duration::from_secs(1));
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
        return (*self.inner.lock().unwrap()).network_id.clone()
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
                        println!("token response error");
                        return None;
                    },
                }
            });
            // TODO: do stuff with token response
            if let Some(Some(tok)) = token_response {
                let id_token = tok.id_token().unwrap();
                let claims = (*self.inner.lock().unwrap()).oidc_client.as_ref().map(|c| {

                });
                let access_token = tok.access_token();
                let refresh_token = tok.refresh_token();
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

            println!("URL: {}", auth_url);

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
