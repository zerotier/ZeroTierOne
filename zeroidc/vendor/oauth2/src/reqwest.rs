use thiserror::Error;

///
/// Error type returned by failed reqwest HTTP requests.
///
#[derive(Debug, Error)]
pub enum Error<T>
where
    T: std::error::Error + 'static,
{
    /// Error returned by reqwest crate.
    #[error("request failed")]
    Reqwest(#[source] T),
    /// Non-reqwest HTTP error.
    #[error("HTTP error")]
    Http(#[source] http::Error),
    /// I/O error.
    #[error("I/O error")]
    Io(#[source] std::io::Error),
    /// Other error.
    #[error("Other error: {}", _0)]
    Other(String),
}

#[cfg(not(target_arch = "wasm32"))]
pub use blocking::http_client;
///
/// Error type returned by failed reqwest blocking HTTP requests.
///
#[cfg(not(target_arch = "wasm32"))]
pub type HttpClientError = Error<blocking::reqwest::Error>;

pub use async_client::async_http_client;

///
/// Error type returned by failed reqwest async HTTP requests.
///
pub type AsyncHttpClientError = Error<reqwest::Error>;

#[cfg(not(target_arch = "wasm32"))]
mod blocking {
    use super::super::{HttpRequest, HttpResponse};
    use super::Error;

    pub use reqwest;
    use reqwest::blocking;
    use reqwest::redirect::Policy as RedirectPolicy;

    use std::io::Read;

    ///
    /// Synchronous HTTP client.
    ///
    pub fn http_client(request: HttpRequest) -> Result<HttpResponse, Error<reqwest::Error>> {
        let client = blocking::Client::builder()
            // Following redirects opens the client up to SSRF vulnerabilities.
            .redirect(RedirectPolicy::none())
            .build()
            .map_err(Error::Reqwest)?;

        #[cfg(feature = "reqwest")]
        let mut request_builder = client
            .request(request.method, request.url.as_str())
            .body(request.body);

        for (name, value) in &request.headers {
            request_builder = request_builder.header(name.as_str(), value.as_bytes());
        }
        let mut response = client
            .execute(request_builder.build().map_err(Error::Reqwest)?)
            .map_err(Error::Reqwest)?;

        let mut body = Vec::new();
        response.read_to_end(&mut body).map_err(Error::Io)?;

        #[cfg(feature = "reqwest")]
        {
            Ok(HttpResponse {
                status_code: response.status(),
                headers: response.headers().to_owned(),
                body,
            })
        }
    }
}

mod async_client {
    use super::super::{HttpRequest, HttpResponse};
    use super::Error;

    pub use reqwest;

    ///
    /// Asynchronous HTTP client.
    ///
    pub async fn async_http_client(
        request: HttpRequest,
    ) -> Result<HttpResponse, Error<reqwest::Error>> {
        let client = {
            let builder = reqwest::Client::builder();

            // Following redirects opens the client up to SSRF vulnerabilities.
            // but this is not possible to prevent on wasm targets
            #[cfg(not(target_arch = "wasm32"))]
            let builder = builder.redirect(reqwest::redirect::Policy::none());

            builder.build().map_err(Error::Reqwest)?
        };

        let mut request_builder = client
            .request(request.method, request.url.as_str())
            .body(request.body);
        for (name, value) in &request.headers {
            request_builder = request_builder.header(name.as_str(), value.as_bytes());
        }
        let request = request_builder.build().map_err(Error::Reqwest)?;

        let response = client.execute(request).await.map_err(Error::Reqwest)?;

        let status_code = response.status();
        let headers = response.headers().to_owned();
        let chunks = response.bytes().await.map_err(Error::Reqwest)?;
        Ok(HttpResponse {
            status_code,
            headers,
            body: chunks.to_vec(),
        })
    }
}
