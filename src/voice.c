#include "voice.h"

#include <math.h>
#include <pthread.h>
#include <stdlib.h>

#include "settings.h"

static uint32_t voice_id_counter = 0;

static float calc_voice_freq(int k_idx) {
    return powf(2.0f, (float)(k_idx - KEY_A4_IDX) / (float)KEY_OCTAVE) * KEY_A4_FREQ;
}

static void voice_set(VoiceArr* v, bool midi, int v_idx, int k_idx, int velocity) {
    v->voice_arr[v_idx].midi = midi;
    v->voice_arr[v_idx].freq = calc_voice_freq(k_idx);
    v->voice_arr[v_idx].key_idx = k_idx;
    v->voice_arr[v_idx].velocity = velocity;
    v->voice_arr[v_idx].id = voice_id_counter++;
}

static void voice_reset(VoiceArr* v, int v_idx) {
    v->voice_arr[v_idx].midi = false;
    v->voice_arr[v_idx].freq = 0.0f;
    v->voice_arr[v_idx].key_idx = KEY_IDX_INVALID;
    v->voice_arr[v_idx].velocity = 0;
    v->voice_arr[v_idx].id = 0;
}

void voice_remove(VoiceArr *v, int v_idx) {
    if (v->voice_count == 0) {
        return;
    }

    v->voice_arr[v_idx] = v->voice_arr[v->voice_count - 1];
    voice_reset(v, v->voice_count - 1);
    v->voice_count--;
}

bool voice_hold_key(VoiceArr *v, bool midi, int k_idx, int velocity, Voice* o_voice) {
    bool existing_voice = false;

    for (int i = 0; i < v->voice_count; i++) {
        if (v->voice_arr[i].key_idx == k_idx) {
            existing_voice = true;

            return existing_voice;
        }
    }

    if (v->voice_count >= VOICE_MAX_COUNT) {
        // Shift voices left first and replace with the last index
        // Probably should do something to oscillator voices too
        voice_remove(v, 0);
    }

    voice_set(v, midi, v->voice_count, k_idx, velocity);
    *o_voice = v->voice_arr[v->voice_count];
    v->voice_count++;

    return existing_voice;
}

void voice_release_key(VoiceArr *v, int k_idx) {
    for (int v_idx = 0; v_idx < v->voice_count; v_idx++) {
        if (v->voice_arr[v_idx].key_idx == k_idx) {
            voice_remove(v, v_idx);

            break;
        }
    }
}

static void osc_voice_set(OscVoiceArr* ov, int ov_idx,
        Voice v, int u_idx, int wave_idx, size_t start_idx) {
    ov->osc_voice_arr[ov_idx].voice = v;
    ov->osc_voice_arr[ov_idx].unison_idx = u_idx;

    ov->osc_voice_arr[ov_idx].start_sample_idx = start_idx;
    ov->osc_voice_arr[ov_idx].release_sample_idx = 0;
    ov->osc_voice_arr[ov_idx].released = false;

    ov->osc_voice_arr[ov_idx].wave_idx = wave_idx;
}

static void osc_voice_reset(OscVoiceArr* ov, int ov_idx) {
    ov->osc_voice_arr[ov_idx].voice.freq = 0.0f;
    ov->osc_voice_arr[ov_idx].voice.key_idx = KEY_IDX_INVALID;
    ov->osc_voice_arr[ov_idx].voice.velocity = 0.0f;

    ov->osc_voice_arr[ov_idx].unison_idx = 0;

    ov->osc_voice_arr[ov_idx].start_sample_idx = 0;
    ov->osc_voice_arr[ov_idx].release_sample_idx = 0;
    ov->osc_voice_arr[ov_idx].released = false;
    ov->osc_voice_arr[ov_idx].dead = false;

    ov->osc_voice_arr[ov_idx].wave_idx = 0;
}

void osc_voice_remove(OscVoiceArr *ov, int ov_idx) {
    if (ov->osc_voice_count == 0) {
        return;
    }

    ov->osc_voice_arr[ov_idx] = ov->osc_voice_arr[ov->osc_voice_count - 1];
    osc_voice_reset(ov, ov->osc_voice_count - 1);
    ov->osc_voice_count--;
}

void osc_voice_add(OscVoiceArr *ov, Voice v, size_t unison_idx, size_t start_idx, bool rand_phase) {
    int wave_idx = 0;
    int wave_length = 0;

    if (ov->osc_voice_count >= VOICE_MAX_COUNT) {
        // Shift voices left first and replace with the last index
        osc_voice_remove(ov, 0);
    }

    if (rand_phase) {
        wave_length = SAMPLE_RATE / v.freq;
        // TODO: Custom RNG
        wave_idx = rand() % wave_length;
    }

    osc_voice_set(ov, ov->osc_voice_count, v, unison_idx, wave_idx, start_idx);
    ov->osc_voice_count++;
}

void osc_voice_add_unison(OscVoiceArr *ov, Voice v, size_t cnt, size_t start_idx, bool rand_phase) {
    for (size_t i = 0; i < cnt; i++) {
        osc_voice_add(ov, v, i, start_idx, rand_phase);
    }
}

void osc_voice_release(OscVoiceArr *ov, int k_idx, size_t release_idx) {
    for (int ov_idx = 0; ov_idx < ov->osc_voice_count; ov_idx++) {
        if (ov->osc_voice_arr[ov_idx].voice.key_idx == k_idx &&
            !ov->osc_voice_arr[ov_idx].released) {
            ov->osc_voice_arr[ov_idx].released = true;
            ov->osc_voice_arr[ov_idx].release_sample_idx = release_idx;
        }
    }
}

OscVoice* osc_voice_get_last(OscVoiceArr *ov) {
    size_t max_start_time_idx = 0;
    for (size_t i = 1; i < ov->osc_voice_count; i++) {
        if (ov->osc_voice_arr[i].start_sample_idx >
            ov->osc_voice_arr[max_start_time_idx].start_sample_idx) {
            max_start_time_idx = i;
        }
    }

    if (max_start_time_idx >= ov->osc_voice_count) {
        return NULL;
    }

    return &ov->osc_voice_arr[max_start_time_idx];
}

void osc_voice_gc(OscVoiceArr *ov) {
    for (int i = 0; i < ov->osc_voice_count; i++) {
        while (i < ov->osc_voice_count && ov->osc_voice_arr[i].dead) {
            osc_voice_remove(ov, i);
        }
    }
}

void osc_voice_arr_init(OscVoiceArr *ov) {
    ov->osc_voice_count = 0;
    for (int i = 0; i < VOICE_MAX_COUNT; i++) {
        osc_voice_reset(ov, i);
    }
}

void osc_voice_arr_deinit(OscVoiceArr *ov) {
    ov->osc_voice_count = 0;
    for (int i = 0; i < VOICE_MAX_COUNT; i++) {
        osc_voice_reset(ov, i);
    }
}

void voice_arr_init(VoiceArr *v) {
    v->voice_count = 0;
    for (int i = 0; i < VOICE_MAX_COUNT; i++) {
        voice_reset(v, i);
    }
}

void voice_arr_deinit(VoiceArr *v) {
    v->voice_count = 0;
    for (int i = 0; i < VOICE_MAX_COUNT; i++) {
        voice_reset(v, i);
    }
}
