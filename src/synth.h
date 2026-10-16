#pragma once

#include <pthread.h>

#include "settings.h"
#include "env.h"
#include "filter.h"
#include "osc.h"
#include "voice.h"

typedef struct SynthGeneralParams {
    char program_name[PROGRAM_NAME_CAPACITY];
    float amp;
    float pan;
} SynthGeneralParams;

typedef struct SynthProfiling {
    timespec_t osc_time[OSC_COUNT];
    timespec_t flt_time;
    timespec_t distortion_time;
    timespec_t delay_time;
    timespec_t amp_time;
    timespec_t total_time;
    size_t     cur_stage;
} SynthProfiling;

typedef struct DistorthonParams {
    float wet_dry_ratio;
    float gain;
} DistortionParams;

typedef struct DelayParams {
    float wet_dry_ratio;
    float feedback;
    float delay_s;
} DelayParams;

typedef struct DelayState {
    int buffer_idx;
    float buffer[(int)DELAY_S_MAX * SAMPLE_RATE];
} DelayState;

typedef struct SynthParams {
    SynthGeneralParams gen;
    OscParams osc_arr[OSC_COUNT];
    Env env_arr[ENV_COUNT];
    FilterParams flt;
    DistortionParams distortion;
    DelayParams delay;
    VoiceArr voice_arr;
} SynthParams;

typedef struct SynthState {
    // Actually global index used to track time for envelopes and stuff
    size_t sample_idx;
    float* sample_buffer;
    size_t sample_buffer_size;

    SynthProfiling prof;

    OscState osc_arr[OSC_COUNT];
    FilterState flt;
    DelayState delay;
} SynthState;

typedef struct Synth {
    bool update_params;
    SynthParams locked_params;
    SynthParams gui_params;
    SynthState state;

    float osc_disp_buffer_arr[2][DISPLAY_BUFFER_SIZE];
    float env_disp_buffer_arr[3][DISPLAY_BUFFER_SIZE];
    float flt_disp_buffer[DISPLAY_BUFFER_SIZE];
} Synth;

void init_synth_params(SynthParams* s);
void init_synth_state(const SynthParams* p, SynthState* s);

void update_synth_params(Synth* s);
void update_synth(Synth* s, float* buffer, size_t n);
