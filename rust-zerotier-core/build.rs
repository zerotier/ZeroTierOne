fn main() {
    let d = env!("CARGO_MANIFEST_DIR");
    println!("cargo:rustc-link-search=native={}/../build/core", d);
    println!("cargo:rustc-link-lib=static=zt_core");
    println!("cargo:rustc-link-lib=c++");
}
