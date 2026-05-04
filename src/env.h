#pragma once

#include <pthread.h>
#include <stdbool.h>

typedef struct Env {
    pthread_rwlock_t rw;
    float attack;
    float decay;
    float sustain;
    float release;
} Env;

float calc_env_value(float time, bool released, float release_time, float last_env, Env *env);
void init_env(Env* env);
void deinit_env(Env* env);
