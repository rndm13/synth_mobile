#pragma once

#include <pthread.h>

#include "settings.h"
#include "env.h"
#include "filter.h"
#include "osc.h"

typedef struct SynthParams {
    pthread_rwlock_t rw;
    float amp;
    float pan;
} SynthParams;

typedef struct SynthProfiling {
    timespec_t osc_time[OSC_COUNT];
    timespec_t flt_time;
    timespec_t total_time;
} SynthProfiling;

typedef struct Synth {
    SynthProfiling prof;
    float key_freq_arr[KEY_COUNT];

    char program_name[PROGRAM_NAME_CAPACITY];
    SynthParams params;
    Osc osc_arr[OSC_COUNT];
    Env env_arr[ENV_COUNT];
    Filter flt;
    VoiceArr voice_arr;
} Synth;

void init_synth(Synth* s);
void deinit_synth(Synth* s);

void synth_add_voice(Synth* s, Voice new_voice, float time);
void synth_release_voice(Synth* s, int k_idx, float time);
void synth_gc_voice(Synth* s);
void update_synth(Synth* s, float time, float* buffer, size_t n);
