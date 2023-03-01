#![forbid(unsafe_code)]
#![warn(
    clippy::semicolon_if_nothing_returned,
    missing_copy_implementations,
    // missing_crate_level_docs, not available in MSRV
    missing_debug_implementations,
    missing_docs,
    rust_2018_idioms,
    trivial_casts,
    trivial_numeric_casts,
    unused_extern_crates,
    unused_import_braces,
    unused_qualifications,
    variant_size_differences
)]
#![doc(test(attr(forbid(unsafe_code))))]
#![doc(test(attr(deny(
    missing_copy_implementations,
    missing_debug_implementations,
    trivial_casts,
    trivial_numeric_casts,
    unused_extern_crates,
    unused_import_braces,
    unused_qualifications,
))))]
#![doc(test(attr(warn(rust_2018_idioms))))]
// Not needed for 2018 edition and conflicts with `rust_2018_idioms`
#![doc(test(no_crate_inject))]
#![doc(html_root_url = "https://docs.rs/serde_with_macros/1.5.2")]
// Necessary to silence the warning about clippy::unknown_clippy_lints on nightly
#![allow(renamed_and_removed_lints)]
// Necessary for nightly clippy lints
#![allow(clippy::unknown_clippy_lints)]

//! proc-macro extensions for [`serde_with`].
//!
//! This crate should **NEVER** be used alone.
//! All macros **MUST** be used via the re-exports in the [`serde_with`] crate.
//!
//! [`serde_with`]: https://crates.io/crates/serde_with/

#[allow(unused_extern_crates)]
extern crate proc_macro;

mod utils;

use crate::utils::{split_with_de_lifetime, DeriveOptions, IteratorExt as _};
use darling::util::Override;
use darling::{Error as DarlingError, FromField, FromMeta};
use proc_macro::TokenStream;
use proc_macro2::{Span, TokenStream as TokenStream2};
use quote::quote;
use syn::punctuated::Pair;
use syn::spanned::Spanned;
use syn::{
    parse_macro_input, parse_quote, AttributeArgs, DeriveInput, Error, Field, Fields,
    GenericArgument, ItemEnum, ItemStruct, Meta, NestedMeta, Path, PathArguments, ReturnType, Type,
};

