//! LiveKit backend with lock-free SPSC ring buffer for audio frames.
//! Producer: FFI call (UE thread) → push PCM i16 into ring (non-blocking).
//! Consumer: Tokio task → every 10ms pops N samples and feeds NativeAudioSource.
//! Underruns are zero-padded; overflow drops tail to avoid stalling UE audio.

use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_int, c_void};
use std::ptr;
use std::sync::{Arc, Mutex};

use anyhow::Result;
use once_cell::sync::OnceCell;
use rtrb::{Producer, RingBuffer};
use tokio::{
    io::AsyncWriteExt,
    runtime::Runtime,
    time::{interval, Duration},
};

use livekit::options::TrackPublishOptions;
use livekit::prelude::*;
use livekit::room::RoomOptions;
use livekit::{ByteStreamWriter, StreamByteOptions};
use livekit::webrtc::native::NativeAudioSource;

// --------- C ABI surface ---------

#[repr(C)]
pub struct LkResult {
    pub code: c_int,
    pub message: *const c_char,
}

fn ok() -> LkResult {
    LkResult {
        code: 0,
        message: ptr::null(),
    }
}
fn err(code: i32, msg: &str) -> LkResult {
    let c = CString::new(msg).unwrap_or_else(|_| CString::new("ffi error").unwrap());
    LkResult {
        code,
        message: c.into_raw(),
    }
}

#[no_mangle]
pub extern "C" fn lk_free_str(p: *mut c_char) {
    if !p.is_null() {
        unsafe {
            let _ = CString::from_raw(p);
        }
    }
}

#[repr(C)]
pub enum LkReliability {
    Reliable = 0,
    Lossy = 1,
}

#[repr(C)]
pub struct LkClientHandle {
    _private: [u8; 0],
}

// --------- Internal state ---------

struct AudioRing {
    prod: Producer<i16>,
    sample_rate: u32,
    channels: u32,
    frame_10ms: usize, // interleaved samples per 10ms
}

struct ClientState {
    room: Option<Room>,
    audio_src: Option<NativeAudioSource>,
    ring: Option<AudioRing>,
    rt: Arc<Runtime>,
}

struct Client(Arc<Mutex<ClientState>>);

static RT: OnceCell<Arc<Runtime>> = OnceCell::new();
fn runtime() -> Arc<Runtime> {
    RT.get_or_init(|| Arc::new(Runtime::new().expect("tokio runtime"))).clone()
}

unsafe fn cstr<'a>(p: *const c_char) -> Result<&'a str> {
    if p.is_null() {
        anyhow::bail!("null pointer")
    }
    Ok(CStr::from_ptr(p).to_str()?)
}

// --------- FFI functions ---------

#[no_mangle]
pub extern "C" fn lk_client_create() -> *mut LkClientHandle {
    let state = ClientState {
        room: None,
        audio_src: None,
        ring: None,
        rt: runtime(),
    };
    let boxed = Box::new(Client(Arc::new(Mutex::new(state))));
    Box::into_raw(boxed) as *mut LkClientHandle
}

#[no_mangle]
pub extern "C" fn lk_client_destroy(client: *mut LkClientHandle) {
    if client.is_null() {
        return;
    }
    unsafe { drop(Box::from_raw(client as *mut Client)); }
}

#[no_mangle]
pub extern "C" fn lk_client_set_data_callback(
    _client: *mut LkClientHandle,
    _cb: Option<extern "C" fn(user: *mut c_void, bytes: *const u8, len: usize)>,
    _user: *mut c_void,
) -> LkResult {
    // Add when implementing subscriber side; publisher-only for now.
    ok()
}

#[no_mangle]
pub extern "C" fn lk_connect(
    client: *mut LkClientHandle,
    url: *const c_char,
    token: *const c_char,
) -> LkResult {
    if client.is_null() {
        return err(1, "client null");
    }

    let url = unsafe { match cstr(url) {
        Ok(s) => s.to_string(),
        Err(e) => return err(2, &e.to_string()),
    }};
    let token = unsafe { match cstr(token) {
        Ok(s) => s.to_string(),
        Err(e) => return err(2, &e.to_string()),
    }};

    let c = unsafe { &*(client as *const Client) };
    let mut g = c.0.lock().unwrap();
    let rt = g.rt.clone();

    let res = rt.block_on(async move {
        let (room, mut events) = Room::connect(&url, &token, RoomOptions::default()).await?;
        // Drain events so the mpsc channel does not back up.
        tokio::spawn(async move {
            while let Some(_e) = events.recv().await {
                // hook for logging if desired
            }
        });
        Ok::<Room, anyhow::Error>(room)
    });

    match res {
        Ok(room) => {
            g.room = Some(room);
            ok()
        }
        Err(e) => err(3, &format!("connect failed: {e}")),
    }
}

#[no_mangle]
pub extern "C" fn lk_disconnect(client: *mut LkClientHandle) -> LkResult {
    if client.is_null() {
        return err(1, "client null");
    }
    let c = unsafe { &*(client as *const Client) };
    let mut g = c.0.lock().unwrap();

    if let Some(room) = g.room.take() {
        let rt = g.rt.clone();
        let _ = rt.block_on(async move {
            let _ = room.close().await; // graceful shutdown
        });
    }
    g.audio_src = None;
    g.ring = None; // dropping prod ends the consumer loop once src drops
    ok()
}

