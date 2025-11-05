# LiveKit FFI + Unreal Bridge (Cross‑Platform)

This repo gives you:
- A **Rust FFI library** that exposes a tiny C ABI (`livekit_ffi.h`).
- A **Unreal Engine plugin** that ships with a ready ThirdParty layout and exposes `ULiveKitPublisherComponent`.

## Two build modes

- **Real LiveKit (recommended):** enable the `with_livekit` feature to wire the LiveKit Rust SDK.
- **Stub mode:** builds a no‑op backend to validate toolchains without pulling LiveKit.

### Enabling the real backend
```
cd livekit_ffi
cargo build --release --features with_livekit
```
Artifacts (Windows MSVC):
- DLL: `target/release/livekit_ffi.dll`
- Import lib: `target/release/livekit_ffi.dll.lib`
- PDB: `target/release/livekit_ffi.pdb`

SDK packaging also includes a static library for advanced use, but the UE plugin links the import lib and delay‑loads the DLL at runtime.

> Requires: Rust toolchain + Cargo. On Windows, make sure you build from a VS **x64 Native Tools** prompt so `cl`/`link` are available.

### Stub build
```
cd livekit_ffi
cargo build --release
```
Produces the same filenames but with no‑op internals.

## Unreal integration

Copy `ue/Plugins/LiveKitBridge` into your UE project’s `Plugins/` folder.

ThirdParty layout (plugin root):
```
Plugins/LiveKitBridge/
	ThirdParty/livekit_ffi/
		include/                 # headers (livekit_ffi.h)
		lib/Win64/Release/       # import lib (livekit_ffi.dll.lib)
		bin/Win64/Release/       # DLL + PDB (livekit_ffi.dll, livekit_ffi.pdb)
```
Build.cs resolves headers via `PluginDirectory` and stages the DLL from `ThirdParty/bin`.

Enable the plugin, add `ULiveKitPublisherComponent` to an actor, set `RoomUrl` & `Token`, and call:
- `PushAudioPCM(InterleavedFrames, FramesPerChannel)` every 10–20ms @ 48kHz mono
- `SendMocap(Bytes, bReliable)` for pose/state (lossy for high‑rate deltas, reliable for sparse state)

Connection behavior:
- The component defaults to non‑blocking connects (`bConnectAsync = true`) using the FFI connection callback.
- The plugin delay‑loads `livekit_ffi.dll` and proactively loads it on module startup; it also falls back to loading from `ThirdParty/bin`.

## Notes

- **Packet sizes:** lossy data ≤ ~1300 bytes; reliable up to ~15KiB. Keep mocap packets small and frequent.
- **Audio pacing:** `capture_frame_*` is blocking until the 50ms buffer accepts the frame; feed in 10–20ms chunks.
- **CRT on Windows:** default is dynamic (/MD). Only switch to static CRT if your UE build does.

MIT

## Local LiveKit server (quickstart)

If you need a local media server for testing, see:

docs/LOCAL_LIVEKIT_QUICKSTART.md

## Token minting (dev tool)

Need long‑lived dev tokens? Use the included utility:

```
pwsh ./tools/mint-token.ps1 -Identity user1 -Room demo -Ttl 168h
```

More options and examples in `docs/TOKEN_MINTING.md`.

