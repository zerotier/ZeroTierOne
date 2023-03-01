use quote::quote;
use std::net::Ipv4Addr;

fn main() {
    let ip = Ipv4Addr::LOCALHOST;
    _ = quote! { #ip };
}
