# LiveKit FFI + Unreal Bridge (Cross‑Platform)

This repo gives you:
- A **Rust FFI library** that exposes a tiny C ABI (`livekit_ffi.h`).
- A **Unreal Engine plugin** that ships with a ready ThirdParty layout and exposes `ULiveKitPublisherComponent`.

## 📚 Documentation

**New to LiveKit FFI?** Start here:
- **[User Guide](docs/USER_GUIDE.md)** - Comprehensive guide covering installation, basic usage, and best practices
- **[Examples](docs/EXAMPLES.md)** - Complete working code examples for common scenarios
- **[Quick Start](#quick-start)** - Get up and running in 5 minutes

**API Reference:**
- **[FFI API Guide](docs/FFI_API_GUIDE.md)** - Detailed C API documentation with examples
- **[Architecture](docs/ARCHITECTURE.md)** - Internal design and system architecture

**Integration Guides:**
- **[Unreal Engine Integration](#unreal-integration)** - How to use with UE (see below)
- **[Adapting to Other Engines](docs/ADAPTING_TO_OTHER_PLUGIN.md)** - Use in other game engines or C++ projects

**Setup & Configuration:**
- **[Local LiveKit Server](docs/LOCAL_LIVEKIT_QUICKSTART.md)** - Run a local server for development
- **[Token Minting](docs/TOKEN_MINTING.md)** - Generate access tokens for testing

**Help & Support:**
- **[Troubleshooting](docs/TROUBLESHOOTING.md)** - Solutions to common issues
- **[GitHub Issues](https://github.com/lifelike-and-believable/livekit-ffi-ue/issues)** - Report bugs or request features

---

## Quick Start

### Building the FFI Library

The library supports two build modes:

**1. Full Build (with LiveKit) - Recommended**
```bash
cd livekit_ffi
cargo build --release --features with_livekit
```

**2. Stub Build (for testing build toolchain)**
```bash
cd livekit_ffi
cargo build --release
```

**Build Artifacts (Windows MSVC):**
- DLL: `target/release/livekit_ffi.dll`
- Import lib: `target/release/livekit_ffi.dll.lib`
- PDB: `target/release/livekit_ffi.pdb`
- Header: `include/livekit_ffi.h`

**Requirements:**
- Rust 1.70+ with Cargo
- Windows: Visual Studio 2019+ with C++ tools (build from x64 Native Tools prompt)
- Linux: GCC 7+ or Clang 10+, libssl-dev
- macOS: Xcode command line tools

For detailed build instructions, see the **[User Guide](docs/USER_GUIDE.md#installation)**.

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

---

## C/C++ Usage Example

```cpp
#include "livekit_ffi.h"

int main() {
    // Create client
    LkClientHandle* client = lk_client_create();
    
    // Connect to room
    LkResult result = lk_connect(client, 
        "wss://your-server.livekit.io", 
        "your-jwt-token");
    
    if (result.code == 0) {
        // Publish audio every 10ms
        int16_t audio[480];  // 10ms @ 48kHz mono
        lk_publish_audio_pcm_i16(client, audio, 480, 1, 48000);
        
        // Send data
        uint8_t data[256];
        lk_send_data(client, data, sizeof(data), LkReliable);
    }
    
    // Cleanup
    lk_disconnect(client);
    lk_client_destroy(client);
    return 0;
}
```

For more examples, see **[Examples](docs/EXAMPLES.md)**.

---

## Important Notes

- **Packet sizes:** Lossy data ≤ ~1300 bytes; reliable up to ~15KiB. Keep mocap packets small and frequent.
- **Audio pacing:** Feed audio in 10–20ms chunks for optimal latency.
- **Thread safety:** All API functions are thread-safe. Callbacks may run on background threads.
- **Error handling:** Always check return codes and free error messages with `lk_free_str()`.

For best practices and performance tips, see the **[User Guide](docs/USER_GUIDE.md#best-practices)**.

---

## Development & Testing

### Local LiveKit Server

For local development, run a LiveKit server:

```bash
docker run --rm -p 7880:7880 -p 7882:7882/udp livekit/livekit-server start --dev
```

See **[Local Server Guide](docs/LOCAL_LIVEKIT_QUICKSTART.md)** for details.

### Token Generation

Generate test tokens for multi-client testing:

**VS Code Task:**
```
Run Task → "Generate Test Tokens (168h)"
```

**PowerShell:**
```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File tools/generate-test-tokens.ps1 -Room test -Ttl 168h
```

**Single Token:**
```powershell
pwsh ./tools/mint-token.ps1 -Identity user1 -Room demo -Ttl 168h
```

**Environment Variables (for production):**
```bash
# Set your LiveKit API credentials
export LIVEKIT_API_KEY="your-key"
export LIVEKIT_API_SECRET="your-secret"
```

See **[Token Minting Guide](docs/TOKEN_MINTING.md)** for more options.

---

## Contributing

Contributions are welcome! Please:
1. Read the **[Architecture Guide](docs/ARCHITECTURE.md)** to understand the system
2. Check existing issues before creating new ones
3. Follow the coding style of the project
4. Add tests for new features
5. Update documentation as needed

---

## Support & Community

- **Documentation:** [docs/](docs/)
- **Issues:** [GitHub Issues](https://github.com/lifelike-and-believable/livekit-ffi-ue/issues)
- **LiveKit Slack:** [livekit.io/slack](https://livekit.io/slack)
- **LiveKit Forums:** [discuss.livekit.io](https://discuss.livekit.io/)

---

## License

MIT

