/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::borrow::Cow;
use std::io::Write;

use crate::bindgen::cdecl;
use crate::bindgen::config::{Config, Language};
use crate::bindgen::declarationtyperesolver::DeclarationTypeResolver;
use crate::bindgen::dependencies::Dependencies;
use crate::bindgen::ir::{GenericParams, GenericPath, Path};
use crate::bindgen::library::Library;
use crate::bindgen::monomorph::Monomorphs;
use crate::bindgen::utilities::IterHelpers;
use crate::bindgen::writer::{Source, SourceWriter};

#[derive(Debug, Clone, PartialEq, Eq, Hash, PartialOrd, Ord)]
pub enum PrimitiveType {
    Void,
    Bool,
    Char,
    SChar,
    UChar,
    Char32,
    Float,
    Double,
    VaList,
    PtrDiffT,
    Integer {
        zeroable: bool,
        signed: bool,
        kind: IntKind,
    },
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, PartialOrd, Ord)]
pub enum IntKind {
    Short,
    Int,
    Long,
    LongLong,
    SizeT,
    Size,
    B8,
    B16,
    B32,
    B64,
}

impl PrimitiveType {
    pub fn maybe(path: &str) -> Option<PrimitiveType> {
        Some(match path {
            "c_void" => PrimitiveType::Void,
            "c_char" => PrimitiveType::Char,
            "c_schar" => PrimitiveType::SChar,
            "c_uchar" => PrimitiveType::UChar,
            "c_float" => PrimitiveType::Float,
            "c_double" => PrimitiveType::Double,
            "ptrdiff_t" => PrimitiveType::PtrDiffT,
            "VaList" => PrimitiveType::VaList,
            "bool" => PrimitiveType::Bool,
            "char" => PrimitiveType::Char32,

            "f32" => PrimitiveType::Float,
            "f64" => PrimitiveType::Double,

            _ => {
                let (kind, signed) = match path {
                    "c_short" => (IntKind::Short, true),
                    "c_int" => (IntKind::Int, true),
                    "c_long" => (IntKind::Long, true),
                    "c_longlong" => (IntKind::LongLong, true),
                    "ssize_t" => (IntKind::SizeT, true),
                    "c_ushort" => (IntKind::Short, false),
                    "c_uint" => (IntKind::Int, false),
                    "c_ulong" => (IntKind::Long, false),
                    "c_ulonglong" => (IntKind::LongLong, false),
                    "size_t" => (IntKind::SizeT, false),

                    "isize" | "intptr_t" => (IntKind::Size, true),
                    "usize" | "uintptr_t" => (IntKind::Size, false),

                    "u8" | "uint8_t" => (IntKind::B8, false),
                    "u16" | "uint16_t" => (IntKind::B16, false),
                    "u32" | "uint32_t" => (IntKind::B32, false),
                    "u64" | "uint64_t" => (IntKind::B64, false),
                    "i8" | "int8_t" => (IntKind::B8, true),
                    "i16" | "int16_t" => (IntKind::B16, true),
                    "i32" | "int32_t" => (IntKind::B32, true),
                    "i64" | "int64_t" => (IntKind::B64, true),
                    _ => return None,
                };
                PrimitiveType::Integer {
                    zeroable: true,
                    signed,
                    kind,
                }
            }
        })
    }

