use super::{Endpoint, Identity};
use std::num::NonZeroI64;

pub trait VL1SystemInterface {
    fn event_node_is_up(&self);
    fn event_node_is_down(&self);
    fn event_identity_collision(&self);
    fn event_online_status_change(&self, online: bool);
    fn event_user_message(&self, source: &Identity, message_type: u64, message: &[u8]);
    fn load_node_identity(&self) -> Option<Vec<u8>>;
    fn save_node_identity(&self, _: &Identity, public: &[u8], secret: &[u8]);
    fn wire_send(&self, endpoint: &Endpoint, local_socket: Option<NonZeroI64>, local_interface: Option<NonZeroI64>, data: &[&[u8]], packet_ttl: u8) -> bool;
    fn check_path(&self, id: &Identity, endpoint: &Endpoint, local_socket: Option<NonZeroI64>, local_interface: Option<NonZeroI64>) -> bool;
    fn get_path_hints(&self, id: &Identity) -> Option<&[(&Endpoint, Option<NonZeroI64>, Option<NonZeroI64>)]>;
    fn time_ticks(&self) -> i64;
    fn time_clock(&self) -> i64;
}
