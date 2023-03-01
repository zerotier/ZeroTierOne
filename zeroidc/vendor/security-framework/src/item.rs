//! Support to search for items in a keychain.

use core_foundation::array::CFArray;
use core_foundation::base::{CFType, TCFType, ToVoid};
use core_foundation::boolean::CFBoolean;
use core_foundation::data::CFData;
use core_foundation::date::CFDate;
use core_foundation::dictionary::{CFDictionary, CFMutableDictionary};
use core_foundation::number::CFNumber;
use core_foundation::string::CFString;
use core_foundation_sys::base::{CFCopyDescription, CFGetTypeID, CFRelease, CFTypeRef};
use core_foundation_sys::string::CFStringRef;
use security_framework_sys::item::*;
use security_framework_sys::keychain_item::{SecItemCopyMatching, SecItemAdd};
use std::collections::HashMap;
use std::fmt;
use std::ptr;

use crate::base::Result;
use crate::certificate::SecCertificate;
use crate::cvt;
use crate::identity::SecIdentity;
use crate::key::SecKey;
#[cfg(target_os = "macos")]
use crate::os::macos::keychain::SecKeychain;

/// Specifies the type of items to search for.
#[derive(Debug, Copy, Clone)]
pub struct ItemClass(CFStringRef);

impl ItemClass {
    /// Look for `SecKeychainItem`s corresponding to generic passwords.
    #[inline(always)]
    #[must_use] pub fn generic_password() -> Self {
        unsafe { Self(kSecClassGenericPassword) }
    }

    /// Look for `SecKeychainItem`s corresponding to internet passwords.
    #[inline(always)]
    #[must_use] pub fn internet_password() -> Self {
        unsafe { Self(kSecClassInternetPassword) }
    }

    /// Look for `SecCertificate`s.
    #[inline(always)]
    #[must_use] pub fn certificate() -> Self {
        unsafe { Self(kSecClassCertificate) }
    }

    /// Look for `SecKey`s.
    #[inline(always)]
    #[must_use] pub fn key() -> Self {
        unsafe { Self(kSecClassKey) }
    }

    /// Look for `SecIdentity`s.
    #[inline(always)]
    #[must_use] pub fn identity() -> Self {
        unsafe { Self(kSecClassIdentity) }
    }

    #[inline]
    fn to_value(self) -> CFType {
        unsafe { CFType::wrap_under_get_rule(self.0.cast()) }
    }
}

/// Specifies the type of keys to search for.
#[derive(Debug, Copy, Clone)]
pub struct KeyClass(CFStringRef);

impl KeyClass {
    /// kSecAttrKeyClassPublic
    #[inline(always)]
    pub fn public() -> Self {
        unsafe { Self(kSecAttrKeyClassPublic) }
    }
    /// kSecAttrKeyClassPrivate
    #[inline(always)]
    pub fn private() -> Self {
        unsafe { Self(kSecAttrKeyClassPrivate) }
    }
    /// kSecAttrKeyClassSymmetric
    #[inline(always)]
    pub fn symmetric() -> Self {
        unsafe { Self(kSecAttrKeyClassSymmetric) }
    }

    #[inline]
    fn to_value(self) -> CFType {
        unsafe { CFType::wrap_under_get_rule(self.0 as *const _) }
    }
}

/// Specifies the number of results returned by a search
#[derive(Debug, Copy, Clone)]
pub enum Limit {
    /// Always return all results
    All,

    /// Return up to the specified number of results
    Max(i64),
}

impl Limit {
    #[inline]
    fn to_value(self) -> CFType {
        match self {
            Self::All => unsafe { CFString::wrap_under_get_rule(kSecMatchLimitAll).as_CFType() },
            Self::Max(l) => CFNumber::from(l).as_CFType(),
        }
    }
}

impl From<i64> for Limit {
    #[inline]
    fn from(limit: i64) -> Self {
        Self::Max(limit)
    }
}

/// A builder type to search for items in keychains.
#[derive(Default)]
pub struct ItemSearchOptions {
    #[cfg(target_os = "macos")]
    keychains: Option<CFArray<SecKeychain>>,
    #[cfg(not(target_os = "macos"))]
    keychains: Option<CFArray<CFType>>,
    class: Option<ItemClass>,
    key_class: Option<KeyClass>,
    load_refs: bool,
    load_attributes: bool,
    load_data: bool,
    limit: Option<Limit>,
    label: Option<CFString>,
    access_group: Option<CFString>,
    pub_key_hash: Option<CFData>,
    app_label: Option<CFData>,
}