    pub fn to_repr_rust(&self) -> &'static str {
        match *self {
            PrimitiveType::Bool => "bool",
            PrimitiveType::Void => "c_void",
            PrimitiveType::Char => "c_char",
            PrimitiveType::SChar => "c_schar",
            PrimitiveType::UChar => "c_uchar",
            PrimitiveType::Char32 => "char",
            PrimitiveType::Integer {
                kind,
                signed,
                zeroable: _,
            } => match kind {
                IntKind::Short => {
                    if signed {
                        "c_short"
                    } else {
                        "c_ushort"
                    }
                }
                IntKind::Int => {
                    if signed {
                        "c_int"
                    } else {
                        "c_uint"
                    }
                }
                IntKind::Long => {
                    if signed {
                        "c_long"
                    } else {
                        "c_ulong"
                    }
                }
                IntKind::LongLong => {
                    if signed {
                        "c_longlong"
                    } else {
                        "c_ulonglong"
                    }
                }
                IntKind::SizeT => {
                    if signed {
                        "ssize_t"
                    } else {
                        "size_t"
                    }
                }
                IntKind::Size => {
                    if signed {
                        "isize"
                    } else {
                        "usize"
                    }
                }
                IntKind::B8 => {
                    if signed {
                        "i8"
                    } else {
                        "u8"
                    }
                }
                IntKind::B16 => {
                    if signed {
                        "i16"
                    } else {
                        "u16"
                    }
                }
                IntKind::B32 => {
                    if signed {
                        "i32"
                    } else {
                        "u32"
                    }
                }
                IntKind::B64 => {
                    if signed {
                        "i64"
                    } else {
                        "u64"
                    }
                }
            },
            PrimitiveType::Float => "f32",
            PrimitiveType::Double => "f64",
            PrimitiveType::PtrDiffT => "ptrdiff_t",
            PrimitiveType::VaList => "va_list",
        }
    }

    pub fn to_repr_c(&self, config: &Config) -> &'static str {
        match *self {
            PrimitiveType::Void => "void",
            PrimitiveType::Bool => "bool",
            PrimitiveType::Char => "char",
            PrimitiveType::SChar => "signed char",
            PrimitiveType::UChar => "unsigned char",
            // NOTE: It'd be nice to use a char32_t, but:
            //
            //  * uchar.h is not present on mac (see #423).
            //
            //  * char32_t isn't required to be compatible with Rust's char, as
            //    the C++ spec only requires it to be the same size as
            //    uint_least32_t, which is _not_ guaranteed to be 4-bytes.
            //
            PrimitiveType::Char32 => "uint32_t",
            PrimitiveType::Integer {
                kind,
                signed,
                zeroable: _,
            } => match kind {
                IntKind::Short => {
                    if signed {
                        "short"
                    } else {
                        "unsigned short"
                    }
                }
                IntKind::Int => {
                    if signed {
                        "int"
                    } else {
                        "unsigned int"
                    }
                }
                IntKind::Long => {
                    if signed {
                        "long"
                    } else {
                        "unsigned long"
                    }
                }
                IntKind::LongLong => {
                    if signed {
                        "long long"
                    } else {
                        "unsigned long long"
                    }
                }
                IntKind::SizeT => {
                    if signed {
                        "ssize_t"
                    } else {
                        "size_t"
                    }
                }
                IntKind::Size => {
                    if config.usize_is_size_t {
                        if signed {
                            "ptrdiff_t"
                        } else {
                            "size_t"
                        }
                    } else if signed {
                        "intptr_t"
                    } else {
                        "uintptr_t"
                    }
                }
                IntKind::B8 => {
                    if signed {
                        "int8_t"
                    } else {
                        "uint8_t"
                    }
                }
                IntKind::B16 => {
                    if signed {
                        "int16_t"
                    } else {
                        "uint16_t"
                    }
                }
                IntKind::B32 => {
                    if signed {
                        "int32_t"
                    } else {
                        "uint32_t"
                    }
                }
                IntKind::B64 => {
                    if signed {
                        "int64_t"
                    } else {
                        "uint64_t"
                    }
                }
            },
            PrimitiveType::Float => "float",
            PrimitiveType::Double => "double",
            PrimitiveType::PtrDiffT => "ptrdiff_t",
            PrimitiveType::VaList => "va_list",
        }
    }

    fn can_cmp_order(&self) -> bool {
        match *self {
            PrimitiveType::Bool => false,
            _ => true,
        }
    }

    fn can_cmp_eq(&self) -> bool {
        true
    }
}

// The `U` part of `[T; U]`
#[derive(Debug, Clone, PartialEq, Eq, Hash, PartialOrd, Ord)]
pub enum ArrayLength {
    Name(String),
    Value(String),
}

impl ArrayLength {
    pub fn as_str(&self) -> &str {
        match self {
            ArrayLength::Name(ref string) | ArrayLength::Value(ref string) => string,
        }
    }