// Ensure NativeAudioSource + ring consumer exist (lazy init).
fn ensure_audio_pipeline(g: &mut ClientState, sample_rate: u32, channels: u32) -> Result<()> {
    if g.audio_src.is_none() {
        let src = NativeAudioSource::new(sample_rate, channels as u16);
        let local = LocalAudioTrack::create_audio_track("ue-audio", src.clone());
        let room = g
            .room
            .as_ref()
            .ok_or_else(|| anyhow::anyhow!("not connected"))?
            .clone();
        let rt = g.rt.clone();

        rt.block_on(async {
            room.local_participant()
                .publish_track(LocalTrack::Audio(local), TrackPublishOptions::default())
                .await
        })?;
        g.audio_src = Some(src);
    }

    if g.ring.is_none() {
        // ≥ 1s buffer to tolerate bursts; adjust if you prefer.
        let capacity = (sample_rate as usize * channels as usize).max(48_000 * channels as usize);
        let (prod, mut cons) = RingBuffer::<i16>::new(capacity);
        let frame_10ms = ((sample_rate as usize / 100) * channels as usize).max(1);
        let src = g.audio_src.as_ref().unwrap().clone();
        let rt = g.rt.clone();

        rt.spawn(async move {
            let mut tick = interval(Duration::from_millis(10));
            let mut buf: Vec<i16> = vec![0; frame_10ms];
            loop {
                tick.tick().await;

                let mut got = 0usize;
                while got < buf.len() {
                    match cons.pop() {
                        Ok(s) => {
                            buf[got] = s;
                            got += 1;
                        }
                        Err(_) => break,
                    }
                }
                if got < buf.len() {
                    // Zero-pad underrun
                    for x in &mut buf[got..] {
                        *x = 0;
                    }
                }

                // Feed one 10ms frame
                // NOTE: NativeAudioSource::capture_frame_i16 accepts interleaved i16.
                // If your SDK expects frames-per-channel too, adjust accordingly.
                let _ = src.capture_frame_i16(&buf);
            }
        });

        g.ring = Some(AudioRing {
            prod,
            sample_rate,
            channels,
            frame_10ms,
        });
    }

    Ok(())
}

#[no_mangle]
pub extern "C" fn lk_publish_audio_pcm_i16(
    client: *mut LkClientHandle,
    pcm: *const i16,
    frames_per_channel: usize,
    channels: c_int,
    sample_rate: c_int,
) -> LkResult {
    if client.is_null() {
        return err(1, "client null");
    }
    if pcm.is_null() {
        return err(4, "pcm null");
    }
    if channels <= 0 || sample_rate <= 0 {
        return err(5, "bad params");
    }

    let c = unsafe { &*(client as *const Client) };
    let mut g = c.0.lock().unwrap();
    if g.room.is_none() {
        return err(6, "not connected");
    }

    let channels = channels as u32;
    let sample_rate = sample_rate as u32;

    if let Err(e) = ensure_audio_pipeline(&mut g, sample_rate, channels) {
        return err(7, &format!("audio pipeline: {e}"));
    }

    let total = frames_per_channel * channels as usize;
    let slice = unsafe { std::slice::from_raw_parts(pcm, total) };

    // Non-blocking push; on overflow, drop tail (prefer fresh audio over stall).
    if let Some(r) = &mut g.ring {
        let mut pushed = 0usize;
        while pushed < slice.len() {
            match r.prod.push(slice[pushed]) {
                Ok(_) => {
                    pushed += 1;
                }
                Err(_) => {
                    break;
                }
            }
        }
    }

    ok()
}

#[no_mangle]
pub extern "C" fn lk_send_data(
    client: *mut LkClientHandle,
    bytes: *const u8,
    len: usize,
    reliability: LkReliability,
) -> LkResult {
    if client.is_null() {
        return err(1, "client null");
    }
    if bytes.is_null() {
        return err(4, "bytes null");
    }

    let c = unsafe { &*(client as *const Client) };
    let g = c.0.lock().unwrap();
    let room = match g.room.as_ref() {
        Some(r) => r.clone(),
        None => return err(6, "not connected"),
    };

    let payload = unsafe { std::slice::from_raw_parts(bytes, len) }.to_vec();
    let topic = match reliability {
        LkReliability::Reliable => "mocap-bin-reliable",
        LkReliability::Lossy => "mocap-bin-lossy",
    }
    .to_string();

    let rt = g.rt.clone();
    let res = rt.block_on(async move {
        let mut writer: ByteStreamWriter = room
            .local_participant()
            .open_byte_stream(&topic, StreamByteOptions::default())
            .await?;
        writer.write_all(&payload).await?;
        writer.close().await?;
        Ok::<(), anyhow::Error>(())
    });

    match res {
        Ok(_) => ok(),
        Err(e) => err(9, &format!("byte_stream write failed: {e}")),
    }
}
