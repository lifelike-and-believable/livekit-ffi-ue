#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ═══════════════════════════════════════════════════════════════════════════
// Core Types
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Result type for FFI functions.
 * - code: 0 = success; non-zero = error
 * - message: allocated error string (caller must free via lk_free_str), or NULL
 *
 * Error code ranges:
 * - 1xx: Connection/Token errors
 * - 2xx: Data send errors
 * - 3xx: Audio publish errors
 * - 4xx: Lifecycle errors
 * - 5xx: Internal errors
 */
typedef struct { int32_t code; const char* message; } LkResult;

/**
 * Free a string allocated by the FFI layer.
 * Safe to call with NULL.
 */
void lk_free_str(char* p);

/**
 * Opaque client handle.
 */
typedef struct LkClientHandle LkClientHandle;

/**
 * Opaque audio track handle for publisher-created tracks.
 */
typedef struct LkAudioTrackHandle LkAudioTrackHandle;

/**
 * Data channel reliability mode.
 */
typedef enum { LkReliable = 0, LkLossy = 1 } LkReliability;

/**
 * Client role for connection.
 */
typedef enum {
  LkRoleAuto = 0,
  LkRolePublisher = 1,
  LkRoleSubscriber = 2,
  LkRoleBoth = 3
} LkRole;

/**
 * Connection state enum for lifecycle tracking.
 */
typedef enum {
  LkConnConnecting = 0,
  LkConnConnected = 1,
  LkConnReconnecting = 2,
  LkConnDisconnected = 3,
  LkConnFailed = 4
} LkConnectionState;

/**
 * Log level for diagnostics.
 */
typedef enum {
  LkLogError = 0,
  LkLogWarn = 1,
  LkLogInfo = 2,
  LkLogDebug = 3,
  LkLogTrace = 4
} LkLogLevel;

// ═══════════════════════════════════════════════════════════════════════════
// Callbacks
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Data callback (original, no label/reliability info).
 * NOTE: Callbacks may be invoked on background threads. Never block internally.
 */
typedef void (*LkDataCallback)(void* user, const uint8_t* bytes, size_t len);

/**
 * Extended data callback with label and reliability.
 * NOTE: Callbacks may be invoked on background threads. Never block internally.
 */
typedef void (*LkDataCallbackEx)(void* user, const char* label, LkReliability reliability, const uint8_t* bytes, size_t len);

/**
 * Audio callback (PCM i16 interleaved).
 * NOTE: Callbacks may be invoked on background threads. Never block internally.
 */
typedef void (*LkAudioCallback)(void* user, const int16_t* pcm_interleaved, size_t frames_per_channel, int32_t channels, int32_t sample_rate);

/**
 * Extended audio callback with per-subject identification.
 * Provides participant name and track name for each audio frame.
 * - participant_name: name of the participant (never NULL)
 * - track_name: name of the audio track (never NULL)
 * NOTE: Callbacks may be invoked on background threads. Never block internally.
 */
typedef void (*LkAudioCallbackEx)(void* user, const int16_t* pcm_interleaved, size_t frames_per_channel, int32_t channels, int32_t sample_rate, const char* participant_name, const char* track_name);

/**
 * Audio format change notification callback.
 * Called when the incoming audio format changes.
 * NOTE: Callbacks may be invoked on background threads. Never block internally.
 */
typedef void (*LkAudioFormatChangeCallback)(void* user, int32_t sample_rate, int32_t channels);

/**
 * Connection state change callback.
 * - state: new connection state
 * - reason_code: error/disconnect reason (0 if normal)
 * - message: optional human-readable message (may be NULL)
 * NOTE: Callbacks may be invoked on background threads. Never block internally.
 */
typedef void (*LkConnectionCallback)(void* user, LkConnectionState state, int32_t reason_code, const char* message);

// ═══════════════════════════════════════════════════════════════════════════
// Diagnostic Structures
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Audio statistics for diagnostics.
 */
typedef struct {
  int32_t sample_rate;
  int32_t channels;
  int32_t ring_capacity_frames;
  int32_t ring_queued_frames;
  int32_t underruns;
  int32_t overruns;
} LkAudioStats;

/**
 * Data channel statistics for diagnostics.
 */
