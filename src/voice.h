#pragma once

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#define VOICE_MAX_COUNT 32

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
    bool midi;
    int8_t key_idx;
    uint8_t velocity;
    float freq;
    uint32_t id;
} Voice;

typedef struct VoiceArr {
    size_t voice_count;
    Voice voice_arr[VOICE_MAX_COUNT];
} VoiceArr;

void voice_arr_init(VoiceArr *v);
void voice_arr_deinit(VoiceArr *v);
void voice_remove(VoiceArr *v, int v_idx);
bool voice_hold_key(VoiceArr *v, bool midi, int k_idx, int velocity, Voice* o_voice);
void voice_release_key(VoiceArr *v, int k_idx);

// Voices that are currently played by oscillators.
// Can last longer than the voices that are held.
typedef struct OscVoice {
    bool released;
    bool dead;
    Voice voice;

    size_t unison_idx;
    size_t start_sample_idx;
    size_t release_sample_idx;

    int wave_idx;
} OscVoice;

typedef struct OscVoiceArr {
    size_t osc_voice_count;
    OscVoice osc_voice_arr[VOICE_MAX_COUNT];
} OscVoiceArr;

void osc_voice_arr_init(OscVoiceArr *ov);
void osc_voice_arr_deinit(OscVoiceArr *v);
void osc_voice_remove(OscVoiceArr *ov, int ov_idx);
void osc_voice_add(OscVoiceArr *ov, Voice v, size_t start_idx, size_t unison_idx, bool rand_phase);
void osc_voice_add_unison(OscVoiceArr *ov, Voice v, size_t cnt, size_t start_idx, bool rand_phase);
void osc_voice_release(OscVoiceArr *ov, int k_idx, size_t release_idx);
OscVoice* osc_voice_get_last(OscVoiceArr *ov);
void osc_voice_gc(OscVoiceArr *ov);
