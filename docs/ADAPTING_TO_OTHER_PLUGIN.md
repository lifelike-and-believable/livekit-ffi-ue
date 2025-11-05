# Using LiveKit FFI in Another Unreal Plugin

This guide shows how to consume the prebuilt LiveKit FFI (headers + libs + DLL) in a separate Unreal Engine plugin, and how to reference `ULiveKitPublisherComponent` for Blueprint/C++ usage patterns.

## 1) Get the SDK artifacts

Options:

- From CI: Download the `livekit-ffi-sdk-windows-x64` artifact (or zip) produced by GitHub Actions.
- From CI (drop-in): Download the `livekit-ffi-plugin-windows-x64` zip for a ready-to-use plugin layout (`ThirdParty/livekit_ffi` with lib and bin subfolders).
- Local build: Inside this repo, run:

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File tools\build.ps1 -WithLiveKit
pwsh -NoProfile -ExecutionPolicy Bypass -File tools\package.ps1 -CrateDir livekit_ffi -OutDir artifacts\windows-x64
```

The SDK layout (Windows x64):

```
include/                # C headers (e.g., livekit_ffi.h)
bin/                    # livekit_ffi.dll (+ PDB)
lib/Win64/Release/      # livekit_ffi.dll.lib + livekit_ffi.lib
```

## 2) Place files into your plugin

Recommended (plugin‑root ThirdParty for portability):

```
YourPlugin/
  YourPlugin.uplugin
  ThirdParty/
    livekit_ffi/
      include/                # copy SDK include/*
      lib/Win64/Release/      # copy SDK lib/Win64/Release/livekit_ffi.dll.lib
      bin/Win64/Release/      # copy SDK bin/{livekit_ffi.dll, livekit_ffi.pdb}
  Source/
    YourPlugin/
      YourPlugin.Build.cs
```

Alternative (module‑local): keep `ThirdParty/livekit_ffi` beneath your module directory and use `ModuleDirectory` in Build.cs. The rest of the guidance is identical.

Notes:
- Keep the folder names as shown for predictable paths in the Build.cs example below.
- The DLL will be staged from `ThirdParty/.../bin/Win64/Release` at build time; no need to place it in `Binaries/Win64` manually.

Shortcut: If you grabbed the `livekit-ffi-plugin-windows-x64` CI artifact, drop its `ThirdParty/livekit_ffi` at the plugin root. The plugin artifact intentionally omits the static `livekit_ffi.lib` (SDK zips still include it).

## 3) Configure YourPlugin.Build.cs

Add include paths, link the import lib, delay-load the DLL, and declare a runtime dependency so the DLL is staged into builds.

```csharp
using UnrealBuildTool;
using System.IO;

public class YourPlugin : ModuleRules
{
  public YourPlugin(ReadOnlyTargetRules Target) : base(Target)
  {
    PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

    PublicDependencyModuleNames.AddRange(new string[]
    {
      "Core", "CoreUObject", "Engine", "Projects"
    });

    // Plugin-root ThirdParty
    string ThirdPartyBase = Path.Combine(PluginDirectory, "ThirdParty", "livekit_ffi");
    string IncludePath = Path.Combine(ThirdPartyBase, "include");
    PublicIncludePaths.Add(IncludePath);
    PublicSystemIncludePaths.Add(IncludePath);

    if (Target.Platform == UnrealTargetPlatform.Win64)
    {
      string LibPath = Path.Combine(ThirdPartyBase, "lib", "Win64", "Release");
      string BinPath = Path.Combine(ThirdPartyBase, "bin", "Win64", "Release");

      PublicAdditionalLibraries.Add(Path.Combine(LibPath, "livekit_ffi.dll.lib"));
      PublicDelayLoadDLLs.Add("livekit_ffi.dll");
      RuntimeDependencies.Add(Path.Combine(BinPath, "livekit_ffi.dll"));
    }
  }
}
```

## 4) Call the FFI from your plugin code (optional)

Include the header and call the C API directly if you are not using our ready-made component:

```cpp
#include "livekit_ffi.h"

// Example: create client and connect (blocking)
LkClientHandle* H = lk_client_create();
LkResult R = lk_connect_with_role(H, "ws://localhost:7880", YourJwtToken, LkRolePublisher);
if (R.code != 0) { /* handle error & free R.message via lk_free_str */ }