#[cfg(target_os = "macos")]
impl crate::ItemSearchOptionsInternals for ItemSearchOptions {
    #[inline]
    fn keychains(&mut self, keychains: &[SecKeychain]) -> &mut Self {
        self.keychains = Some(CFArray::from_CFTypes(keychains));
        self
    }
}

impl ItemSearchOptions {
    /// Creates a new builder with default options.
    #[inline(always)]
    #[must_use] pub fn new() -> Self {
        Self::default()
    }

    /// Search only for items of the specified class.
    #[inline(always)]
    pub fn class(&mut self, class: ItemClass) -> &mut Self {
        self.class = Some(class);
        self
    }

    /// Search only for keys of the specified class. Also sets self.class to
    /// ItemClass::key().
    #[inline(always)]
    pub fn key_class(&mut self, key_class: KeyClass) -> &mut Self {
        self.class(ItemClass::key());
        self.key_class = Some(key_class);
        self
    }

    /// Load Security Framework objects (`SecCertificate`, `SecKey`, etc) for
    /// the results.
    #[inline(always)]
    pub fn load_refs(&mut self, load_refs: bool) -> &mut Self {
        self.load_refs = load_refs;
        self
    }

    /// Load Security Framework object attributes for
    /// the results.
    #[inline(always)]
    pub fn load_attributes(&mut self, load_attributes: bool) -> &mut Self {
        self.load_attributes = load_attributes;
        self
    }

    /// Load Security Framework objects data for
    /// the results.
    #[inline(always)]
    pub fn load_data(&mut self, load_data: bool) -> &mut Self {
        self.load_data = load_data;
        self
    }

    /// Limit the number of search results.
    ///
    /// If this is not called, the default limit is 1.
    #[inline(always)]
    pub fn limit<T: Into<Limit>>(&mut self, limit: T) -> &mut Self {
        self.limit = Some(limit.into());
        self
    }

    /// Search for an item with the given label.
    #[inline(always)]
    pub fn label(&mut self, label: &str) -> &mut Self {
        self.label = Some(CFString::new(label));
        self
    }

    /// Sets `kSecAttrAccessGroup` to `kSecAttrAccessGroupToken`
    #[inline(always)]
    pub fn access_group_token(&mut self) -> &mut Self {
        self.access_group = unsafe { Some(CFString::wrap_under_get_rule(kSecAttrAccessGroupToken)) };
        self
    }

    /// Search for a certificate with the given public key hash.
    /// 
    /// This is only compatible with [ItemClass::certificate], to search for 
    /// a key by public key hash use [ItemSearchOptions::application_label]
    /// instead.
    #[inline(always)]
    pub fn pub_key_hash(&mut self, pub_key_hash: &[u8]) -> &mut Self {
        self.pub_key_hash = Some(CFData::from_buffer(pub_key_hash));
        self
    }

    /// Search for a key with the given public key hash.
    /// 
    /// This is only compatible with [ItemClass::key], to search for a 
    /// certificate by the public key hash use [ItemSearchOptions::pub_key_hash] 
    /// instead.
    #[inline(always)]
    pub fn application_label(&mut self, app_label: &[u8]) -> &mut Self {
        self.app_label = Some(CFData::from_buffer(app_label));
        self
    }

    /// Search for objects.
    pub fn search(&self) -> Result<Vec<SearchResult>> {
        unsafe {
            let mut params = vec![];

            if let Some(ref keychains) = self.keychains {
                params.push((
                    CFString::wrap_under_get_rule(kSecMatchSearchList),
                    keychains.as_CFType(),
                ));
            }

            if let Some(class) = self.class {
                params.push((CFString::wrap_under_get_rule(kSecClass), class.to_value()));
            }

            if let Some(key_class) = self.key_class {
                params.push((CFString::wrap_under_get_rule(kSecAttrKeyClass), key_class.to_value()));
            }

            if self.load_refs {
                params.push((
                    CFString::wrap_under_get_rule(kSecReturnRef),
                    CFBoolean::true_value().as_CFType(),
                ));
            }

            if self.load_attributes {
                params.push((
                    CFString::wrap_under_get_rule(kSecReturnAttributes),
                    CFBoolean::true_value().as_CFType(),
                ));
            }

            if self.load_data {
                params.push((
                    CFString::wrap_under_get_rule(kSecReturnData),
                    CFBoolean::true_value().as_CFType(),
                ));
            }

            if let Some(limit) = self.limit {
                params.push((
                    CFString::wrap_under_get_rule(kSecMatchLimit),
                    limit.to_value(),
                ));
            }

            if let Some(ref label) = self.label {
                params.push((
                    CFString::wrap_under_get_rule(kSecAttrLabel),
                    label.as_CFType(),
                ));
            }

            if let Some(ref access_group) = self.access_group {
                params.push((
                    CFString::wrap_under_get_rule(kSecAttrAccessGroup),
                    access_group.as_CFType(),
                ));
            }

            if let Some(ref pub_key_hash) = self.pub_key_hash {
                params.push((
                    CFString::wrap_under_get_rule(kSecAttrPublicKeyHash),
                    pub_key_hash.as_CFType(),
                ));
            }

            if let Some(ref app_label) = self.app_label {
                params.push((
                    CFString::wrap_under_get_rule(kSecAttrApplicationLabel),
                    app_label.as_CFType(),
                ));
            }

            let params = CFDictionary::from_CFType_pairs(&params);

            let mut ret = ptr::null();
            cvt(SecItemCopyMatching(params.as_concrete_TypeRef(), &mut ret))?;
            if ret.is_null() {
                //  SecItemCopyMatching returns NULL if no load_* was specified,
                //  causing a segfault.
                return Ok(vec![]);
            }
            let type_id = CFGetTypeID(ret);

            let mut items = vec![];

            if type_id == CFArray::<CFType>::type_id() {
                let array: CFArray<CFType> = CFArray::wrap_under_create_rule(ret as *mut _);
                for item in array.iter() {
                    items.push(get_item(item.as_CFTypeRef()));
                }
            } else {
                items.push(get_item(ret));
                // This is a bit janky, but get_item uses wrap_under_get_rule
                // which bumps the refcount but we want create semantics
                CFRelease(ret);
            }

            Ok(items)
        }
    }
}

