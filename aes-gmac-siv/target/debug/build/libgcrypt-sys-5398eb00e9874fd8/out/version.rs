pub const MIN_GCRYPT_VERSION: &str = "1.9.0\0";
#[macro_export]
macro_rules! require_gcrypt_ver {
($ver:tt => { $($t:tt)*  }) => (require_gcrypt_ver! { $ver => { $($t)* } else {} });
((1,0) => { $($t:tt)* } else { $($u:tt)* }) => ($($t)*);
((1,1) => { $($t:tt)* } else { $($u:tt)* }) => ($($t)*);
((1,2) => { $($t:tt)* } else { $($u:tt)* }) => ($($t)*);
((1,3) => { $($t:tt)* } else { $($u:tt)* }) => ($($t)*);
((1,4) => { $($t:tt)* } else { $($u:tt)* }) => ($($t)*);
((1,5) => { $($t:tt)* } else { $($u:tt)* }) => ($($t)*);
((1,6) => { $($t:tt)* } else { $($u:tt)* }) => ($($t)*);
((1,7) => { $($t:tt)* } else { $($u:tt)* }) => ($($t)*);
((1,8) => { $($t:tt)* } else { $($u:tt)* }) => ($($t)*);
((1,9) => { $($t:tt)* } else { $($u:tt)* }) => ($($t)*);
((0,$ver:tt) => { $($t:tt)* } else { $($u:tt)* }) => ($($t)*);
($ver:tt => { $($t:tt)* } else { $($u:tt)* }) => ($($u)*);
}
