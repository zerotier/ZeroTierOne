//!
//! This example showcases the Wunderlist OAuth2 process for requesting access to the user's todo lists.
//! Wunderlist does not implement the correct token response, so this serves as an example of how to
//! implement a custom client.
//!
//! Before running it, you'll need to create your own wunderlist app.
//!
//! In order to run the example call:
//!
//! ```sh
//! WUNDER_CLIENT_ID=xxx WUNDER_CLIENT_SECRET=yyy cargo run --example wunderlist
//! ```
//!
//! ...and follow the instructions.
//!

use oauth2::TokenType;
use oauth2::{
    basic::{
        BasicErrorResponse, BasicRevocationErrorResponse, BasicTokenIntrospectionResponse,
        BasicTokenType,
    },
    revocation::StandardRevocableToken,
};
// Alternatively, this can be `oauth2::curl::http_client` or a custom client.
use oauth2::helpers;
use oauth2::reqwest::http_client;
use oauth2::{
    AccessToken, AuthUrl, AuthorizationCode, Client, ClientId, ClientSecret, CsrfToken,
    EmptyExtraTokenFields, ExtraTokenFields, RedirectUrl, RefreshToken, Scope, TokenResponse,
    TokenUrl,
};

use serde::{Deserialize, Serialize};
use std::time::Duration;

use std::env;
use std::io::{BufRead, BufReader, Write};
use std::net::TcpListener;
use url::Url;

type SpecialTokenResponse = NonStandardTokenResponse<EmptyExtraTokenFields>;
type SpecialClient = Client<
    BasicErrorResponse,
    SpecialTokenResponse,
    BasicTokenType,
    BasicTokenIntrospectionResponse,
    StandardRevocableToken,
    BasicRevocationErrorResponse,
>;

fn default_token_type() -> Option<BasicTokenType> {
    Some(BasicTokenType::Bearer)
}

///
/// Non Standard OAuth2 token response.
///
/// This struct includes the fields defined in
/// [Section 5.1 of RFC 6749](https://tools.ietf.org/html/rfc6749#section-5.1), as well as
/// extensions defined by the `EF` type parameter.
/// In this particular example token_type is optional to showcase how to deal with a non
/// compliant provider.
///
#[derive(Clone, Debug, Deserialize, Serialize)]
pub struct NonStandardTokenResponse<EF: ExtraTokenFields> {
    access_token: AccessToken,
    // In this example wunderlist does not follow the RFC specs and don't return the
    // token_type. `NonStandardTokenResponse` makes the `token_type` optional.
    #[serde(default = "default_token_type")]
    token_type: Option<BasicTokenType>,
    #[serde(skip_serializing_if = "Option::is_none")]
    expires_in: Option<u64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    refresh_token: Option<RefreshToken>,
    #[serde(rename = "scope")]
    #[serde(deserialize_with = "helpers::deserialize_space_delimited_vec")]
    #[serde(serialize_with = "helpers::serialize_space_delimited_vec")]
    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(default)]
    scopes: Option<Vec<Scope>>,

    #[serde(bound = "EF: ExtraTokenFields")]
    #[serde(flatten)]
    extra_fields: EF,
}

impl<EF> TokenResponse<BasicTokenType> for NonStandardTokenResponse<EF>
where
    EF: ExtraTokenFields,
    BasicTokenType: TokenType,
{
    ///
    /// REQUIRED. The access token issued by the authorization server.
    ///
    fn access_token(&self) -> &AccessToken {
        &self.access_token
    }
    ///
    /// REQUIRED. The type of the token issued as described in
    /// [Section 7.1](https://tools.ietf.org/html/rfc6749#section-7.1).
    /// Value is case insensitive and deserialized to the generic `TokenType` parameter.
    /// But in this particular case as the service is non compliant, it has a default value
    ///
    fn token_type(&self) -> &BasicTokenType {
        match &self.token_type {
            Some(t) => t,
            None => &BasicTokenType::Bearer,
        }
    }
    ///
    /// RECOMMENDED. The lifetime in seconds of the access token. For example, the value 3600
    /// denotes that the access token will expire in one hour from the time the response was
    /// generated. If omitted, the authorization server SHOULD provide the expiration time via
    /// other means or document the default value.
    ///
    fn expires_in(&self) -> Option<Duration> {
        self.expires_in.map(Duration::from_secs)
    }
    ///
    /// OPTIONAL. The refresh token, which can be used to obtain new access tokens using the same
    /// authorization grant as described in
    /// [Section 6](https://tools.ietf.org/html/rfc6749#section-6).
    ///
    fn refresh_token(&self) -> Option<&RefreshToken> {
        self.refresh_token.as_ref()
    }
    ///
    /// OPTIONAL, if identical to the scope requested by the client; otherwise, REQUIRED. The
    /// scipe of the access token as described by
    /// [Section 3.3](https://tools.ietf.org/html/rfc6749#section-3.3). If included in the response,
    /// this space-delimited field is parsed into a `Vec` of individual scopes. If omitted from
    /// the response, this field is `None`.
    ///
    fn scopes(&self) -> Option<&Vec<Scope>> {
        self.scopes.as_ref()
    }
}

