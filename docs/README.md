# LiveKit FFI Documentation

Welcome to the comprehensive documentation for the LiveKit FFI C++ library. This directory contains all the guides, references, and resources you need to integrate real-time audio and data streaming into your applications.

## 📖 Documentation Index

### Getting Started

**New to LiveKit FFI? Start here:**

1. **[User Guide](USER_GUIDE.md)** ⭐ **START HERE**
   - Complete guide from installation to advanced usage
   - Covers audio streaming, data channels, connection management
   - Best practices and performance optimization tips
   - ~35,000 words of comprehensive documentation

2. **[Examples](EXAMPLES.md)**
   - 8 complete, working code examples
   - Copy-paste ready C/C++ code
   - Covers common scenarios: voice chat, mocap streaming, multi-track audio
   - Build and run instructions included

### API Reference

3. **[FFI API Guide](FFI_API_GUIDE.md)**
   - Detailed C API documentation
   - Function signatures and parameters
   - Error codes and handling
   - Threading model and safety guarantees

4. **[Architecture](ARCHITECTURE.md)**
   - Internal system design
   - Threading model and data flow
   - Audio pipeline architecture
   - Memory management strategies
   - Design decisions and rationale

### Integration Guides

5. **[Adapting to Other Plugins](ADAPTING_TO_OTHER_PLUGIN.md)**
   - Use LiveKit FFI in other game engines
   - Integration with custom C++ projects
   - Build system configuration (CMake, Visual Studio, etc.)
   - Step-by-step integration tutorial

### Setup & Configuration

6. **[Local LiveKit Server Quickstart](LOCAL_LIVEKIT_QUICKSTART.md)**
   - Run a local LiveKit server for development
   - Docker and standalone installation
   - Port configuration and firewall setup
   - Troubleshooting local server issues

7. **[Token Minting](TOKEN_MINTING.md)**
   - Generate access tokens for testing
   - Token permissions and grants
   - CLI tools and scripts
   - Security best practices

### Troubleshooting

8. **[Troubleshooting Guide](TROUBLESHOOTING.md)** ⚠️ **HAVING ISSUES?**
   - Solutions to common problems
   - Connection, audio, and data channel issues
   - Build and compilation problems
   - Platform-specific troubleshooting
   - Debugging tips and tools

---

## Quick Navigation

### By User Type

