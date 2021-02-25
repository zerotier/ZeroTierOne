#[allow(unused_assignments)]
fn main() {
    let d = env!("CARGO_MANIFEST_DIR");
    println!("cargo:rustc-link-search=native={}/../build/core", d);
    println!("cargo:rustc-link-lib=static=zt_core");

    let mut cpplib = "c++";
    #[cfg(target_os = "linux")] {
        cpplib = "stdc++";
    }
    println!("cargo:rustc-link-lib={}", cpplib);
}
