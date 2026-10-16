#pragma once

#include <stdbool.h>

#include "settings.h"
#include "voice.h"

typedef struct Env {
    float attack;
    float decay;
    float sustain;
    float release;

    ;
} Env;

float calc_env_value(size_t sample_idx, const Env *env, OscVoice *ov);
void init_env(Env* env);
void prepare_env_display(const Env* env, float disp_buffer[DISPLAY_BUFFER_SIZE]);