unsafe fn get_item(item: CFTypeRef) -> SearchResult {
    let type_id = CFGetTypeID(item);

    if type_id == CFData::type_id() {
        let data = CFData::wrap_under_get_rule(item as *mut _);
        let mut buf = Vec::new();
        buf.extend_from_slice(data.bytes());
        return SearchResult::Data(buf);
    }

    if type_id == CFDictionary::<*const u8, *const u8>::type_id() {
        return SearchResult::Dict(CFDictionary::wrap_under_get_rule(item as *mut _));
    }

    #[cfg(target_os = "macos")]
    {
        use crate::os::macos::keychain_item::SecKeychainItem;
        if type_id == SecKeychainItem::type_id() {
            return SearchResult::Ref(Reference::KeychainItem(
                SecKeychainItem::wrap_under_get_rule(item as *mut _),
            ));
        }
    }

    let reference = if type_id == SecCertificate::type_id() {
        Reference::Certificate(SecCertificate::wrap_under_get_rule(item as *mut _))
    } else if type_id == SecKey::type_id() {
        Reference::Key(SecKey::wrap_under_get_rule(item as *mut _))
    } else if type_id == SecIdentity::type_id() {
        Reference::Identity(SecIdentity::wrap_under_get_rule(item as *mut _))
    } else {
        panic!("Got bad type from SecItemCopyMatching: {}", type_id);
    };

    SearchResult::Ref(reference)
}

/// An enum including all objects whose references can be returned from a search.
/// Note that generic _Keychain Items_, such as passwords and preferences, do
/// not have specific object types; they are modeled using dictionaries and so
/// are available directly as search results in variant `SearchResult::Dict`.
#[derive(Debug)]
pub enum Reference {
    /// A `SecIdentity`.
    Identity(SecIdentity),
    /// A `SecCertificate`.
    Certificate(SecCertificate),
    /// A `SecKey`.
    Key(SecKey),
    /// A `SecKeychainItem`.
    ///
    /// Only defined on OSX
    #[cfg(target_os = "macos")]
    KeychainItem(crate::os::macos::keychain_item::SecKeychainItem),
    #[doc(hidden)]
    __NonExhaustive,
}

