use proc_macro2::TokenStream;
use quote::{ToTokens, TokenStreamExt};
use syn::{Ident, Path};

/// This will be in scope during struct initialization after option parsing.
const DEFAULT_STRUCT_NAME: &str = "__default";

/// The fallback value for a field or container.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum DefaultExpression<'a> {
    /// Only valid on fields, `Inherit` indicates that the value should be taken from a pre-constructed
    /// fallback object. The value in the variant is the ident of the field.
    Inherit(&'a Ident),
    Explicit(&'a Path),
    Trait,
}

impl<'a> DefaultExpression<'a> {
    pub fn as_declaration(&'a self) -> DefaultDeclaration<'a> {
        DefaultDeclaration(self)
    }
}

impl<'a> ToTokens for DefaultExpression<'a> {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        tokens.append_all(match *self {
            DefaultExpression::Inherit(ident) => {
                let dsn = Ident::new(DEFAULT_STRUCT_NAME, ::proc_macro2::Span::call_site());
                quote!(#dsn.#ident)
            }
            DefaultExpression::Explicit(path) => quote!(#path()),
            DefaultExpression::Trait => quote!(::darling::export::Default::default()),
        });
    }
}

/// Used only by containers, this wrapper type generates code to declare the fallback instance.
pub struct DefaultDeclaration<'a>(&'a DefaultExpression<'a>);

impl<'a> ToTokens for DefaultDeclaration<'a> {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        let name = Ident::new(DEFAULT_STRUCT_NAME, ::proc_macro2::Span::call_site());
        let expr = self.0;
        tokens.append_all(quote!(let #name: Self = #expr;));
    }
}
