#include "voice.h"
#include <pthread.h>

// TODO: Log log log log
static void voice_set(VoiceArr* v, int v_idx, int k_idx) {
    v->voice_arr[v_idx].key_idx = k_idx;
    v->voice_arr[v_idx].velocity = 40;
}

static void voice_reset(VoiceArr* v, int v_idx) {
    v->voice_arr[v_idx].key_idx = KEY_IDX_INVALID;
    v->voice_arr[v_idx].velocity = 0;
}

static void voice_remove_u(VoiceArr *v, int v_idx) {
    if (v->voice_count == 0) {
        return;
    }

    v->voice_arr[v_idx] = v->voice_arr[v->voice_count - 1];
    voice_reset(v, v->voice_count - 1);
    v->voice_count--;
}

void voice_remove(VoiceArr *v, int v_idx) {
    pthread_rwlock_wrlock(&v->rw);

    voice_remove_u(v, v_idx);

unlock:
    pthread_rwlock_unlock(&v->rw);
}

bool voice_hold_key(VoiceArr *v, int k_idx, Voice* o_voice) {
    bool existing_voice = false;

    pthread_rwlock_rdlock(&v->rw);

    for (int i = 0; i < v->voice_count; i++) {
        if (v->voice_arr[i].key_idx == k_idx) {
            existing_voice = true;
            goto unlock;
        }
    }

    pthread_rwlock_unlock(&v->rw);

    pthread_rwlock_wrlock(&v->rw);

    if (v->voice_count >= VOICE_MAX_COUNT) {
        // Shift voices left first and replace with the last index
        // Probably should do something to oscillator voices too
        voice_remove_u(v, 0);
    }

    voice_set(v, v->voice_count, k_idx);
    *o_voice = v->voice_arr[v->voice_count];
    v->voice_count++;

unlock:
    pthread_rwlock_unlock(&v->rw);

    return existing_voice;
}

void voice_release_key(VoiceArr *v, int k_idx) {
    pthread_rwlock_wrlock(&v->rw);

    for (int v_idx = 0; v_idx < v->voice_count; v_idx++) {
        if (v->voice_arr[v_idx].key_idx == k_idx) {
            voice_remove_u(v, v_idx);
            break;
        }
    }

    pthread_rwlock_unlock(&v->rw);
}

static void osc_voice_set(OscVoiceArr* ov, int ov_idx, Voice v, float time) {
    ov->osc_voice_arr[ov_idx].voice = v;
    ov->osc_voice_arr[ov_idx].wave_idx = 0;
    ov->osc_voice_arr[ov_idx].start_time = time;
    ov->osc_voice_arr[ov_idx].release_time = 0;
    ov->osc_voice_arr[ov_idx].env = 0;
    ov->osc_voice_arr[ov_idx].released = false;
}

static void osc_voice_reset(OscVoiceArr* ov, int ov_idx) {
    ov->osc_voice_arr[ov_idx].voice.key_idx = KEY_IDX_INVALID;
    ov->osc_voice_arr[ov_idx].voice.velocity = 0.0f;
    ov->osc_voice_arr[ov_idx].wave_idx = 0;
    ov->osc_voice_arr[ov_idx].start_time = 0;
    ov->osc_voice_arr[ov_idx].release_time = 0;
    ov->osc_voice_arr[ov_idx].env = 0;
    ov->osc_voice_arr[ov_idx].released = false;
}

void osc_voice_remove_u(OscVoiceArr *ov, int ov_idx) {
    if (ov->osc_voice_count == 0) {
        return;
    }

    ov->osc_voice_arr[ov_idx] = ov->osc_voice_arr[ov->osc_voice_count - 1];
    osc_voice_reset(ov, ov->osc_voice_count - 1);
    ov->osc_voice_count--;
}

void osc_voice_remove(OscVoiceArr *ov, int ov_idx) {
    pthread_rwlock_wrlock(&ov->rw);

    osc_voice_remove_u(ov, ov_idx);

unlock:
    pthread_rwlock_unlock(&ov->rw);
}

void osc_voice_add(OscVoiceArr *ov, Voice v, float time) {
    pthread_rwlock_wrlock(&ov->rw);

    if (ov->osc_voice_count >= VOICE_MAX_COUNT) {
        // Shift voices left first and replace with the last index
        osc_voice_remove_u(ov, 0);
    }

    osc_voice_set(ov, ov->osc_voice_count, v, time);
    ov->osc_voice_count++;

    pthread_rwlock_unlock(&ov->rw);
}

void osc_voice_release(OscVoiceArr *ov, int k_idx, float time) {
    pthread_rwlock_wrlock(&ov->rw);

    for (int ov_idx = 0; ov_idx < ov->osc_voice_count; ov_idx++) {
        if (ov->osc_voice_arr[ov_idx].voice.key_idx == k_idx &&
            !ov->osc_voice_arr[ov_idx].released) {
            ov->osc_voice_arr[ov_idx].release_time = time;
            ov->osc_voice_arr[ov_idx].released = true;
        }
    }

    pthread_rwlock_unlock(&ov->rw);
}

void osc_voice_gc(OscVoiceArr *ov) {
    pthread_rwlock_rdlock(&ov->rw);

    for (int i = 0; i < ov->osc_voice_count; i++) {
        while (
            i < ov->osc_voice_count &&
            ov->osc_voice_arr[i].released &&
            ov->osc_voice_arr[i].env == 0
        ) {
            // Hopefully this works fine-ish
            pthread_rwlock_unlock(&ov->rw);

            osc_voice_remove(ov, i);

            pthread_rwlock_rdlock(&ov->rw);
        }
    }

    pthread_rwlock_unlock(&ov->rw);
}

void osc_voice_arr_init(OscVoiceArr *ov) {
    int e = pthread_rwlock_init(&ov->rw, NULL);
    if (e != 0) {
        // TODO: Log
        return;
    }

    ov->osc_voice_count = 0;
    for (int i = 0; i < VOICE_MAX_COUNT; i++) {
        osc_voice_reset(ov, i);
    }
}

void osc_voice_arr_deinit(OscVoiceArr *ov) {
    int e = pthread_rwlock_destroy(&ov->rw);
    if (e != 0) {
        // TODO: Log
        return;
    }

    ov->osc_voice_count = 0;
    for (int i = 0; i < VOICE_MAX_COUNT; i++) {
        osc_voice_reset(ov, i);
    }
}

void voice_arr_init(VoiceArr *v) {
    int e = pthread_rwlock_init(&v->rw, NULL);
    if (e != 0) {
        // TODO: Log
        return;
    }

    v->voice_count = 0;
    for (int i = 0; i < VOICE_MAX_COUNT; i++) {
        voice_reset(v, i);
    }
}

void voice_arr_deinit(VoiceArr *v) {
    int e = pthread_rwlock_destroy(&v->rw);
    if (e != 0) {
        // TODO: Log
        return;
    }

    v->voice_count = 0;
    for (int i = 0; i < VOICE_MAX_COUNT; i++) {
        voice_reset(v, i);
    }
}
