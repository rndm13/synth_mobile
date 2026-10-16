#include "osc.h"

#include "math.h"
#include "settings.h"

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

void osc_add_voice(const OscParams* params, OscState* osc, Voice new_voice, size_t start_idx) {
    osc_voice_add_unison(&osc->voice_arr, new_voice, params->unison, start_idx, params->rand_phase);
}

void prepare_osc_display(const OscParams* params, float disp_buffer[DISPLAY_BUFFER_SIZE]) {
    int wave_length = DISPLAY_BUFFER_SIZE;

    for (int i = 0; i < wave_length; i++) {
        disp_buffer[i] = calc_osc_value(params->type, i, wave_length / 2);
    }
}

float calc_semi_mul(int semi) {
    return powf(2, semi / (float)KEY_OCTAVE);
}

float calc_cents_mul(float cents) {
    return powf(2, cents / CENTS_IN_OCTAVE);
}

void init_osc_params(OscParams* osc) {
    osc->type = OT_SINE;
    osc->semi = 0;
    osc->cents = 0;
    osc->unison = OSC_UNISON_MIN;
    osc->detune = OSC_DETUNE_MIN;
    osc->volume = 1.0f;
}

void init_osc_state(OscState* osc) {
    osc_voice_arr_init(&osc->voice_arr);
}
