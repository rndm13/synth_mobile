#include "synth.h"

#include "src/filter.h"
#include "src/osc.h"
#include "src/settings.h"
#include "src/voice.h"
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

    s->distortion.wet_dry_ratio = 0.0f;
    s->distortion.gain = DISTORTION_GAIN_MIN;

    s->delay.wet_dry_ratio = 0.0f;
    s->delay.feedback = 0;
    s->delay.delay_s = DELAY_S_MIN;

    e = pthread_rwlock_init(&s->distortion.rw, NULL);
    if (e != 0) {
        return;
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

    pthread_rwlock_destroy(&s->distortion.rw);
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

void update_osc(Osc* osc, Env* env, float global_time, float* buffer, size_t n) {
    OscVoiceArr *ova = &osc->voice_arr;
    OscVoice *osc_voice = NULL;
    Voice voice = {};
    float wave_freq = 0;
    float detune_mul = 0;
    float detune_offset = 0;
    float cents_mul = 0;
    float semi_mul = 0;
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

        detune_offset = osc_voice->unison_idx - (osc->params.unison - 1) / 2.0;
        detune_mul = calc_cents_mul(osc->params.detune * detune_offset);
        cents_mul = calc_cents_mul(osc->params.cents);
        semi_mul = calc_semi_mul(osc->params.semi);

        wave_freq = voice.freq * cents_mul * semi_mul * detune_mul;
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

void update_filter(Filter* flt, Env* env, OscVoice* last_osc_voice, float global_time, float* buffer, size_t n) {
    FilterParams params = flt->params;
    BiquadFilterParams biquad = {};
    float env_val = 0.0f;
    float time = 0.0f;
    float release_time = 0.0f;

    if (params.type == FT_DISABLED) {
        for (size_t i = 0; i < ARRAY_SIZE(flt->x); i++) {
            flt->x[i] = 0;
        }

        for (size_t i = 0; i < ARRAY_SIZE(flt->y); i++) {
            flt->y[i] = 0;
        }

        for (size_t i = 0; i < ARRAY_SIZE(flt->fir_down.history); i++) {
            flt->fir_down.history[i] = 0;
        }

        for (size_t i = 0; i < ARRAY_SIZE(flt->fir_up.history); i++) {
            flt->fir_up.history[i] = 0;
        }

        return;
    }

    if (NULL != last_osc_voice) {
        time = global_time - last_osc_voice->start_time;
        release_time = last_osc_voice->release_time - last_osc_voice->start_time;

        env_val = calc_env_value(
                time, last_osc_voice->released,
                release_time, flt->last_env, env);
        if (!last_osc_voice->released) {
            flt->last_env = env_val;
        }

        params.cutoff = Clamp(env_val * params.env2_int + params.cutoff,
                FLT_CUTOFF_MIN, FLT_CUTOFF_MAX);
    }

    calc_biquad_filter_params(&params, &biquad);

    upsample_filter_u(flt, buffer, n);

    // Converts the buffer data before using it
    for (size_t i = 0; i < n * FLT_OVERSAMPLING; i++) {
        // Move old state
        flt->y[2] = flt->y[1];
        flt->y[1] = flt->y[0];
        flt->x[2] = flt->x[1];
        flt->x[1] = flt->x[0];

        flt->x[0] = flt->oversampled_buffer[i];
        flt->y[0] =
            biquad.b[0] * flt->x[0] +
            biquad.b[1] * flt->x[1] +
            biquad.b[2] * flt->x[2] -
            biquad.a[1] * flt->y[1] -
            biquad.a[2] * flt->y[2] +
            EPSILON; // Anti-denormal offset

        flt->oversampled_buffer[i] = flt->y[0];
    }

    downsample_filter_u(flt, buffer, n);
}

void update_distortion(Distortion* d, float* buffer, size_t n) {
    float wet = 0.0f;

    int e = pthread_rwlock_rdlock(&d->rw);
    if (e != 0) {
        // TODO: Log
        return;
    }

    if (d->wet_dry_ratio == 0) {
        goto unlock;
    }

    for (size_t i = 0; i < n; i++) {
        wet = tanh(buffer[i] * d->gain);
        buffer[i] =
            buffer[i] * (1 - d->wet_dry_ratio) +
            wet * d->wet_dry_ratio;
    }

    for (size_t i = 0; i < n; i++) {
        buffer[i] = tanh(buffer[i]);
    }

unlock:
    e = pthread_rwlock_unlock(&d->rw);
    if (e != 0) {
        // TODO: Log
        return;
    }
}

void update_delay(Delay* d, float* buffer, size_t n) {
    float wet = 0.0f;
    int delay_length = 0;

    int e = pthread_rwlock_rdlock(&d->rw);
    if (e != 0) {
        // TODO: Log
        return;
    }

    delay_length = d->delay_s * SAMPLE_RATE;

    if (d->wet_dry_ratio == 0) {
        goto unlock;
    }


    for (size_t i = 0; i < n; i++) {
        wet = d->buffer[(d->buffer_idx + delay_length - 1) % delay_length];
        d->buffer[d->buffer_idx] = buffer[i] + d->buffer[d->buffer_idx] * d->feedback;

        d->buffer_idx++;
        d->buffer_idx %= delay_length;

        buffer[i] =
            buffer[i] * (1 - d->wet_dry_ratio) +
            wet * d->wet_dry_ratio;
    }

    for (size_t i = 0; i < n; i++) {
        buffer[i] = tanh(buffer[i]);
    }

unlock:
    e = pthread_rwlock_unlock(&d->rw);
    if (e != 0) {
        // TODO: Log
        return;
    }
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

timespec_t duration_from(timespec_t start_time) {
    timespec_t end_time = {0};
    timespec_get(&end_time, TIME_UTC);

    end_time.tv_sec -= start_time.tv_sec;
    end_time.tv_nsec -= start_time.tv_nsec;

    end_time.tv_nsec += S_TO_NS(end_time.tv_sec);
    end_time.tv_sec = 0;

    return end_time;
}

void update_synth(Synth* s, float global_time, float* buffer, size_t n) {
    OscVoice* ov = osc_voice_get_last(&s->osc_arr[0].voice_arr);
    timespec_t start_time = {0};
    timespec_t section_time = {0};

    for (size_t i = 0; i < n; i++) {
        buffer[i] = 0.0f;
    }

    s->prof.cur_stage = 0;
    timespec_get(&start_time, TIME_UTC);

    timespec_get(&section_time, TIME_UTC);
    update_osc(&s->osc_arr[0], &s->env_arr[0], global_time, buffer, n);
    s->prof.osc_time[0] = duration_from(section_time);
    s->prof.cur_stage++;

    timespec_get(&section_time, TIME_UTC);
    update_osc(&s->osc_arr[1], &s->env_arr[1], global_time, buffer, n);
    s->prof.osc_time[1] = duration_from(section_time);
    s->prof.cur_stage++;

    timespec_get(&section_time, TIME_UTC);
    update_filter(&s->flt, &s->env_arr[2], ov, global_time, buffer, n);
    s->prof.flt_time = duration_from(section_time);
    s->prof.cur_stage++;

    timespec_get(&section_time, TIME_UTC);
    update_distortion(&s->distortion, buffer, n);
    s->prof.distortion_time = duration_from(section_time);
    s->prof.cur_stage++;

    timespec_get(&section_time, TIME_UTC);
    update_delay(&s->delay, buffer, n);
    s->prof.delay_time = duration_from(section_time);
    s->prof.cur_stage++;

    timespec_get(&section_time, TIME_UTC);
    update_synth_amp(s, buffer, n);
    s->prof.amp_time = duration_from(section_time);
    s->prof.cur_stage++;

    s->prof.total_time = duration_from(start_time);
}