fn main() {
    let client_id_str = env::var("WUNDERLIST_CLIENT_ID")
        .expect("Missing the WUNDERLIST_CLIENT_ID environment variable.");

    let client_secret_str = env::var("WUNDERLIST_CLIENT_SECRET")
        .expect("Missing the WUNDERLIST_CLIENT_SECRET environment variable.");

    let wunder_client_id = ClientId::new(client_id_str.clone());
    let wunderlist_client_secret = ClientSecret::new(client_secret_str.clone());
    let auth_url = AuthUrl::new("https://www.wunderlist.com/oauth/authorize".to_string())
        .expect("Invalid authorization endpoint URL");
    let token_url = TokenUrl::new("https://www.wunderlist.com/oauth/access_token".to_string())
        .expect("Invalid token endpoint URL");

    // Set up the config for the Wunderlist OAuth2 process.
    let client = SpecialClient::new(
        wunder_client_id,
        Some(wunderlist_client_secret),
        auth_url,
        Some(token_url),
    )
    // This example will be running its own server at localhost:8080.
    // See below for the server implementation.
    .set_redirect_uri(
        RedirectUrl::new("http://localhost:8080".to_string()).expect("Invalid redirect URL"),
    );

    // Generate the authorization URL to which we'll redirect the user.
    let (authorize_url, csrf_state) = client.authorize_url(CsrfToken::new_random).url();

    println!(
        "Open this URL in your browser:\n{}\n",
        authorize_url.to_string()
    );

    // A very naive implementation of the redirect server.
    let listener = TcpListener::bind("127.0.0.1:8080").unwrap();
    for stream in listener.incoming() {
        if let Ok(mut stream) = stream {
            let code;
            let state;
            {
                let mut reader = BufReader::new(&stream);

                let mut request_line = String::new();
                reader.read_line(&mut request_line).unwrap();

                let redirect_url = request_line.split_whitespace().nth(1).unwrap();
                let url = Url::parse(&("http://localhost".to_string() + redirect_url)).unwrap();

                let code_pair = url
                    .query_pairs()
                    .find(|pair| {
                        let &(ref key, _) = pair;
                        key == "code"
                    })
                    .unwrap();

                let (_, value) = code_pair;
                code = AuthorizationCode::new(value.into_owned());

                let state_pair = url
                    .query_pairs()
                    .find(|pair| {
                        let &(ref key, _) = pair;
                        key == "state"
                    })
                    .unwrap();

                let (_, value) = state_pair;
                state = CsrfToken::new(value.into_owned());
            }

            let message = "Go back to your terminal :)";
            let response = format!(
                "HTTP/1.1 200 OK\r\ncontent-length: {}\r\n\r\n{}",
                message.len(),
                message
            );
            stream.write_all(response.as_bytes()).unwrap();

            println!(
                "Wunderlist returned the following code:\n{}\n",
                code.secret()
            );
            println!(
                "Wunderlist returned the following state:\n{} (expected `{}`)\n",
                state.secret(),
                csrf_state.secret()
            );

            // Exchange the code with a token.
            let token_res = client
                .exchange_code(code)
                .add_extra_param("client_id", client_id_str)
                .add_extra_param("client_secret", client_secret_str)
                .request(http_client);

            println!(
                "Wunderlist returned the following token:\n{:?}\n",
                token_res
            );

            break;
        }
    }
}
