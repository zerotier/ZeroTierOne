pub mod ext;

extern crate base64;
extern crate openidconnect;
extern crate url;

use std::sync::{Arc, Mutex};
use std::thread::{sleep, spawn, JoinHandle};
use std::time::Duration;

use openidconnect::core::{CoreClient, CoreProviderMetadata, CoreResponseType};
use openidconnect::reqwest::http_client;
use openidconnect::{AuthenticationFlow, PkceCodeVerifier};
use openidconnect::{ClientId, CsrfToken, IssuerUrl, Nonce, PkceCodeChallenge, RedirectUrl, Scope};

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
    pkce_verifier: PkceCodeVerifier,
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
            Err(e) => return Err(e.to_string()),
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
            Err(e) => return Err(e.to_string()),
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
                .add_scope(Scope::new("read".to_string()))
                .add_scope(Scope::new("offline_access".to_string()))
                .add_scope(Scope::new("openid".to_string()))
                .set_pkce_challenge(pkce_challenge)
                .add_extra_param("network_id", network_id)
                .url();

            return AuthInfo {
                url: auth_url,
                csrf_token,
                nonce,
                pkce_verifier,
            };
        });

        r
    }
}
