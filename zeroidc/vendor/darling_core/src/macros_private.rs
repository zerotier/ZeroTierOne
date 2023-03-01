macro_rules! quote {
    ($($tt:tt)*) => {
        quote_spanned!(::proc_macro2::Span::call_site() => $($tt)*)
    };
}

macro_rules! path {
    ($($path:tt)+) => {
        parse_quote!($($path)+)
        //stringify!($($path)+).parse().unwrap()
    };
}