typedef struct {
  int64_t reliable_sent_bytes;
  int64_t reliable_dropped;
  int64_t lossy_sent_bytes;
  int64_t lossy_dropped;
} LkDataStats;

// ═══════════════════════════════════════════════════════════════════════════
// Client Lifecycle
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Create a new client handle.
 */
LkClientHandle* lk_client_create(void);

/**
 * Destroy a client handle and free resources.
 * After this call returns, no callbacks will fire.
 */
void lk_client_destroy(LkClientHandle*);

/**
 * Set data callback (original).
 */
LkResult lk_client_set_data_callback(LkClientHandle*, LkDataCallback cb, void* user);

/**
 * Set extended data callback (with label and reliability).
 */
LkResult lk_client_set_data_callback_ex(LkClientHandle*, LkDataCallbackEx cb, void* user);

/**
 * Set audio callback.
 */
LkResult lk_client_set_audio_callback(LkClientHandle*, LkAudioCallback cb, void* user);

/**
 * Set extended audio callback with per-subject identification.
 * Provides participant and track names for each audio frame.
 * Overrides any previously set standard audio callback.
 */
LkResult lk_client_set_audio_callback_ex(LkClientHandle*, LkAudioCallbackEx cb, void* user);

/**
 * Set audio format change callback.
 * Called when incoming audio format changes.
 */
LkResult lk_set_audio_format_change_callback(LkClientHandle*, LkAudioFormatChangeCallback cb, void* user);

/**
 * Set connection state callback.
 */
LkResult lk_set_connection_callback(LkClientHandle*, LkConnectionCallback cb, void* user);

/**
 * Connect to LiveKit room (defaults to LkRoleBoth).
 */
LkResult lk_connect(LkClientHandle*, const char* url, const char* token);

/**
 * Connect to LiveKit room with specified role.
 */
LkResult lk_connect_with_role(LkClientHandle*, const char* url, const char* token, LkRole role);

/**
 * Asynchronously connect to LiveKit room (defaults to LkRoleBoth).
 * Returns immediately; connection result will be delivered via lk_set_connection_callback.
 */
LkResult lk_connect_async(LkClientHandle*, const char* url, const char* token);

/**
 * Asynchronously connect to LiveKit room with specified role.
 * Returns immediately; connection result will be delivered via lk_set_connection_callback.
 */
LkResult lk_connect_with_role_async(LkClientHandle*, const char* url, const char* token, LkRole role);

/**
 * Disconnect from LiveKit room.
 * Blocks until disconnect is complete and callbacks are quiesced.
 */
LkResult lk_disconnect(LkClientHandle*);

/**
 * Check if client is connected and ready.
 * Returns 1 if ready, 0 otherwise.
 */
int32_t lk_client_is_ready(LkClientHandle*);

// ═══════════════════════════════════════════════════════════════════════════
// Audio Configuration
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Configure audio publish options.
 * - bitrate_bps: target bitrate in bits per second (e.g., 24000-48000)
 * - enable_dtx: 1 to enable Discontinuous Transmission, 0 to disable
 * - stereo: 1 for stereo, 0 for mono (default mono)
 *
 * Call before first audio publish or disconnect/reconnect to apply changes.
 */
LkResult lk_set_audio_publish_options(LkClientHandle*, int32_t bitrate_bps, int32_t enable_dtx, int32_t stereo);

/**
 * Set desired audio output format for subscribed audio.
 * The FFI layer will resample/downmix incoming audio to this format.
 * - sample_rate: desired sample rate (e.g., 48000)
 * - channels: desired channel count (1 or 2)
 *
 * Call before connecting or subscribing to audio.
 */
LkResult lk_set_audio_output_format(LkClientHandle*, int32_t sample_rate, int32_t channels);

// ═══════════════════════════════════════════════════════════════════════════
// Audio Publishing
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Publish PCM i16 audio frame.
 * - pcm_interleaved: interleaved i16 samples
 * - frames_per_channel: number of frames per channel
 * - channels: channel count
 * - sample_rate: sample rate in Hz
 */
LkResult lk_publish_audio_pcm_i16(
  LkClientHandle*,
  const int16_t* pcm_interleaved,
  size_t frames_per_channel,
  int32_t channels,
  int32_t sample_rate);

/**
 * Audio track configuration for dedicated publisher tracks.
 * - track_name: optional track label (NULL uses default)
 * - sample_rate / channels: format for the track
 * - buffer_ms: desired ring buffer depth in milliseconds (0 = default)
 */
