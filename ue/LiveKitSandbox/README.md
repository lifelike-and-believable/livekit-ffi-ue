# LiveKitSandbox (UE 5.6)

A minimal Unreal Engine 5.6 C++ project for testing the `LiveKitBridge` plugin and the `livekit_ffi` library.

## Setup

1) Link the plugin into the project (creates a junction under `Plugins/`):

```pwsh
# From repo root
powershell -NoProfile -ExecutionPolicy Bypass -File tools/link-plugin.ps1 -ProjectDir "ue/LiveKitSandbox" -PluginSrc "ue/Plugins/LiveKitBridge"
```

Alternatively, use the VS Code task:

- Terminal → Run Task → "Link LiveKit Plugin to Sandbox"

2) Build the FFI library (copies artifacts into the plugin `Binaries/Win64`):

```pwsh
# From repo root
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1 -WithLiveKit -UePluginDir "ue/Plugins/LiveKitBridge"
```

3) Open the project in UE 5.6:

- Double-click `ue/LiveKitSandbox/LiveKitSandbox.uproject`.
- Ensure the plugin `LiveKitBridge` is enabled in the Plugins browser.

## Notes
- This repo uses a local plugin at `ue/Plugins/LiveKitBridge`. The link script creates a junction at `ue/LiveKitSandbox/Plugins/LiveKitBridge` so the project sees it.
- The FFI crate builds a staticlib; the plugin’s `Binaries/Win64` folder is populated by the build script if dynamic artifacts are produced.