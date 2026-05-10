#include "synth.h"

#include "stdlib.h"
#include "raymath.h"

void init_synth(Synth* s) {
    int e = 0;

    e = pthread_rwlock_init(&s->params.rw, NULL);
    if (e != 0) {
        return;
    }

    snprintf(s->program_name, PROGRAM_NAME_CAPACITY, "%s", PROGRAM_NAME_INIT);
    s->params.amp = 0.2;
    s->params.pan = 0.5f;
    voice_arr_init(&s->voice_arr);

    for (size_t i = 0; i < ARRAY_SIZE(s->env_arr); i++) {
        init_env(&s->env_arr[i]);
    }

    for (size_t i = 0; i < ARRAY_SIZE(s->osc_arr); i++) {
        init_osc(&s->osc_arr[i]);
    }

    init_filter(&s->flt);

    for (int i = 0; i < ARRAY_SIZE(s->key_freq_arr); i++) {
        s->key_freq_arr[i] = powf(2.0f, (float)(i - KEY_A4_IDX) / (float)KEY_OCTAVE) * KEY_A4_FREQ;
    }
}

void deinit_synth(Synth* s) {
    pthread_rwlock_destroy(&s->params.rw);

    voice_arr_deinit(&s->voice_arr);

    for (size_t i = 0; i < ARRAY_SIZE(s->env_arr); i++) {
        deinit_env(&s->env_arr[i]);
    }

    for (size_t i = 0; i < ARRAY_SIZE(s->osc_arr); i++) {
        deinit_osc(&s->osc_arr[i]);
    }

    deinit_filter(&s->flt);
}

void synth_add_voice(Synth* s, Voice new_voice, float time) {
    for (size_t i = 0; i < ARRAY_SIZE(s->osc_arr); i++) {
        osc_add_voice(&s->osc_arr[i], new_voice, time);
    }
}

void synth_release_voice(Synth* s, int k_idx, float time) {
    for (size_t i = 0; i < ARRAY_SIZE(s->osc_arr); i++) {
        osc_voice_release(&s->osc_arr[i].voice_arr, k_idx, time);
    }
}

void synth_gc_voice(Synth* s) {
    for (size_t i = 0; i < ARRAY_SIZE(s->osc_arr); i++) {
        osc_voice_gc(&s->osc_arr[i].voice_arr);
    }
}

void update_osc_voice(const Osc* osc, Env* env, OscVoice* osc_voice, float global_time, float wave_freq, float* buffer, size_t n) {
    Voice voice = osc_voice->voice;

    float vel_mul = voice.velocity / MAX_VELOCITY;
    float time = global_time - osc_voice->start_time;
    float release_time = osc_voice->release_time - osc_voice->start_time;
    float env_val = 0;

    for (int j = 0; j < n; j++) {
        float wave_length = SAMPLE_RATE / wave_freq;
        float dt = j / (float)SAMPLE_RATE;
        float kernel = calc_osc_value(osc->params.type, osc_voice->wave_idx, wave_length);

        env_val = calc_env_value(
                time + dt, osc_voice->released,
                release_time, osc_voice->last_env, env);
        if (!osc_voice->released || FloatEquals(env_val, 0)) {
            osc_voice->last_env = env_val;
        }

        buffer[j] += osc->params.volume * env_val * vel_mul * kernel;
        osc_voice->wave_idx++;
        if (osc_voice->wave_idx >= wave_length) {
            osc_voice->wave_idx = 0;
        }
    }
}

void update_osc(Synth* s, Osc* osc, Env* env, float global_time, float* buffer, size_t n) {
    OscVoiceArr *ova = &osc->voice_arr;
    OscVoice *osc_voice = NULL;
    Voice voice = {};
    int key_idx = 0;
    float wave_freq = 0;
    int e = 0;

    e = pthread_rwlock_rdlock(&osc->params.rw);
    if (e != 0) {
        // TODO: Log
        return;
    }

    if (FloatEquals(osc->params.volume, 0.0f)) {
        goto params_unlock;
    }

    e = pthread_rwlock_rdlock(&ova->rw);
    if (e != 0) {
        // TODO: Log
        return;
    }

    for (int v = 0; v < ova->osc_voice_count; v++) {
        osc_voice = &ova->osc_voice_arr[v];
        voice = osc_voice->voice;

        key_idx = voice.key_idx + osc->params.semi;
        if (key_idx < 0) {
            key_idx = 0;
        } else if (key_idx > KEY_COUNT) {
            key_idx = KEY_COUNT - 1;
        }

        wave_freq = s->key_freq_arr[key_idx] * osc->params.cents_mul * osc_voice->detune_mul;
        update_osc_voice(osc, env, osc_voice, global_time, wave_freq, buffer, n);
    }

    e = pthread_rwlock_unlock(&ova->rw);
    if (e != 0) {
        // TODO: Log
        return;
    }

params_unlock:
    e = pthread_rwlock_unlock(&osc->params.rw);
    if (e != 0) {
        // TODO: Log
        return;
    }
}

timespec_t duration_from(timespec_t start_time) {
    timespec_t end_time = {0};
    timespec_get(&end_time, TIME_UTC);

    end_time.tv_sec -= start_time.tv_sec;
    end_time.tv_nsec -= start_time.tv_nsec;

    end_time.tv_nsec += S_TO_NS(end_time.tv_sec);
    end_time.tv_sec = 0;

    return end_time;
}

void update_synth_amp(Synth* s, float* buffer, size_t n) {
    int e = pthread_rwlock_rdlock(&s->params.rw);
    if (e != 0) {
        // TODO: Log
        return;
    }

    for (int i = 0; i < n; i++) {
        buffer[i] = s->params.amp * buffer[i];
    }

    e = pthread_rwlock_unlock(&s->params.rw);
    if (e != 0) {
        // TODO: Log
        return;
    }
}

void update_synth(Synth* s, float global_time, float* buffer, size_t n) {
    timespec_t start_time = {0};
    timespec_t section_time = {0};

    for (size_t i = 0; i < n; i++) {
        buffer[i] = 0.0f;
    }

    timespec_get(&start_time, TIME_UTC);

    timespec_get(&section_time, TIME_UTC);
    update_osc(s, &s->osc_arr[0], &s->env_arr[0], global_time, buffer, n);
    s->prof.osc_time[0] = duration_from(section_time);

    timespec_get(&section_time, TIME_UTC);
    update_osc(s, &s->osc_arr[1], &s->env_arr[1], global_time, buffer, n);
    s->prof.osc_time[1] = duration_from(section_time);

    timespec_get(&section_time, TIME_UTC);
    update_filter(&s->flt, buffer, n);
    s->prof.flt_time = duration_from(section_time);

    timespec_get(&section_time, TIME_UTC);
    update_synth_amp(s, buffer, n);

    s->prof.total_time = duration_from(start_time);
}
