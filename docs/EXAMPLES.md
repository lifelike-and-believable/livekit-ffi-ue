# LiveKit FFI - Code Examples

This document provides complete, working code examples for common use cases with the LiveKit FFI library.

## Table of Contents

1. [Simple Voice Chat](#simple-voice-chat)
2. [Motion Capture Streaming](#motion-capture-streaming)
3. [Multi-Track Audio Publisher](#multi-track-audio-publisher)
4. [Data Channel Communication](#data-channel-communication)
5. [Connection State Management](#connection-state-management)
6. [Audio Recording and Playback](#audio-recording-and-playback)
7. [Real-Time Diagnostics Monitor](#real-time-diagnostics-monitor)
8. [Cross-Platform Client](#cross-platform-client)

---

## Simple Voice Chat

A minimal example of publishing and receiving audio for voice chat.

```cpp
#include "livekit_ffi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <Windows.h>
#define sleep_ms(ms) Sleep(ms)
#else
#include <unistd.h>
#define sleep_ms(ms) usleep((ms) * 1000)
#endif

// Simple audio callback that logs received audio
void on_audio_received(void* user, const int16_t* pcm, size_t frames,
                       int32_t channels, int32_t sample_rate) {
    printf("Received audio: %zu frames @ %dHz (%d channels)\n", 
           frames, sample_rate, channels);
    
    // In a real app, you would:
    // - Queue this for playback
    // - Write to audio output device
    // - Process/mix with other audio
}

// Connection state callback
void on_connection_state(void* user, LkConnectionState state,
                         int32_t code, const char* msg) {
    switch (state) {
        case LkConnConnecting:
            printf("[Connection] Connecting...\n");
            break;
        case LkConnConnected:
            printf("[Connection] Connected!\n");
            break;
        case LkConnReconnecting:
            printf("[Connection] Reconnecting...\n");
            break;
        case LkConnDisconnected:
            printf("[Connection] Disconnected\n");
            break;
        case LkConnFailed:
            printf("[Connection] Failed (code %d): %s\n", code, msg);
            break;
    }
}

// Simulate capturing audio from microphone
void capture_audio(int16_t* buffer, size_t frames, int channels) {
    // Generate a simple sine wave for testing
    static float phase = 0.0f;
    const float frequency = 440.0f;  // A4 note
    const float sample_rate = 48000.0f;
    
    for (size_t i = 0; i < frames; i++) {
        float sample = sinf(2.0f * 3.14159f * frequency * phase / sample_rate);
        int16_t value = (int16_t)(sample * 0.2f * 32767.0f);
        
        for (int ch = 0; ch < channels; ch++) {
            buffer[i * channels + ch] = value;
        }
        
        phase += 1.0f;
        if (phase >= sample_rate) phase -= sample_rate;
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("Usage: %s <room_url> <token>\n", argv[0]);
        printf("Example: %s ws://localhost:7880 eyJhbG...\n", argv[0]);
        return 1;
    }
    
    const char* url = argv[1];
    const char* token = argv[2];
    
    printf("=== LiveKit Voice Chat Example ===\n");
    
    // Create client
    LkClientHandle* client = lk_client_create();
    if (!client) {
        fprintf(stderr, "Failed to create client\n");
        return 1;
    }
    
    // Configure audio output format
    lk_set_audio_output_format(client, 48000, 1);  // 48kHz mono
    
    // Configure audio publishing (32kbps, DTX enabled, mono)
    lk_set_audio_publish_options(client, 32000, 1, 0);
    
    // Set callbacks
    lk_client_set_audio_callback(client, on_audio_received, NULL);
    lk_set_connection_callback(client, on_connection_state, NULL);
    
    // Connect to room
    printf("Connecting to %s...\n", url);
    LkResult result = lk_connect(client, url, token);
    if (result.code != 0) {
        fprintf(stderr, "Connection failed: %s\n", result.message);
        lk_free_str((char*)result.message);
        lk_client_destroy(client);
        return 1;
    }
    
    printf("Connected! Publishing audio for 30 seconds...\n");
    printf("Press Ctrl+C to stop\n\n");
    
    // Main loop: publish audio every 20ms
    const int sample_rate = 48000;
    const int channels = 1;
    const int frames_per_chunk = 960;  // 20ms @ 48kHz
    int16_t audio_buffer[960];
    
    for (int i = 0; i < 1500 && lk_client_is_ready(client); i++) {  // 30 seconds
        // Capture audio (simulated)
        capture_audio(audio_buffer, frames_per_chunk, channels);
        
        // Publish audio
        result = lk_publish_audio_pcm_i16(client, audio_buffer, 
                                         frames_per_chunk, channels, sample_rate);
        if (result.code != 0) {
            fprintf(stderr, "Failed to publish audio: %s\n", result.message);
            lk_free_str((char*)result.message);
        }
        
        // Wait 20ms
        sleep_ms(20);
        
        // Print status every 5 seconds
        if (i % 250 == 0) {
            LkAudioStats stats;
            if (lk_get_audio_stats(client, &stats).code == 0) {
                printf("[%02d:%02d] Buffer: %d/%d frames, Underruns: %d, Overruns: %d\n",
                       i / 3000, (i / 50) % 60,
                       stats.ring_queued_frames, stats.ring_capacity_frames,
                       stats.underruns, stats.overruns);
            }
        }
    }
    
    printf("\nDisconnecting...\n");
    lk_disconnect(client);
    lk_client_destroy(client);
    
    printf("Done!\n");
    return 0;
}
```

**Compile:**
```bash
# Windows (MSVC)
cl voice_chat.c livekit_ffi.dll.lib /I"path/to/include"

# Linux/macOS
gcc voice_chat.c -o voice_chat -I./include -L./lib -llivekit_ffi -lm
```

---

## Motion Capture Streaming

Stream motion capture data with lossy channel for high-frequency updates.

```cpp
#include "livekit_ffi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Motion capture data structure
typedef struct {
    uint32_t timestamp_ms;
    uint8_t actor_id;
    uint8_t bone_count;
    struct {
        float position[3];    // x, y, z
        float rotation[4];    // quaternion (x, y, z, w)
    } bones[32];
} MocapFrame;

// Serialize mocap frame to bytes
size_t serialize_mocap(uint8_t* buffer, size_t buffer_size, const MocapFrame* frame) {
    size_t offset = 0;
    
    // Write header
    memcpy(buffer + offset, &frame->timestamp_ms, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    buffer[offset++] = frame->actor_id;
    buffer[offset++] = frame->bone_count;
    
    // Write bones
    for (int i = 0; i < frame->bone_count; i++) {
        memcpy(buffer + offset, frame->bones[i].position, sizeof(float) * 3);
        offset += sizeof(float) * 3;
        memcpy(buffer + offset, frame->bones[i].rotation, sizeof(float) * 4);
        offset += sizeof(float) * 4;
    }
    
    return offset;
}

// Deserialize mocap frame from bytes
bool deserialize_mocap(MocapFrame* frame, const uint8_t* buffer, size_t size) {
    if (size < 6) return false;
    
    size_t offset = 0;
    memcpy(&frame->timestamp_ms, buffer + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    frame->actor_id = buffer[offset++];
    frame->bone_count = buffer[offset++];
    
    if (frame->bone_count > 32) return false;
    
    size_t expected_size = 6 + frame->bone_count * (3 + 4) * sizeof(float);
    if (size < expected_size) return false;
    
    for (int i = 0; i < frame->bone_count; i++) {
        memcpy(frame->bones[i].position, buffer + offset, sizeof(float) * 3);
        offset += sizeof(float) * 3;
        memcpy(frame->bones[i].rotation, buffer + offset, sizeof(float) * 4);
        offset += sizeof(float) * 4;
    }
    
    return true;
}

// Data callback for receiving mocap
void on_mocap_received(void* user, const char* label, LkReliability reliability,
                       const uint8_t* bytes, size_t len) {
    if (strcmp(label, "mocap") == 0) {
        MocapFrame frame;
        if (deserialize_mocap(&frame, bytes, len)) {
            printf("Received mocap: timestamp=%u, actor=%d, bones=%d\n",
                   frame.timestamp_ms, frame.actor_id, frame.bone_count);
        }
    }
}

// Generate fake mocap data for testing
void generate_mocap_frame(MocapFrame* frame, uint8_t actor_id) {
    static uint32_t start_time_ms = 0;
    if (start_time_ms == 0) {
        start_time_ms = (uint32_t)(clock() * 1000 / CLOCKS_PER_SEC);
    }
    
    frame->timestamp_ms = (uint32_t)(clock() * 1000 / CLOCKS_PER_SEC) - start_time_ms;
    frame->actor_id = actor_id;
    frame->bone_count = 10;
    
    // Generate fake bone transforms
    for (int i = 0; i < frame->bone_count; i++) {
        frame->bones[i].position[0] = (float)i * 0.1f;
        frame->bones[i].position[1] = sinf((float)frame->timestamp_ms * 0.001f + i);
        frame->bones[i].position[2] = cosf((float)frame->timestamp_ms * 0.001f + i);
        
        frame->bones[i].rotation[0] = 0.0f;
        frame->bones[i].rotation[1] = 0.0f;
        frame->bones[i].rotation[2] = 0.0f;
        frame->bones[i].rotation[3] = 1.0f;
    }
}

int main(int argc, char** argv) {
    if (argc < 4) {
        printf("Usage: %s <url> <token> <mode>\n", argv[0]);
        printf("  mode: publisher or subscriber\n");
        return 1;
    }
    
    const char* url = argv[1];
    const char* token = argv[2];
    const char* mode = argv[3];
    
    bool is_publisher = (strcmp(mode, "publisher") == 0);
    
    printf("=== LiveKit Mocap Streaming Example ===\n");
    printf("Mode: %s\n", is_publisher ? "Publisher" : "Subscriber");
    
    // Create client
    LkClientHandle* client = lk_client_create();
    
    // Set data callback for subscribers
    if (!is_publisher) {
        lk_client_set_data_callback_ex(client, on_mocap_received, NULL);
    }
    
    // Connect with appropriate role
    LkRole role = is_publisher ? LkRolePublisher : LkRoleSubscriber;
    LkResult result = lk_connect_with_role(client, url, token, role);
    
    if (result.code != 0) {
        fprintf(stderr, "Connection failed: %s\n", result.message);
        lk_free_str((char*)result.message);
        lk_client_destroy(client);
        return 1;
    }
    
    printf("Connected! %s mocap data...\n", 
           is_publisher ? "Publishing" : "Receiving");
    
    if (is_publisher) {
        // Publisher: send mocap at 30 FPS
        uint8_t buffer[1024];
        MocapFrame frame;
        int frames_sent = 0;
        
        for (int i = 0; i < 900; i++) {  // 30 seconds @ 30 FPS
            generate_mocap_frame(&frame, 1);
            size_t size = serialize_mocap(buffer, sizeof(buffer), &frame);
            
            // Send on lossy channel (unordered, best effort)
            result = lk_send_data_ex(client, buffer, size, LkLossy, 0, "mocap");
            
            if (result.code == 0) {
                frames_sent++;
                if (frames_sent % 30 == 0) {
                    printf("Sent %d frames\n", frames_sent);
                }
            } else {
                fprintf(stderr, "Send failed: %s\n", result.message);
                lk_free_str((char*)result.message);
            }
            
            sleep_ms(33);  // ~30 FPS
        }
        
        printf("Published %d mocap frames\n", frames_sent);
        
    } else {
        // Subscriber: receive for 30 seconds
        printf("Listening for mocap data...\n");
        sleep_ms(30000);
    }
    
    // Cleanup
    lk_disconnect(client);
    lk_client_destroy(client);
    
    return 0;
}
```

---

## Multi-Track Audio Publisher

Publish multiple audio tracks (e.g., microphone + game audio).

```cpp
#include "livekit_ffi.h"
#include <stdio.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Track context
typedef struct {
    LkAudioTrackHandle* handle;
    const char* name;
    int sample_rate;
    int channels;
    float phase;
    float frequency;
} AudioTrack;

// Generate sine wave audio
void generate_audio(int16_t* buffer, size_t frames, int channels,
                   float* phase, float frequency, float sample_rate) {
    for (size_t i = 0; i < frames; i++) {
        float sample = sinf(2.0f * M_PI * frequency * (*phase) / sample_rate);
        int16_t value = (int16_t)(sample * 0.3f * 32767.0f);
        
        for (int ch = 0; ch < channels; ch++) {
            buffer[i * channels + ch] = value;
        }
        
        (*phase) += 1.0f;
        if (*phase >= sample_rate) *phase -= sample_rate;
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("Usage: %s <url> <token>\n", argv[0]);
        return 1;
    }
    
    const char* url = argv[1];
    const char* token = argv[2];
    
    printf("=== Multi-Track Audio Publisher ===\n");
    
    // Create client
    LkClientHandle* client = lk_client_create();
    
    // Connect as publisher
    LkResult result = lk_connect_with_role(client, url, token, LkRolePublisher);
    if (result.code != 0) {
        fprintf(stderr, "Connection failed: %s\n", result.message);
        lk_free_str((char*)result.message);
        lk_client_destroy(client);
        return 1;
    }
    
    printf("Connected! Creating audio tracks...\n");
    
    // Create microphone track (mono, 48kHz)
    AudioTrack mic_track = {
        .name = "microphone",
        .sample_rate = 48000,
        .channels = 1,
        .phase = 0.0f,
        .frequency = 440.0f  // A4
    };
    
    LkAudioTrackConfig mic_config = {
        .track_name = mic_track.name,
        .sample_rate = mic_track.sample_rate,
        .channels = mic_track.channels,
        .buffer_ms = 1000
    };
    
    result = lk_audio_track_create(client, &mic_config, &mic_track.handle);
    if (result.code != 0) {
        fprintf(stderr, "Failed to create mic track: %s\n", result.message);
        lk_free_str((char*)result.message);
        lk_disconnect(client);
        lk_client_destroy(client);
        return 1;
    }
    
    // Create game audio track (stereo, 48kHz)
    AudioTrack game_track = {
        .name = "game-audio",
        .sample_rate = 48000,
        .channels = 2,
        .phase = 0.0f,
        .frequency = 880.0f  // A5
    };
    
    LkAudioTrackConfig game_config = {
        .track_name = game_track.name,
        .sample_rate = game_track.sample_rate,
        .channels = game_track.channels,
        .buffer_ms = 1000
    };
    
    result = lk_audio_track_create(client, &game_config, &game_track.handle);
    if (result.code != 0) {
        fprintf(stderr, "Failed to create game track: %s\n", result.message);
        lk_free_str((char*)result.message);
        lk_audio_track_destroy(mic_track.handle);
        lk_disconnect(client);
        lk_client_destroy(client);
        return 1;
    }
    
    printf("Created tracks: %s, %s\n", mic_track.name, game_track.name);
    printf("Publishing for 30 seconds...\n");
    
    // Publish audio to both tracks
    const int frames_per_chunk = 480;  // 10ms @ 48kHz
    int16_t mic_buffer[480];
    int16_t game_buffer[960];  // Stereo = 2x samples
    
    for (int i = 0; i < 3000; i++) {  // 30 seconds
        // Generate microphone audio
        generate_audio(mic_buffer, frames_per_chunk, mic_track.channels,
                      &mic_track.phase, mic_track.frequency, 
                      (float)mic_track.sample_rate);
        
        // Generate game audio
        generate_audio(game_buffer, frames_per_chunk, game_track.channels,
                      &game_track.phase, game_track.frequency,
                      (float)game_track.sample_rate);
        
        // Publish to tracks
        lk_audio_track_publish_pcm_i16(mic_track.handle, mic_buffer, frames_per_chunk);
        lk_audio_track_publish_pcm_i16(game_track.handle, game_buffer, frames_per_chunk);
        
        // Progress indicator
        if (i % 100 == 0) {
            printf("Published %d chunks to each track\n", i);
        }
        
        sleep_ms(10);
    }
    
    printf("\nCleaning up...\n");
    
    // Destroy tracks
    lk_audio_track_destroy(mic_track.handle);
    lk_audio_track_destroy(game_track.handle);
    
    // Disconnect
    lk_disconnect(client);
    lk_client_destroy(client);
    
    printf("Done!\n");
    return 0;
}
```

---

## Data Channel Communication

Structured request-response communication using data channels.

```cpp
#include "livekit_ffi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Message types
typedef enum {
    MSG_PING = 1,
    MSG_PONG = 2,
    MSG_REQUEST = 3,
    MSG_RESPONSE = 4,
    MSG_NOTIFICATION = 5
} MessageType;

// Message structure
typedef struct {
    uint8_t type;
    uint32_t id;
    uint16_t payload_size;
    uint8_t payload[512];
} Message;

// Serialize message
size_t serialize_message(uint8_t* buffer, const Message* msg) {
    size_t offset = 0;
    buffer[offset++] = msg->type;
    memcpy(buffer + offset, &msg->id, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(buffer + offset, &msg->payload_size, sizeof(uint16_t));
    offset += sizeof(uint16_t);
    memcpy(buffer + offset, msg->payload, msg->payload_size);
    offset += msg->payload_size;
    return offset;
}

// Deserialize message
bool deserialize_message(Message* msg, const uint8_t* buffer, size_t size) {
    if (size < 7) return false;
    
    size_t offset = 0;
    msg->type = buffer[offset++];
    memcpy(&msg->id, buffer + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(&msg->payload_size, buffer + offset, sizeof(uint16_t));
    offset += sizeof(uint16_t);
    
    if (size < offset + msg->payload_size) return false;
    memcpy(msg->payload, buffer + offset, msg->payload_size);
    
    return true;
}

// Application context
typedef struct {
    LkClientHandle* client;
    uint32_t next_message_id;
    bool is_server;
} AppContext;

// Data callback
void on_data_received(void* user, const char* label, LkReliability reliability,
                     const uint8_t* bytes, size_t len) {
    AppContext* ctx = (AppContext*)user;
    
    Message msg;
    if (!deserialize_message(&msg, bytes, len)) {
        fprintf(stderr, "Failed to deserialize message\n");
        return;
    }
    
    printf("[Received] Type=%d, ID=%u, PayloadSize=%u\n", 
           msg.type, msg.id, msg.payload_size);
    
    // Handle message based on type
    switch (msg.type) {
        case MSG_PING:
            printf("  PING received, sending PONG\n");
            if (ctx->is_server) {
                // Send pong response
                Message pong = {
                    .type = MSG_PONG,
                    .id = msg.id,
                    .payload_size = 0
                };
                
                uint8_t buffer[520];
                size_t size = serialize_message(buffer, &pong);
                lk_send_data_ex(ctx->client, buffer, size, LkReliable, 1, "rpc");
            }
            break;
            
        case MSG_PONG:
            printf("  PONG received for message %u\n", msg.id);
            break;
            
        case MSG_REQUEST:
            printf("  REQUEST: %.*s\n", msg.payload_size, msg.payload);
            if (ctx->is_server) {
                // Send response
                Message response = {
                    .type = MSG_RESPONSE,
                    .id = msg.id,
                    .payload_size = snprintf((char*)response.payload, 
                                           sizeof(response.payload),
                                           "Response to: %.*s", 
                                           msg.payload_size, msg.payload)
                };
                
                uint8_t buffer[520];
                size_t size = serialize_message(buffer, &response);
                lk_send_data_ex(ctx->client, buffer, size, LkReliable, 1, "rpc");
            }
            break;
            
        case MSG_RESPONSE:
            printf("  RESPONSE: %.*s\n", msg.payload_size, msg.payload);
            break;
            
        case MSG_NOTIFICATION:
            printf("  NOTIFICATION: %.*s\n", msg.payload_size, msg.payload);
            break;
    }
}

void send_ping(AppContext* ctx) {
    Message msg = {
        .type = MSG_PING,
        .id = ctx->next_message_id++,
        .payload_size = 0
    };
    
    uint8_t buffer[520];
    size_t size = serialize_message(buffer, &msg);
    
    LkResult result = lk_send_data_ex(ctx->client, buffer, size, 
                                      LkReliable, 1, "rpc");
    if (result.code == 0) {
        printf("[Sent] PING (id=%u)\n", msg.id);
    } else {
        fprintf(stderr, "Send failed: %s\n", result.message);
        lk_free_str((char*)result.message);
    }
}

void send_request(AppContext* ctx, const char* request_text) {
    Message msg = {
        .type = MSG_REQUEST,
        .id = ctx->next_message_id++,
        .payload_size = strlen(request_text)
    };
    
    memcpy(msg.payload, request_text, msg.payload_size);
    
    uint8_t buffer[520];
    size_t size = serialize_message(buffer, &msg);
    
    LkResult result = lk_send_data_ex(ctx->client, buffer, size,
                                      LkReliable, 1, "rpc");
    if (result.code == 0) {
        printf("[Sent] REQUEST (id=%u): %s\n", msg.id, request_text);
    }
}

int main(int argc, char** argv) {
    if (argc < 4) {
        printf("Usage: %s <url> <token> <mode>\n", argv[0]);
        printf("  mode: client or server\n");
        return 1;
    }
    
    const char* url = argv[1];
    const char* token = argv[2];
    const char* mode = argv[3];
    
    AppContext ctx = {
        .next_message_id = 1,
        .is_server = (strcmp(mode, "server") == 0)
    };
    
    printf("=== Data Channel RPC Example ===\n");
    printf("Mode: %s\n", ctx.is_server ? "Server" : "Client");
    
    // Create and connect
    ctx.client = lk_client_create();
    lk_client_set_data_callback_ex(ctx.client, on_data_received, &ctx);
    
    LkResult result = lk_connect(ctx.client, url, token);
    if (result.code != 0) {
        fprintf(stderr, "Connection failed: %s\n", result.message);
        lk_free_str((char*)result.message);
        lk_client_destroy(ctx.client);
        return 1;
    }
    
    printf("Connected!\n");
    
    if (ctx.is_server) {
        // Server: just listen and respond
        printf("Server listening for requests...\n");
        sleep_ms(30000);  // Run for 30 seconds
    } else {
        // Client: send periodic requests
        printf("Client sending requests...\n");
        
        for (int i = 0; i < 10; i++) {
            // Send ping
            send_ping(&ctx);
            sleep_ms(1000);
            
            // Send request
            char request[128];
            snprintf(request, sizeof(request), "Request #%d", i + 1);
            send_request(&ctx, request);
            sleep_ms(2000);
        }
    }
    
    // Cleanup
    lk_disconnect(ctx.client);
    lk_client_destroy(ctx.client);
    
    return 0;
}
```

---

## Connection State Management

Robust connection handling with reconnection logic.

```cpp
#include "livekit_ffi.h"
#include <stdio.h>
#include <stdbool.h>
#include <time.h>

typedef struct {
    LkClientHandle* client;
    const char* url;
    const char* token;
    bool should_reconnect;
    int reconnect_count;
    time_t last_connected_time;
    bool is_connected;
} ConnectionManager;

void on_connection_change(void* user, LkConnectionState state,
                         int32_t code, const char* msg) {
    ConnectionManager* mgr = (ConnectionManager*)user;
    
    switch (state) {
        case LkConnConnecting:
            printf("[%ld] Connecting...\n", time(NULL));
            break;
            
        case LkConnConnected:
            printf("[%ld] Connected successfully!\n", time(NULL));
            mgr->is_connected = true;
            mgr->reconnect_count = 0;
            mgr->last_connected_time = time(NULL);
            break;
            
        case LkConnReconnecting:
            printf("[%ld] Connection lost, SDK is reconnecting...\n", time(NULL));
            mgr->is_connected = false;
            break;
            
        case LkConnDisconnected:
            printf("[%ld] Disconnected%s%s\n", time(NULL),
                   msg ? ": " : "", msg ? msg : "");
            mgr->is_connected = false;
            
            // Calculate uptime
            if (mgr->last_connected_time > 0) {
                time_t uptime = time(NULL) - mgr->last_connected_time;
                printf("  Session uptime: %ld seconds\n", uptime);
            }
            break;
            
        case LkConnFailed:
            fprintf(stderr, "[%ld] Connection FAILED (code %d): %s\n",
                   time(NULL), code, msg ? msg : "unknown");
            mgr->is_connected = false;
            
            // Implement reconnection logic
            if (mgr->should_reconnect && mgr->reconnect_count < 5) {
                mgr->reconnect_count++;
                int delay = mgr->reconnect_count * 2;  // Exponential backoff
                printf("  Will retry in %d seconds (attempt %d/5)\n", 
                       delay, mgr->reconnect_count);
                
                sleep_ms(delay * 1000);
                
                printf("  Attempting reconnection...\n");
                LkResult result = lk_connect(mgr->client, mgr->url, mgr->token);
                if (result.code != 0) {
                    fprintf(stderr, "  Reconnect failed: %s\n", result.message);
                    lk_free_str((char*)result.message);
                }
            } else {
                printf("  Max reconnection attempts reached\n");
                mgr->should_reconnect = false;
            }
            break;
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("Usage: %s <url> <token>\n", argv[0]);
        return 1;
    }
    
    printf("=== Connection State Management Example ===\n");
    
    ConnectionManager mgr = {
        .url = argv[1],
        .token = argv[2],
        .should_reconnect = true,
        .reconnect_count = 0,
        .last_connected_time = 0,
        .is_connected = false
    };
    
    // Create client
    mgr.client = lk_client_create();
    lk_set_connection_callback(mgr.client, on_connection_change, &mgr);
    
    // Configure reconnection
    lk_set_reconnect_backoff(mgr.client, 500, 10000, 1.5f);
    
    // Initial connection
    printf("Initiating connection...\n");
    LkResult result = lk_connect(mgr.client, mgr.url, mgr.token);
    if (result.code != 0) {
        fprintf(stderr, "Initial connection failed: %s\n", result.message);
        lk_free_str((char*)result.message);
    }
    
    // Main loop: monitor connection for 60 seconds
    printf("\nMonitoring connection for 60 seconds...\n");
    printf("(You can test reconnection by stopping/starting the server)\n\n");
    
    for (int i = 0; i < 60; i++) {
        sleep_ms(1000);
        
        // Check connection state
        bool ready = lk_client_is_ready(mgr.client);
        
        // Print status every 10 seconds
        if (i % 10 == 0) {
            printf("[%02d/%02d] Status: %s\n", i, 60,
                   ready ? "Connected" : "Disconnected");
            
            if (ready) {
                // Get statistics
                LkAudioStats audio_stats;
                LkDataStats data_stats;
                
                lk_get_audio_stats(mgr.client, &audio_stats);
                lk_get_data_stats(mgr.client, &data_stats);
                
                printf("  Audio: %d underruns, %d overruns\n",
                       audio_stats.underruns, audio_stats.overruns);
                printf("  Data: %lld reliable bytes, %lld lossy bytes\n",
                       data_stats.reliable_sent_bytes, data_stats.lossy_sent_bytes);
            }
        }
        
        if (!mgr.should_reconnect && !ready) {
            printf("Connection lost permanently, exiting early\n");
            break;
        }
    }
    
    // Cleanup
    printf("\nShutting down...\n");
    mgr.should_reconnect = false;  // Prevent reconnection during shutdown
    lk_disconnect(mgr.client);
    lk_client_destroy(mgr.client);
    
    printf("Done!\n");
    return 0;
}
```

---

## Audio Recording and Playback

Record received audio to a file and play back local audio.

```cpp
#include "livekit_ffi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// WAV file header
typedef struct {
    char riff[4];           // "RIFF"
    uint32_t file_size;     // File size - 8
    char wave[4];           // "WAVE"
    char fmt[4];            // "fmt "
    uint32_t fmt_size;      // 16 for PCM
    uint16_t audio_format;  // 1 for PCM
    uint16_t channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char data[4];           // "data"
    uint32_t data_size;
} WAVHeader;

// Recording context
typedef struct {
    FILE* file;
    size_t samples_written;
    int sample_rate;
    int channels;
} RecordingContext;

// Write WAV header
void write_wav_header(FILE* file, int sample_rate, int channels) {
    WAVHeader header = {
        .riff = {'R', 'I', 'F', 'F'},
        .file_size = 0,  // Will update later
        .wave = {'W', 'A', 'V', 'E'},
        .fmt = {'f', 'm', 't', ' '},
        .fmt_size = 16,
        .audio_format = 1,  // PCM
        .channels = channels,
        .sample_rate = sample_rate,
        .byte_rate = sample_rate * channels * 2,
        .block_align = channels * 2,
        .bits_per_sample = 16,
        .data = {'d', 'a', 't', 'a'},
        .data_size = 0  // Will update later
    };
    
    fwrite(&header, sizeof(WAVHeader), 1, file);
}

// Update WAV header with final sizes
void finalize_wav_file(FILE* file, size_t samples_written, int channels) {
    uint32_t data_size = samples_written * channels * 2;
    uint32_t file_size = data_size + sizeof(WAVHeader) - 8;
    
    // Update file size
    fseek(file, 4, SEEK_SET);
    fwrite(&file_size, sizeof(uint32_t), 1, file);
    
    // Update data size
    fseek(file, sizeof(WAVHeader) - 4, SEEK_SET);
    fwrite(&data_size, sizeof(uint32_t), 1, file);
}

// Audio callback for recording
void on_audio_for_recording(void* user, const int16_t* pcm, size_t frames,
                           int32_t channels, int32_t sample_rate) {
    RecordingContext* ctx = (RecordingContext*)user;
    
    if (!ctx->file) return;
    
    // Write samples to file
    size_t total_samples = frames * channels;
    size_t written = fwrite(pcm, sizeof(int16_t), total_samples, ctx->file);
    ctx->samples_written += written / channels;
    
    printf("\rRecorded %zu frames (%.1f seconds)", 
           ctx->samples_written,
           (float)ctx->samples_written / sample_rate);
    fflush(stdout);
}

// Read audio from WAV file
bool read_wav_file(const char* filename, int16_t** out_samples, 
                  size_t* out_frames, int* out_channels, int* out_sample_rate) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Failed to open %s\n", filename);
        return false;
    }
    
    WAVHeader header;
    if (fread(&header, sizeof(WAVHeader), 1, file) != 1) {
        fprintf(stderr, "Failed to read WAV header\n");
        fclose(file);
        return false;
    }
    
    // Validate header
    if (memcmp(header.riff, "RIFF", 4) != 0 ||
        memcmp(header.wave, "WAVE", 4) != 0 ||
        header.audio_format != 1) {
        fprintf(stderr, "Invalid WAV file format\n");
        fclose(file);
        return false;
    }
    
    *out_channels = header.channels;
    *out_sample_rate = header.sample_rate;
    size_t total_samples = header.data_size / 2;
    *out_frames = total_samples / header.channels;
    
    *out_samples = (int16_t*)malloc(header.data_size);
    if (!*out_samples) {
        fprintf(stderr, "Failed to allocate memory\n");
        fclose(file);
        return false;
    }
    
    if (fread(*out_samples, 1, header.data_size, file) != header.data_size) {
        fprintf(stderr, "Failed to read audio data\n");
        free(*out_samples);
        fclose(file);
        return false;
    }
    
    fclose(file);
    return true;
}

int main(int argc, char** argv) {
    if (argc < 4) {
        printf("Usage: %s <url> <token> <mode> [input.wav]\n", argv[0]);
        printf("  mode: record or playback\n");
        printf("  input.wav: required for playback mode\n");
        return 1;
    }
    
    const char* url = argv[1];
    const char* token = argv[2];
    const char* mode = argv[3];
    bool is_recording = (strcmp(mode, "record") == 0);
    
    printf("=== Audio Recording/Playback Example ===\n");
    
    LkClientHandle* client = lk_client_create();
    
    if (is_recording) {
        // Set up recording
        RecordingContext ctx = {
            .file = fopen("recording.wav", "wb"),
            .samples_written = 0,
            .sample_rate = 48000,
            .channels = 1
        };
        
        if (!ctx.file) {
            fprintf(stderr, "Failed to create output file\n");
            lk_client_destroy(client);
            return 1;
        }
        
        write_wav_header(ctx.file, ctx.sample_rate, ctx.channels);
        
        lk_set_audio_output_format(client, ctx.sample_rate, ctx.channels);
        lk_client_set_audio_callback(client, on_audio_for_recording, &ctx);
        
        // Connect and record
        LkResult result = lk_connect_with_role(client, url, token, LkRoleSubscriber);
        if (result.code != 0) {
            fprintf(stderr, "Connection failed: %s\n", result.message);
            lk_free_str((char*)result.message);
            fclose(ctx.file);
            remove("recording.wav");
            lk_client_destroy(client);
            return 1;
        }
        
        printf("Recording for 30 seconds...\n");
        sleep_ms(30000);
        
        // Finalize WAV file
        finalize_wav_file(ctx.file, ctx.samples_written, ctx.channels);
        fclose(ctx.file);
        
        printf("\nRecording saved to recording.wav\n");
        
    } else {
        // Playback mode
        if (argc < 5) {
            fprintf(stderr, "Input file required for playback\n");
            lk_client_destroy(client);
            return 1;
        }
        
        const char* input_file = argv[4];
        int16_t* samples = NULL;
        size_t frames = 0;
        int channels = 0, sample_rate = 0;
        
        if (!read_wav_file(input_file, &samples, &frames, &channels, &sample_rate)) {
            lk_client_destroy(client);
            return 1;
        }
        
        printf("Loaded %s: %zu frames @ %dHz (%d channels)\n",
               input_file, frames, sample_rate, channels);
        
        // Connect as publisher
        LkResult result = lk_connect_with_role(client, url, token, LkRolePublisher);
        if (result.code != 0) {
            fprintf(stderr, "Connection failed: %s\n", result.message);
            lk_free_str((char*)result.message);
            free(samples);
            lk_client_destroy(client);
            return 1;
        }
        
        // Publish audio in chunks
        printf("Publishing audio...\n");
        size_t frames_per_chunk = 480;  // 10ms @ 48kHz
        size_t offset = 0;
        
        while (offset < frames) {
            size_t chunk_frames = (offset + frames_per_chunk <= frames) ? 
                                 frames_per_chunk : (frames - offset);
            
            lk_publish_audio_pcm_i16(client, 
                                    samples + (offset * channels),
                                    chunk_frames, channels, sample_rate);
            
            offset += chunk_frames;
            
            printf("\rPublished %zu/%zu frames (%.1f%%)", 
                   offset, frames, (float)offset / frames * 100.0f);
            fflush(stdout);
            
            sleep_ms(10);
        }
        
        printf("\nPlayback complete!\n");
        free(samples);
    }
    
    // Cleanup
    lk_disconnect(client);
    lk_client_destroy(client);
    
    return 0;
}
```

---

## Real-Time Diagnostics Monitor

Monitor and log all statistics in real-time.

```cpp
#include "livekit_ffi.h"
#include <stdio.h>
#include <time.h>

typedef struct {
    time_t start_time;
    FILE* log_file;
} DiagnosticsContext;

void log_diagnostics(LkClientHandle* client, DiagnosticsContext* ctx) {
    time_t now = time(NULL);
    time_t elapsed = now - ctx->start_time;
    
    // Get statistics
    LkAudioStats audio_stats;
    LkDataStats data_stats;
    
    LkResult r1 = lk_get_audio_stats(client, &audio_stats);
    LkResult r2 = lk_get_data_stats(client, &data_stats);
    
    if (r1.code != 0 || r2.code != 0) {
        return;
    }
    
    // Format timestamp
    char timestamp[32];
    struct tm* timeinfo = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
    
    // Console output
    printf("\n=== Diagnostics @ %s (T+%ld sec) ===\n", timestamp, elapsed);
    
    printf("Audio:\n");
    printf("  Format: %dHz, %d channels\n", 
           audio_stats.sample_rate, audio_stats.channels);
    printf("  Ring Buffer: %d/%d frames (%.1f%% full)\n",
           audio_stats.ring_queued_frames,
           audio_stats.ring_capacity_frames,
           (float)audio_stats.ring_queued_frames / audio_stats.ring_capacity_frames * 100.0f);
    printf("  Underruns: %d\n", audio_stats.underruns);
    printf("  Overruns: %d\n", audio_stats.overruns);
    
    printf("Data Channels:\n");
    printf("  Reliable: %lld bytes sent, %lld dropped\n",
           data_stats.reliable_sent_bytes, data_stats.reliable_dropped);
    printf("  Lossy: %lld bytes sent, %lld dropped\n",
           data_stats.lossy_sent_bytes, data_stats.lossy_dropped);
    
    // File logging (CSV format)
    if (ctx->log_file) {
        fprintf(ctx->log_file, "%ld,%d,%d,%d,%d,%d,%d,%lld,%lld,%lld,%lld\n",
                elapsed,
                audio_stats.sample_rate,
                audio_stats.channels,
                audio_stats.ring_queued_frames,
                audio_stats.ring_capacity_frames,
                audio_stats.underruns,
                audio_stats.overruns,
                data_stats.reliable_sent_bytes,
                data_stats.reliable_dropped,
                data_stats.lossy_sent_bytes,
                data_stats.lossy_dropped);
        fflush(ctx->log_file);
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("Usage: %s <url> <token>\n", argv[0]);
        return 1;
    }
    
    printf("=== Real-Time Diagnostics Monitor ===\n");
    
    DiagnosticsContext ctx = {
        .start_time = time(NULL),
        .log_file = fopen("diagnostics.csv", "w")
    };
    
    if (ctx.log_file) {
        fprintf(ctx.log_file, "elapsed_sec,sample_rate,channels,ring_queued,"
                             "ring_capacity,underruns,overruns,reliable_sent,"
                             "reliable_dropped,lossy_sent,lossy_dropped\n");
    }
    
    // Create and connect
    LkClientHandle* client = lk_client_create();
    lk_set_log_level(client, LkLogInfo);
    
    LkResult result = lk_connect(client, argv[1], argv[2]);
    if (result.code != 0) {
        fprintf(stderr, "Connection failed: %s\n", result.message);
        lk_free_str((char*)result.message);
        if (ctx.log_file) fclose(ctx.log_file);
        lk_client_destroy(client);
        return 1;
    }
    
    printf("Connected! Monitoring for 60 seconds...\n");
    printf("Logging to diagnostics.csv\n\n");
    
    // Monitor loop: log every 5 seconds
    for (int i = 0; i < 60; i += 5) {
        sleep_ms(5000);
        log_diagnostics(client, &ctx);
    }
    
    // Cleanup
    printf("\nShutting down...\n");
    lk_disconnect(client);
    lk_client_destroy(client);
    
    if (ctx.log_file) {
        fclose(ctx.log_file);
        printf("Diagnostics saved to diagnostics.csv\n");
    }
    
    return 0;
}
```

---

## Cross-Platform Client

A complete cross-platform example with platform-specific audio capture.

```cpp
#include "livekit_ffi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Platform-specific includes
#ifdef _WIN32
    #include <Windows.h>
    #define SLEEP_MS(ms) Sleep(ms)
#else
    #include <unistd.h>
    #define SLEEP_MS(ms) usleep((ms) * 1000)
#endif

// Platform abstraction for audio capture
typedef struct AudioCapture AudioCapture;

#ifdef _WIN32
// Windows audio capture stub
struct AudioCapture {
    int sample_rate;
    int channels;
};

AudioCapture* audio_capture_create(int sample_rate, int channels) {
    AudioCapture* cap = (AudioCapture*)malloc(sizeof(AudioCapture));
    cap->sample_rate = sample_rate;
    cap->channels = channels;
    printf("[Windows] Audio capture initialized\n");
    return cap;
}

void audio_capture_read(AudioCapture* cap, int16_t* buffer, size_t frames) {
    // Stub: generate silence
    memset(buffer, 0, frames * cap->channels * sizeof(int16_t));
}

void audio_capture_destroy(AudioCapture* cap) {
    free(cap);
}

#else
// Linux/macOS audio capture stub
struct AudioCapture {
    int sample_rate;
    int channels;
};

AudioCapture* audio_capture_create(int sample_rate, int channels) {
    AudioCapture* cap = (AudioCapture*)malloc(sizeof(AudioCapture));
    cap->sample_rate = sample_rate;
    cap->channels = channels;
    printf("[Unix] Audio capture initialized\n");
    return cap;
}

void audio_capture_read(AudioCapture* cap, int16_t* buffer, size_t frames) {
    // Stub: generate silence
    memset(buffer, 0, frames * cap->channels * sizeof(int16_t));
}

void audio_capture_destroy(AudioCapture* cap) {
    free(cap);
}
#endif

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("Usage: %s <url> <token>\n", argv[0]);
        return 1;
    }
    
    printf("=== Cross-Platform LiveKit Client ===\n");
    printf("Platform: ");
    #ifdef _WIN32
        printf("Windows\n");
    #elif defined(__APPLE__)
        printf("macOS\n");
    #elif defined(__linux__)
        printf("Linux\n");
    #else
        printf("Unknown\n");
    #endif
    
    // Initialize audio capture
    const int sample_rate = 48000;
    const int channels = 1;
    const int frames_per_chunk = 480;  // 10ms
    
    AudioCapture* capture = audio_capture_create(sample_rate, channels);
    
    // Create LiveKit client
    LkClientHandle* client = lk_client_create();
    lk_set_audio_publish_options(client, 32000, 1, 0);
    
    // Connect
    LkResult result = lk_connect(client, argv[1], argv[2]);
    if (result.code != 0) {
        fprintf(stderr, "Connection failed: %s\n", result.message);
        lk_free_str((char*)result.message);
        audio_capture_destroy(capture);
        lk_client_destroy(client);
        return 1;
    }
    
    printf("Connected! Streaming audio for 30 seconds...\n");
    
    // Publish loop
    int16_t buffer[480];
    for (int i = 0; i < 3000 && lk_client_is_ready(client); i++) {
        // Capture audio
        audio_capture_read(capture, buffer, frames_per_chunk);
        
        // Publish
        lk_publish_audio_pcm_i16(client, buffer, frames_per_chunk, 
                                channels, sample_rate);
        
        SLEEP_MS(10);
    }
    
    // Cleanup
    printf("\nCleaning up...\n");
    lk_disconnect(client);
    lk_client_destroy(client);
    audio_capture_destroy(capture);
    
    printf("Done!\n");
    return 0;
}
```

**Cross-platform compilation:**

```bash
# Windows (MSVC)
cl cross_platform.c livekit_ffi.dll.lib /I"include"

# Linux
gcc cross_platform.c -o cross_platform -I./include -L./lib -llivekit_ffi -lm

# macOS
clang cross_platform.c -o cross_platform -I./include -L./lib -llivekit_ffi
```

---

## Building and Running Examples

### Prerequisites

1. Place `livekit_ffi.h` in an `include/` directory
2. Place library files in a `lib/` directory:
   - Windows: `livekit_ffi.dll.lib` (import lib) and `livekit_ffi.dll`
   - Linux: `liblivekit_ffi.so`
   - macOS: `liblivekit_ffi.dylib`

### Compilation

**Windows (MSVC):**
```cmd
cl example.c livekit_ffi.dll.lib /I"include" /link /OUT:example.exe
```

**Linux:**
```bash
gcc example.c -o example -I./include -L./lib -llivekit_ffi -lm
export LD_LIBRARY_PATH=./lib:$LD_LIBRARY_PATH
./example
```

**macOS:**
```bash
clang example.c -o example -I./include -L./lib -llivekit_ffi
export DYLD_LIBRARY_PATH=./lib:$DYLD_LIBRARY_PATH
./example
```

### Running

```bash
# Start local LiveKit server first (see LOCAL_LIVEKIT_QUICKSTART.md)
docker run --rm -p 7880:7880 livekit/livekit-server start --dev

# Generate token (see TOKEN_MINTING.md)
export TOKEN=$(node tools/token-mint/index.js --identity user1 --room test)

# Run example
./example ws://localhost:7880 $TOKEN
```

---

## Further Resources

- **[User Guide](USER_GUIDE.md)** - Comprehensive documentation
- **[FFI API Reference](FFI_API_GUIDE.md)** - Detailed API documentation
- **[Local Server Setup](LOCAL_LIVEKIT_QUICKSTART.md)** - Run LiveKit locally
- **[Token Generation](TOKEN_MINTING.md)** - Create access tokens

## License

MIT
