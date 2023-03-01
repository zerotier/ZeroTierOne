fn main() {
    let cfg = match autocfg::AutoCfg::new() {
        Ok(cfg) => cfg,
        Err(e) => {
            // If we couldn't detect the compiler version and features, just
            // print a warning. This isn't a fatal error: we can still build
            // Slab, we just can't enable cfgs automatically.
            println!(
                "cargo:warning=slab: failed to detect compiler features: {}",
                e
            );
            return;
        }
    };
    // Note that this is `no_`*, not `has_*`. This allows treating as the latest
    // stable rustc is used when the build script doesn't run. This is useful
    // for non-cargo build systems that don't run the build script.
    if !cfg.probe_rustc_version(1, 39) {
        println!("cargo:rustc-cfg=slab_no_const_vec_new");
    }
    if !cfg.probe_rustc_version(1, 46) {
        println!("cargo:rustc-cfg=slab_no_track_caller");
    }
}
