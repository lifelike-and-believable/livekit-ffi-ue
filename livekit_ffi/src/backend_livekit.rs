//! LiveKit backend with lock-free SPSC ring buffer for audio frames.
//! Producer: FFI call (UE thread) → push PCM i16 into ring (non-blocking).
//! Consumer: Tokio task → every 10ms pops N samples and feeds NativeAudioSource.
//! Underruns are zero-padded; overflow drops tail to avoid stalling UE audio.

use std::ffi::{CStr, CString};
use std::borrow::Cow;
use std::os::raw::{c_char, c_int, c_void};
use std::ptr;
use std::sync::{Arc, Mutex};

use anyhow::Result;
use once_cell::sync::OnceCell;
use rtrb::{Producer, RingBuffer};
use tokio::{
    runtime::Runtime,
    time::{interval, Duration},
};
use futures::StreamExt;

use livekit::options::TrackPublishOptions;
use livekit::prelude::*;
use livekit::RoomOptions;
use livekit::{ByteStreamWriter, StreamByteOptions, StreamWriter};
use livekit::RoomEvent;
// use livekit::data_stream::ByteStreamReader; // not currently used
use livekit::StreamReader;
use livekit::webrtc::audio_source::{native::NativeAudioSource, AudioSourceOptions, RtcAudioSource};
use livekit::webrtc::prelude::AudioFrame;
use livekit::webrtc::audio_stream::native::NativeAudioStream;

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
#[derive(Copy, Clone, Debug)]
pub enum LkRole {
    Auto = 0,
    Publisher = 1,
    Subscriber = 2,
    Both = 3,
}

#[repr(C)]
pub struct LkClientHandle {
    _private: [u8; 0],
}

// --------- Internal state ---------

struct AudioRing {
    prod: Producer<i16>,
}

struct UserPtr(*mut c_void);
unsafe impl Send for UserPtr {}
unsafe impl Sync for UserPtr {}

struct ClientState {
    room: Option<Room>,
    audio_src: Option<NativeAudioSource>,
    // Keep the published local audio track alive to ensure publication persists
    local_audio_track: Option<LocalAudioTrack>,
    ring: Option<AudioRing>,
    rt: Arc<Runtime>,
    data_cb: Option<(extern "C" fn(*mut c_void, *const u8, usize), UserPtr)>,
    audio_cb: Option<(extern "C" fn(*mut c_void, *const i16, usize, c_int, c_int), UserPtr)>,
    role: LkRole,
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
        local_audio_track: None,
        ring: None,
        rt: runtime(),
        data_cb: None,
        audio_cb: None,
        role: LkRole::Both,
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
    client: *mut LkClientHandle,
    cb: Option<extern "C" fn(user: *mut c_void, bytes: *const u8, len: usize)>,
    user: *mut c_void,
) -> LkResult {
    if client.is_null() { return err(1, "client null"); }
    let c = unsafe { &*(client as *const Client) };
    let mut g = c.0.lock().unwrap();
    g.data_cb = cb.map(|f| (f, UserPtr(user)));
    ok()
}

#[no_mangle]
pub extern "C" fn lk_client_set_audio_callback(
    client: *mut LkClientHandle,
    cb: Option<extern "C" fn(user: *mut c_void, pcm: *const i16, frames_per_channel: usize, channels: c_int, sample_rate: c_int)>,
    user: *mut c_void,
) -> LkResult {
    if client.is_null() { return err(1, "client null"); }
    let c = unsafe { &*(client as *const Client) };
    let mut g = c.0.lock().unwrap();
    g.audio_cb = cb.map(|f| (f, UserPtr(user)));
    ok()
}

#[no_mangle]
pub extern "C" fn lk_connect(
    client: *mut LkClientHandle,
    url: *const c_char,
    token: *const c_char,
) -> LkResult {
    // Default to Both
    lk_connect_with_role(client, url, token, LkRole::Both)
}

