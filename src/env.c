#include "env.h"

#include "utils.h"

float calc_env_value(double time, bool released, double release_time, float release_start_env, Env *env) {
    float result = 0.0f;
    float time_in_release = time - release_time;
    float time_in_decay = 0.0f;
    float decay_progress = 0.0f;
    int e = 0;

    e = pthread_rwlock_rdlock(&env->rw);
    if (e != 0) {
        // TODO: Log
        return result;
    }

    // 1. Handle Release Phase
    if (released) {
        time_in_release = time - release_time;
        if (time_in_release >= env->release) {
            result = 0.0f;
            goto unlock;
        }

        result = release_start_env * (1.0f - (time_in_release / env->release));

        goto unlock;
    }

    // 2. Attack Phase
    if (time < env->attack) {
        result = time / env->attack;
        goto unlock;
    }

    // 3. Decay Phase
    time_in_decay = time - env->attack;
    decay_progress = time_in_decay / env->decay;
    if (time_in_decay < env->decay) {
        result = 1.0f - (decay_progress * (1.0f - env->sustain));
        goto unlock;
    }

    // 4. Sustain Phase
    result = env->sustain;

unlock:
    e = pthread_rwlock_unlock(&env->rw);
    if (e != 0) {
        // TODO: Log
        return result;
    }

    return result;
}

void init_env(Env* env) {
    int e = pthread_rwlock_init(&env->rw, NULL);
    if (e != 0) {
        return;
    }

    env->attack = ENV_A_MIN;
    env->decay = ENV_D_MIN;
    env->sustain = 1.0f;
    env->release = ENV_R_MIN;

    prepare_env_display(env);
}

void deinit_env(Env* env) {
    pthread_rwlock_destroy(&env->rw);
}

void prepare_env_display(Env* env) {
    int e = 0;
    size_t i = 0;
    bool released = false;
    float time = 0;
    const float total_time = ENV_A_MAX + ENV_D_MAX + ENV_R_MAX;

    e = pthread_rwlock_rdlock(&env->rw);
    if (e != 0) {
        // TODO: Log
        return;
    }


    for (i = 0; i < ARRAY_SIZE(env->disp_buffer); i++) {
        time = total_time * i / (float)DISPLAY_BUFFER_SIZE;

        released = time >= env->attack + env->decay;
        env->disp_buffer[i] = 1.8 * calc_env_value(
                time, released, env->attack + env->decay,
                env->sustain, env) - 0.9;
    }

    e = pthread_rwlock_unlock(&env->rw);
    if (e != 0) {
        // TODO: Log
        return;
    }

    return;
}
