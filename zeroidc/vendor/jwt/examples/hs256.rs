use hmac::{Hmac, Mac};
use jwt::{RegisteredClaims, SignWithKey, VerifyWithKey};
use sha2::Sha256;

fn new_token(user_id: &str, password: &str) -> Result<String, &'static str> {
    // Dummy auth
    if password != "password" {
        return Err("Wrong password");
    }

    let claims = RegisteredClaims {
        issuer: Some("mikkyang.com".into()),
        subject: Some(user_id.into()),
        ..Default::default()
    };

    let key: Hmac<Sha256> = Hmac::new_from_slice(b"secret_key").map_err(|_e| "Invalid key")?;

    let signed_token = claims.sign_with_key(&key).map_err(|_e| "Sign failed")?;

    Ok(signed_token)
}

fn login(token: &str) -> Result<String, &'static str> {
    let key: Hmac<Sha256> = Hmac::new_from_slice(b"secret_key").map_err(|_e| "Invalid key")?;
    let claims: RegisteredClaims =
        VerifyWithKey::verify_with_key(token, &key).map_err(|_e| "Parse failed")?;

    claims.subject.ok_or("Missing subject")
}

fn main() -> Result<(), &'static str> {
    let token = new_token("Michael Yang", "password")?;

    let logged_in_user = login(&*token)?;

    assert_eq!(logged_in_user, "Michael Yang");
    Ok(())
}
