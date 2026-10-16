#include "env.h"

#include "settings.h"

static float calc_env_value_args(size_t sample_idx, const Env *env, size_t start_sample_idx, size_t release_sample_idx, bool released) {
    float result = 0.0f;
    float release_start_env = 0.0f;
    float time = (float)(sample_idx - start_sample_idx) / SAMPLE_RATE;
    float release_time = 0;
    float decay_time = 0;
    float decay_progress = 0.0f;
    int e = 0;

    // 1. Handle Release Phase
    if (released) {
        release_start_env = calc_env_value_args(release_sample_idx, env, start_sample_idx, release_sample_idx, false);
        release_time = (float)(sample_idx - release_sample_idx) / SAMPLE_RATE;

        if (release_time >= env->release) {
            result = 0.0f;

            return result;
        }

        result = release_start_env * (1.0f - (release_time / env->release));

        return result;
    }

    // 2. Attack Phase
    if (time < env->attack) {
        result = time / env->attack;

        return result;
    }

    // 3. Decay Phase
    decay_time = time - env->attack;
    decay_progress = decay_time / env->decay;
    if (decay_time < env->decay) {
        result = 1.0f - (decay_progress * (1.0f - env->sustain));

        return result;
    }

    // 4. Sustain Phase
    result = env->sustain;

    return result;

}

float calc_env_value(size_t sample_idx, const Env *env, OscVoice *ov) {
    return calc_env_value_args(sample_idx, env, ov->start_sample_idx, ov->release_sample_idx, ov->released);
}

void init_env(Env* env) {
    env->attack = ENV_A_MIN;
    env->decay = ENV_D_MIN;
    env->sustain = 1.0f;
    env->release = ENV_R_MIN;
}

void prepare_env_display(const Env* env, float disp_buffer[DISPLAY_BUFFER_SIZE]) {
    int e = 0;
    size_t i = 0;
    bool released = false;
    float time = 0;
    const float total_time = ENV_A_MAX + ENV_D_MAX + ENV_R_MAX;

    for (i = 0; i < DISPLAY_BUFFER_SIZE; i++) {
        time = total_time * i / (float)DISPLAY_BUFFER_SIZE;

        released = time >= env->attack + env->decay;
        disp_buffer[i] = 1.8 * calc_env_value_args(i, env, 0, (env->attack + env->decay) / DISPLAY_BUFFER_SIZE, released) - 0.9;
    }
}
