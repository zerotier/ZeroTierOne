extern crate base64;
extern crate openidconnect;
extern crate url;

use std::sync::{Arc, Mutex};
use std::thread::{sleep, spawn, JoinHandle};
use std::time::Duration;

use openidconnect::core::{CoreClient, CoreProviderMetadata, CoreResponseType};
use openidconnect::reqwest::http_client;
use openidconnect::AuthenticationFlow;
use openidconnect::{ClientId, CsrfToken, IssuerUrl, Nonce, PkceCodeChallenge, RedirectUrl, Scope};

use url::Url;

use std::ffi::CStr;
use std::os::raw::c_char;

pub struct ZeroIDC {
    inner: Arc<Mutex<Inner>>,
}

struct Inner {
    running: bool,
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

struct authres {
    url: Url,
    csrf_token: CsrfToken,
    nonce: Nonce,
}

impl ZeroIDC {
    fn new(
        issuer: &str,
        client_id: &str,
        auth_ep: &str,
        local_web_port: u16,
    ) -> Result<ZeroIDC, String> {
        let idc = ZeroIDC {
            inner: Arc::new(Mutex::new(Inner {
                running: false,
                auth_endpoint: auth_ep.to_string(),
                oidc_thread: None,
                oidc_client: None,
            })),
        };

        let iss = match IssuerUrl::new(issuer.to_string()) {
            Ok(i) => i,
            Err(e) => return Err(e.to_string()),
        };

        let provider_meta = match CoreProviderMetadata::discover(&iss, http_client) {
            Ok(m) => m,
            Err(e) => return Err(e.to_string()),
        };

        let r = format!("http://localhost:{}/sso", local_web_port);
        let redir_url = match Url::parse(&r) {
            Ok(s) => s,
            Err(e) => return Err(e.to_string()),
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

    fn get_auth_url(&mut self) -> Option<authres> {
        let (pkce_challenge, pkce_verifier) = PkceCodeChallenge::new_random_sha256();

        let r = (*self.inner.lock().unwrap()).oidc_client.as_ref().map(|c| {
            let (auth_url, csrf_token, nonce) = c
                .authorize_url(
                    AuthenticationFlow::<CoreResponseType>::AuthorizationCode,
                    csrf_func("my-csrf".to_string()),
                    nonce_func("my-nonce".to_string()),
                )
                .add_scope(Scope::new("read".to_string()))
                .add_scope(Scope::new("read".to_string()))
                .add_scope(Scope::new("openid".to_string()))
                .set_pkce_challenge(pkce_challenge)
                .url();

            return authres {
                url: auth_url,
                csrf_token,
                nonce,
            };
        });

        r
    }
}

#[no_mangle]
pub extern "C" fn zeroidc_new(
    issuer: *const c_char,
    client_id: *const c_char,
    auth_endpoint: *const c_char,
    web_listen_port: u16,
) -> *mut ZeroIDC {
    if issuer.is_null() {
        println!("issuer is null");
        return std::ptr::null_mut();
    }

    if client_id.is_null() {
        println!("client_id is null");
        return std::ptr::null_mut();
    }

    if auth_endpoint.is_null() {
        println!("auth_endpoint is null");
        return std::ptr::null_mut();
    }

    let iss = unsafe { CStr::from_ptr(issuer) };
    let c_id = unsafe { CStr::from_ptr(client_id) };
    let auth_endpoint = unsafe { CStr::from_ptr(auth_endpoint) };
    match ZeroIDC::new(
        iss.to_str().unwrap(),
        c_id.to_str().unwrap(),
        auth_endpoint.to_str().unwrap(),
        web_listen_port,
    ) {
        Ok(idc) => {
            return Box::into_raw(Box::new(idc));
        }
        Err(s) => {
            println!("Error creating ZeroIDC instance: {}", s);
            return std::ptr::null_mut();
        }
    }
}

#[no_mangle]
pub extern "C" fn zeroidc_delete(ptr: *mut ZeroIDC) {
    if ptr.is_null() {
        return;
    }
    unsafe {
        Box::from_raw(ptr);
    }
}

#[no_mangle]
pub extern "C" fn zeroidc_start(ptr: *mut ZeroIDC) {
    let idc = unsafe {
        assert!(!ptr.is_null());
        &mut *ptr
    };
    idc.start();
}

#[no_mangle]
pub extern "C" fn zeroidc_stop(ptr: *mut ZeroIDC) {
    let idc = unsafe {
        assert!(!ptr.is_null());
        &mut *ptr
    };
    idc.stop();
}
