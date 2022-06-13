/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::collections::HashMap;
use std::mem;

use crate::bindgen::ir::{Enum, GenericPath, OpaqueItem, Path, Struct, Type, Typedef, Union};
use crate::bindgen::library::Library;

#[derive(Default, Clone, Debug)]
pub struct Monomorphs {
    replacements: HashMap<GenericPath, Path>,
    opaques: Vec<OpaqueItem>,
    structs: Vec<Struct>,
    unions: Vec<Union>,
    typedefs: Vec<Typedef>,
    enums: Vec<Enum>,
}

impl Monomorphs {
    pub fn contains(&self, path: &GenericPath) -> bool {
        self.replacements.contains_key(path)
    }

    pub fn insert_struct(
        &mut self,
        library: &Library,
        generic: &Struct,
        monomorph: Struct,
        parameters: Vec<Type>,
    ) {
        let replacement_path = GenericPath::new(generic.path.clone(), parameters);

        debug_assert!(generic.generic_params.len() > 0);
        debug_assert!(!self.contains(&replacement_path));

        self.replacements
            .insert(replacement_path, monomorph.path.clone());

        monomorph.add_monomorphs(library, self);

        self.structs.push(monomorph);
    }

    pub fn insert_enum(
        &mut self,
        library: &Library,
        generic: &Enum,
        monomorph: Enum,
        parameters: Vec<Type>,
    ) {
        let replacement_path = GenericPath::new(generic.path.clone(), parameters);

        debug_assert!(generic.generic_params.len() > 0);
        debug_assert!(!self.contains(&replacement_path));

        self.replacements
            .insert(replacement_path, monomorph.path.clone());

        monomorph.add_monomorphs(library, self);

        self.enums.push(monomorph);
    }

    pub fn insert_union(
        &mut self,
        library: &Library,
        generic: &Union,
        monomorph: Union,
        parameters: Vec<Type>,
    ) {
        let replacement_path = GenericPath::new(generic.path.clone(), parameters);

        debug_assert!(generic.generic_params.len() > 0);
        debug_assert!(!self.contains(&replacement_path));

        self.replacements
            .insert(replacement_path, monomorph.path.clone());

        monomorph.add_monomorphs(library, self);

        self.unions.push(monomorph);
    }

    pub fn insert_opaque(
        &mut self,
        generic: &OpaqueItem,
        monomorph: OpaqueItem,
        parameters: Vec<Type>,
    ) {
        let replacement_path = GenericPath::new(generic.path.clone(), parameters);

        debug_assert!(generic.generic_params.len() > 0);
        debug_assert!(!self.contains(&replacement_path));

        self.replacements
            .insert(replacement_path, monomorph.path.clone());
        self.opaques.push(monomorph);
    }

    pub fn insert_typedef(
        &mut self,
        library: &Library,
        generic: &Typedef,
        monomorph: Typedef,
        parameters: Vec<Type>,
    ) {
        let replacement_path = GenericPath::new(generic.path.clone(), parameters);

        debug_assert!(generic.generic_params.len() > 0);
        debug_assert!(!self.contains(&replacement_path));

        self.replacements
            .insert(replacement_path, monomorph.path.clone());

        monomorph.add_monomorphs(library, self);

        self.typedefs.push(monomorph);
    }

    pub fn mangle_path(&self, path: &GenericPath) -> Option<&Path> {
        self.replacements.get(path)
    }

    pub fn drain_opaques(&mut self) -> Vec<OpaqueItem> {
        mem::take(&mut self.opaques)
    }

    pub fn drain_structs(&mut self) -> Vec<Struct> {
        mem::take(&mut self.structs)
    }

    pub fn drain_unions(&mut self) -> Vec<Union> {
        mem::take(&mut self.unions)
    }

    pub fn drain_typedefs(&mut self) -> Vec<Typedef> {
        mem::take(&mut self.typedefs)
    }

    pub fn drain_enums(&mut self) -> Vec<Enum> {
        mem::take(&mut self.enums)
    }
}
