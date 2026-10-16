#include "synth.h"

#include <stdlib.h>
#include <raymath.h>
#include <stdio.h>

#include "filter.h"
#include "log.h"
#include "osc.h"
#include "settings.h"
#include "voice.h"

void init_synth_params(SynthParams* s) {
    snprintf(s->gen.program_name, PROGRAM_NAME_CAPACITY, "%s", PROGRAM_NAME_INIT);
    s->gen.amp = 0.2;
    s->gen.pan = 0.5f;
    voice_arr_init(&s->voice_arr);

    for (size_t i = 0; i < ARRAY_SIZE(s->env_arr); i++) {
        init_env(&s->env_arr[i]);
    }

    for (size_t i = 0; i < ARRAY_SIZE(s->osc_arr); i++) {
        init_osc_params(&s->osc_arr[i]);
    }

    init_filter_params(&s->flt);

    s->distortion.wet_dry_ratio = 0.0f;
    s->distortion.gain = DISTORTION_GAIN_MIN;

    s->delay.wet_dry_ratio = 0.0f;
    s->delay.feedback = 0;
    s->delay.delay_s = DELAY_S_MIN;
}


void init_synth_state(const SynthParams *p, SynthState* s) {
    for (size_t i = 0; i < ARRAY_SIZE(s->osc_arr); i++) {
        init_osc_state(&s->osc_arr[i]);
    }

    init_filter_state(&p->flt, &s->flt);

    s->sample_idx = 0;
}

static
void synth_add_voice(const SynthParams* p, SynthState* s, Voice new_voice) {
    for (size_t i = 0; i < ARRAY_SIZE(s->osc_arr); i++) {
        osc_add_voice(&p->osc_arr[i], &s->osc_arr[i], new_voice, s->sample_idx);
    }
}

static
void synth_release_voice(SynthState* s, int k_idx) {
    for (size_t i = 0; i < ARRAY_SIZE(s->osc_arr); i++) {
        osc_voice_release(&s->osc_arr[i].voice_arr, k_idx, s->sample_idx);
    }
}

void synth_gc_voice_arr(SynthState* s) {
    for (size_t i = 0; i < ARRAY_SIZE(s->osc_arr); i++) {
        osc_voice_gc(&s->osc_arr[i].voice_arr);
    }
}

void synth_update_voice_arr(const SynthParams* p, SynthState* s) {
    size_t i = 0;
    size_t v = 0;
    size_t o = 0;
    bool found = false;

    synth_gc_voice_arr(s);

    // TODO: Optimize this
    for (i = 0; i < ARRAY_SIZE(s->osc_arr); i++) {
        /* Release oscillator voices */
        for (o = 0; o < s->osc_arr[i].voice_arr.osc_voice_count; o++) {
            found = false;

            for (v = 0; v < p->voice_arr.voice_count; v++) {
                if (s->osc_arr[i].voice_arr.osc_voice_arr[o].voice.id == p->voice_arr.voice_arr[v].id) {
                    found = true;

                    break;
                }
            }

            if (false == found) {
                synth_release_voice(s, s->osc_arr[i].voice_arr.osc_voice_arr[o].voice.key_idx);
            }
        }

        /* Add new oscillator voices */
        for (v = 0; v < p->voice_arr.voice_count; v++) {
            found = false;

            for (o = 0; o < s->osc_arr[i].voice_arr.osc_voice_count; o++) {
                if (s->osc_arr[i].voice_arr.osc_voice_arr[o].voice.id == p->voice_arr.voice_arr[v].id) {
                    found = true;

                    break;
                }
            }

            if (false == found) {
                synth_add_voice(p, s, p->voice_arr.voice_arr[v]);
            }
        }
    }
}

void update_osc_voice(SynthState* s, const OscParams* params, Env* env, OscVoice* osc_voice, float wave_freq) {
    Voice voice = osc_voice->voice;

    float vel_mul = (float)voice.velocity / MAX_VELOCITY;
    float env_val = 1.0f;

    for (int j = 0; j < s->sample_buffer_size; j++) {
        int wave_length = SAMPLE_RATE / wave_freq;
        double dt = j / (float)SAMPLE_RATE;
        float osc_value = calc_osc_value(params->type, osc_voice->wave_idx, wave_length);

        env_val = calc_env_value(s->sample_idx + j, env, osc_voice);
        if (osc_voice->released && FloatEquals(env_val, 0)) {
            osc_voice->dead = true;
        }

        s->sample_buffer[j] += params->volume * env_val * vel_mul * osc_value;
        osc_voice->wave_idx = (osc_voice->wave_idx + 1) % wave_length;
    }
}

void update_osc(SynthState* s, const OscParams* params, OscState* osc, Env* env) {
    OscVoiceArr *ova = &osc->voice_arr;
    OscVoice *osc_voice = NULL;
    Voice voice = {};
    float wave_freq = 0;
    float detune_mul = 0;
    float detune_offset = 0;
    float cents_mul = 0;
    float semi_mul = 0;
    int e = 0;

    for (int v = 0; v < ova->osc_voice_count; v++) {
        osc_voice = &ova->osc_voice_arr[v];
        voice = osc_voice->voice;

        detune_offset = osc_voice->unison_idx - (params->unison - 1) / 2.0;
        detune_mul = calc_cents_mul(params->detune * detune_offset);
        cents_mul = calc_cents_mul(params->cents);
        semi_mul = calc_semi_mul(params->semi);

        wave_freq = voice.freq * cents_mul * semi_mul * detune_mul;
        update_osc_voice(s, params, env, osc_voice, wave_freq);
    }
}