// Set callbacks
lk_client_set_data_callback(H, YourDataCb, YourUserPtr);
lk_client_set_audio_callback(H, YourAudioCb, YourUserPtr);

// Send data
lk_send_data(H, payload, payloadLen, LkReliability::Reliable);

// Publish audio (interleaved i16)
lk_publish_audio_pcm_i16(H, interleaved, framesPerChannel, channels, sampleRate);

// Disconnect
lk_disconnect(H);
```

Async connection with callbacks:

```c
void on_conn(void* user, LkConnectionState st, int32_t code, const char* msg) {
  // handle connecting/connected/failed/disconnected on your thread of choice
}

LkClientHandle* H = lk_client_create();
lk_set_connection_callback(H, on_conn, user);
LkResult R = lk_connect_with_role_async(H, "ws://localhost:7880", YourJwtToken, LkRolePublisher);
// returns immediately; watch on_conn for state changes
```

See `livekit_ffi/include/livekit_ffi.h` for function signatures.

## 5) Using LiveKitPublisherComponent as a reference

`ULiveKitPublisherComponent` (in this repo) provides a ready-to-use pattern for:

- Role-aware connection (Publisher/Subscriber/Both)
- Data send/receive via LiveKit byte streams
- Audio publish/receive using a ring buffer → `NativeAudioSource` → `LocalAudioTrack`
- Readiness gating and initial delays to let negotiation settle
- Blueprint events for UX feedback

Key properties:

- `RoomUrl`: e.g. `ws://localhost:7880`
- `Token`: LiveKit JWT with unique identity and proper grants
- `Role`: `Publisher`, `Subscriber`, `Both`, or `Auto`
- `bStartTestData`, `TestDataRateHz`, `TestDataPayloadBytes`, `bTestDataReliable`
- `bStartDebugTone`, `ToneFrequencyHz`, `ToneAmplitude`, `SampleRate`, `Channels`
- `bReceiveMocap`, `bReceiveAudio`

Blueprint events you can implement:

- `OnConnected(Url, Role, bRecvMocap, bRecvAudio)`
- `OnDisconnected()`
- `OnAudioPublishReady(SampleRate, Channels)`
- `OnFirstAudioReceived(SampleRate, Channels, FramesPerChannel)`
- `OnMocapSent(Bytes, bReliable)`
- `OnMocapSendFailed(Bytes, bReliable, Reason)`
- `OnMocapReceived(Payload)` (already existed)

Minimal flow:

1. Place a `ULiveKitPublisherComponent` in your actor.
2. Set `RoomUrl` and `Token` (unique `identity` per client!).
3. For publisher testing, set `Role=Publisher`, enable `bStartDebugTone` and/or `bStartTestData`.
4. For subscribers, set `Role=Subscriber`, enable `bReceiveMocap` and `bReceiveAudio`.
5. Bind to `OnMocapReceived` and audio events for confirmation and UX.

## 6) Token and permissions checklist

- Each client must use a token with a unique `identity`.
- Publisher grants: `roomJoin: true`, `canPublish: true`, `canPublishData: true` (if sending data), optionally `canSubscribe` for Both.
- Subscriber grants: `roomJoin: true`, `canSubscribe: true`.
- Using the same token/identity concurrently will replace the previous participant (disconnect behavior).

## 7) Troubleshooting

- "engine is closed" when publishing audio: often caused by identity collisions or server-side rejection; verify token grants and unique identities.
- Data send "internal error": ensure `canPublishData: true` for the publisher.
- No receives: subscribers need `auto_subscribe` enabled (default for subscribers/Both). In our implementation, we disable `auto_subscribe` only for explicit Publisher role.
- Hot reload: Use Ctrl+Alt+F11 (Live Coding) for C++ changes; restart the editor when replacing the DLL.

## 8) Building from source (Windows)

If you prefer to build the FFI once inside your own repo:

```powershell
# Static MSVC runtime to match libwebrtc_sys
$env:RUSTFLAGS = "-C target-feature=+crt-static"
cd livekit_ffi
cargo build --release --features with_livekit
```

Artifacts appear in `livekit_ffi/target/release`.

---

For additional examples, see `ue/Plugins/LiveKitBridge/` in this repo.
