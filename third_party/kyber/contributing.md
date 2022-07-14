# Contributing

Contributions always welcome. For pull requests create a feature fork and submit it to the development branch. If possible please run a benchmark first for any significant regressions. 

Current areas of focus aka "aspirational TODO's":

* **Idiomatic rust** - For the moment this crate closely follows the C reference code and will likely stay that way until the PQC standardization project finishes. Still there is a few things that could cleaned up to make it more rusty in preparation for that.
* **Neon ARM intrinsics** - There is a [neon library](https://github.com/cothan/kyber/tree/round3/neon) for Kyber, though currently many ARM intrinsics still don't exist in rust, so there's two branches, `neon` is a rust port of his work that will have to wait until the intrinsics are upstream, `neon_c` is using the original C code with a FFI.
* **Translated Docs**: Localization of readmes and other docs.
* **Optimizations** - See the benchmarking readme, possibly some fat that can still be trimmed off.
* **Add RustCrypto primitives feature for 90s mode** - This is half done yet commented out, still needs some cleaning up to fit in.
* **Add a zeroize feature** - Clean out ephemeral keys after use, also might be worthwhile to zero out polynomials and other internals, which will need benchmarking.
* **Serde** - Implement Serialize/Deserialize traits for the structs and put it behind a feature gate.



By submitting any code to this repository you agree to have it licensed under both Apache 2.0 and MIT.  