mod fastudp;
mod localconfig;

fn main() {
    println!("Hello, world!");

    let tokio_rt = tokio::runtime::Runtime::new().unwrap();
    tokio_rt.block_on(async {
        // TODO: init warp http server and anything else using tokio
    });
}
