use hmac::{Hmac, Mac};
use jwt::{Header, SignWithKey, Token, VerifyWithKey};
use serde::{Deserialize, Serialize};
use sha2::Sha256;

#[derive(Default, Deserialize, Serialize)]
struct Custom {
    sub: String,
    rhino: bool,
}

fn new_token(user_id: &str, password: &str) -> Result<String, &'static str> {
    // Dummy auth
    if password != "password" {
        return Err("Wrong password");
    }

    let header: Header = Default::default();
    let claims = Custom {
        sub: user_id.into(),
        rhino: true,
    };
    let unsigned_token = Token::new(header, claims);

    let key: Hmac<Sha256> = Hmac::new_from_slice(b"secret_key").map_err(|_e| "Invalid key")?;

    let signed_token = unsigned_token
        .sign_with_key(&key)
        .map_err(|_e| "Sign error")?;
    Ok(signed_token.into())
}

fn login(token: &str) -> Result<String, &'static str> {
    let key: Hmac<Sha256> = Hmac::new_from_slice(b"secret_key").map_err(|_e| "Invalid key")?;

    let token: Token<Header, Custom, _> =
        VerifyWithKey::verify_with_key(token, &key).map_err(|_e| "Verification failed")?;

    let (_, claims) = token.into();
    Ok(claims.sub)
}

fn main() -> Result<(), &'static str> {
    let token = new_token("Michael Yang", "password")?;

    let logged_in_user = login(&*token)?;

    assert_eq!(logged_in_user, "Michael Yang");
    Ok(())
}
