use http::{
    header::{HeaderMap, HeaderValue, CONTENT_TYPE},
    method::Method,
    status::StatusCode,
};

use super::{HttpRequest, HttpResponse};

///
/// Error type returned by failed ureq HTTP requests.
///
#[derive(Debug, thiserror::Error)]
pub enum Error {
    /// Non-ureq HTTP error.
    #[error("HTTP error")]
    Http(#[from] http::Error),
    /// IO error
    #[error("IO error")]
    IO(#[from] std::io::Error),
    /// Other error.
    #[error("Other error: {}", _0)]
    Other(String),
    /// Error returned by ureq crate.
    // boxed due to https://github.com/algesten/ureq/issues/296
    #[error("ureq request failed")]
    Ureq(#[from] Box<ureq::Error>),
}

///
/// Synchronous HTTP client for ureq.
///
pub fn http_client(request: HttpRequest) -> Result<HttpResponse, Error> {
    let mut req = if let Method::POST = request.method {
        ureq::post(&request.url.to_string())
    } else {
        ureq::get(&request.url.to_string())
    };

    for (name, value) in request.headers {
        if let Some(name) = name {
            req = req.set(
                &name.to_string(),
                value.to_str().map_err(|_| {
                    Error::Other(format!(
                        "invalid {} header value {:?}",
                        name,
                        value.as_bytes()
                    ))
                })?,
            );
        }
    }

    let response = if let Method::POST = request.method {
        req.send(&*request.body)
    } else {
        req.call()
    }
    .map_err(Box::new)?;

    Ok(HttpResponse {
        status_code: StatusCode::from_u16(response.status())
            .map_err(|err| Error::Http(err.into()))?,
        headers: vec![(
            CONTENT_TYPE,
            HeaderValue::from_str(response.content_type())
                .map_err(|err| Error::Http(err.into()))?,
        )]
        .into_iter()
        .collect::<HeaderMap>(),
        body: response.into_string()?.as_bytes().into(),
    })
}
