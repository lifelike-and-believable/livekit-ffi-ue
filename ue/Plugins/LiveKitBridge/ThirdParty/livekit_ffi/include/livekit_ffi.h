#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { int32_t code; const char* message; } LkResult;
void lk_free_str(char* p);

typedef struct LkClientHandle LkClientHandle;
typedef enum { LkReliable = 0, LkLossy = 1 } LkReliability;

typedef void (*LkDataCallback)(void* user, const uint8_t* bytes, size_t len);
typedef void (*LkAudioCallback)(void* user, const int16_t* pcm_interleaved, size_t frames_per_channel, int32_t channels, int32_t sample_rate);

typedef enum {
  LkRoleAuto = 0,
  LkRolePublisher = 1,
  LkRoleSubscriber = 2,
  LkRoleBoth = 3
} LkRole;

LkClientHandle* lk_client_create(void);
void lk_client_destroy(LkClientHandle*);

LkResult lk_client_set_data_callback(LkClientHandle*, LkDataCallback cb, void* user);
LkResult lk_client_set_audio_callback(LkClientHandle*, LkAudioCallback cb, void* user);
LkResult lk_connect(LkClientHandle*, const char* url, const char* token);
LkResult lk_connect_with_role(LkClientHandle*, const char* url, const char* token, LkRole role);
LkResult lk_disconnect(LkClientHandle*);
int32_t lk_client_is_ready(LkClientHandle*);

LkResult lk_publish_audio_pcm_i16(
  LkClientHandle*,
  const int16_t* pcm_interleaved,
  size_t frames_per_channel,
  int32_t channels,
  int32_t sample_rate);

LkResult lk_send_data(
  LkClientHandle*,
  const uint8_t* bytes,
  size_t len,
  LkReliability reliability);

#ifdef __cplusplus
}
#endif