typedef struct {
  const char* track_name;
  int32_t sample_rate;
  int32_t channels;
  int32_t buffer_ms;
} LkAudioTrackConfig;

/**
 * Create a dedicated audio track for publishing.
 * Returns the track handle via out param on success.
 */
LkResult lk_audio_track_create(
  LkClientHandle*,
  const LkAudioTrackConfig* config,
  LkAudioTrackHandle** out_track);

/**
 * Destroy a dedicated audio track handle and stop publishing it.
 *
 * Safe to call with NULL (no-op).
 */
LkResult lk_audio_track_destroy(LkAudioTrackHandle*);

/**
 * Publish PCM audio to a dedicated audio track handle.
 * Format is determined by the track's configuration.
 */
LkResult lk_audio_track_publish_pcm_i16(
  LkAudioTrackHandle*,
  const int16_t* pcm_interleaved,
  size_t frames_per_channel);

// ═══════════════════════════════════════════════════════════════════════════
// Data Channel
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Send data (original API).
 * Size guidance: lossy ≤ ~1300 bytes, reliable ≤ ~15 KiB.
 * Exceeding these limits will result in an error.
 */
LkResult lk_send_data(
  LkClientHandle*,
  const uint8_t* bytes,
  size_t len,
  LkReliability reliability);

/**
 * Send data with extended options.
 * - ordered: 1 to preserve order, 0 for unordered (default 1)
 * - label: optional label for the data channel (NULL uses default)
 *
 * Size guidance: lossy ≤ ~1300 bytes, reliable ≤ ~15 KiB.
 */
LkResult lk_send_data_ex(
  LkClientHandle*,
  const uint8_t* bytes,
  size_t len,
  LkReliability reliability,
  int32_t ordered,
  const char* label);

/**
 * Set default labels for reliable and lossy data channels.
 * If NULL, uses built-in defaults.
 */
LkResult lk_set_default_data_labels(LkClientHandle*, const char* reliable_label, const char* lossy_label);

// ═══════════════════════════════════════════════════════════════════════════
// Reconnection and Token Management
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Set reconnection backoff parameters.
 * - initial_ms: initial backoff in milliseconds
 * - max_ms: maximum backoff in milliseconds
 * - multiplier: backoff multiplier (e.g., 1.5)
 *
 * Call before connecting.
 */
LkResult lk_set_reconnect_backoff(LkClientHandle*, int32_t initial_ms, int32_t max_ms, float multiplier);

/**
 * Refresh JWT token at runtime (if SDK supports it).
 * If not supported, returns error; fallback is disconnect + reconnect.
 */
LkResult lk_refresh_token(LkClientHandle*, const char* token);

/**
 * Set client role dynamically (if SDK supports it).
 * - role: new role
 * - auto_subscribe: 1 to enable auto-subscribe, 0 to disable
 *
 * If not supported, returns error; fallback is disconnect + reconnect.
 */
LkResult lk_set_role(LkClientHandle*, LkRole role, int32_t auto_subscribe);

// ═══════════════════════════════════════════════════════════════════════════
// Diagnostics and Metrics
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Set log level for diagnostics.
 */
LkResult lk_set_log_level(LkClientHandle*, LkLogLevel level);

/**
 * Get audio statistics.
 * Returns current audio ring buffer state and error counters.
 */
LkResult lk_get_audio_stats(LkClientHandle*, LkAudioStats* out_stats);

/**
 * Get data channel statistics.
 * Returns cumulative send/drop counters.
 */
LkResult lk_get_data_stats(LkClientHandle*, LkDataStats* out_stats);

// ═══════════════════════════════════════════════════════════════════════════
// Threading and Safety Guarantees
// ═══════════════════════════════════════════════════════════════════════════
//
// - All callbacks may be invoked on background threads.
// - Callbacks must not block or perform long-running operations.
// - API calls are thread-safe and may be called from any thread.
// - After lk_disconnect() or lk_client_destroy() returns, no further callbacks
//   will be invoked.
// - Reentrancy: It is safe to call API functions from non-callback threads
//   while callbacks are in flight.
//
// ═══════════════════════════════════════════════════════════════════════════

#ifdef __cplusplus
}
#endif
