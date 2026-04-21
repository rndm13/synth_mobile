#pragma once

#include <pthread.h>
#include <stdbool.h>

#define VOICE_MAX_COUNT 8

#define KEY_IDX_INVALID -1
#define VOICE_IDX_INVALID -1

#define KEY_C_OFF    4
#define OCTAVE_COUNT 8
#define KEY_OCTAVE   12
#define KEY_COUNT    (KEY_C_OFF + KEY_OCTAVE * OCTAVE_COUNT)

#define KEY_A4_IDX  49
#define KEY_C4_IDX  (KEY_A4_IDX - 9)

#define KEY_A4_FREQ 440.0f

// Voices that are currently held on a virtual keyboard
typedef struct Voice {
    int key_idx;
    int velocity;
} Voice;

typedef struct VoiceArr {
    pthread_rwlock_t rw;

    size_t voice_count;
    Voice voice_arr[VOICE_MAX_COUNT];
} VoiceArr;

void voice_arr_init(VoiceArr *v);
void voice_arr_deinit(VoiceArr *v);
void voice_remove(VoiceArr *v, int v_idx);
bool voice_hold_key(VoiceArr *v, int k_idx, Voice* o_voice);
void voice_release_key(VoiceArr *v, int k_idx);

// Voices that are currently played by oscillators.
// Can last longer than the voices that are held.
typedef struct OscVoice {
    // Initial
    Voice voice;
    float start_time;

    // Once during runtime
    float release_time;
    bool released;

    // Private data for oscillator
    int wave_idx;
    float env;
} OscVoice;

typedef struct OscVoiceArr {
    pthread_rwlock_t rw;

    size_t osc_voice_count;
    OscVoice osc_voice_arr[VOICE_MAX_COUNT];
} OscVoiceArr;

void osc_voice_arr_init(OscVoiceArr *ov);
void osc_voice_arr_deinit(OscVoiceArr *v);
void osc_voice_remove(OscVoiceArr *ov, int ov_idx);
void osc_voice_add(OscVoiceArr *ov, Voice v, float time);
void osc_voice_release(OscVoiceArr *ov, int k_idx, float time);
void osc_voice_gc(OscVoiceArr *ov);
