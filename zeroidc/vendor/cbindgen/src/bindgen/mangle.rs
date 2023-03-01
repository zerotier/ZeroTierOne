/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::bindgen::config::MangleConfig;
use crate::bindgen::ir::{Path, Type};
use crate::bindgen::rename::IdentifierType;

pub fn mangle_path(path: &Path, generic_values: &[Type], config: &MangleConfig) -> Path {
    Path::new(mangle_name(path.name(), generic_values, config))
}

pub fn mangle_name(name: &str, generic_values: &[Type], config: &MangleConfig) -> String {
    Mangler::new(name, generic_values, /* last = */ true, config).mangle()
}

enum Separator {
    OpeningAngleBracket = 1,
    Comma,
    ClosingAngleBracket,
    BeginMutPtr,
    BeginConstPtr,
    BeginFn,
    BetweenFnArg,
    EndFn,
}

struct Mangler<'a> {
    input: &'a str,
    generic_values: &'a [Type],
    output: String,
    last: bool,
    config: &'a MangleConfig,
}

impl<'a> Mangler<'a> {
    fn new(
        input: &'a str,
        generic_values: &'a [Type],
        last: bool,
        config: &'a MangleConfig,
    ) -> Self {
        Self {
            input,
            generic_values,
            output: String::new(),
            last,
            config,
        }
    }

    fn mangle(mut self) -> String {
        self.mangle_internal();
        self.output
    }

    fn push(&mut self, id: Separator) {
        let count = id as usize;
        let separator = if self.config.remove_underscores {
            ""
        } else {
            "_"
        };
        self.output.extend(std::iter::repeat(separator).take(count));
    }

    fn append_mangled_type(&mut self, ty: &Type, last: bool) {
        match *ty {
            Type::Path(ref generic) => {
                let sub_path =
                    Mangler::new(generic.export_name(), generic.generics(), last, self.config)
                        .mangle();

                self.output.push_str(
                    &self
                        .config
                        .rename_types
                        .apply(&sub_path, IdentifierType::Type),
                );
            }
            Type::Primitive(ref primitive) => {
                self.output.push_str(
                    &self
                        .config
                        .rename_types
                        .apply(primitive.to_repr_rust(), IdentifierType::Type),
                );
            }
            Type::Ptr {
                ref ty, is_const, ..
            } => {
                self.push(if is_const {
                    Separator::BeginConstPtr
                } else {
                    Separator::BeginMutPtr
                });
                self.append_mangled_type(&**ty, last);
            }
            Type::FuncPtr {
                ref ret, ref args, ..
            } => {
                self.push(Separator::BeginFn);
                self.append_mangled_type(&**ret, args.is_empty());
                for (i, arg) in args.iter().enumerate() {
                    self.push(Separator::BetweenFnArg);
                    let last = last && i == args.len() - 1;
                    self.append_mangled_type(&arg.1, last);
                }
                if !self.last {
                    self.push(Separator::EndFn);
                }
            }
            Type::Array(..) => {
                unimplemented!(
                    "Unable to mangle generic parameter {:?} for '{}'",
                    ty,
                    self.input
                );
            }
        }
    }

    fn mangle_internal(&mut self) {
        debug_assert!(self.output.is_empty());
        self.output = self.input.to_owned();
        if self.generic_values.is_empty() {
            return;
        }

        self.push(Separator::OpeningAngleBracket);
        for (i, ty) in self.generic_values.iter().enumerate() {
            if i != 0 {
                self.push(Separator::Comma);
            }
            let last = self.last && i == self.generic_values.len() - 1;
            self.append_mangled_type(ty, last);
        }

        // Skip writing the trailing '>' mangling when possible
        if !self.last {
            self.push(Separator::ClosingAngleBracket)
        }
    }
}

