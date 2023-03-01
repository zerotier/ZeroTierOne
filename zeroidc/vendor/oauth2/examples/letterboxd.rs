//!
//! This example showcases the Letterboxd OAuth2 process for requesting access
//! to the API features restricted by authentication. Letterboxd requires all
//! requests being signed as described in http://api-docs.letterboxd.com/#signing.
//! So this serves as an example how to implement a custom client, which signs
//! requests and appends the signature to the url query.
//!
//! Before running it, you'll need to get access to the API.
//!
//! In order to run the example call:
//!
//! ```sh
//! LETTERBOXD_CLIENT_ID=xxx LETTERBOXD_CLIENT_SECRET=yyy LETTERBOXD_USERNAME=www LETTERBOXD_PASSWORD=zzz cargo run --example letterboxd
//! ```

use hex::ToHex;
use hmac::{Hmac, Mac};
use oauth2::{
    basic::BasicClient, AuthType, AuthUrl, ClientId, ClientSecret, HttpRequest, HttpResponse,
    ResourceOwnerPassword, ResourceOwnerUsername, TokenUrl,
};
use sha2::Sha256;
use url::Url;

use std::env;
use std::time;

fn main() -> Result<(), anyhow::Error> {
    // a.k.a api key in Letterboxd API documentation
    let letterboxd_client_id = ClientId::new(
        env::var("LETTERBOXD_CLIENT_ID")
            .expect("Missing the LETTERBOXD_CLIENT_ID environment variable."),
    );
    // a.k.a api secret in Letterboxd API documentation
    let letterboxd_client_secret = ClientSecret::new(
        env::var("LETTERBOXD_CLIENT_SECRET")
            .expect("Missing the LETTERBOXD_CLIENT_SECRET environment variable."),
    );
    // Letterboxd uses the Resource Owner flow and does not have an auth url
    let auth_url = AuthUrl::new("https://api.letterboxd.com/api/v0/auth/404".to_string())?;
    let token_url = TokenUrl::new("https://api.letterboxd.com/api/v0/auth/token".to_string())?;

    // Set up the config for the Letterboxd OAuth2 process.
    let client = BasicClient::new(
        letterboxd_client_id.clone(),
        Some(letterboxd_client_secret.clone()),
        auth_url,
        Some(token_url),
    );

    // Resource Owner flow uses username and password for authentication
    let letterboxd_username = ResourceOwnerUsername::new(
        env::var("LETTERBOXD_USERNAME")
            .expect("Missing the LETTERBOXD_USERNAME environment variable."),
    );
    let letterboxd_password = ResourceOwnerPassword::new(
        env::var("LETTERBOXD_PASSWORD")
            .expect("Missing the LETTERBOXD_PASSWORD environment variable."),
    );

    // All API requests must be signed as described at http://api-docs.letterboxd.com/#signing;
    // for that, we use a custom http client.
    let http_client = SigningHttpClient::new(letterboxd_client_id, letterboxd_client_secret);

    let token_result = client
        .set_auth_type(AuthType::RequestBody)
        .exchange_password(&letterboxd_username, &letterboxd_password)
        .request(|request| http_client.execute(request))?;

    println!("{:?}", token_result);

    Ok(())
}

/// Custom HTTP client which signs requests.
///
/// See http://api-docs.letterboxd.com/#signing.
#[derive(Debug, Clone)]
struct SigningHttpClient {
    client_id: ClientId,
    client_secret: ClientSecret,
}

impl SigningHttpClient {
    fn new(client_id: ClientId, client_secret: ClientSecret) -> Self {
        Self {
            client_id,
            client_secret,
        }
    }

    /// Signs the request before calling `oauth2::reqwest::http_client`.
    fn execute(&self, mut request: HttpRequest) -> Result<HttpResponse, impl std::error::Error> {
        let signed_url = self.sign_url(request.url, &request.method, &request.body);
        request.url = signed_url;
        oauth2::reqwest::http_client(request)
    }

    /// Signs the request based on a random and unique nonce, timestamp, and
    /// client id and secret.
    ///
    /// The client id, nonce, timestamp and signature are added to the url's
    /// query.
    ///
    /// See http://api-docs.letterboxd.com/#signing.
    fn sign_url(&self, mut url: Url, method: &http::method::Method, body: &[u8]) -> Url {
        let nonce = uuid::Uuid::new_v4(); // use UUID as random and unique nonce

        let timestamp = time::SystemTime::now()
            .duration_since(time::UNIX_EPOCH)
            .expect("SystemTime::duration_since failed")
            .as_secs();

        url.query_pairs_mut()
            .append_pair("apikey", &self.client_id)
            .append_pair("nonce", &format!("{}", nonce))
            .append_pair("timestamp", &format!("{}", timestamp));

        // create signature
        let mut hmac = Hmac::<Sha256>::new_from_slice(&self.client_secret.secret().as_bytes())
            .expect("HMAC can take key of any size");
        hmac.update(method.as_str().as_bytes());
        hmac.update(&[b'\0']);
        hmac.update(url.as_str().as_bytes());
        hmac.update(&[b'\0']);
        hmac.update(body);
        let signature: String = hmac.finalize().into_bytes().encode_hex();

        url.query_pairs_mut().append_pair("signature", &signature);

        url
    }
}
