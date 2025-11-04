//! Stub backend builds on any platform without pulling LiveKit deps.

use std::os::raw::{c_char, c_int, c_void};
use std::ffi::CString;
use std::sync::{Arc, Mutex};

#[repr(C)]
pub struct LkResult { pub code: c_int, pub message: *const c_char }
fn ok() -> LkResult { LkResult { code: 0, message: std::ptr::null() } }
fn err(msg: &str, code: i32) -> LkResult {
    let c = CString::new(msg).unwrap_or_else(|_| CString::new("ffi error").unwrap());
    LkResult { code, message: c.into_raw() }
}

#[no_mangle] pub extern "C" fn lk_free_str(p: *mut c_char) { if !p.is_null() { unsafe { let _ = CString::from_raw(p); } } }

#[repr(C)] pub enum LkReliability { Reliable = 0, Lossy = 1 }
#[repr(C)] pub struct LkClientHandle { _private: [u8;0] }

struct ClientState { connected: bool }
struct Client(std::sync::Arc<std::sync::Mutex<ClientState>>);

#[no_mangle] pub extern "C" fn lk_client_create() -> *mut LkClientHandle {
    let state = ClientState { connected: false };
    let boxed = Box::new(Client(Arc::new(Mutex::new(state))));
    Box::into_raw(boxed) as *mut LkClientHandle
}

#[no_mangle] pub extern "C" fn lk_client_destroy(client: *mut LkClientHandle) {
    if client.is_null() { return; }
    unsafe { drop(Box::from_raw(client as *mut Client)); }
}

#[no_mangle] pub extern "C" fn lk_client_set_data_callback(
    _client: *mut LkClientHandle,
    _cb: Option<extern "C" fn(user:*mut c_void, bytes:*const u8, len:usize)>,
    _user: *mut c_void
) -> LkResult { ok() }

#[no_mangle] pub extern "C" fn lk_connect(client:*mut LkClientHandle, _url:*const c_char, _token:*const c_char) -> LkResult {
    if client.is_null() { return err("client null", 1); }
    let c = unsafe { &*(client as *const Client) };
    let mut g = c.0.lock().unwrap();
    g.connected = true;
    ok()
}

#[no_mangle] pub extern "C" fn lk_disconnect(client:*mut LkClientHandle) -> LkResult {
    if client.is_null() { return err("client null", 1); }
    let c = unsafe { &*(client as *const Client) };
    let mut g = c.0.lock().unwrap();
    g.connected = false;
    ok()
}

#[no_mangle] pub extern "C" fn lk_publish_audio_pcm_i16(
    client:*mut LkClientHandle,
    _pcm:*const i16,
    _frames_per_ch: usize,
    channels:c_int,
    sample_rate:c_int
) -> LkResult {
    if client.is_null() { return err("client null", 1); }
    if channels <= 0 || sample_rate <= 0 { return err("bad params", 3); }
    ok()
}

#[no_mangle] pub extern "C" fn lk_send_data(
    client:*mut LkClientHandle,
    _bytes:*const u8,
    _len: usize,
    _rel: LkReliability
) -> LkResult {
    if client.is_null() { return err("client null", 1); }
    ok()
}
