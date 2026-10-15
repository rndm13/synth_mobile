#pragma once

#include <pthread.h>
#include <stdbool.h>

#include "settings.h"
#include "voice.h"

typedef struct Env {
    pthread_rwlock_t rw;
    float attack;
    float decay;
    float sustain;
    float release;

    float disp_buffer[DISPLAY_BUFFER_SIZE];
} Env;

float calc_env_value(size_t sample_idx, Env *env, OscVoice *ov);
void init_env(Env* env);
void deinit_env(Env* env);
void prepare_env_display(Env* env);

