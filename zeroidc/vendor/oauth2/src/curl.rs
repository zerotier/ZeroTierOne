use std::io::Read;

use curl::easy::Easy;
use http::header::{HeaderMap, HeaderValue, CONTENT_TYPE};
use http::method::Method;
use http::status::StatusCode;

use super::{HttpRequest, HttpResponse};

///
/// Error type returned by failed curl HTTP requests.
///
#[derive(Debug, thiserror::Error)]
pub enum Error {
    /// Error returned by curl crate.
    #[error("curl request failed")]
    Curl(#[source] curl::Error),
    /// Non-curl HTTP error.
    #[error("HTTP error")]
    Http(#[source] http::Error),
    /// Other error.
    #[error("Other error: {}", _0)]
    Other(String),
}

///
/// Synchronous HTTP client.
///
pub fn http_client(request: HttpRequest) -> Result<HttpResponse, Error> {
    let mut easy = Easy::new();
    easy.url(&request.url.to_string()[..])
        .map_err(Error::Curl)?;

    let mut headers = curl::easy::List::new();
    request
        .headers
        .iter()
        .map(|(name, value)| {
            headers
                .append(&format!(
                    "{}: {}",
                    name,
                    value.to_str().map_err(|_| Error::Other(format!(
                        "invalid {} header value {:?}",
                        name,
                        value.as_bytes()
                    )))?
                ))
                .map_err(Error::Curl)
        })
        .collect::<Result<_, _>>()?;

    easy.http_headers(headers).map_err(Error::Curl)?;

    if let Method::POST = request.method {
        easy.post(true).map_err(Error::Curl)?;
        easy.post_field_size(request.body.len() as u64)
            .map_err(Error::Curl)?;
    } else {
        assert_eq!(request.method, Method::GET);
    }

    let mut form_slice = &request.body[..];
    let mut data = Vec::new();
    {
        let mut transfer = easy.transfer();

        transfer
            .read_function(|buf| Ok(form_slice.read(buf).unwrap_or(0)))
            .map_err(Error::Curl)?;

        transfer
            .write_function(|new_data| {
                data.extend_from_slice(new_data);
                Ok(new_data.len())
            })
            .map_err(Error::Curl)?;

        transfer.perform().map_err(Error::Curl)?;
    }

    let status_code = easy.response_code().map_err(Error::Curl)? as u16;

    Ok(HttpResponse {
        status_code: StatusCode::from_u16(status_code).map_err(|err| Error::Http(err.into()))?,
        headers: easy
            .content_type()
            .map_err(Error::Curl)?
            .map(|content_type| {
                Ok(vec![(
                    CONTENT_TYPE,
                    HeaderValue::from_str(content_type).map_err(|err| Error::Http(err.into()))?,
                )]
                .into_iter()
                .collect::<HeaderMap>())
            })
            .transpose()?
            .unwrap_or_else(HeaderMap::new),
        body: data,
    })
}