    fn rename_for_config(&mut self, config: &Config) {
        if let ArrayLength::Name(ref mut name) = self {
            config.export.rename(name);
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Hash, PartialOrd, Ord)]
pub enum Type {
    Ptr {
        ty: Box<Type>,
        is_const: bool,
        is_nullable: bool,
        // FIXME: This is a bit of a hack, this is only to get us to codegen
        // `T&` / `const T&`, but we should probably pass that down as an option
        // to code generation or something.
        is_ref: bool,
    },
    Path(GenericPath),
    Primitive(PrimitiveType),
    Array(Box<Type>, ArrayLength),
    FuncPtr {
        ret: Box<Type>,
        args: Vec<(Option<String>, Type)>,
        is_nullable: bool,
    },
}

impl Type {
    pub fn const_ref_to(ty: &Self) -> Self {
        Type::Ptr {
            ty: Box::new(ty.clone()),
            is_const: true,
            is_nullable: false,
            is_ref: true,
        }
    }

    pub fn load(ty: &syn::Type) -> Result<Option<Type>, String> {
        let converted = match *ty {
            syn::Type::Reference(ref reference) => {
                let converted = Type::load(&reference.elem)?;

                let converted = match converted {
                    Some(converted) => converted,
                    None => Type::Primitive(PrimitiveType::Void),
                };

                // TODO(emilio): we could make these use is_ref: true.
                let is_const = reference.mutability.is_none();
                Type::Ptr {
                    ty: Box::new(converted),
                    is_const,
                    is_nullable: false,
                    is_ref: false,
                }
            }
            syn::Type::Ptr(ref pointer) => {
                let converted = Type::load(&pointer.elem)?;

                let converted = match converted {
                    Some(converted) => converted,
                    None => Type::Primitive(PrimitiveType::Void),
                };

                let is_const = pointer.mutability.is_none();
                Type::Ptr {
                    ty: Box::new(converted),
                    is_const,
                    is_nullable: true,
                    is_ref: false,
                }
            }
            syn::Type::Path(ref path) => {
                let generic_path = GenericPath::load(&path.path)?;

                if generic_path.name() == "PhantomData" || generic_path.name() == "PhantomPinned" {
                    return Ok(None);
                }

                if let Some(prim) = PrimitiveType::maybe(generic_path.name()) {
                    if !generic_path.generics().is_empty() {
                        return Err("Primitive has generics.".to_owned());
                    }
                    Type::Primitive(prim)
                } else {
                    Type::Path(generic_path)
                }
            }
            syn::Type::Array(syn::TypeArray {
                ref elem,
                len: syn::Expr::Path(ref path),
                ..
            }) => {
                let converted = Type::load(elem)?;

                let converted = match converted {
                    Some(converted) => converted,
                    None => return Err("Cannot have an array of zero sized types.".to_owned()),
                };
                let generic_path = GenericPath::load(&path.path)?;
                let len = ArrayLength::Name(generic_path.export_name().to_owned());
                Type::Array(Box::new(converted), len)
            }
            syn::Type::Array(syn::TypeArray {
                ref elem,
                len:
                    syn::Expr::Lit(syn::ExprLit {
                        lit: syn::Lit::Int(ref len),
                        ..
                    }),
                ..
            }) => {
                let converted = Type::load(elem)?;

                let converted = match converted {
                    Some(converted) => converted,
                    None => return Err("Cannot have an array of zero sized types.".to_owned()),
                };

                let len = ArrayLength::Value(len.base10_digits().to_string());
                // panic!("panic -> value: {:?}", len);
                Type::Array(Box::new(converted), len)
            }
            syn::Type::BareFn(ref function) => {
                let mut wildcard_counter = 0;
                let args = function.inputs.iter().try_skip_map(|x| {
                    Type::load(&x.ty).map(|opt_ty| {
                        opt_ty.map(|ty| {
                            (
                                x.name.as_ref().map(|(ref ident, _)| {
                                    if ident == "_" {
                                        wildcard_counter += 1;
                                        if wildcard_counter == 1 {
                                            "_".to_owned()
                                        } else {
                                            format!("_{}", wildcard_counter - 1)
                                        }
                                    } else {
                                        ident.to_string()
                                    }
                                }),
                                ty,
                            )
                        })
                    })
                })?;
                let ret = match function.output {
                    syn::ReturnType::Default => Type::Primitive(PrimitiveType::Void),
                    syn::ReturnType::Type(_, ref ty) => {
                        if let Some(x) = Type::load(ty)? {
                            x
                        } else {
                            Type::Primitive(PrimitiveType::Void)
                        }
                    }
                };

                Type::FuncPtr {
                    ret: Box::new(ret),
                    args,
                    is_nullable: false,
                }
            }
            syn::Type::Tuple(ref tuple) => {
                if tuple.elems.is_empty() {
                    return Ok(None);
                }
                return Err("Tuples are not supported types.".to_owned());
            }
            _ => return Err(format!("Unsupported type: {:?}", ty)),
        };

        Ok(Some(converted))
    }