#[test]
fn generics() {
    use crate::bindgen::ir::{GenericPath, PrimitiveType};
    use crate::bindgen::rename::RenameRule::{self, PascalCase};

    fn float() -> Type {
        Type::Primitive(PrimitiveType::Float)
    }

    fn c_char() -> Type {
        Type::Primitive(PrimitiveType::Char)
    }

    fn path(path: &str) -> Type {
        generic_path(path, &[])
    }

    fn generic_path(path: &str, generics: &[Type]) -> Type {
        let path = Path::new(path);
        let generic_path = GenericPath::new(path, generics.to_owned());
        Type::Path(generic_path)
    }

    // Foo<f32> => Foo_f32
    assert_eq!(
        mangle_path(&Path::new("Foo"), &[float()], &MangleConfig::default()),
        Path::new("Foo_f32")
    );

    // Foo<Bar<f32>> => Foo_Bar_f32
    assert_eq!(
        mangle_path(
            &Path::new("Foo"),
            &[generic_path("Bar", &[float()])],
            &MangleConfig::default(),
        ),
        Path::new("Foo_Bar_f32")
    );

    // Foo<Bar> => Foo_Bar
    assert_eq!(
        mangle_path(&Path::new("Foo"), &[path("Bar")], &MangleConfig::default()),
        Path::new("Foo_Bar")
    );

    // Foo<Bar> => FooBar
    assert_eq!(
        mangle_path(
            &Path::new("Foo"),
            &[path("Bar")],
            &MangleConfig {
                remove_underscores: true,
                rename_types: RenameRule::None,
            }
        ),
        Path::new("FooBar")
    );

    // Foo<Bar<f32>> => FooBarF32
    assert_eq!(
        mangle_path(
            &Path::new("Foo"),
            &[generic_path("Bar", &[float()])],
            &MangleConfig {
                remove_underscores: true,
                rename_types: PascalCase,
            },
        ),
        Path::new("FooBarF32")
    );

    // Foo<Bar<c_char>> => FooBarCChar
    assert_eq!(
        mangle_path(
            &Path::new("Foo"),
            &[generic_path("Bar", &[c_char()])],
            &MangleConfig {
                remove_underscores: true,
                rename_types: PascalCase,
            },
        ),
        Path::new("FooBarCChar")
    );

    // Foo<Bar<T>> => Foo_Bar_T
    assert_eq!(
        mangle_path(
            &Path::new("Foo"),
            &[generic_path("Bar", &[path("T")])],
            &MangleConfig::default(),
        ),
        Path::new("Foo_Bar_T")
    );

    // Foo<Bar<T>, E> => Foo_Bar_T_____E
    assert_eq!(
        mangle_path(
            &Path::new("Foo"),
            &[generic_path("Bar", &[path("T")]), path("E")],
            &MangleConfig::default(),
        ),
        Path::new("Foo_Bar_T_____E")
    );

    // Foo<Bar<T>, Bar<E>> => Foo_Bar_T_____Bar_E
    assert_eq!(
        mangle_path(
            &Path::new("Foo"),
            &[
                generic_path("Bar", &[path("T")]),
                generic_path("Bar", &[path("E")]),
            ],
            &MangleConfig::default(),
        ),
        Path::new("Foo_Bar_T_____Bar_E")
    );

    // Foo<Bar<T>, E> => FooBarTE
    assert_eq!(
        mangle_path(
            &Path::new("Foo"),
            &[generic_path("Bar", &[path("T")]), path("E")],
            &MangleConfig {
                remove_underscores: true,
                rename_types: PascalCase,
            },
        ),
        Path::new("FooBarTE")
    );

    // Foo<Bar<T>, Bar<E>> => FooBarTBarE
    assert_eq!(
        mangle_path(
            &Path::new("Foo"),
            &[
                generic_path("Bar", &[path("T")]),
                generic_path("Bar", &[path("E")]),
            ],
            &MangleConfig {
                remove_underscores: true,
                rename_types: PascalCase,
            },
        ),
        Path::new("FooBarTBarE")
    );
}
