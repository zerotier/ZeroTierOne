#![allow(unused_imports)]
use super::*;
use wasm_bindgen::prelude::*;
#[wasm_bindgen]
extern "C" {
    # [wasm_bindgen (extends = :: js_sys :: Object , js_name = UnderlyingSource)]
    #[derive(Debug, Clone, PartialEq, Eq)]
    #[doc = "The `UnderlyingSource` dictionary."]
    #[doc = ""]
    #[doc = "*This API requires the following crate features to be activated: `UnderlyingSource`*"]
    pub type UnderlyingSource;
}
impl UnderlyingSource {
    #[doc = "Construct a new `UnderlyingSource`."]
    #[doc = ""]
    #[doc = "*This API requires the following crate features to be activated: `UnderlyingSource`*"]
    pub fn new() -> Self {
        #[allow(unused_mut)]
        let mut ret: Self = ::wasm_bindgen::JsCast::unchecked_into(::js_sys::Object::new());
        ret
    }
    #[doc = "Change the `autoAllocateChunkSize` field of this object."]
    #[doc = ""]
    #[doc = "*This API requires the following crate features to be activated: `UnderlyingSource`*"]
    pub fn auto_allocate_chunk_size(&mut self, val: f64) -> &mut Self {
        use wasm_bindgen::JsValue;
        let r = ::js_sys::Reflect::set(
            self.as_ref(),
            &JsValue::from("autoAllocateChunkSize"),
            &JsValue::from(val),
        );
        debug_assert!(
            r.is_ok(),
            "setting properties should never fail on our dictionary objects"
        );
        let _ = r;
        self
    }
    #[doc = "Change the `cancel` field of this object."]
    #[doc = ""]
    #[doc = "*This API requires the following crate features to be activated: `UnderlyingSource`*"]
    pub fn cancel(&mut self, val: &::js_sys::Function) -> &mut Self {
        use wasm_bindgen::JsValue;
        let r =
            ::js_sys::Reflect::set(self.as_ref(), &JsValue::from("cancel"), &JsValue::from(val));
        debug_assert!(
            r.is_ok(),
            "setting properties should never fail on our dictionary objects"
        );
        let _ = r;
        self
    }
    #[doc = "Change the `pull` field of this object."]
    #[doc = ""]
    #[doc = "*This API requires the following crate features to be activated: `UnderlyingSource`*"]
    pub fn pull(&mut self, val: &::js_sys::Function) -> &mut Self {
        use wasm_bindgen::JsValue;
        let r = ::js_sys::Reflect::set(self.as_ref(), &JsValue::from("pull"), &JsValue::from(val));
        debug_assert!(
            r.is_ok(),
            "setting properties should never fail on our dictionary objects"
        );
        let _ = r;
        self
    }
    #[doc = "Change the `start` field of this object."]
    #[doc = ""]
    #[doc = "*This API requires the following crate features to be activated: `UnderlyingSource`*"]
    pub fn start(&mut self, val: &::js_sys::Function) -> &mut Self {
        use wasm_bindgen::JsValue;
        let r = ::js_sys::Reflect::set(self.as_ref(), &JsValue::from("start"), &JsValue::from(val));
        debug_assert!(
            r.is_ok(),
            "setting properties should never fail on our dictionary objects"
        );
        let _ = r;
        self
    }
    #[cfg(feature = "ReadableStreamType")]
    #[doc = "Change the `type` field of this object."]
    #[doc = ""]
    #[doc = "*This API requires the following crate features to be activated: `ReadableStreamType`, `UnderlyingSource`*"]
    pub fn type_(&mut self, val: ReadableStreamType) -> &mut Self {
        use wasm_bindgen::JsValue;
        let r = ::js_sys::Reflect::set(self.as_ref(), &JsValue::from("type"), &JsValue::from(val));
        debug_assert!(
            r.is_ok(),
            "setting properties should never fail on our dictionary objects"
        );
        let _ = r;
        self
    }
}
impl Default for UnderlyingSource {
    fn default() -> Self {
        Self::new()
    }
}
