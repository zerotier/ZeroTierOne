// (c) 2020-2022 ZeroTier, Inc. -- currently proprietary pending actual release and licensing. See LICENSE.md.

pub mod rootset;

pub struct Flags {
    pub json_output: bool,
    pub base_path: String,
    pub auth_token_path_override: Option<String>,
    pub auth_token_override: Option<String>,
}