**Game Developer / UE User:**
1. Start with [User Guide § Unreal Engine Integration](USER_GUIDE.md#unreal-engine-integration)
2. Read [Adapting to Other Plugins](ADAPTING_TO_OTHER_PLUGIN.md) for detailed UE setup
3. Use [Examples](EXAMPLES.md) as reference

**C++ Developer:**
1. Read [User Guide](USER_GUIDE.md) for complete overview
2. Study [Examples](EXAMPLES.md) for practical code
3. Reference [FFI API Guide](FFI_API_GUIDE.md) for API details

**System Architect:**
1. Review [Architecture](ARCHITECTURE.md) for system design
2. Study [User Guide § Core Concepts](USER_GUIDE.md#core-concepts)
3. Read [Best Practices](USER_GUIDE.md#best-practices)

### By Task

**Setting up for the first time:**
- [User Guide § Installation](USER_GUIDE.md#installation)
- [Local Server Quickstart](LOCAL_LIVEKIT_QUICKSTART.md)
- [Token Minting](TOKEN_MINTING.md)

**Implementing voice chat:**
- [Examples § Simple Voice Chat](EXAMPLES.md#simple-voice-chat)
- [User Guide § Audio Streaming](USER_GUIDE.md#audio-streaming)
- [FFI API Guide § Audio Configuration](FFI_API_GUIDE.md#audio-configuration)

**Streaming motion capture data:**
- [Examples § Motion Capture Streaming](EXAMPLES.md#motion-capture-streaming)
- [User Guide § Data Channels](USER_GUIDE.md#data-channels)
- [FFI API Guide § Data Channel Usage](FFI_API_GUIDE.md#data-channel-usage)

**Debugging issues:**
- [Troubleshooting Guide](TROUBLESHOOTING.md)
- [User Guide § Diagnostics and Monitoring](USER_GUIDE.md#diagnostics-and-monitoring)
- [Architecture § Performance Characteristics](ARCHITECTURE.md#performance-characteristics)

**Optimizing performance:**
- [User Guide § Performance Optimization](USER_GUIDE.md#performance-optimization)
- [User Guide § Best Practices](USER_GUIDE.md#best-practices)
- [Troubleshooting § Performance Issues](TROUBLESHOOTING.md#performance-issues)

---

## Documentation Statistics

| Document | Size | Topics Covered |
|----------|------|----------------|
| **USER_GUIDE.md** | ~123 KB | Installation, API usage, best practices, performance |
| **EXAMPLES.md** | ~45 KB | 8 complete working examples with build instructions |
| **ARCHITECTURE.md** | ~21 KB | System design, threading, memory, internals |
| **FFI_API_GUIDE.md** | ~21 KB | C API reference, error handling, threading |
| **TROUBLESHOOTING.md** | ~20 KB | Common issues, debugging, platform-specific problems |
| **ADAPTING_TO_OTHER_PLUGIN.md** | ~8 KB | Integration guide for other engines |
| **LOCAL_LIVEKIT_QUICKSTART.md** | ~4 KB | Local server setup |
| **TOKEN_MINTING.md** | ~3 KB | Token generation |
| **Total** | **~245 KB** | **Complete LiveKit FFI documentation** |

---

## Key Features Documented

### Audio Streaming
- ✅ Publishing PCM audio in real-time
- ✅ Subscribing to remote audio streams
- ✅ Multi-track audio (microphone, game audio, etc.)
- ✅ Audio format configuration (sample rate, channels, bitrate)
- ✅ Audio diagnostics and monitoring

### Data Channels
- ✅ Reliable and lossy data transmission
- ✅ Custom channel labels and ordering
- ✅ Size limits and best practices
- ✅ Structured messaging patterns

### Connection Management
- ✅ Synchronous and asynchronous connection
- ✅ Connection state tracking
- ✅ Automatic reconnection
- ✅ Role-based permissions

### Advanced Features
- ✅ Multiple audio tracks
- ✅ Extended callbacks with metadata
- ✅ Diagnostics and statistics
- ✅ Custom reconnection backoff
- ✅ Log level configuration

### Integration
- ✅ Unreal Engine plugin (ULiveKitPublisherComponent)
- ✅ CMake integration examples
- ✅ Visual Studio project setup
- ✅ Cross-platform support (Windows, Linux, macOS)

---

## Contributing to Documentation

We welcome contributions to improve the documentation! If you find:
- ❌ **Errors or typos** - Please submit a PR with fixes
- 📝 **Missing information** - Open an issue describing what's needed
- 💡 **Better examples** - Share your use cases and code
- 🔍 **Unclear explanations** - Let us know what's confusing

### Documentation Style Guide

When contributing:
1. **Be clear and concise** - Avoid jargon where possible
2. **Use examples** - Show, don't just tell
3. **Test your code** - All examples should compile and run
4. **Link related docs** - Cross-reference relevant sections
5. **Keep it current** - Update docs when APIs change

---

## Getting Help

### Documentation Not Enough?

1. **Check [Troubleshooting](TROUBLESHOOTING.md)** first
2. **Search [GitHub Issues](https://github.com/lifelike-and-believable/livekit-ffi-ue/issues)** for similar problems
3. **Ask in [LiveKit Slack](https://livekit.io/slack)** community
4. **Post on [LiveKit Forums](https://discuss.livekit.io/)** for async help
5. **Create an issue** with detailed information

### Reporting Documentation Issues

When reporting docs issues, include:
- 📍 Document name and section
- ❓ What you were trying to do
- 📋 What you expected vs. what you found
- 💭 Suggestions for improvement

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2025-11 | Initial comprehensive documentation release |
| | | - USER_GUIDE.md with 12 major sections |
| | | - 8 complete examples in EXAMPLES.md |
| | | - ARCHITECTURE.md covering system design |
| | | - TROUBLESHOOTING.md with 50+ solutions |

---

## External Resources

### LiveKit Documentation
- **[LiveKit Docs](https://docs.livekit.io/)** - Official LiveKit documentation
- **[Rust SDK](https://github.com/livekit/rust-sdks)** - LiveKit Rust SDK (underlying implementation)
- **[Protocol Reference](https://docs.livekit.io/protocol/)** - LiveKit protocol specification

### WebRTC Resources
- **[WebRTC.org](https://webrtc.org/)** - WebRTC standards and guides
- **[MDN WebRTC API](https://developer.mozilla.org/en-US/docs/Web/API/WebRTC_API)** - Browser WebRTC documentation

### Rust FFI
- **[Rust FFI Guide](https://doc.rust-lang.org/nomicon/ffi.html)** - Official Rust FFI documentation
- **[cbindgen](https://github.com/eqrion/cbindgen)** - Tool for generating C bindings

---

## License

All documentation is released under the MIT license, same as the code.

---

**Happy coding! 🚀**

For questions or feedback, visit our [GitHub repository](https://github.com/lifelike-and-believable/livekit-ffi-ue).
