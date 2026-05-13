#pragma once

#include <pthread.h>

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
    pthread_rwlock_t rw;

    OscType type;
    int semi;
    int cents;
    float volume;

    int unison;
    int detune;
} OscParams;

typedef struct Osc {
    // RW protected
    OscParams params;
    OscVoiceArr voice_arr;

    // Output
    float disp_buffer[DISPLAY_BUFFER_SIZE];
} Osc;

float calc_osc_value(OscType type, int wave_idx, int wave_length);
void osc_add_voice(Osc* osc, Voice new_voice, float time);
void prepare_osc_display_buffer(Osc* osc);
float calc_semi_mul(int semi);
float calc_cents_mul(float cents);
void init_osc(Osc* osc);
void deinit_osc(Osc* osc);
