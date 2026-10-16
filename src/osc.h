#pragma once

#include "voice.h"
#include "settings.h"
#include "utils.h"

#define OSC_TYPE_X(X)           \
    X(OT_SINE, "Sine")          \
    X(OT_TRIANGLE, "Triangle")  \
    X(OT_SQUARE, "Square")      \
    X(OT_SAW, "Sawtooth")       \

typedef enum OscType {
    OSC_TYPE_X(X_ENUM)
    OT_MAX,
} OscType;

typedef struct OscParams {
    bool rand_phase;
    OscType type;
    int semi;
    int cents;
    float volume;

    int unison;
    int detune;
} OscParams;

typedef struct OscState {
    OscVoiceArr voice_arr;
} OscState;

float calc_osc_value(OscType type, int wave_idx, int wave_length);
float calc_semi_mul(int semi);
float calc_cents_mul(float cents);

void osc_add_voice(const OscParams* params, OscState* osc, Voice new_voice, size_t start_idx);
void prepare_osc_display(const OscParams* osc, float disp_buffer[DISPLAY_BUFFER_SIZE]);
void init_osc_params(OscParams* osc);
void init_osc_state(OscState* osc);