/// Apply function on every field of structs or enums
fn apply_function_to_struct_and_enum_fields<F>(
    input: TokenStream,
    function: F,
) -> Result<TokenStream2, Error>
where
    F: Copy,
    F: Fn(&mut Field) -> Result<(), String>,
{
    /// Handle a single struct or a single enum variant
    fn apply_on_fields<F>(fields: &mut Fields, function: F) -> Result<(), Error>
    where
        F: Fn(&mut Field) -> Result<(), String>,
    {
        match fields {
            // simple, no fields, do nothing
            Fields::Unit => Ok(()),
            Fields::Named(ref mut fields) => fields
                .named
                .iter_mut()
                .map(|field| function(field).map_err(|err| Error::new(field.span(), err)))
                .collect_error(),
            Fields::Unnamed(ref mut fields) => fields
                .unnamed
                .iter_mut()
                .map(|field| function(field).map_err(|err| Error::new(field.span(), err)))
                .collect_error(),
        }
    }

    // For each field in the struct given by `input`, add the `skip_serializing_if` attribute,
    // if and only if, it is of type `Option`
    if let Ok(mut input) = syn::parse::<ItemStruct>(input.clone()) {
        apply_on_fields(&mut input.fields, function)?;
        Ok(quote!(#input))
    } else if let Ok(mut input) = syn::parse::<ItemEnum>(input) {
        input
            .variants
            .iter_mut()
            .map(|variant| apply_on_fields(&mut variant.fields, function))
            .collect_error()?;
        Ok(quote!(#input))
    } else {
        Err(Error::new(
            Span::call_site(),
            "The attribute can only be applied to struct or enum definitions.",
        ))
    }
}

/// Like [apply_function_to_struct_and_enum_fields] but for darling errors
fn apply_function_to_struct_and_enum_fields_darling<F>(
    input: TokenStream,
    serde_with_crate_path: &Path,
    function: F,
) -> Result<TokenStream2, DarlingError>
where
    F: Copy,
    F: Fn(&mut Field) -> Result<(), DarlingError>,
{
    /// Handle a single struct or a single enum variant
    fn apply_on_fields<F>(fields: &mut Fields, function: F) -> Result<(), DarlingError>
    where
        F: Fn(&mut Field) -> Result<(), DarlingError>,
    {
        match fields {
            // simple, no fields, do nothing
            Fields::Unit => Ok(()),
            Fields::Named(ref mut fields) => {
                let errors: Vec<DarlingError> = fields
                    .named
                    .iter_mut()
                    .map(|field| function(field).map_err(|err| err.with_span(&field)))
                    // turn the Err variant into the Some, such that we only collect errors
                    .filter_map(|res| match res {
                        Err(e) => Some(e),
                        Ok(()) => None,
                    })
                    .collect();
                if errors.is_empty() {
                    Ok(())
                } else {
                    Err(DarlingError::multiple(errors))
                }
            }
            Fields::Unnamed(ref mut fields) => {
                let errors: Vec<DarlingError> = fields
                    .unnamed
                    .iter_mut()
                    .map(|field| function(field).map_err(|err| err.with_span(&field)))
                    // turn the Err variant into the Some, such that we only collect errors
                    .filter_map(|res| match res {
                        Err(e) => Some(e),
                        Ok(()) => None,
                    })
                    .collect();
                if errors.is_empty() {
                    Ok(())
                } else {
                    Err(DarlingError::multiple(errors))
                }
            }
        }
    }

    // Add a dummy derive macro which consumes (makes inert) all field attributes
    let consume_serde_as_attribute = parse_quote!(
        #[derive(#serde_with_crate_path::__private_consume_serde_as_attributes)]
    );

    // For each field in the struct given by `input`, add the `skip_serializing_if` attribute,
    // if and only if, it is of type `Option`
    if let Ok(mut input) = syn::parse::<ItemStruct>(input.clone()) {
        apply_on_fields(&mut input.fields, function)?;
        input.attrs.push(consume_serde_as_attribute);
        Ok(quote!(#input))
    } else if let Ok(mut input) = syn::parse::<ItemEnum>(input) {
        let errors: Vec<DarlingError> = input
            .variants
            .iter_mut()
            .map(|variant| apply_on_fields(&mut variant.fields, function))
            // turn the Err variant into the Some, such that we only collect errors
            .filter_map(|res| match res {
                Err(e) => Some(e),
                Ok(()) => None,
            })
            .collect();
        if errors.is_empty() {
            input.attrs.push(consume_serde_as_attribute);
            Ok(quote!(#input))
        } else {
            Err(DarlingError::multiple(errors))
        }
    } else {
        Err(DarlingError::custom(
            "The attribute can only be applied to struct or enum definitions.",
        )
        .with_span(&Span::call_site()))
    }
}

/// Add `skip_serializing_if` annotations to [`Option`] fields.
///
/// The attribute can be added to structs and enums.
///
/// Import this attribute with `use serde_with::skip_serializing_none;`.
///
/// # Example
///
/// JSON APIs sometimes have many optional values.
/// Missing values should not be serialized, to keep the serialized format smaller.
/// Such a data type might look like:
///
/// ```rust
/// # use serde::Serialize;
/// #
/// #[derive(Serialize)]
/// struct Data {
///     #[serde(skip_serializing_if = "Option::is_none")]
///     a: Option<String>,
///     #[serde(skip_serializing_if = "Option::is_none")]
///     b: Option<u64>,
///     #[serde(skip_serializing_if = "Option::is_none")]
///     c: Option<String>,
///     #[serde(skip_serializing_if = "Option::is_none")]
///     d: Option<bool>,
/// }
/// ```
///
/// The `skip_serializing_if` annotation is repetitive and harms readability.
/// Instead, the same struct can be written as:
///
/// ```rust
/// # use serde::Serialize;
/// # use serde_with_macros::skip_serializing_none;
/// #[skip_serializing_none]
/// #[derive(Serialize)]
/// struct Data {
///     a: Option<String>,
///     b: Option<u64>,
///     c: Option<String>,
///     // Always serialize field d even if None
///     #[serialize_always]
///     d: Option<bool>,
/// }
/// ```
///
/// Existing `skip_serializing_if` annotations will not be altered.
///
/// If some values should always be serialized, then the `serialize_always` can be used.
///
/// # Limitations
///
/// The `serialize_always` cannot be used together with a manual `skip_serializing_if` annotations, as these conflict in their meaning.
/// A compile error will be generated if this occurs.
///
/// The `skip_serializing_none` only works if the type is called [`Option`], [`std::option::Option`], or [`core::option::Option`].
/// Type aliasing an [`Option`] and giving it another name, will cause this field to be ignored.
/// This cannot be supported, as proc-macros run before type checking, thus it is not possible to determine if a type alias refers to an [`Option`].
///
/// ```rust
/// # use serde::Serialize;
/// # use serde_with_macros::skip_serializing_none;
/// type MyOption<T> = Option<T>;
///
/// #[skip_serializing_none]
/// #[derive(Serialize)]
/// struct Data {
///     a: MyOption<String>, // This field will not be skipped
/// }
/// ```
///
/// Likewise, if you import a type and name it `Option`, the `skip_serializing_if` attributes will be added and compile errors will occur, if `Option::is_none` is not a valid function.
/// Here the function `Vec::is_none` does not exist and therefore the example fails to compile.
///
/// ```rust,compile_fail
/// # use serde::Serialize;
/// # use serde_with_macros::skip_serializing_none;
/// use std::vec::Vec as Option;
///
/// #[skip_serializing_none]
/// #[derive(Serialize)]
/// struct Data {
///     a: Option<String>,
/// }
/// ```
#[proc_macro_attribute]
pub fn skip_serializing_none(_args: TokenStream, input: TokenStream) -> TokenStream {
    let res = match apply_function_to_struct_and_enum_fields(
        input,
        skip_serializing_none_add_attr_to_field,
    ) {
        Ok(res) => res,
        Err(err) => err.to_compile_error(),
    };
    TokenStream::from(res)
}

/// Add the skip_serializing_if annotation to each field of the struct
fn skip_serializing_none_add_attr_to_field(field: &mut Field) -> Result<(), String> {
    if let Type::Path(path) = &field.ty {
        if is_std_option(&path.path) {
            let has_skip_serializing_if =
                field_has_attribute(field, "serde", "skip_serializing_if");

            // Remove the `serialize_always` attribute
            let mut has_always_attr = false;
            field.attrs.retain(|attr| {
                let has_attr = attr.path.is_ident("serialize_always");
                has_always_attr |= has_attr;
                !has_attr
            });

            // Error on conflicting attributes
            if has_always_attr && has_skip_serializing_if {
                let mut msg = r#"The attributes `serialize_always` and `serde(skip_serializing_if = "...")` cannot be used on the same field"#.to_string();
                if let Some(ident) = &field.ident {
                    msg += ": `";
                    msg += &ident.to_string();
                    msg += "`";
                }
                msg += ".";
                return Err(msg);
            }

            // Do nothing if `skip_serializing_if` or `serialize_always` is already present
            if has_skip_serializing_if || has_always_attr {
                return Ok(());
            }

            // Add the `skip_serializing_if` attribute
            let attr = parse_quote!(
                #[serde(skip_serializing_if = "Option::is_none")]
            );
            field.attrs.push(attr);
        } else {
            // Warn on use of `serialize_always` on non-Option fields
            let has_attr = field
                .attrs
                .iter()
                .any(|attr| attr.path.is_ident("serialize_always"));
            if has_attr {
                return Err(
                    "`serialize_always` may only be used on fields of type `Option`.".into(),
                );
            }
        }
    }
    Ok(())
}

/// Return `true`, if the type path refers to `std::option::Option`
///
/// Accepts
///
/// * `Option`
/// * `std::option::Option`, with or without leading `::`
/// * `core::option::Option`, with or without leading `::`
fn is_std_option(path: &Path) -> bool {
    (path.leading_colon.is_none() && path.segments.len() == 1 && path.segments[0].ident == "Option")
        || (path.segments.len() == 3
            && (path.segments[0].ident == "std" || path.segments[0].ident == "core")
            && path.segments[1].ident == "option"
            && path.segments[2].ident == "Option")
}

/// Determine if the `field` has an attribute with given `namespace` and `name`
///
/// On the example of
/// `#[serde(skip_serializing_if = "Option::is_none")]`
///
/// * `serde` is the outermost path, here namespace
/// * it contains a Meta::List
/// * which contains in another Meta a Meta::NameValue
/// * with the name being `skip_serializing_if`
fn field_has_attribute(field: &Field, namespace: &str, name: &str) -> bool {
    for attr in &field.attrs {
        if attr.path.is_ident(namespace) {
            // Ignore non parsable attributes, as these are not important for us
            if let Ok(Meta::List(expr)) = attr.parse_meta() {
                for expr in expr.nested {
                    if let NestedMeta::Meta(Meta::NameValue(expr)) = expr {
                        if let Some(ident) = expr.path.get_ident() {
                            if *ident == name {
                                return true;
                            }
                        }
                    }
                }
            }
        }
    }
    false
}

/// Convenience macro to use the [`serde_as`] system.
///
/// The [`serde_as`] system is designed as a more flexible alternative to serde's with-annotation.
///
/// # Example
///
/// ```rust,ignore
/// use serde_with::{serde_as, DisplayFromStr};
/// use std::collections::HashMap;
///
/// #[serde_as]
/// #[derive(Serialize, Deserialize)]
/// struct Data {
///     /// Serialize into number
///     #[serde_as(as = "_")]
///     a: u32,
///
///     /// Serialize into String
///     #[serde_as(as = "DisplayFromStr")]
///     b: u32,
///
///     /// Serialize into a map from String to String
///     #[serde_as(as = "HashMap<DisplayFromStr, _>")]
///     c: Vec<(u32, String)>,
/// }
/// ```
///
/// # Alternative path to `serde_with` crate
///
/// If `serde_with` is not available at the default path, its path should be specified with the
/// `crate` argument. See [re-exporting `serde_as`] for more use case information.
///
/// ```rust,ignore
/// #[serde_as(crate = "::some_other_lib::serde_with")]
/// #[derive(Deserialize)]
/// struct Data {
///     #[serde_as(as = "_")]
///     a: u32,
/// }
/// ```
///
/// # What this macro does
///
/// The `serde_as` macro only serves a convenience function.
/// All the steps it performs, can easily be done manually, in case the cost of an attribute macro is deemed to high.
/// The functionality can best be described with an example.
///
/// ```rust,ignore
/// #[serde_as]
/// #[derive(serde::Serialize)]
/// struct Foo {
///     #[serde_as(as = "Vec<_>")]
///     bar: Vec<u32>,
/// }
/// ```
///
/// 1. All the placeholder type `_` will be replaced with `::serde_with::Same`.
///     The placeholder type `_` marks all the places where the types `Serialize` implementation should be used.
///     In the example, it means that the `u32` values will serialize with the `Serialize` implementation of `u32`.
///     The `Same` type implements `SerializeAs` whenever the underlying type implements `Serialize` and is used to make the two traits compatible.
///
///     If you specify a custom path for `serde_with` via the `crate` attribute, the path to the `Same` type will be altered accordingly.
/// 2. Wrap the type from the annotation inside a `::serde_with::As`.
///     In the above example we know have something like `::serde_with::As::<Vec<::serde_with::Same>>`.
///     The `As` type acts as the opposite of the `Same` type.
///     It allows using a `SerializeAs` type whenever a `Serialize` is required.
/// 3. Translate the `*as` attributes into the serde equivalent ones.
///     `#[serde_as(as = ...)]` will become `#[serde(with = ...)]`.
///     Similarly, `serialize_as` is translated to `serialize_with`.
///
///     The field attributes will be kept on the struct/enum such that other macros can use them too.
/// 4. It searches `#[serde_as(as = ...)]` if there is a type named `BorrowCow` under any path.
///     If `BorrowCow` is found, the attribute `#[serde(borrow)]` is added to the field.
///     If `#[serde(borrow)]` or `#[serde(borrow = "...")]` is already present, this step will be skipped.
///
/// After all these steps, the code snippet will have transformed into roughly this.
///
/// ```rust,ignore
/// #[derive(serde::Serialize)]
/// struct Foo {
///     #[serde_as(as = "Vec<_>")]
///     #[serde(with = "::serde_with::As::<Vec<::serde_with::Same>>")]
///     bar: Vec<u32>,
/// }
/// ```
///
/// [`serde_as`]: https://docs.rs/serde_with/1.12.1/serde_with/guide/index.html
/// [re-exporting `serde_as`]: https://docs.rs/serde_with/1.12.1/serde_with/guide/serde_as/index.html#re-exporting-serde_as
#[proc_macro_attribute]
pub fn serde_as(args: TokenStream, input: TokenStream) -> TokenStream {
    #[derive(FromMeta, Debug)]
    struct SerdeContainerOptions {
        #[darling(rename = "crate", default)]
        alt_crate_path: Option<String>,
    }

    let args: AttributeArgs = parse_macro_input!(args);
    let container_options = match SerdeContainerOptions::from_list(&args) {
        Ok(v) => v,
        Err(e) => {
            return TokenStream::from(e.write_errors());
        }
    };

    let serde_with_crate_path = container_options
        .alt_crate_path
        .as_deref()
        .unwrap_or("::serde_with");
    let serde_with_crate_path = match syn::parse_str(serde_with_crate_path) {
        Ok(path) => path,
        Err(err) => return TokenStream::from(DarlingError::from(err).write_errors()),
    };

    // Convert any error message into a nice compiler error
    let res = match apply_function_to_struct_and_enum_fields_darling(
        input,
        &serde_with_crate_path,
        |field| serde_as_add_attr_to_field(field, &serde_with_crate_path),
    ) {
        Ok(res) => res,
        Err(err) => err.write_errors(),
    };
    TokenStream::from(res)
}

/// Inspect the field and convert the `serde_as` attribute into the classical `serde`
fn serde_as_add_attr_to_field(
    field: &mut Field,
    serde_with_crate_path: &Path,
) -> Result<(), DarlingError> {
    #[derive(FromField, Debug)]
    #[darling(attributes(serde_as))]
    struct SerdeAsOptions {
        #[darling(rename = "as", default)]
        r#as: Option<Type>,
        #[darling(default)]
        deserialize_as: Option<Type>,
        #[darling(default)]
        serialize_as: Option<Type>,
    }

    impl SerdeAsOptions {
        fn has_any_set(&self) -> bool {
            self.r#as.is_some() || self.deserialize_as.is_some() || self.serialize_as.is_some()
        }
    }

    #[derive(FromField, Debug)]
    #[darling(attributes(serde), allow_unknown_fields)]
    struct SerdeOptions {
        #[darling(default)]
        with: Option<String>,
        #[darling(default)]
        deserialize_with: Option<String>,
        #[darling(default)]
        serialize_with: Option<String>,

        #[darling(default)]
        borrow: Option<Override<String>>,
    }

    impl SerdeOptions {
        fn has_any_set(&self) -> bool {
            self.with.is_some() || self.deserialize_with.is_some() || self.serialize_with.is_some()
        }
    }

    // Check if there even is any `serde_as` attribute and exit early if not.
    if !field
        .attrs
        .iter()
        .any(|attr| attr.path.is_ident("serde_as"))
    {
        return Ok(());
    }
    let serde_as_options = SerdeAsOptions::from_field(field)?;
    let serde_options = SerdeOptions::from_field(field)?;

    let mut errors = Vec::new();
    if !serde_as_options.has_any_set() {
        errors.push(DarlingError::custom("An empty `serde_as` attribute on a field has no effect. You are missing an `as`, `serialize_as`, or `deserialize_as` parameter."));
    }

    // Check if there are any conflicting attributes
    if serde_as_options.has_any_set() && serde_options.has_any_set() {
        errors.push(DarlingError::custom("Cannot combine `serde_as` with serde's `with`, `deserialize_with`, or `serialize_with`."));
    }

    if serde_as_options.r#as.is_some() && serde_as_options.deserialize_as.is_some() {
        errors.push(DarlingError::custom("Cannot combine `as` with `deserialize_as`. Use `serialize_as` to specify different serialization code."));
    } else if serde_as_options.r#as.is_some() && serde_as_options.serialize_as.is_some() {
        errors.push(DarlingError::custom("Cannot combine `as` with `serialize_as`. Use `deserialize_as` to specify different deserialization code."));
    }

    if !errors.is_empty() {
        return Err(DarlingError::multiple(errors));
    }

    let type_same = &syn::parse_quote!(#serde_with_crate_path::Same);
    let type_borrowcow = &syn::parse_quote!(BorrowCow);
    if let Some(type_) = serde_as_options.r#as {
        // If the field is not borrowed yet, check if we need to borrow it.
        if serde_options.borrow.is_none() && has_type_embedded(&type_, type_borrowcow) {
            let attr_borrow = parse_quote!(#[serde(borrow)]);
            field.attrs.push(attr_borrow);
        }

        let replacement_type = replace_infer_type_with_type(type_, type_same);
        let attr_inner_tokens = quote!(#serde_with_crate_path::As::<#replacement_type>).to_string();
        let attr = parse_quote!(#[serde(with = #attr_inner_tokens)]);
        field.attrs.push(attr);
    }
    if let Some(type_) = serde_as_options.deserialize_as {
        // If the field is not borrowed yet, check if we need to borrow it.
        if serde_options.borrow.is_none() && has_type_embedded(&type_, type_borrowcow) {
            let attr_borrow = parse_quote!(#[serde(borrow)]);
            field.attrs.push(attr_borrow);
        }

        let replacement_type = replace_infer_type_with_type(type_, type_same);
        let attr_inner_tokens =
            quote!(#serde_with_crate_path::As::<#replacement_type>::deserialize).to_string();
        let attr = parse_quote!(#[serde(deserialize_with = #attr_inner_tokens)]);
        field.attrs.push(attr);
    }
    if let Some(type_) = serde_as_options.serialize_as {
        let replacement_type = replace_infer_type_with_type(type_, type_same);
        let attr_inner_tokens =
            quote!(#serde_with_crate_path::As::<#replacement_type>::serialize).to_string();
        let attr = parse_quote!(#[serde(serialize_with = #attr_inner_tokens)]);
        field.attrs.push(attr);
    }

    Ok(())
}

/// Recursively replace all occurrences of `_` with `replacement` in a [Type][]
///
/// The [serde_as][macro@serde_as] macro allows to use the infer type, i.e., `_`, as shortcut for `serde_with::As`.
/// This function replaces all occurrences of the infer type with another type.
fn replace_infer_type_with_type(to_replace: Type, replacement: &Type) -> Type {
    match to_replace {
        // Base case
        // Replace the infer type with the actual replacement type
        Type::Infer(_) => replacement.clone(),

        // Recursive cases
        // Iterate through all positions where a type could occur and recursively call this function
        Type::Array(mut inner) => {
            *inner.elem = replace_infer_type_with_type(*inner.elem, replacement);
            Type::Array(inner)
        }
        Type::Group(mut inner) => {
            *inner.elem = replace_infer_type_with_type(*inner.elem, replacement);
            Type::Group(inner)
        }
        Type::Never(inner) => Type::Never(inner),
        Type::Paren(mut inner) => {
            *inner.elem = replace_infer_type_with_type(*inner.elem, replacement);
            Type::Paren(inner)
        }
        Type::Path(mut inner) => {
            match inner.path.segments.pop() {
                Some(Pair::End(mut t)) | Some(Pair::Punctuated(mut t, _)) => {
                    t.arguments = match t.arguments {
                        PathArguments::None => PathArguments::None,
                        PathArguments::AngleBracketed(mut inner) => {
                            // Iterate over the args between the angle brackets
                            inner.args = inner
                                .args
                                .into_iter()
                                .map(|generic_argument| match generic_argument {
                                    // replace types within the generics list, but leave other stuff like lifetimes untouched
                                    GenericArgument::Type(type_) => GenericArgument::Type(
                                        replace_infer_type_with_type(type_, replacement),
                                    ),
                                    ga => ga,
                                })
                                .collect();
                            PathArguments::AngleBracketed(inner)
                        }
                        PathArguments::Parenthesized(mut inner) => {
                            inner.inputs = inner
                                .inputs
                                .into_iter()
                                .map(|type_| replace_infer_type_with_type(type_, replacement))
                                .collect();
                            inner.output = match inner.output {
                                ReturnType::Type(arrow, mut type_) => {
                                    *type_ = replace_infer_type_with_type(*type_, replacement);
                                    ReturnType::Type(arrow, type_)
                                }
                                default => default,
                            };
                            PathArguments::Parenthesized(inner)
                        }
                    };
                    inner.path.segments.push(t);
                }
                None => {}
            }
            Type::Path(inner)
        }
        Type::Ptr(mut inner) => {
            *inner.elem = replace_infer_type_with_type(*inner.elem, replacement);
            Type::Ptr(inner)
        }
        Type::Reference(mut inner) => {
            *inner.elem = replace_infer_type_with_type(*inner.elem, replacement);
            Type::Reference(inner)
        }
        Type::Slice(mut inner) => {
            *inner.elem = replace_infer_type_with_type(*inner.elem, replacement);
            Type::Slice(inner)
        }
        Type::Tuple(mut inner) => {
            inner.elems = inner
                .elems
                .into_pairs()
                .map(|pair| match pair {
                    Pair::Punctuated(type_, p) => {
                        Pair::Punctuated(replace_infer_type_with_type(type_, replacement), p)
                    }
                    Pair::End(type_) => Pair::End(replace_infer_type_with_type(type_, replacement)),
                })
                .collect();
            Type::Tuple(inner)
        }

        // Pass unknown types or non-handleable types (e.g., bare Fn) without performing any replacements
        type_ => type_,
    }
}

/// Check if a type ending in the `syn::Ident` `embedded_type` is contained in `type_`.
fn has_type_embedded(type_: &Type, embedded_type: &syn::Ident) -> bool {
    match type_ {
        // Base cases
        Type::Infer(_) => false,
        Type::Never(_inner) => false,

        // Recursive cases
        // Iterate through all positions where a type could occur and recursively call this function
        Type::Array(inner) => has_type_embedded(&inner.elem, embedded_type),
        Type::Group(inner) => has_type_embedded(&inner.elem, embedded_type),
        Type::Paren(inner) => has_type_embedded(&inner.elem, embedded_type),
        Type::Path(inner) => {
            match inner.path.segments.last() {
                Some(t) => {
                    if t.ident == *embedded_type {
                        return true;
                    }

                    match &t.arguments {
                        PathArguments::None => false,
                        PathArguments::AngleBracketed(inner) => {
                            // Iterate over the args between the angle brackets
                            inner
                                .args
                                .iter()
                                .any(|generic_argument| match generic_argument {
                                    // replace types within the generics list, but leave other stuff like lifetimes untouched
                                    GenericArgument::Type(type_) => {
                                        has_type_embedded(type_, embedded_type)
                                    }
                                    _ga => false,
                                })
                        }
                        PathArguments::Parenthesized(inner) => {
                            inner
                                .inputs
                                .iter()
                                .any(|type_| has_type_embedded(type_, embedded_type))
                                || match &inner.output {
                                    ReturnType::Type(_arrow, type_) => {
                                        has_type_embedded(type_, embedded_type)
                                    }
                                    _default => false,
                                }
                        }
                    }
                }
                None => false,
            }
        }
        Type::Ptr(inner) => has_type_embedded(&inner.elem, embedded_type),
        Type::Reference(inner) => has_type_embedded(&inner.elem, embedded_type),
        Type::Slice(inner) => has_type_embedded(&inner.elem, embedded_type),
        Type::Tuple(inner) => inner.elems.pairs().any(|pair| match pair {
            Pair::Punctuated(type_, _) | Pair::End(type_) => {
                has_type_embedded(type_, embedded_type)
            }
        }),

        // Pass unknown types or non-handleable types (e.g., bare Fn) without performing any replacements
        _type_ => false,
    }
}

/// Deserialize value by using its [`FromStr`] implementation
///
/// This is an alternative way to implement `Deserialize` for types which also implement [`FromStr`] by deserializing the type from string.
/// Ensure that the struct/enum also implements [`FromStr`].
/// If the implementation is missing, you will get an error message like
/// ```text
/// error[E0277]: the trait bound `Struct: std::str::FromStr` is not satisfied
/// ```
/// Additionally, `FromStr::Err` **must** implement [`Display`] as otherwise you will see a rather unhelpful error message
///
/// Serialization with [`Display`] is available with the matching [`SerializeDisplay`] derive.
///
/// # Attributes
///
/// Attributes for the derive can be specified via the `#[serde_with(...)]` attribute on the struct or enum.
/// Currently, these arguments to the attribute are possible:
///
/// * **`#[serde_with(crate = "...")]`**: This allows using `DeserializeFromStr` when `serde_with` is not available from the crate root.
///     This happens while [renaming dependencies in Cargo.toml][cargo-toml-rename] or when re-exporting the macro from a different crate.
///
///     This argument is analogue to [serde's crate argument][serde-crate] and the [crate argument to `serde_as`][serde-as-crate].
///
/// # Example
///
/// ```rust,ignore
/// use std::str::FromStr;
///
/// #[derive(DeserializeFromStr)]
/// struct A {
///     a: u32,
///     b: bool,
/// }
///
/// impl FromStr for A {
///     type Err = String;
///
///     /// Parse a value like `123<>true`
///     fn from_str(s: &str) -> Result<Self, Self::Err> {
///         let mut parts = s.split("<>");
///         let number = parts
///             .next()
///             .ok_or_else(|| "Missing first value".to_string())?
///             .parse()
///             .map_err(|err: ParseIntError| err.to_string())?;
///         let bool = parts
///             .next()
///             .ok_or_else(|| "Missing second value".to_string())?
///             .parse()
///             .map_err(|err: ParseBoolError| err.to_string())?;
///         Ok(Self { a: number, b: bool })
///     }
/// }
///
/// let a: A = serde_json::from_str("\"159<>true\"").unwrap();
/// assert_eq!(A { a: 159, b: true }, a);
/// ```
///
/// [`Display`]: std::fmt::Display
/// [`FromStr`]: std::str::FromStr
/// [cargo-toml-rename]: https://doc.rust-lang.org/cargo/reference/specifying-dependencies.html#renaming-dependencies-in-cargotoml
/// [serde-as-crate]: https://docs.rs/serde_with/1.12.1/serde_with/guide/serde_as/index.html#re-exporting-serde_as
/// [serde-crate]: https://serde.rs/container-attrs.html#crate
#[proc_macro_derive(DeserializeFromStr, attributes(serde_with))]
pub fn derive_deserialize_fromstr(item: TokenStream) -> TokenStream {
    let input: DeriveInput = parse_macro_input!(item);
    let derive_options = match DeriveOptions::from_derive_input(&input) {
        Ok(opt) => opt,
        Err(err) => {
            return err;
        }
    };
    TokenStream::from(deserialize_fromstr(
        input,
        derive_options.get_serde_with_path(),
    ))
}

fn deserialize_fromstr(mut input: DeriveInput, serde_with_crate_path: Path) -> TokenStream2 {
    let ident = input.ident;
    let where_clause = &mut input.generics.make_where_clause().predicates;
    where_clause.push(parse_quote!(Self: ::std::str::FromStr));
    where_clause.push(parse_quote!(
        <Self as ::std::str::FromStr>::Err: ::std::fmt::Display
    ));
    let (de_impl_generics, ty_generics, where_clause) = split_with_de_lifetime(&input.generics);
    quote! {
        #[automatically_derived]
        impl #de_impl_generics #serde_with_crate_path::serde::Deserialize<'de> for #ident #ty_generics #where_clause {
            fn deserialize<D>(deserializer: D) -> ::std::result::Result<Self, D::Error>
            where
                D: #serde_with_crate_path::serde::Deserializer<'de>,
            {
                struct Helper<S>(::std::marker::PhantomData<S>);

                impl<'de, S> #serde_with_crate_path::serde::de::Visitor<'de> for Helper<S>
                where
                    S: ::std::str::FromStr,
                    <S as ::std::str::FromStr>::Err: ::std::fmt::Display,
                {
                    type Value = S;

                    fn expecting(&self, formatter: &mut ::std::fmt::Formatter<'_>) -> ::std::fmt::Result {
                        ::std::write!(formatter, "string")
                    }

                    fn visit_str<E>(self, value: &str) -> ::std::result::Result<Self::Value, E>
                    where
                        E: #serde_with_crate_path::serde::de::Error,
                    {
                        value.parse::<Self::Value>().map_err(#serde_with_crate_path::serde::de::Error::custom)
                    }

                    fn visit_bytes<E>(self, value: &[u8]) -> ::std::result::Result<Self::Value, E>
                    where
                        E: #serde_with_crate_path::serde::de::Error,
                    {
                        let utf8 = ::std::str::from_utf8(value).map_err(#serde_with_crate_path::serde::de::Error::custom)?;
                        self.visit_str(utf8)
                    }
                }

                deserializer.deserialize_str(Helper(::std::marker::PhantomData))
            }
        }
    }
}

/// Serialize value by using it's [`Display`] implementation
///
/// This is an alternative way to implement `Serialize` for types which also implement [`Display`] by serializing the type as string.
/// Ensure that the struct/enum also implements [`Display`].
/// If the implementation is missing, you will get an error message like
/// ```text
/// error[E0277]: `Struct` doesn't implement `std::fmt::Display`
/// ```
///
/// Deserialization with [`FromStr`] is available with the matching [`DeserializeFromStr`] derive.
///
/// # Attributes
///
/// Attributes for the derive can be specified via the `#[serde_with(...)]` attribute on the struct or enum.
/// Currently, these arguments to the attribute are possible:
///
/// * **`#[serde_with(crate = "...")]`**: This allows using `SerializeDisplay` when `serde_with` is not available from the crate root.
///     This happens while [renaming dependencies in Cargo.toml][cargo-toml-rename] or when re-exporting the macro from a different crate.
///
///     This argument is analogue to [serde's crate argument][serde-crate] and the [crate argument to `serde_as`][serde-as-crate].
///
/// # Example
///
/// ```rust,ignore
/// use std::fmt;
///
/// #[derive(SerializeDisplay)]
/// struct A {
///     a: u32,
///     b: bool,
/// }
///
/// impl fmt::Display for A {
///     fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
///         write!(f, "{}<>{}", self.a, self.b)
///     }
/// }
///
/// let a = A { a: 123, b: false };
/// assert_eq!(r#""123<>false""#, serde_json::to_string(&a).unwrap());
/// ```
///
/// [`Display`]: std::fmt::Display
/// [`FromStr`]: std::str::FromStr
/// [cargo-toml-rename]: https://doc.rust-lang.org/cargo/reference/specifying-dependencies.html#renaming-dependencies-in-cargotoml
/// [serde-as-crate]: https://docs.rs/serde_with/1.12.1/serde_with/guide/serde_as/index.html#re-exporting-serde_as
/// [serde-crate]: https://serde.rs/container-attrs.html#crate
#[proc_macro_derive(SerializeDisplay, attributes(serde_with))]
pub fn derive_serialize_display(item: TokenStream) -> TokenStream {
    let input: DeriveInput = parse_macro_input!(item);
    let derive_options = match DeriveOptions::from_derive_input(&input) {
        Ok(opt) => opt,
        Err(err) => {
            return err;
        }
    };
    TokenStream::from(serialize_display(
        input,
        derive_options.get_serde_with_path(),
    ))
}

fn serialize_display(mut input: DeriveInput, serde_with_crate_path: Path) -> TokenStream2 {
    let ident = input.ident;
    input
        .generics
        .make_where_clause()
        .predicates
        .push(parse_quote!(Self: ::std::fmt::Display));
    let (impl_generics, ty_generics, where_clause) = input.generics.split_for_impl();
    quote! {
        #[automatically_derived]
        impl #impl_generics #serde_with_crate_path::serde::Serialize for #ident #ty_generics #where_clause {
            fn serialize<S>(&self, serializer: S) -> ::std::result::Result<S::Ok, S::Error>
            where
                S: #serde_with_crate_path::serde::Serializer,
            {
                serializer.collect_str(&self)
            }
        }
    }
}

#[doc(hidden)]
/// Private function. Not part of the public API
///
/// The only task of this derive macro is to consume any `serde_as` attributes and turn them into inert attributes.
/// This allows the serde_as macro to keep the field attributes without causing compiler errors.
/// The intend is that keeping the field attributes allows downstream crates to consume and act on them without causing an ordering dependency to the serde_as macro.
/// Otherwise, downstream proc-macros would need to be placed *in front of* the main `#[serde_as]` attribute, since otherwise the field attributes would already be stripped off.
///
/// More details about the use-cases in the Github discussion: <https://github.com/jonasbb/serde_with/discussions/260>.
#[proc_macro_derive(__private_consume_serde_as_attributes, attributes(serde_as))]
pub fn __private_consume_serde_as_attributes(_: TokenStream) -> TokenStream {
    TokenStream::new()
}
