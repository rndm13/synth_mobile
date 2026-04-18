#include "raylib.h"
#include "raymath.h"

#include <math.h>
#include <stddef.h>
#include <complex.h>
#include <pthread.h>
#include <string.h>

#include "gui_elements.h"
#include "voice.h"
#include "fft.h"

#define SAMPLE_RATE   44100

#define ARRAY_SIZE(X) (sizeof(X) / sizeof(*(X)))

#define OSC_SEMI_RANGE 12

#define ENV_A_MIN 0.1f
#define ENV_A_MAX 5.0f
#define ENV_D_MIN 0.1f
#define ENV_D_MAX 5.0f
#define ENV_R_MIN 0.1f
#define ENV_R_MAX 5.0f

#define FLT_OVERSAMPLING  2
#define FLT_CUTOFF_MIN    50.0f
#define FLT_CUTOFF_MAX    (SAMPLE_RATE / 2.0f)
#define FLT_RESONANCE_MIN 0.7071f
#define FLT_RESONANCE_MAX 20.0f
#define FLT_GAIN_MIN      -20.0f
#define FLT_GAIN_MAX      20.0f

#define KEY_C_OFF    4
#define OCTAVE_COUNT 8
#define KEY_OCTAVE   12
#define KEY_COUNT    (KEY_C_OFF + KEY_OCTAVE * OCTAVE_COUNT)

#define KEY_A4_IDX  49
#define KEY_A4_FREQ 440.0f
#define KEY_C4_IDX  (KEY_A4_IDX - 9)

#define ENV_COUNT    2
#define OSC_COUNT    2

#define DISPLAY_BUFFER_SIZE 256
#define BUFFER_SIZE 4096
#define FFT_BUFFER_SIZE_MUL 4
#define FFT_BUFFER_SIZE BUFFER_SIZE * FFT_BUFFER_SIZE_MUL

#define MAX_VELOCITY 80.0f

#define X_ENUM(v, s) \
    v,

#define X_STR_ARR(v, s) \
    s,

#define X_STR_CASE(v, s) \
    case v: return s;

typedef struct Key {
    Vector2 pos;
    Vector2 size;

    float freq;
} Key;

typedef struct Env {
    pthread_rwlock_t rw;
    float attack;
    float decay;
    float sustain;
    float release;
} Env;

#define OSC_TYPE_X(X)           \
    X(OT_SINE, "Sine")          \
    X(OT_TRIANGLE, "Triangle")  \
    X(OT_SQUARE, "Square")      \
    X(OT_SAW, "Sawtooth")       \

typedef enum OscType {
    OSC_TYPE_X(X_ENUM)
    OT_MAX,
} OscType;

typedef struct OscParams {
    pthread_rwlock_t rw;

    OscType type;
    int semi;
    float volume;
} OscParams;

typedef struct Osc {
    // RW protected
    OscParams params;
    OscVoiceArr voice_arr;

    // Output
    float buffer[BUFFER_SIZE];
    float disp_buffer[DISPLAY_BUFFER_SIZE];
} Osc;

#define FLT_TYPE_X(X)             \
    X(FT_LPF, "Low-pass filter")  \
    X(FT_HPF, "High-pass filter") \
    X(FT_BPF, "Band-pass filter") \

typedef enum FilterType {
    FLT_TYPE_X(X_ENUM)
    FT_MAX,
} FilterType;

const char* ft2str(FilterType ft) {
    switch (ft) {
        FLT_TYPE_X(X_STR_CASE)
        default:
            break;
    }
    return "Unknown";
}

typedef struct FilterParams {
    pthread_rwlock_t rw;

    FilterType type;
    float cutoff;
    float resonance;
    float gain;

    // Parameters calculated when any of the arguments are changed
    double norm;
    double a[3]; // Poles
    double b[3]; // Zeros
} FilterParams;

typedef struct Filter {
    // RW protected
    FilterParams params;

    // Filter state
    double x[3];
    double y[3];

    // Filter output
    float disp_buffer[DISPLAY_BUFFER_SIZE];
    float buffer[BUFFER_SIZE];
} Filter;

#define TAB_X(X)                \
    X(TAB_SYNTH, "Synth")       \
    X(TAB_OSC,   "Oscillators") \
    X(TAB_ENV,   "Envelopes")   \
    X(TAB_FLT,   "Filters")     \
    X(TAB_KEYS,  "Keys")        \

