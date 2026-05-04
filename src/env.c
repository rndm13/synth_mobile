#include "env.h"

#include <math.h>
#include "settings.h"

float calc_env_value(float time, bool released, float release_time, float last_env, Env *env) {
    float result = 0.0f;
    int e = pthread_rwlock_rdlock(&env->rw);
    if (e != 0) {
        // TODO: Log
        return result;
    }

    // 1. Handle Release Phase
    if (released) {
        float time_in_release = time - release_time;
        if (time_in_release >= env->release) {
            result = 0.0f;
            goto unlock;
        }

        // We calculate the value starting from the sustain level or previous env value down to 0
        result = fmin(env->sustain * (1.0f - (time_in_release / env->release)), last_env);
        goto unlock;
    }

    // 2. Attack Phase
    if (time < env->attack) {
        result = time / env->attack;
        goto unlock;
    }

    // 3. Decay Phase
    float timeInDecay = time - env->attack;
    if (timeInDecay < env->decay) {
        float decayProgress = timeInDecay / env->decay;

        result = 1.0f - (decayProgress * (1.0f - env->sustain));
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
}

void deinit_env(Env* env) {
    int e = pthread_rwlock_destroy(&env->rw);
}

