#include "osc.h"

#include "math.h"

float calc_osc_value(OscType type, int wave_idx, int wave_length) {
    wave_idx %= wave_length;

    switch (type) {
        case OT_SINE:
            return sin(2 * PI * wave_idx / wave_length);
            break;
        case OT_TRIANGLE:
            return 2 * fabs(2 * wave_idx / (float)wave_length - 1.0f) - 1.0f;
            break;
        case OT_SQUARE:
            return (wave_idx / (float)wave_length > 0.5f) ? -1.0f : 1.0f;
            break;
        case OT_SAW:
            return 2 * wave_idx / (float)wave_length - 1.0f;
            break;
        default:
            break;
    }

    return 0.0f;
}

void osc_add_voice(Osc* osc, Voice new_voice, float time) {
    int unison = 0;

    int e = pthread_rwlock_rdlock(&osc->params.rw);
    if (e != 0) {
        // TODO: Log
        return;
    }

    unison = osc->params.unison;

    e = pthread_rwlock_unlock(&osc->params.rw);
    if (e != 0) {
        // TODO: Log
        return;
    }

    osc_voice_add_unison(&osc->voice_arr, new_voice, unison, time);
}

void prepare_osc_display_buffer(Osc* osc) {
    int e = pthread_rwlock_rdlock(&osc->params.rw);
    if (e != 0) {
        // TODO: Log
        return;
    }

    int wave_length = ARRAY_SIZE(osc->disp_buffer);
    for (int i = 0; i < wave_length; i++) {
        osc->disp_buffer[i] = calc_osc_value(osc->params.type, i, wave_length / 2);
    }

    e = pthread_rwlock_unlock(&osc->params.rw);
    if (e != 0) {
        // TODO: Log
        return;
    }
}

float calc_semi_mul(int semi) {
    return powf(2, semi / (float)KEY_OCTAVE);
}

float calc_cents_mul(float cents) {
    return powf(2, cents / CENTS_IN_OCTAVE);
}

void init_osc(Osc* osc) {
    int e = pthread_rwlock_init(&osc->params.rw, NULL);
    if (e != 0) {
        return;
    }

    osc_voice_arr_init(&osc->voice_arr);
    osc->params.type = OT_SINE;
    osc->params.semi = 0;
    osc->params.cents = 0;
    osc->params.unison = OSC_UNISON_MIN;
    osc->params.detune = OSC_DETUNE_MIN;
    osc->params.volume = 1.0f;

    prepare_osc_display_buffer(osc);
}

void deinit_osc(Osc* osc) {
    pthread_rwlock_destroy(&osc->params.rw);
    osc_voice_arr_deinit(&osc->voice_arr);
}

