use quote::quote;

struct Ipv4Addr;

fn main() {
    let ip = Ipv4Addr;
    _ = quote! { #(#ip)* };
}