    pub fn is_primitive_or_ptr_primitive(&self) -> bool {
        match *self {
            Type::Primitive(..) => true,
            Type::Ptr { ref ty, .. } => match ty.as_ref() {
                Type::Primitive(..) => true,
                _ => false,
            },
            _ => false,
        }
    }

    pub fn make_zeroable(&self) -> Option<Self> {
        let (kind, signed) = match *self {
            Type::Primitive(PrimitiveType::Integer {
                zeroable: false,
                kind,
                signed,
            }) => (kind, signed),
            _ => return None,
        };

        Some(Type::Primitive(PrimitiveType::Integer {
            kind,
            signed,
            zeroable: true,
        }))
    }

    pub fn make_nullable(&self) -> Option<Self> {
        match *self {
            Type::Ptr {
                ref ty,
                is_const,
                is_ref,
                is_nullable: false,
            } => Some(Type::Ptr {
                ty: ty.clone(),
                is_const,
                is_ref,
                is_nullable: true,
            }),
            Type::FuncPtr {
                ref ret,
                ref args,
                is_nullable: false,
            } => Some(Type::FuncPtr {
                ret: ret.clone(),
                args: args.clone(),
                is_nullable: true,
            }),
            _ => None,
        }
    }

    fn nonzero_to_primitive(&self) -> Option<Self> {
        let path = match *self {
            Type::Path(ref p) => p,
            _ => return None,
        };

        if !path.generics().is_empty() {
            return None;
        }

        let name = path.name();
        if !name.starts_with("NonZero") {
            return None;
        }

        let (kind, signed) = match path.name() {
            "NonZeroU8" => (IntKind::B8, false),
            "NonZeroU16" => (IntKind::B16, false),
            "NonZeroU32" => (IntKind::B32, false),
            "NonZeroU64" => (IntKind::B64, false),
            "NonZeroUSize" => (IntKind::Size, false),
            "NonZeroI8" => (IntKind::B8, true),
            "NonZeroI16" => (IntKind::B16, true),
            "NonZeroI32" => (IntKind::B32, true),
            "NonZeroI64" => (IntKind::B64, true),
            "NonZeroISize" => (IntKind::Size, true),
            _ => return None,
        };

        Some(Type::Primitive(PrimitiveType::Integer {
            zeroable: false,
            signed,
            kind,
        }))
    }

    fn simplified_type(&self, config: &Config) -> Option<Self> {
        let path = match *self {
            Type::Path(ref p) => p,
            _ => return None,
        };

        if path.generics().is_empty() {
            return self.nonzero_to_primitive();
        }

        if path.generics().len() != 1 {
            return None;
        }

        let unsimplified_generic = &path.generics()[0];
        let generic = match unsimplified_generic.simplified_type(config) {
            Some(generic) => Cow::Owned(generic),
            None => Cow::Borrowed(unsimplified_generic),
        };
        match path.name() {
            "Option" => {
                if let Some(nullable) = generic.make_nullable() {
                    return Some(nullable);
                }
                if let Some(zeroable) = generic.make_zeroable() {
                    return Some(zeroable);
                }
                None
            }
            "NonNull" => Some(Type::Ptr {
                ty: Box::new(generic.into_owned()),
                is_const: false,
                is_nullable: false,
                is_ref: false,
            }),
            "Box" if config.language != Language::Cxx => Some(Type::Ptr {
                ty: Box::new(generic.into_owned()),
                is_const: false,
                is_nullable: false,
                is_ref: false,
            }),
            "Cell" => Some(generic.into_owned()),
            "ManuallyDrop" | "MaybeUninit" | "Pin" if config.language != Language::Cxx => {
                Some(generic.into_owned())
            }
            _ => None,
        }
    }

    pub fn simplify_standard_types(&mut self, config: &Config) {
        self.visit_types(|ty| ty.simplify_standard_types(config));
        if let Some(ty) = self.simplified_type(config) {
            *self = ty;
        }
    }