/// An individual search result.
pub enum SearchResult {
    /// A reference to the Security Framework object, if asked for.
    Ref(Reference),
    /// A dictionary of data about the Security Framework object, if asked for.
    Dict(CFDictionary),
    /// The Security Framework object as bytes, if asked for.
    Data(Vec<u8>),
    /// An unknown representation of the Security Framework object.
    Other,
}

impl fmt::Debug for SearchResult {
    #[cold]
    fn fmt(&self, fmt: &mut fmt::Formatter<'_>) -> fmt::Result {
        match *self {
            Self::Ref(ref reference) => fmt
                .debug_struct("SearchResult::Ref")
                .field("reference", reference)
                .finish(),
            Self::Data(ref buf) => fmt
                .debug_struct("SearchResult::Data")
                .field("data", buf)
                .finish(),
            Self::Dict(_) => {
                let mut debug = fmt.debug_struct("SearchResult::Dict");
                for (k, v) in self.simplify_dict().unwrap() {
                    debug.field(&k, &v);
                }
                debug.finish()
            }
            Self::Other => write!(fmt, "SearchResult::Other"),
        }
    }
}

impl SearchResult {
    /// If the search result is a `CFDict`, simplify that to a
    /// `HashMap<String, String>`. This transformation isn't
    /// comprehensive, it only supports `CFString`, `CFDate`, and `CFData`
    /// value types.
    #[must_use] pub fn simplify_dict(&self) -> Option<HashMap<String, String>> {
        match *self {
            Self::Dict(ref d) => unsafe {
                let mut retmap = HashMap::new();
                let (keys, values) = d.get_keys_and_values();
                for (k, v) in keys.iter().zip(values.iter()) {
                    let keycfstr = CFString::wrap_under_get_rule((*k).cast());
                    let val: String = match CFGetTypeID(*v) {
                        cfstring if cfstring == CFString::type_id() => {
                            format!("{}", CFString::wrap_under_get_rule((*v).cast()))
                        }
                        cfdata if cfdata == CFData::type_id() => {
                            let buf = CFData::wrap_under_get_rule((*v).cast());
                            let mut vec = Vec::new();
                            vec.extend_from_slice(buf.bytes());
                            format!("{}", String::from_utf8_lossy(&vec))
                        }
                        cfdate if cfdate == CFDate::type_id() => format!(
                            "{}",
                            CFString::wrap_under_create_rule(CFCopyDescription(*v))
                        ),
                        _ => String::from("unknown"),
                    };
                    retmap.insert(format!("{}", keycfstr), val);
                }
                Some(retmap)
            },
            _ => None,
        }
    }
}

/// Builder-pattern struct for specifying options for `add_item` (`SecAddItem`
/// wrapper).
///
/// When finished populating options, call `to_dictionary()` and pass the
/// resulting CFDictionary to `add_item`.
pub struct ItemAddOptions {
    /// The value (by ref or data) of the item to add, required.
    pub value: ItemAddValue,
    /// Optional kSecAttrLabel attribute.
    pub label: Option<String>,
    /// Optional keychain location.
    pub location: Option<Location>,
}