typedef enum Tab {
    TAB_X(X_ENUM)
} Tab;

typedef struct SynthParams {
    pthread_rwlock_t rw;
    float amp;
    float pan;
} SynthParams;

typedef struct Synth {
    int screen_w;
    int screen_h;

    Tab cur_tab;

    int cur_octave;
    Key key_arr[KEY_COUNT];

    float buffer_fft[FFT_BUFFER_SIZE];
    float buffer_fft_r[FFT_BUFFER_SIZE];
    float buffer_fft_i[FFT_BUFFER_SIZE];
    float buffer_fft_w[FFT_BUFFER_SIZE];
    float buffer[BUFFER_SIZE];
    AudioStream stream;

    // Parameters
    SynthParams params;
    Osc osc_arr[OSC_COUNT];
    Env env_arr[ENV_COUNT];
    Filter flt;
    VoiceArr voice_arr;
} Synth;

Synth g_s;

float get_env_value(float time, bool released, float release_time, float last_env, Env *env) {
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

bool key_is_black(int k) {
    static const int black_idx_arr[] = {
        1, 3, 6, 8, 10
    };

    k -= KEY_C_OFF;
    k %= KEY_OCTAVE;

    for (int i = 0; i < ARRAY_SIZE(black_idx_arr); i++) {
        if (k == black_idx_arr[i]) {
            return true;
        }
    }

    return false;
}

float get_osc_kernel(OscType type, int wave_idx, int wave_length) {
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

void prepare_key_pos() {
    const float k_w = KEY_WIDTH;
    const float k_ws = GUI_GAP;
    const float k_h = KEY_HEIGHT;
    const float k_hs = GUI_GAP;
    float k_x = k_ws;

    const float black_y = g_s.screen_h - 2 * (k_hs + k_h);
    const float white_y = g_s.screen_h - 1 * (k_hs + k_h);

    for (int i = 0; i < ARRAY_SIZE(g_s.key_arr); i++) {
        g_s.key_arr[i].pos.x = 0;
        g_s.key_arr[i].pos.y = 0;
        g_s.key_arr[i].size.x = 0;
        g_s.key_arr[i].size.y = 0;
    }

    for (int i = g_s.cur_octave * KEY_OCTAVE + KEY_C_OFF; i < g_s.cur_octave * KEY_OCTAVE + KEY_C_OFF + 2 * KEY_OCTAVE; i++) {
        g_s.key_arr[i].size.x = k_w;
        g_s.key_arr[i].size.y = k_h;

        if (key_is_black(i)) {
            g_s.key_arr[i].pos.x = k_x - (k_w + k_ws) / 2;
            g_s.key_arr[i].pos.y = black_y;
        } else {
            g_s.key_arr[i].pos.x = k_x;
            g_s.key_arr[i].pos.y = white_y;

            k_x += k_w;
            k_x += k_ws;
        }
    }
}

void prepare_keys() {
    prepare_key_pos();

    for (int i = 0; i < ARRAY_SIZE(g_s.key_arr); i++) {
        g_s.key_arr[i].freq = powf(2.0f, (float)(i - KEY_A4_IDX) / (float)KEY_OCTAVE) * KEY_A4_FREQ;
    }
}


void audio_callback(void *_buffer, unsigned int frames);

void prepare_audio() {
    InitAudioDevice();

    // Set the number of samples the stream will keep in memory at a time to BUFFER_SIZE
    SetAudioStreamBufferSizeDefault(BUFFER_SIZE);
    // Init raw audio stream (sample rate: 44100, sample size: 32bit-float, channels: 1-mono)
    g_s.stream = LoadAudioStream(SAMPLE_RATE, 32, 1);
    SetAudioStreamPan(g_s.stream, g_s.params.pan);
    PlayAudioStream(g_s.stream);
    SetAudioStreamCallback(g_s.stream, audio_callback);
}

void prepare_osc_display_buffer(Osc* osc) {
    int e = pthread_rwlock_rdlock(&osc->params.rw);
    if (e != 0) {
        // TODO: Log
        return;
    }

    int wave_length = ARRAY_SIZE(osc->disp_buffer);
    for (int i = 0; i < wave_length; i++) {
        osc->disp_buffer[i] = get_osc_kernel(osc->params.type, i, wave_length / 2);
    }

    e = pthread_rwlock_unlock(&osc->params.rw);
    if (e != 0) {
        // TODO: Log
        return;
    }
}

void prepare_filter_params() {
    Filter* flt = &g_s.flt;
    FilterParams* params = &flt->params;

    int e = pthread_rwlock_wrlock(&params->rw);
    if (e != 0) {
        // TODO: Log
        return;
    }

    // double V = powf(10, fabs(params->gain) / 20);
    double K = tan(PI * params->cutoff / SAMPLE_RATE);

    // for (size_t i = 0; i < ARRAY_SIZE(flt->x); i++) {
    //     flt->x[i] = 0;
    //     flt->y[i] = 0;
    // }

    params->a[0] = 1;
    switch (params->type) {
        case FT_LPF:
            params->norm = 1 / (1 + K / params->resonance + K * K);
            params->b[0] = K * K * params->norm;
            params->b[1] = 2 * params->b[0];
            params->b[2] = params->b[0];
            params->a[1] = 2 * (K * K - 1) * params->norm;
            params->a[2] = (1 - K / params->resonance + K * K) * params->norm;
            break;

        case FT_HPF:
            params->norm = 1 / (1 + K / params->resonance + K * K);
            params->b[0] = 1 * params->norm;
            params->b[1] = -2 * params->b[0];
            params->b[2] = params->b[0];
            params->a[1] = 2 * (K * K - 1) * params->norm;
            params->a[2] = (1 - K / params->resonance + K * K) * params->norm;
            break;

        case FT_BPF:
            params->norm = 1 / (1 + K / params->resonance + K * K);
            params->b[0] = K / params->resonance * params->norm;
            params->b[1] = 0;
            params->b[2] = -params->b[0];
            params->a[1] = 2 * (K * K - 1) * params->norm;
            params->a[2] = (1 - K / params->resonance + K * K) * params->norm;
            break;

        default:
            break;
    }

    e = pthread_rwlock_unlock(&params->rw);
    if (e != 0) {
        // TODO: Log
        return;
    }
}

void prepare_filter_display() {
    Filter* flt = &g_s.flt;
    FilterParams* params = &flt->params;

    int e = pthread_rwlock_rdlock(&params->rw);
    if (e != 0) {
        // TODO: Log
        return;
    }

    for (size_t i = 0; i < DISPLAY_BUFFER_SIZE; i++) {
        // Current frequency in radians (0 to PI)
        double w = PI * (double)i / DISPLAY_BUFFER_SIZE;

        double complex z1 = cexp(-I * w);
        double complex z2 = cexp(-I * 2.0 * w);

        // H(z) = (b0 + b1*z^-1 + b2*z^-2) / (a0 + a1*z^-1 + a2*z^-2)
        double complex num = params->a[0] + params->a[1] * z1 + params->a[2] * z2;
        double complex den = params->b[0] + params->b[1] * z1 + params->b[2] * z2;

        double nMag = cabs(num);
        double dMag = cabs(den);

        if (dMag < EPSILON) {
            flt->disp_buffer[i] = -1.0f;
        } else {
            double magnitude = nMag / dMag;

            flt->disp_buffer[i] = Clamp(-log10f(magnitude), -1, 1);
        }
    }

    e = pthread_rwlock_unlock(&params->rw);
    if (e != 0) {
        // TODO: Log
        return;
    }
}

void prepare_filter() {
    prepare_filter_params();
    prepare_filter_display();
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

void init_osc(Osc* osc) {
    int e = pthread_rwlock_init(&osc->params.rw, NULL);
    if (e != 0) {
        return;
    }
    osc_voice_arr_init(&osc->voice_arr);
    osc->params.type = OT_SINE;
    osc->params.semi = 0;
    osc->params.volume = 1.0f;
    prepare_osc_display_buffer(osc);
}

void init_synth() {
    int e = 0;
    g_s.screen_w = GetScreenWidth();
    g_s.screen_h = GetScreenHeight();

    g_s.cur_tab = TAB_KEYS;
    g_s.cur_octave = 4;

    e = pthread_rwlock_init(&g_s.params.rw, NULL);
    if (e != 0) {
        return;
    }
    voice_arr_init(&g_s.voice_arr);
    g_s.params.amp = 0.2;
    g_s.params.pan = 0.5f;
    for (size_t i = 0; i < ARRAY_SIZE(g_s.buffer_fft_w); i++) {
        g_s.buffer_fft_w[i] = 0.5f * (1 - cos(2 * PI * i / (float)(BUFFER_SIZE - 1)));
    }

    for (size_t i = 0; i < ARRAY_SIZE(g_s.env_arr); i++) {
        init_env(&g_s.env_arr[i]);
    }

    for (size_t i = 0; i < ARRAY_SIZE(g_s.osc_arr); i++) {
        init_osc(&g_s.osc_arr[i]);
    }

    e = pthread_rwlock_init(&g_s.flt.params.rw, NULL);
    if (e != 0) {
        return;
    }
    g_s.flt.params.type = FT_LPF;
    g_s.flt.params.cutoff = FLT_CUTOFF_MAX;
    g_s.flt.params.resonance = FLT_RESONANCE_MIN;
    g_s.flt.params.gain = 0.0f;
    prepare_filter();


    prepare_keys();
    prepare_audio();
}

void deinit_env(Env* env) {
    int e = pthread_rwlock_destroy(&env->rw);
}

void deinit_osc(Osc* osc) {
    int e = pthread_rwlock_destroy(&osc->params.rw);
    osc_voice_arr_deinit(&osc->voice_arr);
}

void deinit_synth() {
    int e = 0;
    // TODO: Log
    e = pthread_rwlock_destroy(&g_s.flt.params.rw);
    e = pthread_rwlock_destroy(&g_s.params.rw);

    voice_arr_deinit(&g_s.voice_arr);

    for (size_t i = 0; i < ARRAY_SIZE(g_s.env_arr); i++) {
        deinit_env(&g_s.env_arr[i]);
    }

    for (size_t i = 0; i < ARRAY_SIZE(g_s.osc_arr); i++) {
        deinit_osc(&g_s.osc_arr[i]);
    }

    UnloadAudioStream(g_s.stream);
    CloseAudioDevice();
}

void process_screen() {
    g_s.screen_w = GetScreenWidth();
    g_s.screen_h = GetScreenHeight();
}

bool point_rect_intersection(Vector2 p, Vector2 rp, Vector2 rs) {
    return p.x >= rp.x && p.x <= rp.x + rs.x && p.y >= rp.y && p.y <= rp.y + rs.y;
}

void osc_arr_add_voice(Voice new_voice) {
    for (size_t i = 0; i < ARRAY_SIZE(g_s.osc_arr); i++) {
        osc_voice_add(&g_s.osc_arr[i].voice_arr, new_voice, GetTime());
    }
}

void osc_arr_release_voice(int k_idx) {
    for (size_t i = 0; i < ARRAY_SIZE(g_s.osc_arr); i++) {
        osc_voice_release(&g_s.osc_arr[i].voice_arr, k_idx, GetTime());
    }
}

void osc_arr_gc() {
    for (size_t i = 0; i < ARRAY_SIZE(g_s.osc_arr); i++) {
        osc_voice_gc(&g_s.osc_arr[i].voice_arr);
    }
}

void process_keys() {
    static Vector2 touch_pos[MAX_TOUCH_POINTS] = { 0 };
    VoiceArr *va = &g_s.voice_arr;
    int t_count = GetTouchPointCount();

    // Clamp touch points available ( set the maximum touch points allowed )
    if (t_count > MAX_TOUCH_POINTS) {
        t_count = MAX_TOUCH_POINTS;
    }

    // Get touch points positions
    for (int i = 0; i < t_count; i++) {
        touch_pos[i] = GetTouchPosition(i);
    }

    for (int i = 0; i < t_count; i++) {
        for (int k = g_s.cur_octave * KEY_OCTAVE + KEY_C_OFF; k < g_s.cur_octave * KEY_OCTAVE + KEY_C_OFF + 2 * KEY_OCTAVE; k++) {
            Key key = g_s.key_arr[k];
            if (point_rect_intersection(touch_pos[i], key.pos, key.size)) {
                Voice new_voice = {0};
                bool hold = voice_hold_key(va, k, &new_voice);
                if (!hold) {
                    osc_arr_add_voice(new_voice);
                }
                break; // Found a key for this touch. See next touch.
            }
        }
    }

    // Release unheld keys
    pthread_rwlock_rdlock(&va->rw);

    for (int v = 0; v < va->voice_count; v++) {
        int k = va->voice_arr[v].key_idx;
        Key key = g_s.key_arr[k];
        bool found = false;
        for (int i = 0; i < t_count; i++) {
            if (point_rect_intersection(touch_pos[i], key.pos, key.size)) {
                found = true;
                break; // Found a touch for the held key.
            }
        }

        if (!found) {
            pthread_rwlock_unlock(&va->rw);

            voice_remove(&g_s.voice_arr, v);

            pthread_rwlock_rdlock(&va->rw);

            osc_arr_release_voice(k);
        }
    }

    pthread_rwlock_unlock(&va->rw);

    osc_arr_gc();
}

void update_osc(Osc* osc, Env* env) {
    OscVoiceArr *ova = &osc->voice_arr;

    for (int j = 0; j < BUFFER_SIZE; j++) {
        osc->buffer[j] = 0;
    }

    int e = pthread_rwlock_rdlock(&osc->params.rw);
    if (e != 0) {
        // TODO: Log
        return;
    }

    e = pthread_rwlock_rdlock(&ova->rw);
    if (e != 0) {
        // TODO: Log
        return;
    }

    for (int v = 0; v < ova->osc_voice_count; v++) {
        OscVoice *osc_voice = &ova->osc_voice_arr[v];
        Voice voice = osc_voice->voice;

        int key_idx = voice.key_idx + osc->params.semi;
        if (key_idx < 0) {
            key_idx = 0;
        } else if (key_idx > KEY_COUNT) {
            key_idx = KEY_COUNT;
        }

        float wave_freq = g_s.key_arr[key_idx].freq;
        float vel_mul = voice.velocity / MAX_VELOCITY;
        float time = GetTime() - osc_voice->start_time;
        float release_time = osc_voice->release_time - osc_voice->start_time;

        for (int j = 0; j < BUFFER_SIZE; j++) {
            float wave_length = SAMPLE_RATE / wave_freq;
            float dt = j / (float)SAMPLE_RATE;
            float kernel = get_osc_kernel(osc->params.type, osc_voice->wave_idx, wave_length);

            osc_voice->env = get_env_value(
                    time + dt, osc_voice->released,
                    release_time, osc_voice->env, env);

            osc->buffer[j] += osc->params.volume * osc_voice->env * vel_mul * kernel;
            osc_voice->wave_idx++;
            if (osc_voice->wave_idx >= wave_length) {
                osc_voice->wave_idx = 0;
            }
        }
    }

    e = pthread_rwlock_unlock(&osc->params.rw);
    if (e != 0) {
        // TODO: Log
        return;
    }

    e = pthread_rwlock_unlock(&ova->rw);
    if (e != 0) {
        // TODO: Log
        return;
    }
}

void update_filter() {
    Filter* flt = &g_s.flt;

    for (size_t i = 0; i < ARRAY_SIZE(flt->buffer); i++) {
        flt->buffer[i] = 0.0f;
        for (size_t j = 0; j < ARRAY_SIZE(g_s.osc_arr); j++) {
            flt->buffer[i] += g_s.osc_arr[j].buffer[i];
        }
    }

    int e = pthread_rwlock_rdlock(&flt->params.rw);
    if (e != 0) {
        // TODO: Log
        return;
    }

    // Converts the buffer data before using it
    for (size_t i = 0; i < ARRAY_SIZE(flt->buffer); i++) {
        // Move old state
        flt->y[2] = flt->y[1];
        flt->y[1] = flt->y[0];
        flt->x[2] = flt->x[1];
        flt->x[1] = flt->x[0];

        flt->x[0] = flt->buffer[i];
        flt->y[0] =
            flt->params.a[0] * flt->x[0] +
            flt->params.a[1] * flt->x[1] +
            flt->params.a[2] * flt->x[2] -
            flt->params.b[1] * flt->y[1] -
            flt->params.b[2] * flt->y[2] +
            EPSILON; // Anti-denormal offset

        flt->buffer[i] = flt->y[0];
    }

    e = pthread_rwlock_unlock(&flt->params.rw);
    if (e != 0) {
        // TODO: Log
        return;
    }
}

void update_stream() {
    int e = pthread_rwlock_rdlock(&g_s.params.rw);
    if (e != 0) {
        // TODO: Log
        return;
    }

    for (int i = 0; i < BUFFER_SIZE; i++) {
        g_s.buffer[i] = g_s.params.amp * g_s.flt.buffer[i];
    }

    for (int i = 0; i < FFT_BUFFER_SIZE - BUFFER_SIZE; i++) {
        g_s.buffer_fft[i] = g_s.buffer_fft[FFT_BUFFER_SIZE - 1 - i];
    }

    for (int i = 0; i < BUFFER_SIZE; i++) {
        g_s.buffer_fft[i + (FFT_BUFFER_SIZE - 1 - BUFFER_SIZE)] = g_s.buffer[i];
    }

    e = pthread_rwlock_unlock(&g_s.params.rw);
    if (e != 0) {
        // TODO: Log
        return;
    }
}

void update_fft() {
    for (size_t i = 0; i < FFT_BUFFER_SIZE; i++) {
        g_s.buffer_fft_r[i] = g_s.buffer_fft[i] * g_s.buffer_fft_w[i];
        g_s.buffer_fft_i[i] = 0.0f;
    }

    fft(g_s.buffer_fft_r, g_s.buffer_fft_i, FFT_BUFFER_SIZE);

    for (size_t i = 0; i < FFT_BUFFER_SIZE / 2; i++) {
        g_s.buffer_fft_r[i] = sqrt(powf(g_s.buffer_fft_r[i], 2) + powf(g_s.buffer_fft_i[i], 2)) / (BUFFER_SIZE / 2.0);
    }
}

void audio_callback(void *_buffer, unsigned int frames) {
    static int buffer_idx = BUFFER_SIZE;

    float *out_buffer = (float *)_buffer;
    for (size_t i = 0; i < frames; i++) {
        if (buffer_idx >= BUFFER_SIZE) {
            buffer_idx = 0;

            update_osc(&g_s.osc_arr[0], &g_s.env_arr[0]);
            update_osc(&g_s.osc_arr[1], &g_s.env_arr[1]);
            update_filter();
            update_stream();
        }

        out_buffer[i] = g_s.buffer[buffer_idx++];
    }
}

void draw_voice_arr() {
    // pthread_rwlock_rdlock(&g_s.voice_arr.rw);

    // for (int i = 0; i < g_s.voice_arr.voice_count; i++) {
    //     int key_idx = g_s.voice_arr.voice_arr[i].key_idx;
    //     float wave_freq = g_s.key_arr[key_idx].freq;
    //     DrawText(
    //             TextFormat("sine frequency: %.2f, key idx: %d", wave_freq, key_idx),
    //             GUI_GAP, GUI_GAP + FONT_SIZE * i,
    //             FONT_SIZE, RED);
    // }

    // pthread_rwlock_unlock(&g_s.voice_arr.rw);

    // pthread_rwlock_rdlock(&g_s.osc.voice_arr.rw);

    // for (int i = 0; i < g_s.osc.voice_arr.osc_voice_count; i++) {
    //     int k_idx = g_s.osc.voice_arr.osc_voice_arr[i].voice.key_idx;
    //     float start_time = g_s.osc.voice_arr.osc_voice_arr[i].start_time;
    //     float release_time = g_s.osc.voice_arr.osc_voice_arr[i].release_time;
    //     float env = g_s.osc.voice_arr.osc_voice_arr[i].env;
    //     DrawText(
    //             TextFormat(
    //                 "key: %d, t: %.2f, r: %.2f, env: %.2f",
    //                 k_idx, GetTime() - start_time, release_time - start_time, env),
    //             g_s.screen_w / 2 + GUI_GAP, GUI_GAP + FONT_SIZE * i,
    //             FONT_SIZE, RED);
    // }

    // pthread_rwlock_unlock(&g_s.osc.voice_arr.rw);
}

void draw_keys() {
    for (int i = g_s.cur_octave * KEY_OCTAVE + KEY_C_OFF; i < g_s.cur_octave * KEY_OCTAVE + KEY_C_OFF + 2 * KEY_OCTAVE; i++) {
        Rectangle r;
        r.x = g_s.key_arr[i].pos.x;
        r.y = g_s.key_arr[i].pos.y;
        r.width = g_s.key_arr[i].size.x;
        r.height = g_s.key_arr[i].size.y;

        Color k_color = KEY_WHITE_COLOR;
        Color t_color = BLACK;
        if (key_is_black(i)) {
            k_color = KEY_BLACK_COLOR;
            t_color = WHITE;
        }

        DrawRectangleRec(r, k_color);
        DrawRectangleLinesEx(r, 5, KEY_OUTER_COLOR);
        DrawText(
                TextFormat("%d", i),
                g_s.key_arr[i].pos.x + 5,
                g_s.key_arr[i].pos.y + g_s.key_arr[i].size.y / 2,
                FONT_SIZE, t_color);
    }
}

void draw_fps() {
    DrawText(
            TextFormat("FPS: %d", GetFPS()),
            g_s.screen_w - 100, 10,
            FONT_SIZE, GREEN);
}

void draw_tab_synth() {
    Vector2 cursor_p = {GUI_GAP, TAB_H + 2 * GUI_GAP};

    SetDir(GD_HORIZONTAL);
    DrawKnob("Amp", &cursor_p, KNOB_RADIUS, &g_s.params.amp, 0.0f, 1.0f, &g_s.params.rw);
    DrawKnob("Pan", &cursor_p, KNOB_RADIUS, &g_s.params.pan, 0.0f, 1.0f, &g_s.params.rw);
}

void draw_tab_osc() {
    Vector2 wave_s = {(g_s.screen_w - 2 * GUI_GAP) / 2.0f - GUI_GAP, WAVE_SIZE_H};

    for (size_t i = 0; i < ARRAY_SIZE(g_s.osc_arr); i++) {
        Vector2 cursor_p = {GUI_GAP + i * (wave_s.x + GUI_GAP), TAB_H + 2 * GUI_GAP};
        Osc *osc = &g_s.osc_arr[i];
        OscParams *params = &osc->params;
        int e = 0;

        SetDir(GD_VERTICAL);
        if (DrawWave(&cursor_p, wave_s, osc->disp_buffer, ARRAY_SIZE(osc->disp_buffer))) {
            e = pthread_rwlock_wrlock(&params->rw);
            if (e != 0) {
                // TODO: Log
                return;
            }

            params->type++;
            params->type %= OT_MAX;

            e = pthread_rwlock_unlock(&params->rw);
            if (e != 0) {
                // TODO: Log
                return;
            }

            prepare_osc_display_buffer(osc);
        }

        SetDir(GD_HORIZONTAL);
        DrawKnobI("Semitones", &cursor_p, KNOB_RADIUS, &params->semi, -OSC_SEMI_RANGE, OSC_SEMI_RANGE, &params->rw);
        DrawKnob("Volume", &cursor_p, KNOB_RADIUS, &params->volume, 0.0f, 1.0f, &params->rw);
    }
}

void draw_tab_env() {
    Vector2 wave_s = {(g_s.screen_w - 2 * GUI_GAP) / 2.0f - GUI_GAP, WAVE_SIZE_H};

    for (size_t i = 0; i < ARRAY_SIZE(g_s.env_arr); i++) {
        Env *env = &g_s.env_arr[i];
        Vector2 cursor_p = {GUI_GAP + i * (wave_s.x + GUI_GAP), TAB_H + 2 * GUI_GAP};

        SetDir(GD_HORIZONTAL);
        // TODO: Env wave
        DrawKnob("Attack", &cursor_p, KNOB_RADIUS, &env->attack, ENV_A_MIN, ENV_A_MAX, &env->rw);
        DrawKnob("Decay", &cursor_p, KNOB_RADIUS, &env->decay, ENV_D_MIN, ENV_D_MAX, &env->rw);
        DrawKnob("Sustain", &cursor_p, KNOB_RADIUS, &env->sustain, 0.0f, 1.0f, &env->rw);
        DrawKnob("Release", &cursor_p, KNOB_RADIUS, &env->release, ENV_R_MIN, ENV_R_MAX, &env->rw);
    }
}

void draw_tab_filter() {
    bool changed_type = false;
    bool changed = false;
    FilterParams *params = &g_s.flt.params;
    Vector2 cursor_p = {GUI_GAP, TAB_H + 2 * GUI_GAP};
    Vector2 wave_s = {g_s.screen_w - GUI_GAP, WAVE_SIZE_H};
    Vector2 slider_s = {wave_s.x, SLIDER_SIZE_H};

    SetDir(GD_VERTICAL);
    changed_type = DrawWave(&cursor_p, wave_s, g_s.flt.disp_buffer, DISPLAY_BUFFER_SIZE);
    if (changed_type) {
        changed |= true;

        int e = pthread_rwlock_wrlock(&params->rw);
        if (e != 0) {
            // TODO: Log
            return;
        }

        params->type++;
        params->type %= FT_MAX;

        e = pthread_rwlock_unlock(&params->rw);
        if (e != 0) {
            // TODO: Log
            return;
        }
    }

    // DrawText(
    //         TextFormat("Click to change type. Current type: %s", ft2str(params->type)),
    //         cursor_p.x, cursor_p.y, FONT_SIZE, TEXT_COLOR);
    // cursor_p.y += FONT_SIZE + GUI_GAP;

    changed |= DrawSlider(&cursor_p, slider_s, &params->cutoff, FLT_CUTOFF_MIN, FLT_CUTOFF_MAX, &params->rw);

    SetDir(GD_HORIZONTAL);
    changed |= DrawKnob("Resonance", &cursor_p, KNOB_RADIUS, &params->resonance, FLT_RESONANCE_MIN, FLT_RESONANCE_MAX, &params->rw);
    changed |= DrawKnob("Gain", &cursor_p, KNOB_RADIUS, &params->gain, FLT_GAIN_MIN, FLT_GAIN_MAX, &params->rw);

    // Right now this is a bit dumb, IMO there should be a copied struct.
    if (changed) {
        prepare_filter();
    }
}

void draw_tab_keys() {
    Vector2 cursor_p = {GUI_GAP, TAB_H + 2 * GUI_GAP};
    Vector2 wave_s = {g_s.screen_w - GUI_GAP, WAVE_SIZE_H};

    DrawWave(&cursor_p, wave_s, g_s.buffer_fft_r, ARRAY_SIZE(g_s.buffer_fft_r) / 2);
    draw_keys();

    cursor_p.x = g_s.screen_w - GUI_GAP - KNOB_SIZE_W;
    cursor_p.y = g_s.screen_h - GUI_GAP - KNOB_SIZE_H;
    if (DrawKnobI("Octave", &cursor_p, KNOB_RADIUS, &g_s.cur_octave, 0, OCTAVE_COUNT - 2, NULL)) {
        prepare_keys();
    }

    // TODO: move this to debug only.
    draw_voice_arr();
}

void process_ui() {
    process_screen();

    switch (g_s.cur_tab) {
    case TAB_SYNTH:
        break;
    case TAB_OSC:
        break;
    case TAB_ENV:
        break;
    case TAB_FLT:
        break;
    case TAB_KEYS:
        process_keys();
        break;
    }
}

void draw_ui() {
    Rectangle tab_r = {GUI_GAP, GUI_GAP, g_s.screen_w - 2 * GUI_GAP, TAB_H};
    const char* tab_l[] = {
        TAB_X(X_STR_ARR)
    };

    DrawTabMenu(tab_r, tab_l, ARRAY_SIZE(tab_l), (int*)&g_s.cur_tab);
    switch (g_s.cur_tab) {
    case TAB_SYNTH:
        draw_tab_synth();
        break;
    case TAB_OSC:
        draw_tab_osc();
        break;
    case TAB_ENV:
        draw_tab_env();
        break;
    case TAB_FLT:
        draw_tab_filter();
        break;
    case TAB_KEYS:
        draw_tab_keys();
        break;
    }
}

int main(void) {
    InitWindow(WIN_SIZE_W, WIN_SIZE_H, "Synth");
    SetTargetFPS(60);

    init_synth();

    while (!WindowShouldClose())
    {
        // Process
        process_ui();

        // Update
        update_fft();

        // Draw
        BeginDrawing();
            ClearBackground(BG_COLOR);

            draw_ui();
            draw_fps();
        EndDrawing();
    }

    deinit_synth();

    CloseWindow();

    return 0;
}