    pub fn replace_self_with(&mut self, self_ty: &Path) {
        if let Type::Path(ref mut generic_path) = *self {
            generic_path.replace_self_with(self_ty);
        }
        self.visit_types(|ty| ty.replace_self_with(self_ty))
    }

    fn visit_types(&mut self, mut visitor: impl FnMut(&mut Type)) {
        match *self {
            Type::Array(ref mut ty, ..) | Type::Ptr { ref mut ty, .. } => visitor(ty),
            Type::Path(ref mut path) => {
                for generic in path.generics_mut() {
                    visitor(generic);
                }
            }
            Type::Primitive(..) => {}
            Type::FuncPtr {
                ref mut ret,
                ref mut args,
                ..
            } => {
                visitor(ret);
                for arg in args {
                    visitor(&mut arg.1)
                }
            }
        }
    }

    pub fn get_root_path(&self) -> Option<Path> {
        let mut current = self;
        loop {
            match *current {
                Type::Ptr { ref ty, .. } => current = ty,
                Type::Path(ref generic) => {
                    return Some(generic.path().clone());
                }
                Type::Primitive(..) => {
                    return None;
                }
                Type::Array(..) => {
                    return None;
                }
                Type::FuncPtr { .. } => {
                    return None;
                }
            };
        }
    }

    pub fn specialize(&self, mappings: &[(&Path, &Type)]) -> Type {
        match *self {
            Type::Ptr {
                ref ty,
                is_const,
                is_nullable,
                is_ref,
            } => Type::Ptr {
                ty: Box::new(ty.specialize(mappings)),
                is_const,
                is_nullable,
                is_ref,
            },
            Type::Path(ref generic_path) => {
                for &(param, value) in mappings {
                    if generic_path.path() == param {
                        return value.clone();
                    }
                }

                let specialized = GenericPath::new(
                    generic_path.path().clone(),
                    generic_path
                        .generics()
                        .iter()
                        .map(|x| x.specialize(mappings))
                        .collect(),
                );
                Type::Path(specialized)
            }
            Type::Primitive(ref primitive) => Type::Primitive(primitive.clone()),
            Type::Array(ref ty, ref constant) => {
                Type::Array(Box::new(ty.specialize(mappings)), constant.clone())
            }
            Type::FuncPtr {
                ref ret,
                ref args,
                is_nullable,
            } => Type::FuncPtr {
                ret: Box::new(ret.specialize(mappings)),
                args: args
                    .iter()
                    .cloned()
                    .map(|(name, ty)| (name, ty.specialize(mappings)))
                    .collect(),
                is_nullable,
            },
        }
    }

    pub fn add_dependencies_ignoring_generics(
        &self,
        generic_params: &GenericParams,
        library: &Library,
        out: &mut Dependencies,
    ) {
        match *self {
            Type::Ptr { ref ty, .. } => {
                ty.add_dependencies_ignoring_generics(generic_params, library, out);
            }
            Type::Path(ref generic) => {
                for generic_value in generic.generics() {
                    generic_value.add_dependencies_ignoring_generics(generic_params, library, out);
                }
                let path = generic.path();
                if !generic_params.contains(path) {
                    if let Some(items) = library.get_items(path) {
                        if !out.items.contains(path) {
                            out.items.insert(path.clone());

                            for item in &items {
                                item.deref().add_dependencies(library, out);
                            }
                            for item in items {
                                out.order.push(item);
                            }
                        }
                    } else {
                        warn!(
                            "Can't find {}. This usually means that this type was incompatible or \
                             not found.",
                            path
                        );
                    }
                }
            }
            Type::Primitive(_) => {}
            Type::Array(ref ty, _) => {
                ty.add_dependencies_ignoring_generics(generic_params, library, out);
            }
            Type::FuncPtr {
                ref ret, ref args, ..
            } => {
                ret.add_dependencies_ignoring_generics(generic_params, library, out);
                for (_, ref arg) in args {
                    arg.add_dependencies_ignoring_generics(generic_params, library, out);
                }
            }
        }
    }

    pub fn add_dependencies(&self, library: &Library, out: &mut Dependencies) {
        self.add_dependencies_ignoring_generics(&GenericParams::default(), library, out)
    }