#[no_mangle]
pub extern "C" fn lk_connect_with_role(
    client: *mut LkClientHandle,
    url: *const c_char,
    token: *const c_char,
    role: LkRole,
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

    let role_copy = role; // copy enum (Copy)
    let res = rt.block_on(async move {
        let mut opts = RoomOptions::default();
        // If explicit Publisher, disable auto_subscribe to avoid subscribing to media.
        if matches!(role_copy, LkRole::Publisher) { opts.auto_subscribe = false; }
        let (room, events) = Room::connect(&url, &token, opts).await?;
        Ok::<(Room, tokio::sync::mpsc::UnboundedReceiver<RoomEvent>), anyhow::Error>((room, events))
    });

    match res {
        Ok((room, mut events)) => {
            g.role = role_copy;
            let client_arc = c.0.clone();
            println!(
                "[livekit_ffi] Connected. role={:?} auto_subscribe={}", 
                role_copy, !matches!(role_copy, LkRole::Publisher)
            );
            // Spawn event processor to handle incoming data/audio
            g.rt.spawn(async move {
                while let Some(ev) = events.recv().await {
                    match ev {
                        RoomEvent::ByteStreamOpened { reader, topic: _, participant_identity: _ } => {
                            let Some(reader) = reader.take() else { continue; };
                            // Read all bytes, then invoke callback if set
                            let bytes_res = reader.read_all().await;
                            if let Ok(content) = bytes_res {
                                // Copy to Vec to ensure stable backing memory for callback
                                let buf: Vec<u8> = content.to_vec();
                                println!("[livekit_ffi] ByteStreamOpened: received {} bytes", buf.len());
                                let guard_opt = client_arc.lock().ok();
                                if let Some(guard) = guard_opt {
                                    if let Some((cb, user)) = guard.data_cb.as_ref() {
                                        // SAFETY: We call user-provided callback synchronously
                                        cb(user.0, buf.as_ptr(), buf.len());
                                    }
                                }
                                drop(buf);
                            }
                        }
                        RoomEvent::Disconnected { reason } => {
                            println!("[livekit_ffi] Disconnected event: reason={:?}", reason);
                        }
                        RoomEvent::ConnectionStateChanged(state) => {
                            println!("[livekit_ffi] ConnectionStateChanged: {:?}", state);
                        }
                        RoomEvent::TrackSubscribed { track, publication, participant: _ } => {
                            // Remote audio subscribed - set up a NativeAudioStream and forward frames to audio callback
                            if let RemoteTrack::Audio(audio) = track {
                                println!(
                                    "[livekit_ffi] TrackSubscribed audio: name='{}', sid='{}'",
                                    publication.name(), publication.sid()
                                );
                                // Extract underlying RTC track to build a stream reader
                                let rtc = audio.rtc_track();
                                let client_arc2 = client_arc.clone();
                                // Default to 48kHz mono unless otherwise required by your pipeline
                                let sample_rate = 48_000u32;
                                let channels = 1u32;
                                
                                // Spawn a task to poll audio frames and invoke the user callback synchronously per frame
                                tokio::spawn(async move {
                                    let mut stream = NativeAudioStream::new(rtc, sample_rate as i32, channels as i32);
                                    let mut logged_first = false;
                                    while let Some(frame) = stream.next().await {
                                        // Copy to Vec to ensure stable memory for callback
                                        let buf: Vec<i16> = frame.data.as_ref().to_vec();

                                        if let Ok(guard) = client_arc2.lock() {
                                            if let Some((cb, user)) = guard.audio_cb.as_ref() {
                                                let frames_per_channel = frame.samples_per_channel as usize;
                                                let ch = frame.num_channels as c_int;
                                                let sr = frame.sample_rate as c_int;
                                                cb(user.0, buf.as_ptr(), frames_per_channel, ch, sr);
                                            }
                                        }
                                        // buf drops after callback returns

                                        if !logged_first {
                                            println!("[livekit_ffi] First remote audio frame: sr={}Hz, ch={}, fpc={}", frame.sample_rate, frame.num_channels, frame.samples_per_channel);
                                            logged_first = true;
                                        }
                                    }
                                });
                            }
                        }
                        other => {
                            // Log other events at low verbosity to aid diagnostics
                            println!("[livekit_ffi] Event: {:?}", other);
                        }
                    }
                }
            });
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
    println!("[livekit_ffi] Disconnected");
    g.audio_src = None;
    g.ring = None; // dropping prod ends the consumer loop once src drops
    ok()
}

#[no_mangle]
pub extern "C" fn lk_client_is_ready(client: *mut LkClientHandle) -> c_int {
    if client.is_null() {
        return 0;
    }
    let c = unsafe { &*(client as *const Client) };
    let g = c.0.lock().unwrap();
    if g.room.is_some() { 1 } else { 0 }
}

// Ensure NativeAudioSource + ring consumer exist (lazy init).
fn ensure_audio_pipeline(g: &mut ClientState, sample_rate: u32, channels: u32) -> Result<()> {
    if g.audio_src.is_none() {
        let samples_per_10ms = sample_rate / 100;
        let src = NativeAudioSource::new(AudioSourceOptions::default(), sample_rate, channels, samples_per_10ms);
        let local = LocalAudioTrack::create_audio_track("ue-audio", RtcAudioSource::Native(src.clone()));
        let room = g
            .room
            .as_ref()
            .ok_or_else(|| anyhow::anyhow!("not connected"))?;
        let rt = g.rt.clone();

        let pub_res = rt.block_on(async {
            room.local_participant()
                .publish_track(LocalTrack::Audio(local.clone()), TrackPublishOptions::default())
                .await
        });
        match pub_res {
            Ok(_) => {
                println!("[livekit_ffi] Published local audio track (sr={} ch={})", sample_rate, channels);
                g.local_audio_track = Some(local);
                g.audio_src = Some(src);
            }
            Err(e) => {
                println!("[livekit_ffi] Failed to publish audio track: {}", e);
                return Err(e.into());
            }
        }
    }

    if g.ring.is_none() {
        // ≥ 1s buffer to tolerate bursts; adjust if you prefer.
        let capacity = (sample_rate as usize * channels as usize).max(48_000 * channels as usize);
        let (prod, mut cons) = RingBuffer::<i16>::new(capacity);
        let frame_10ms = ((sample_rate as usize / 100) * channels as usize).max(1);
        let src = g.audio_src.as_ref().unwrap().clone();
        let rt = g.rt.clone();
        let ch = channels;
        let sr = sample_rate;

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
                let samples_per_channel = (buf.len() as u32) / ch;
                let frame = AudioFrame {
                    data: Cow::Borrowed(&buf[..]),
                    sample_rate: sr,
                    num_channels: ch,
                    samples_per_channel,
                };
                let _ = src.capture_frame(&frame).await;
            }
        });

        g.ring = Some(AudioRing { prod });
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
        // Provide a useful error string for the caller
        let msg = format!("audio pipeline init failed: {}", e);
        println!("[livekit_ffi] {}", msg);
        return err(7, &msg);
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
                    // Ring full; drop remainder to avoid stalling
                    break;
                }
            }
        }
    } else {
        let msg = "audio ring not initialized";
        println!("[livekit_ffi] {}", msg);
        return err(8, msg);
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
        Some(r) => r,
        None => return err(6, "not connected"),
    };

    let payload = unsafe { std::slice::from_raw_parts(bytes, len) }.to_vec();
    let topic = match reliability {
        LkReliability::Reliable => "mocap-bin-reliable",
        LkReliability::Lossy => "mocap-bin-lossy",
    }
    .to_string();

    let rt = g.rt.clone();
    let res = rt.block_on(async {
        // Helper to perform one send attempt
        async fn send_once(
            room: &Room,
            topic: &str,
            payload: &[u8],
        ) -> Result<(), anyhow::Error> {
            let options = StreamByteOptions { topic: topic.to_string(), ..Default::default() };
            let writer: ByteStreamWriter = room
                .local_participant()
                .stream_bytes(options)
                .await?;
            writer.write(payload).await?;
            writer.close().await?;
            Ok(())
        }

        // First attempt
        match send_once(room, &topic, &payload).await {
            Ok(_) => Ok(()),
            Err(e1) => {
                // Brief backoff then one retry; common when engine is still settling right after join
                println!("[livekit_ffi] send_data first attempt failed, retrying: {}", e1);
                tokio::time::sleep(Duration::from_millis(100)).await;
                send_once(room, &topic, &payload).await
            }
        }
    });

    match res {
        Ok(_) => {
            println!("[livekit_ffi] Sent data: {} bytes, topic='{}'", len, topic);
            ok()
        },
        Err(e) => {
            let msg = format!("byte_stream write failed: {}", e);
            println!("[livekit_ffi] {}", msg);
            err(9, &msg)
        },
    }
}
