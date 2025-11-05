# LiveKit FFI API Guide

This guide documents the enhanced LiveKit FFI C API for Unreal Engine and O3DS integration.

## Table of Contents
1. [Quick Start](#quick-start)
2. [Audio Configuration](#audio-configuration)
3. [Data Channel Usage](#data-channel-usage)
4. [Connection Lifecycle](#connection-lifecycle)
5. [Diagnostics and Monitoring](#diagnostics-and-monitoring)
6. [Error Handling](#error-handling)
7. [Threading Model](#threading-model)

## Quick Start

### Basic Connection (No Changes Required)

```c
// Existing code continues to work unchanged
LkClientHandle* client = lk_client_create();

LkResult result = lk_connect(client, "wss://your-server.com", "your-token");
if (result.code != 0) {
    printf("Connection failed: %s\n", result.message);
    lk_free_str((char*)result.message);
}

// ... use client ...

lk_disconnect(client);
lk_client_destroy(client);
```

## Audio Configuration

### Configure Audio Publishing

Control encoder bitrate, DTX (Discontinuous Transmission), and stereo/mono mode:

```c
LkClientHandle* client = lk_client_create();

// Set audio publish options before connecting
lk_set_audio_publish_options(client, 
    32000,  // bitrate in bps (24000-48000 typical)
    1,      // enable DTX (0=off, 1=on)
    0       // mono (0=mono, 1=stereo)
);

// Now connect and publish audio as usual
lk_connect(client, url, token);
lk_publish_audio_pcm_i16(client, pcm_data, frames, channels, sample_rate);
```

### Configure Audio Subscription Format

Request a specific output format for subscribed audio:

```c
// Set desired output format before subscribing
lk_set_audio_output_format(client, 48000, 1);  // 48kHz mono

// Set audio callback
lk_client_set_audio_callback(client, on_audio_frame, user_data);

// Audio frames will be resampled/downmixed to the requested format
```

### Audio Format Change Notifications

Get notified when the source audio format changes:

```c
void on_format_change(void* user, int32_t sample_rate, int32_t channels) {
    printf("Audio format changed: %dHz, %d channels\n", sample_rate, channels);
}

lk_set_audio_format_change_callback(client, on_format_change, user_data);
```

## Data Channel Usage

### Basic Data Sending (No Changes Required)

```c
// Existing code works unchanged
uint8_t data[] = { 0x01, 0x02, 0x03 };
lk_send_data(client, data, sizeof(data), LkReliable);
```

### Extended Data Sending with Labels

```c
// Send with custom label
uint8_t pose_data[512];
lk_send_data_ex(client, pose_data, sizeof(pose_data), 
    LkReliable,    // reliability
    1,             // ordered (1=ordered, 0=unordered)
    "pose-update"  // custom label (or NULL for default)
);

// Enforce size limits automatically:
// - Lossy: max 1300 bytes (returns error 201 if exceeded)
// - Reliable: max 15 KiB (returns error 202 if exceeded)
```

### Custom Default Labels

```c
// Set custom default labels for reliable/lossy channels
lk_set_default_data_labels(client, "my-reliable-channel", "my-lossy-channel");

// Now lk_send_data uses these labels
lk_send_data(client, data, len, LkReliable);  // Uses "my-reliable-channel"
```

### Extended Data Callback

Receive label and reliability information:

```c
void on_data_ex(void* user, const char* label, LkReliability reliability, 
                const uint8_t* bytes, size_t len) {
    printf("Received %zu bytes on '%s' (%s)\n", 
        len, label, reliability == LkReliable ? "reliable" : "lossy");
}

lk_client_set_data_callback_ex(client, on_data_ex, user_data);
```

## Connection Lifecycle

### Monitor Connection State

```c
void on_connection_state(void* user, LkConnectionState state, 
                         int32_t reason_code, const char* message) {
    switch (state) {
        case LkConnConnecting:
            printf("Connecting...\n");
            break;
        case LkConnConnected:
            printf("Connected!\n");
            break;
        case LkConnReconnecting:
            printf("Reconnecting...\n");
            break;
        case LkConnDisconnected:
            printf("Disconnected: %s\n", message ? message : "normal");
            break;
        case LkConnFailed:
            printf("Connection failed (code %d): %s\n", 
                reason_code, message ? message : "unknown");
            break;
    }
}

lk_set_connection_callback(client, on_connection_state, user_data);
lk_connect(client, url, token);
```

### Connection State Management

```c
// Check if connected
if (lk_client_is_ready(client)) {
    // Client is connected and ready
}

// Graceful disconnect (waits for callbacks to complete)
lk_disconnect(client);
```

## Diagnostics and Monitoring

### Audio Statistics

Monitor audio ring buffer health:

```c
LkAudioStats audio_stats;
lk_get_audio_stats(client, &audio_stats);

printf("Audio: %dHz, %dch\n", audio_stats.sample_rate, audio_stats.channels);
printf("Ring: %d/%d frames queued\n", 
    audio_stats.ring_queued_frames, 
    audio_stats.ring_capacity_frames);
printf("Underruns: %d, Overruns: %d\n", 
    audio_stats.underruns, 
    audio_stats.overruns);

// High underruns: audio thread not feeding fast enough
// High overruns: ring buffer too small or audio thread too fast
```

### Data Statistics

Track data channel performance:

```c
LkDataStats data_stats;
lk_get_data_stats(client, &data_stats);

printf("Reliable: %lld bytes sent, %lld dropped\n",
    data_stats.reliable_sent_bytes,
    data_stats.reliable_dropped);
printf("Lossy: %lld bytes sent, %lld dropped\n",
    data_stats.lossy_sent_bytes,
    data_stats.lossy_dropped);
```

### Logging

Control log verbosity:

```c
lk_set_log_level(client, LkLogDebug);  // Error, Warn, Info, Debug, Trace
```

## Error Handling

### Error Code Taxonomy

```c
LkResult result = lk_send_data(client, data, size, LkReliable);
if (result.code != 0) {
    // Error code ranges:
    // 1xx: Connection/Token errors (e.g., not connected)
    // 2xx: Data send errors
    //   201: Lossy data too large (> 1300 bytes)
    //   202: Reliable data too large (> 15 KiB)
    //   203: Send operation failed
    // 3xx: Audio publish errors
    // 4xx: Lifecycle errors
    // 5xx: Internal/unsupported errors (e.g., 501 = not supported)
    
    printf("Error %d: %s\n", result.code, result.message);
    lk_free_str((char*)result.message);  // Always free error messages
}
```

### Best Practices

1. **Always check return codes**:
   ```c
   LkResult r = lk_connect(client, url, token);
   if (r.code != 0) { /* handle error */ }
   ```

2. **Free error messages**:
   ```c
   if (result.message) {
       lk_free_str((char*)result.message);
   }
   ```

3. **Respect size limits**:
   ```c
   // Check before sending large payloads
   if (size > 1300 && reliability == LkLossy) {
       // Split or use reliable channel
   }
   ```

## Threading Model

### Callback Threading

**All callbacks may be invoked on background threads:**

```c
void on_audio_frame(void* user, const int16_t* pcm, size_t frames, 
                    int32_t channels, int32_t sample_rate) {
    // ⚠️ This runs on a background thread!
    // - Do NOT block or perform long operations
    // - Do NOT call sleep(), wait for locks, or do heavy processing
    // - Copy data quickly and return
    
    // ✅ Good: Quick copy and signal
    memcpy(my_buffer, pcm, frames * channels * sizeof(int16_t));
    signal_event();
    
    // ❌ Bad: Blocking operations
    // pthread_mutex_lock(&slow_lock);  // DON'T DO THIS
    // process_heavy_dsp(pcm);          // DON'T DO THIS
}
```

### API Thread Safety

**All API functions are thread-safe:**

```c
// Safe to call from any thread, even concurrently
void audio_thread() {
    lk_publish_audio_pcm_i16(client, pcm, frames, ch, sr);
}

void data_thread() {
    lk_send_data(client, data, len, LkReliable);
}

void ui_thread() {
    LkAudioStats stats;
    lk_get_audio_stats(client, &stats);
}
```

### Shutdown Guarantees

```c
// After disconnect/destroy returns, NO callbacks will fire
lk_disconnect(client);       // Blocks until callbacks quiesced
// NOW SAFE: No more callbacks

lk_client_destroy(client);   // Also blocks until callbacks quiesced
```

## Advanced Features

### Token Refresh

**Note:** Token refresh at runtime is not currently supported by the underlying LiveKit SDK.

```c
// This will return error 501 (not supported)
LkResult r = lk_refresh_token(client, new_token);
if (r.code == 501) {
    // Fallback: disconnect and reconnect with new token
    lk_disconnect(client);
    lk_connect(client, url, new_token);
}
```

### Dynamic Role Switching

**Note:** Role switching without reconnect is not currently supported.

```c
// This will return error 501 (not supported)
LkResult r = lk_set_role(client, LkRolePublisher, 0);
if (r.code == 501) {
    // Fallback: disconnect and reconnect with new role
    lk_disconnect(client);
    lk_connect_with_role(client, url, token, LkRolePublisher);
}
```

### Reconnection Backoff

**Note:** This is a placeholder for future SDK support.

```c
// Currently a no-op; SDK manages reconnection internally
lk_set_reconnect_backoff(client, 100, 5000, 1.5);
```

## Migration Guide

### From Original API

**No changes required!** All original functions work unchanged:

```c
// All of this continues to work exactly as before:
LkClientHandle* client = lk_client_create();
lk_client_set_data_callback(client, on_data, user);
lk_client_set_audio_callback(client, on_audio, user);
lk_connect(client, url, token);
lk_publish_audio_pcm_i16(client, pcm, frames, ch, sr);
lk_send_data(client, data, len, LkReliable);
lk_disconnect(client);
lk_client_destroy(client);
```

### Adding New Features

Simply add calls to new functions as needed:

```c
LkClientHandle* client = lk_client_create();

// NEW: Configure before connecting
lk_set_audio_publish_options(client, 32000, 1, 0);
lk_set_connection_callback(client, on_connection, user);

// Original workflow continues unchanged
lk_connect(client, url, token);
lk_publish_audio_pcm_i16(client, pcm, frames, ch, sr);

// NEW: Monitor statistics periodically
LkAudioStats stats;
lk_get_audio_stats(client, &stats);
```

## Complete Example

```c
#include "livekit_ffi.h"
#include <stdio.h>
#include <string.h>

void on_connection(void* user, LkConnectionState state, int32_t code, const char* msg) {
    printf("Connection state: %d\n", state);
}

void on_audio(void* user, const int16_t* pcm, size_t frames, int32_t ch, int32_t sr) {
    // Process audio quickly - we're on a background thread
    // Copy to ring buffer or process with minimal latency
}

void on_data_ex(void* user, const char* label, LkReliability rel, 
                const uint8_t* bytes, size_t len) {
    printf("Received %zu bytes on '%s'\n", len, label);
}

int main() {
    LkClientHandle* client = lk_client_create();
    
    // Configure
    lk_set_audio_publish_options(client, 32000, 1, 0);
    lk_set_audio_output_format(client, 48000, 1);
    lk_set_log_level(client, LkLogInfo);
    
    // Set callbacks
    lk_set_connection_callback(client, on_connection, NULL);
    lk_client_set_audio_callback(client, on_audio, NULL);
    lk_client_set_data_callback_ex(client, on_data_ex, NULL);
    
    // Connect
    LkResult result = lk_connect(client, "wss://server.com", "token");
    if (result.code != 0) {
        printf("Connect failed: %s\n", result.message);
        lk_free_str((char*)result.message);
        lk_client_destroy(client);
        return 1;
    }
    
    // Main loop
    while (lk_client_is_ready(client)) {
        // Publish audio
        int16_t audio[480];  // 10ms @ 48kHz
        // ... fill audio ...
        lk_publish_audio_pcm_i16(client, audio, 480, 1, 48000);
        
        // Send data
        uint8_t mocap[256];
        // ... fill mocap ...
        lk_send_data_ex(client, mocap, sizeof(mocap), LkLossy, 1, "mocap");
        
        // Check stats periodically
        LkAudioStats stats;
        lk_get_audio_stats(client, &stats);
        if (stats.underruns > 0 || stats.overruns > 0) {
            printf("Audio issues: underruns=%d, overruns=%d\n", 
                stats.underruns, stats.overruns);
        }
        
        // Sleep or yield
        usleep(10000);  // 10ms
    }
    
    // Cleanup
    lk_disconnect(client);
    lk_client_destroy(client);
    return 0;
}
```

## Troubleshooting

### Audio Underruns

**Symptom:** `lk_get_audio_stats` shows high underrun count

**Causes:**
- Not calling `lk_publish_audio_pcm_i16` frequently enough
- Network issues causing frame drops

**Solutions:**
- Call publish every 10-20ms consistently
- Increase buffer size if needed

### Audio Overruns

**Symptom:** `lk_get_audio_stats` shows high overrun count

**Causes:**
- Publishing audio faster than consumer can process
- Ring buffer too small

**Solutions:**
- Reduce publish frequency
- Increase ring buffer capacity (rebuild required)

### Data Drops

**Symptom:** `lk_get_data_stats` shows dropped packets

**Causes:**
- Network congestion
- Sending too much data
- Exceeding size limits

**Solutions:**
- Reduce data rate for lossy channel
- Use reliable channel for important data
- Split large payloads into smaller chunks

### Connection Issues

**Symptom:** Connection callback shows Reconnecting/Failed states

**Causes:**
- Network interruption
- Invalid token
- Server unavailable

**Solutions:**
- Check network connectivity
- Verify token is valid and not expired
- Monitor connection callback for state transitions
- Implement reconnection logic in your app

## Performance Tips

1. **Audio**: Publish 10-20ms chunks @ 48kHz for optimal latency/throughput balance
2. **Data**: Keep lossy packets ≤ 1000 bytes for best reliability
3. **Statistics**: Query stats every 1-5 seconds, not every frame
4. **Callbacks**: Copy data quickly and process off the callback thread
5. **Threading**: Don't block in callbacks - use lock-free queues if possible

## Further Reading

- [LiveKit Documentation](https://docs.livekit.io/)
- [Original FFI README](../README.md)
- [Local Server Setup](LOCAL_LIVEKIT_QUICKSTART.md)