void update_filter(SynthState* s, const FilterParams* params, FilterState* flt) {
    double env_val = 0.0f;
    double time = 0.0f;
    double release_time = 0.0f;

    if (params->type == FT_DISABLED) {
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

    upsample_filter(flt, s->sample_buffer, s->sample_buffer_size);

    // Converts the buffer data before using it
    for (size_t i = 0; i < s->sample_buffer_size * FLT_OVERSAMPLING; i++) {
        // Move old state
        flt->y[2] = flt->y[1];
        flt->y[1] = flt->y[0];
        flt->x[2] = flt->x[1];
        flt->x[1] = flt->x[0];

        flt->x[0] = flt->oversampled_buffer[i];
        flt->y[0] =
            flt->biquad.b[0] * flt->x[0] +
            flt->biquad.b[1] * flt->x[1] +
            flt->biquad.b[2] * flt->x[2] -
            flt->biquad.a[1] * flt->y[1] -
            flt->biquad.a[2] * flt->y[2] +
            EPSILON; // Anti-denormal offset

        flt->oversampled_buffer[i] = flt->y[0];
    }

    downsample_filter(flt, s->sample_buffer, s->sample_buffer_size);
}

void update_distortion(SynthState* s, const DistortionParams* d) {
    float wet = 0.0f;

    for (size_t i = 0; i < s->sample_buffer_size; i++) {
        wet = tanh(s->sample_buffer[i] * d->gain);

        s->sample_buffer[i] = s->sample_buffer[i] * (1 - d->wet_dry_ratio) + wet * d->wet_dry_ratio;
    }

    for (size_t i = 0; i < s->sample_buffer_size; i++) {
        s->sample_buffer[i] = tanh(s->sample_buffer[i]);
    }
}

void update_delay(SynthState* s, const DelayParams* params, DelayState* d) {
    float wet = 0.0f;
    int delay_length = 0;

    delay_length = params->delay_s * SAMPLE_RATE;

    for (size_t i = 0; i < s->sample_buffer_size; i++) {
        wet = d->buffer[(d->buffer_idx + delay_length - 1) % delay_length];
        d->buffer[d->buffer_idx] = s->sample_buffer[i] + d->buffer[d->buffer_idx] * params->feedback;

        d->buffer_idx++;
        d->buffer_idx %= delay_length;

        s->sample_buffer[i] = s->sample_buffer[i] * (1 - params->wet_dry_ratio) + wet * params->wet_dry_ratio;
    }
}

void update_synth_amp(SynthState* s, const SynthGeneralParams* params) {
    for (int i = 0; i < s->sample_buffer_size; i++) {
        s->sample_buffer[i] = params->amp * s->sample_buffer[i];
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

void update_synth_state(Synth* s) {
    if (false == s->update_params) {
        return;
    }

    s->locked_params = s->gui_params;
    s->update_params = false;

    synth_update_voice_arr(&s->locked_params, &s->state);

    calc_biquad_filter_params(&s->locked_params.flt, &s->state.flt.biquad);
}

void update_synth_params(Synth* s) {
    s->update_params = true;
}

void update_synth(Synth* s, float* buffer, size_t n) {
    timespec_t start_time = {0};
    timespec_t section_time = {0};

    s->state.sample_buffer = buffer;
    s->state.sample_buffer_size = n;

    for (size_t i = 0; i < s->state.sample_buffer_size; i++) {
        s->state.sample_buffer[i] = 0.0f;
    }

    update_synth_state(s);

    s->state.prof.cur_stage = 0;
    timespec_get(&start_time, TIME_UTC);

    timespec_get(&section_time, TIME_UTC);
    update_osc(&s->state, &s->locked_params.osc_arr[0], &s->state.osc_arr[0], &s->locked_params.env_arr[0]);
    s->state.prof.osc_time[0] = duration_from(section_time);
    s->state.prof.cur_stage++;

    timespec_get(&section_time, TIME_UTC);
    update_osc(&s->state, &s->locked_params.osc_arr[1], &s->state.osc_arr[1], &s->locked_params.env_arr[0]);
    s->state.prof.osc_time[1] = duration_from(section_time);
    s->state.prof.cur_stage++;

    timespec_get(&section_time, TIME_UTC);
    update_filter(&s->state, &s->locked_params.flt, &s->state.flt);
    s->state.prof.flt_time = duration_from(section_time);
    s->state.prof.cur_stage++;

    timespec_get(&section_time, TIME_UTC);
    update_distortion(&s->state, &s->locked_params.distortion);
    s->state.prof.distortion_time = duration_from(section_time);
    s->state.prof.cur_stage++;

    timespec_get(&section_time, TIME_UTC);
    update_delay(&s->state, &s->locked_params.delay, &s->state.delay);
    s->state.prof.delay_time = duration_from(section_time);
    s->state.prof.cur_stage++;

    timespec_get(&section_time, TIME_UTC);
    update_synth_amp(&s->state, &s->locked_params.gen);
    s->state.prof.amp_time = duration_from(section_time);
    s->state.prof.cur_stage++;

    s->state.prof.total_time = duration_from(start_time);

    s->state.sample_idx += s->state.sample_buffer_size;
}