impl ItemAddOptions {
    /// Specifies the item to add.
    pub fn new(value: ItemAddValue) -> Self {
        Self{ value, label: None, location: None }
    }
    /// Specifies the kSecAttrLabel attribute.
    pub fn set_label(&mut self, label: impl Into<String>) -> &mut Self {
        self.label = Some(label.into());
        self
    }
    /// Specifies which keychain to add the item to.
    pub fn set_location(&mut self, location: Location) -> &mut Self {
        self.location = Some(location);
        self
    }
    /// Populates a CFDictionary to be passed to
    pub fn to_dictionary(&self) -> CFDictionary {
        let mut dict = CFMutableDictionary::from_CFType_pairs(&[]);

        let class_opt = match &self.value {
            ItemAddValue::Ref(ref_) => ref_.class(),
            ItemAddValue::Data { class, .. } => Some(*class),
        };
        if let Some(class) = class_opt {
            dict.add(&unsafe{kSecClass}.to_void(), &class.0.to_void());
        }

        let value_pair = match &self.value{
            ItemAddValue::Ref(ref_) => (unsafe {kSecValueRef}.to_void(), ref_.ref_()),
            ItemAddValue::Data { data, ..} => (unsafe {kSecValueData}.to_void(), data.to_void()),
        };
        dict.add(&value_pair.0, &value_pair.1);


        if let Some(location) = &self.location {
            match location{
                #[cfg(any(feature = "OSX_10_15", target_os="ios"))]
                Location::DataProtectionKeychain => {
                    dict.add(&unsafe { kSecUseDataProtectionKeychain }.to_void(), &CFBoolean::true_value().to_void());
                },
                #[cfg(target_os="macos")]
                Location::DefaultFileKeychain => {},
                #[cfg(target_os="macos")]
                Location::FileKeychain(keychain) => {
                    dict.add(&unsafe { kSecUseKeychain }.to_void(), &keychain.to_void());
                },
            }
        }

        let label = self.label.as_deref().map(CFString::from);
        if let Some(label) = &label {
            dict.add(&unsafe {kSecAttrLabel}.to_void(), &label.to_void());
        }

        dict.to_immutable()
    }
}

/// Value of an item to add to the keychain.
pub enum ItemAddValue {
    /// Pass item by Ref (kSecValueRef)
    Ref(AddRef),
    /// Pass item by Data (kSecValueData)
    Data{
        /// The item class (kSecClass).
        class: ItemClass,
        /// The item data.
        data: CFData
    },
}


/// Type of Ref to add to the keychain.
pub enum AddRef {
    /// SecKey
    Key(SecKey),
    /// SecIdentity
    Identity(SecIdentity),
    /// SecCertificate
    Certificate(SecCertificate),
}

impl AddRef {
    fn class(&self) -> Option<ItemClass> {
        match self {
            AddRef::Key(_) => Some(ItemClass::key()),
            //  kSecClass should not be specified when adding a SecIdentityRef:
            //  https://developer.apple.com/forums/thread/25751
            AddRef::Identity(_) => None,
            AddRef::Certificate(_) => Some(ItemClass::certificate()),
        }
    }
    fn ref_(&self) -> CFTypeRef {
        match self {
            AddRef::Key(key) => key.as_CFTypeRef(),
            AddRef::Identity(id) => id.as_CFTypeRef(),
            AddRef::Certificate(cert) => cert.as_CFTypeRef(),
        }
    }
}

/// Which keychain to add an item to.
///
/// <https://developer.apple.com/documentation/technotes/tn3137-on-mac-keychains>
pub enum Location {
    /// Store the item in the newer DataProtectionKeychain. This is the only
    /// keychain on iOS. On macOS, this is the newer and more consistent
    /// keychain implementation. Keys stored in the Secure Enclave _must_ use
    /// this keychain.
    ///
    /// This keychain requires the calling binary to be codesigned with
    /// entitlements for the KeychainAccessGroups it is supposed to
    /// access.
    #[cfg(any(feature = "OSX_10_15", target_os="ios"))]
    DataProtectionKeychain,
    /// Store the key in the default file-based keychain. On macOS, defaults to
    /// the Login keychain.
    #[cfg(target_os="macos")]
    DefaultFileKeychain,
    /// Store the key in a specific file-based keychain.
    #[cfg(target_os="macos")]
    FileKeychain(crate::os::macos::keychain::SecKeychain)
}

/// Translates to SecItemAdd. Use `ItemAddOptions` to build an `add_params`
/// `CFDictionary`.
pub fn add_item(add_params: CFDictionary) -> Result<()> {
    cvt(unsafe { SecItemAdd(add_params.as_concrete_TypeRef(), std::ptr::null_mut()) })
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn find_nothing() {
        assert!(ItemSearchOptions::new().search().is_err());
    }

    #[test]
    fn limit_two() {
        let results = ItemSearchOptions::new()
            .class(ItemClass::certificate())
            .limit(2)
            .search()
            .unwrap();
        assert_eq!(results.len(), 2);
    }

    #[test]
    fn limit_all() {
        let results = ItemSearchOptions::new()
            .class(ItemClass::certificate())
            .limit(Limit::All)
            .search()
            .unwrap();
        assert!(results.len() >= 2);
    }
}