    pub fn add_monomorphs(&self, library: &Library, out: &mut Monomorphs) {
        match *self {
            Type::Ptr { ref ty, .. } => {
                ty.add_monomorphs(library, out);
            }
            Type::Path(ref generic) => {
                if generic.generics().is_empty() || out.contains(generic) {
                    return;
                }
                let path = generic.path();
                if let Some(items) = library.get_items(path) {
                    for item in items {
                        item.deref()
                            .instantiate_monomorph(generic.generics(), library, out);
                    }
                }
            }
            Type::Primitive(_) => {}
            Type::Array(ref ty, _) => {
                ty.add_monomorphs(library, out);
            }
            Type::FuncPtr {
                ref ret, ref args, ..
            } => {
                ret.add_monomorphs(library, out);
                for (_, ref arg) in args {
                    arg.add_monomorphs(library, out);
                }
            }
        }
    }

    pub fn rename_for_config(&mut self, config: &Config, generic_params: &GenericParams) {
        match *self {
            Type::Ptr { ref mut ty, .. } => {
                ty.rename_for_config(config, generic_params);
            }
            Type::Path(ref mut ty) => {
                ty.rename_for_config(config, generic_params);
            }
            Type::Primitive(_) => {}
            Type::Array(ref mut ty, ref mut len) => {
                ty.rename_for_config(config, generic_params);
                len.rename_for_config(config);
            }
            Type::FuncPtr {
                ref mut ret,
                ref mut args,
                ..
            } => {
                ret.rename_for_config(config, generic_params);
                for (_, arg) in args {
                    arg.rename_for_config(config, generic_params);
                }
            }
        }
    }

    pub fn resolve_declaration_types(&mut self, resolver: &DeclarationTypeResolver) {
        match *self {
            Type::Ptr { ref mut ty, .. } => {
                ty.resolve_declaration_types(resolver);
            }
            Type::Path(ref mut generic_path) => {
                generic_path.resolve_declaration_types(resolver);
            }
            Type::Primitive(_) => {}
            Type::Array(ref mut ty, _) => {
                ty.resolve_declaration_types(resolver);
            }
            Type::FuncPtr {
                ref mut ret,
                ref mut args,
                ..
            } => {
                ret.resolve_declaration_types(resolver);
                for (_, ref mut arg) in args {
                    arg.resolve_declaration_types(resolver);
                }
            }
        }
    }

    pub fn mangle_paths(&mut self, monomorphs: &Monomorphs) {
        match *self {
            Type::Ptr { ref mut ty, .. } => {
                ty.mangle_paths(monomorphs);
            }
            Type::Path(ref mut generic_path) => {
                if generic_path.generics().is_empty() {
                    return;
                }

                if let Some(mangled_path) = monomorphs.mangle_path(generic_path) {
                    *generic_path = GenericPath::new(mangled_path.clone(), vec![]);
                } else {
                    warn!(
                        "Cannot find a mangling for generic path {:?}. This usually means that a \
                         type referenced by this generic was incompatible or not found.",
                        generic_path
                    );
                }
            }
            Type::Primitive(_) => {}
            Type::Array(ref mut ty, _) => {
                ty.mangle_paths(monomorphs);
            }
            Type::FuncPtr {
                ref mut ret,
                ref mut args,
                ..
            } => {
                ret.mangle_paths(monomorphs);
                for (_, ref mut arg) in args {
                    arg.mangle_paths(monomorphs);
                }
            }
        }
    }

    pub fn can_cmp_order(&self) -> bool {
        match *self {
            // FIXME: Shouldn't this look at ty.can_cmp_order() as well?
            Type::Ptr { is_ref, .. } => !is_ref,
            Type::Path(..) => true,
            Type::Primitive(ref p) => p.can_cmp_order(),
            Type::Array(..) => false,
            Type::FuncPtr { .. } => false,
        }
    }

    pub fn can_cmp_eq(&self) -> bool {
        match *self {
            Type::Ptr { ref ty, is_ref, .. } => !is_ref || ty.can_cmp_eq(),
            Type::Path(..) => true,
            Type::Primitive(ref p) => p.can_cmp_eq(),
            Type::Array(..) => false,
            Type::FuncPtr { .. } => true,
        }
    }
}

impl Source for String {
    fn write<F: Write>(&self, _config: &Config, out: &mut SourceWriter<F>) {
        write!(out, "{}", self);
    }
}

impl Source for Type {
    fn write<F: Write>(&self, config: &Config, out: &mut SourceWriter<F>) {
        cdecl::write_type(out, self, config);
    }
}
