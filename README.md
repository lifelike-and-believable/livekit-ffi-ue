# LiveKit FFI + Unreal Bridge (Cross‑Platform)

This repo gives you:
- A **Rust staticlib** that exposes a tiny C ABI (`livekit_ffi.h`).
- A **Unreal Engine plugin** that links that lib and exposes `ULiveKitPublisherComponent`.

## Two build modes

- **Real LiveKit (recommended):** enable the `with_livekit` feature to wire the LiveKit Rust SDK.
- **Stub mode:** builds a no‑op backend to validate toolchains without pulling LiveKit.

### Enabling the real backend
```
cd livekit_ffi
cargo build --release --features with_livekit
```
Artifacts:
- Windows (MSVC): `target/release/livekit_ffi.lib`
- Linux/macOS: `target/release/liblivekit_ffi.a`

> Requires: Rust toolchain + Cargo. On Windows, make sure you build from a VS **x64 Native Tools** prompt so `cl`/`link` are available.

### Stub build
```
cd livekit_ffi
cargo build --release
```
Produces the same filenames but with no‑op internals.

## Unreal integration

Copy `ue/Plugins/LiveKitBridge` into your UE project’s `Plugins/` folder.

Place the built static library into:
```
Plugins/LiveKitBridge/Source/LiveKitBridge/ThirdParty/livekit_ffi/lib/<Platform>/Release/
```
(Win64: `livekit_ffi.lib`, Linux/macOS: `liblivekit_ffi.a`)

`livekit_ffi.h` is mirrored under:
```
Plugins/LiveKitBridge/Source/LiveKitBridge/ThirdParty/livekit_ffi/include/
```

Enable the plugin, add `ULiveKitPublisherComponent` to an actor, set `RoomUrl` & `Token`, and call:
- `PushAudioPCM(InterleavedFrames, FramesPerChannel)` every 10–20ms @ 48kHz mono
- `SendMocap(Bytes, bReliable)` for pose/state (lossy for high‑rate deltas, reliable for sparse state)

## Notes

- **Packet sizes:** lossy data ≤ ~1300 bytes; reliable up to ~15KiB. Keep mocap packets small and frequent.
- **Audio pacing:** `capture_frame_*` is blocking until the 50ms buffer accepts the frame; feed in 10–20ms chunks.
- **CRT on Windows:** default is dynamic (/MD). Only switch to static CRT if your UE build does.

MIT

## Local LiveKit server (quickstart)

If you need a local media server for testing, see:

docs/LOCAL_LIVEKIT_QUICKSTART.md

