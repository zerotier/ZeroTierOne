//! Types for "shape" validation. This allows types deriving `FromDeriveInput` etc. to declare
//! that they only work on - for example - structs with named fields, or newtype enum variants.

use proc_macro2::TokenStream;
use quote::{ToTokens, TokenStreamExt};
use syn::{Meta, NestedMeta};

use crate::{Error, FromMeta, Result};

/// Receiver struct for shape validation. Shape validation allows a deriving type
/// to declare that it only accepts - for example - named structs, or newtype enum
/// variants.
///
/// # Usage
/// Because `Shape` implements `FromMeta`, the name of the field where it appears is
/// controlled by the struct that declares `Shape` as a member. That field name is
/// shown as `ignore` below.
///
/// ```rust,ignore
/// #[ignore(any, struct_named, enum_newtype)]
/// ```
#[derive(Debug, Clone)]
pub struct Shape {
    enum_values: DataShape,
    struct_values: DataShape,
    any: bool,
}

impl Default for Shape {
    fn default() -> Self {
        Shape {
            enum_values: DataShape::new("enum_"),
            struct_values: DataShape::new("struct_"),
            any: Default::default(),
        }
    }
}

impl FromMeta for Shape {
    fn from_list(items: &[NestedMeta]) -> Result<Self> {
        let mut new = Shape::default();
        for item in items {
            if let NestedMeta::Meta(Meta::Path(ref path)) = *item {
                let ident = &path.segments.first().unwrap().ident;
                let word = ident.to_string();
                if word == "any" {
                    new.any = true;
                } else if word.starts_with("enum_") {
                    new.enum_values
                        .set_word(&word)
                        .map_err(|e| e.with_span(&ident))?;
                } else if word.starts_with("struct_") {
                    new.struct_values
                        .set_word(&word)
                        .map_err(|e| e.with_span(&ident))?;
                } else {
                    return Err(Error::unknown_value(&word).with_span(&ident));
                }
            } else {
                return Err(Error::unsupported_format("non-word").with_span(item));
            }
        }

        Ok(new)
    }
}

impl ToTokens for Shape {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        let fn_body = if self.any {
            quote!(::darling::export::Ok(()))
        } else {
            let en = &self.enum_values;
            let st = &self.struct_values;
            quote! {
                match *__body {
                    ::syn::Data::Enum(ref data) => {
                        fn validate_variant(data: &::syn::Fields) -> ::darling::Result<()> {
                            #en
                        }

                        for variant in &data.variants {
                            validate_variant(&variant.fields)?;
                        }

                        Ok(())
                    }
                    ::syn::Data::Struct(ref struct_data) => {
                        let data = &struct_data.fields;
                        #st
                    }
                    ::syn::Data::Union(_) => unreachable!(),
                }
            }
        };

        tokens.append_all(quote! {
            #[allow(unused_variables)]
            fn __validate_body(__body: &::syn::Data) -> ::darling::Result<()> {
                #fn_body
            }
        });
    }
}

/// Receiver for shape information within a struct or enum context. See `Shape` for more information
/// on valid uses of shape validation.
#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct DataShape {
    /// The kind of shape being described. This can be `struct_` or `enum_`.
    prefix: &'static str,
    newtype: bool,
    named: bool,
    tuple: bool,
    unit: bool,
    any: bool,
    /// Control whether the emitted code should be inside a function or not.
    /// This is `true` when creating a `Shape` for `FromDeriveInput`, but false
    /// when deriving `FromVariant`.
    embedded: bool,
}

impl DataShape {
    fn new(prefix: &'static str) -> Self {
        DataShape {
            prefix,
            embedded: true,
            ..Default::default()
        }
    }

    fn supports_none(&self) -> bool {
        !(self.any || self.newtype || self.named || self.tuple || self.unit)
    }

    fn set_word(&mut self, word: &str) -> Result<()> {
        match word.trim_start_matches(self.prefix) {
            "newtype" => {
                self.newtype = true;
                Ok(())
            }
            "named" => {
                self.named = true;
                Ok(())
            }
            "tuple" => {
                self.tuple = true;
                Ok(())
            }
            "unit" => {
                self.unit = true;
                Ok(())
            }
            "any" => {
                self.any = true;
                Ok(())
            }
            _ => Err(Error::unknown_value(word)),
        }
    }
}

impl FromMeta for DataShape {
    fn from_list(items: &[NestedMeta]) -> Result<Self> {
        let mut errors = Error::accumulator();
        let mut new = DataShape::default();

        for item in items {
            if let NestedMeta::Meta(Meta::Path(ref path)) = *item {
                errors.handle(new.set_word(&path.segments.first().unwrap().ident.to_string()));
            } else {
                errors.push(Error::unsupported_format("non-word").with_span(item));
            }
        }

        errors.finish_with(new)
    }
}

impl ToTokens for DataShape {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        let body = if self.any {
            quote!(::darling::export::Ok(()))
        } else if self.supports_none() {
            let ty = self.prefix.trim_end_matches('_');
            quote!(::darling::export::Err(::darling::Error::unsupported_shape(#ty)))
        } else {
            let unit = match_arm("unit", self.unit);
            let newtype = match_arm("newtype", self.newtype);
            let named = match_arm("named", self.named);
            let tuple = match_arm("tuple", self.tuple);
            quote! {
                match *data {
                    ::syn::Fields::Unit => #unit,
                    ::syn::Fields::Unnamed(ref fields) if fields.unnamed.len() == 1 => #newtype,
                    ::syn::Fields::Unnamed(_) => #tuple,
                    ::syn::Fields::Named(_) => #named,
                }
            }
        };

        if self.embedded {
            body.to_tokens(tokens);
        } else {
            tokens.append_all(quote! {
                fn __validate_data(data: &::syn::Fields) -> ::darling::Result<()> {
                    #body
                }
            });
        }
    }
}

fn match_arm(name: &'static str, is_supported: bool) -> TokenStream {
    if is_supported {
        quote!(::darling::export::Ok(()))
    } else {
        quote!(::darling::export::Err(::darling::Error::unsupported_shape(#name)))
    }
}

#[cfg(test)]
mod tests {
    use proc_macro2::TokenStream;

    use super::Shape;
    use crate::FromMeta;

    /// parse a string as a syn::Meta instance.
    fn pm(tokens: TokenStream) -> ::std::result::Result<syn::Meta, String> {
        let attribute: syn::Attribute = parse_quote!(#[#tokens]);
        attribute.parse_meta().map_err(|_| "Unable to parse".into())
    }

    fn fm<T: FromMeta>(tokens: TokenStream) -> T {
        FromMeta::from_meta(&pm(tokens).expect("Tests should pass well-formed input"))
            .expect("Tests should pass valid input")
    }

    #[test]
    fn supports_any() {
        let decl = fm::<Shape>(quote!(ignore(any)));
        assert!(decl.any);
    }

    #[test]
    fn supports_struct() {
        let decl = fm::<Shape>(quote!(ignore(struct_any, struct_newtype)));
        assert!(decl.struct_values.any);
        assert!(decl.struct_values.newtype);
    }

    #[test]
    fn supports_mixed() {
        let decl = fm::<Shape>(quote!(ignore(struct_newtype, enum_newtype, enum_tuple)));
        assert!(decl.struct_values.newtype);
        assert!(decl.enum_values.newtype);
        assert!(decl.enum_values.tuple);
        assert!(!decl.struct_values.any);
    }
}
