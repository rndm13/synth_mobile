#include <errno.h>
#include <ftw.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <stddef.h>
#include <complex.h>
#include <pthread.h>
#include <string.h>
#include <time.h>

#include "raylib.h"
#include "raymath.h"

#include "ini.h"

#include "utils.h"
#include "settings.h"
#include "gui_elements.h"
#include "voice.h"
#include "fft.h"
#include "env.h"
#include "osc.h"
#include "filter.h"


typedef struct Key {
    Vector2 pos;
    Vector2 size;

    float freq;
} Key;

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

typedef struct SynthProfiling {
    timespec_t osc_time[OSC_COUNT];
    timespec_t flt_time;
    timespec_t total_time;
} SynthProfiling;

typedef struct ProgramSelection {
    char   filepath_arr[PROGRAM_COUNT_MAX][FILEPATH_CAPACITY];
    char   program_name_arr[PROGRAM_COUNT_MAX][PROGRAM_NAME_CAPACITY];
    size_t program_count;
    int    selected_program_idx;
} ProgramSelection;

typedef struct Synth {
    int screen_w;
    int screen_h;

    Tab cur_tab;

    int cur_octave;
    Key key_arr[KEY_COUNT];

    size_t buffer_fft_idx;
    float buffer_fft[FFT_BUFFER_SIZE];
    float buffer_fft_r[FFT_BUFFER_SIZE];
    float buffer_fft_i[FFT_BUFFER_SIZE];
    float buffer_fft_w[FFT_BUFFER_SIZE];
    AudioStream stream;

    bool show_fft;

    bool show_debug;
    bool show_prof;
    SynthProfiling prof;

    ProgramSelection program_selection;

    // Parameters
    char program_name[PROGRAM_NAME_CAPACITY];
    SynthParams params;
    Osc osc_arr[OSC_COUNT];
    Env env_arr[ENV_COUNT];
    Filter flt;
    VoiceArr voice_arr;
} Synth;

Synth g_s;

int add_program_entry(
        const char *filepath, const struct stat *info,
        const int typeflag, struct FTW *pathinfo) {
    const char* filename = filepath + pathinfo->base;

    // const char* extension_substr = strstr(filename, PROGRAM_EXTENSION);
    // if (extension_substr == NULL) {
    //     return 0;
    // }
    //
    // size_t program_len = MIN(extension_substr - filename, PROGRAM_NAME_CAPACITY);

    snprintf(g_s.program_selection.filepath_arr[g_s.program_selection.program_count], FILEPATH_CAPACITY, "%s", filepath);
    snprintf(g_s.program_selection.program_name_arr[g_s.program_selection.program_count], PROGRAM_NAME_CAPACITY, "%s", filename);

    g_s.program_selection.program_count++;

    return 0;
}

void init_program_selection() {
    int e = 0;

    g_s.program_selection.program_count = 0;
    g_s.program_selection.selected_program_idx = 0;

    e = nftw(".", add_program_entry, 10, 0);
    if (e != 0) {
        add_toast("Failed to search for existing programs: %s", strerror(errno));
    } else {
        add_toast("Successfully loaded existing programs");
    }
}

void add_program() {
    char filepath[FILEPATH_CAPACITY] = {0};

    snprintf(
        filepath, FILEPATH_CAPACITY,
        "%s/%s%s", PROGRAM_PATH, g_s.program_name, PROGRAM_EXTENSION);

    snprintf(
        g_s.program_selection.filepath_arr[g_s.program_selection.program_count],
        FILEPATH_CAPACITY, "%s", filepath);
    snprintf(
        g_s.program_selection.program_name_arr[g_s.program_selection.program_count],
        PROGRAM_NAME_CAPACITY, "%s", g_s.program_name);

    g_s.program_selection.program_count++;
}

static int write_ini_value_s(FILE* file, const char* value, const char* section, const char* name) {
    int e = 0;

    e = fprintf(file, "[%s]\n%s = %s\n", section, name, value);
    if (e < 0) {
        return errno;
    }

    return 0;
}

static int write_ini_value_i(FILE* file, int value, const char* section, const char* name) {
    int e = 0;

    e = fprintf(file, "[%s]\n%s = %d\n", section, name, value);
    if (e < 0) {
        return errno;
    }

    return 0;
}

static int write_ini_value_f(FILE* file, float value, const char* section, const char* name) {
    int e = 0;

    e = fprintf(file, "[%s]\n%s = %f\n", section, name, value);
    if (e < 0) {
        return errno;
    }

    return 0;
}

static int write_ini_file() {
    char cur_section[INI_SECTION_CAPACITY] = {};
    char filepath[FILEPATH_CAPACITY] = {};
    int e = 0;
    FILE* file = NULL;
    OscParams *oparams = NULL;
    Env *eparams = NULL;
    FilterParams *fparams = NULL;
    SynthParams *sparams = NULL;

    snprintf(
            filepath, FILEPATH_CAPACITY, "%s.ini",
            g_s.program_selection.program_name_arr[g_s.program_selection.selected_program_idx]);

    file = fopen(filepath, "w");

    if (NULL == file) {
        return errno;
    }

    for (size_t i = 0; i < ARRAY_SIZE(g_s.osc_arr); i++) {
        snprintf(cur_section, INI_SECTION_CAPACITY, "%s%zu", "osc", i);

        oparams = &g_s.osc_arr[i].params;

        e = write_ini_value_i(file, oparams->cents, cur_section, "cents");
        if (e != 0) {
            goto close_file;
        }

        e = write_ini_value_i(file, oparams->detune, cur_section, "detune");
        if (e != 0) {
            goto close_file;
        }

        e = write_ini_value_i(file, oparams->unison, cur_section, "unison");
        if (e != 0) {
            goto close_file;
        }

        e = write_ini_value_i(file, oparams->semi, cur_section, "semi");
        if (e != 0) {
            goto close_file;
        }

        e = write_ini_value_f(file, oparams->volume, cur_section, "volume");
        if (e != 0) {
            goto close_file;
        }

        e = write_ini_value_i(file, oparams->type, cur_section, "type");
        if (e != 0) {
            goto close_file;
        }
    }

    for (size_t i = 0; i < ARRAY_SIZE(g_s.env_arr); i++) {
        snprintf(cur_section, INI_SECTION_CAPACITY, "%s%zu", "env", i);

        eparams = &g_s.env_arr[i];

        e = write_ini_value_f(file, eparams->attack, cur_section, "attack");
        if (e != 0) {
            goto close_file;
        }

        e = write_ini_value_f(file, eparams->decay, cur_section, "decay");
        if (e != 0) {
            goto close_file;
        }

        e = write_ini_value_f(file, eparams->sustain, cur_section, "sustain");
        if (e != 0) {
            goto close_file;
        }

        e = write_ini_value_f(file, eparams->release, cur_section, "release");
        if (e != 0) {
            goto close_file;
        }
    }

    {
        snprintf(cur_section, INI_SECTION_CAPACITY, "%s%d", "flt", 0);

        fparams = &g_s.flt.params;

        e = write_ini_value_i(file, fparams->type, cur_section, "type");
        if (e != 0) {
            goto close_file;
        }

        e = write_ini_value_f(file, fparams->cutoff, cur_section, "cutoff");
        if (e != 0) {
            goto close_file;
        }

        e = write_ini_value_f(file, fparams->gain, cur_section, "gain");
        if (e != 0) {
            goto close_file;
        }
    }

    {
        snprintf(cur_section, INI_SECTION_CAPACITY, "%s", "synth");

        sparams = &g_s.params;

        e = write_ini_value_f(file, sparams->amp, cur_section, "amp");
        if (e != 0) {
            goto close_file;
        }

        e = write_ini_value_f(file, sparams->pan, cur_section, "pan");
        if (e != 0) {
            goto close_file;
        }

        e = write_ini_value_s(file, g_s.program_name, cur_section, "name");
        if (e != 0) {
            goto close_file;
        }
    }

close_file:
    fclose(file);
    return e;
}

void save_program() {
    const char* current_program = NULL;
    int e = 0;
    bool found = false;

    for (size_t i = 0; i < g_s.program_selection.program_count; i++) {
        current_program = g_s.program_selection.program_name_arr[i];
        if (0 == strncmp(current_program, g_s.program_name, PROGRAM_NAME_CAPACITY)) {
            found = true;
            break;
        }
    }

    if (!found) {
        add_program();
    }

    pthread_rwlock_rdlock(&g_s.osc_arr[0].params.rw);
    pthread_rwlock_rdlock(&g_s.osc_arr[1].params.rw);
    pthread_rwlock_rdlock(&g_s.env_arr[0].rw);
    pthread_rwlock_rdlock(&g_s.env_arr[1].rw);
    pthread_rwlock_rdlock(&g_s.flt.params.rw);
    pthread_rwlock_rdlock(&g_s.params.rw);

    e = write_ini_file();
    if (e != 0) {
        add_toast("Failed writing to INI file: %s", strerror(errno));
    }

    pthread_rwlock_unlock(&g_s.osc_arr[0].params.rw);
    pthread_rwlock_unlock(&g_s.osc_arr[1].params.rw);
    pthread_rwlock_unlock(&g_s.env_arr[0].rw);
    pthread_rwlock_unlock(&g_s.env_arr[1].rw);
    pthread_rwlock_unlock(&g_s.flt.params.rw);
    pthread_rwlock_unlock(&g_s.params.rw);
}

#define MATCH_S(var, exp_section, exp_name, section, name, value)                       \
    do {                                                                                \
        if (strcmp((exp_section), (section)) == 0 && strcmp((exp_name), (name)) == 0) { \
            snprintf(var, sizeof(var), "%s", value);                                    \
        }                                                                               \
    } while (0);

#define MATCH_I(var, exp_section, exp_name, section, name, value)                       \
    do {                                                                                \
        if (strcmp((exp_section), (section)) == 0 && strcmp((exp_name), (name)) == 0) { \
            (var) = atoi(value);                                                        \
        }                                                                               \
    } while (0);

#define MATCH_F(var, exp_section, exp_name, section, name, value)                       \
    do {                                                                                \
        if (strcmp((exp_section), (section)) == 0 && strcmp((exp_name), (name)) == 0) { \
            (var) = atof(value);                                                        \
        }                                                                               \
    } while (0);

static int read_ini_value(
    void* user, const char* section, const char* name,
    const char* value) {
    char cur_section[INI_SECTION_CAPACITY] = {};

    for (size_t i = 0; i < ARRAY_SIZE(g_s.osc_arr); i++) {
        snprintf(cur_section, INI_SECTION_CAPACITY, "%s%zu", "osc", i);

        OscParams *params = &g_s.osc_arr[i].params;

        MATCH_I(params->cents, cur_section, "cents", section, name, value);
        MATCH_I(params->detune, cur_section, "detune", section, name, value);
        MATCH_I(params->unison, cur_section, "unison", section, name, value);
        MATCH_I(params->semi, cur_section, "semi", section, name, value);
        MATCH_F(params->volume, cur_section, "volume", section, name, value);
        MATCH_I(params->type, cur_section, "type", section, name, value);
        params->cents_mul = calc_cents_mul(params->cents);
    }

    for (size_t i = 0; i < ARRAY_SIZE(g_s.env_arr); i++) {
        snprintf(cur_section, INI_SECTION_CAPACITY, "%s%zu", "env", i);

        Env *params = &g_s.env_arr[i];

        MATCH_F(params->attack, cur_section, "attack", section, name, value);
        MATCH_F(params->decay, cur_section, "decay", section, name, value);
        MATCH_F(params->sustain, cur_section, "sustain", section, name, value);
        MATCH_F(params->release, cur_section, "release", section, name, value);
    }

    {
        snprintf(cur_section, INI_SECTION_CAPACITY, "%s%d", "flt", 0);

        FilterParams *params = &g_s.flt.params;

        MATCH_I(params->type, cur_section, "type", section, name, value);
        MATCH_F(params->cutoff, cur_section, "cutoff", section, name, value);
        MATCH_F(params->gain, cur_section, "gain", section, name, value);
    }

    {
        snprintf(cur_section, INI_SECTION_CAPACITY, "%s", "synth");

        SynthParams *params = &g_s.params;

        MATCH_F(params->amp, cur_section, "amp", section, name, value);
        MATCH_F(params->pan, cur_section, "pan", section, name, value);
        MATCH_S(g_s.program_name, cur_section, "name", section, name, value);
    }

    return 1;
}

#undef MATCH_I
#undef MATCH_F

void open_program() {
    int e = 0;
    pthread_rwlock_wrlock(&g_s.osc_arr[0].params.rw);
    pthread_rwlock_wrlock(&g_s.osc_arr[1].params.rw);
    pthread_rwlock_wrlock(&g_s.env_arr[0].rw);
    pthread_rwlock_wrlock(&g_s.env_arr[1].rw);
    pthread_rwlock_wrlock(&g_s.flt.params.rw);
    pthread_rwlock_wrlock(&g_s.params.rw);

    e = ini_parse(g_s.program_selection.filepath_arr[g_s.program_selection.selected_program_idx], read_ini_value, NULL);
    if (e != 0) {
        add_toast("Failed parsing INI file: %d", e);
    }

    prepare_osc_display_buffer(&g_s.osc_arr[0]);
    prepare_osc_display_buffer(&g_s.osc_arr[1]);
    prepare_filter(&g_s.flt);

    pthread_rwlock_unlock(&g_s.osc_arr[0].params.rw);
    pthread_rwlock_unlock(&g_s.osc_arr[1].params.rw);
    pthread_rwlock_unlock(&g_s.env_arr[0].rw);
    pthread_rwlock_unlock(&g_s.env_arr[1].rw);
    pthread_rwlock_unlock(&g_s.flt.params.rw);
    pthread_rwlock_unlock(&g_s.params.rw);

    add_toast("Successfully opened program");
}

void randomize_program() {
    srand(time(NULL));

#define RAND_RANGE(min, max) ((rand() % ((max) - (min))) + (min))
#define RAND_RANGEF(min, max) (RAND_RANGE((int)((min) * 100), (int)((max) * 100)) / 100.0f)

    for (size_t i = 0; i < ARRAY_SIZE(g_s.osc_arr); i++) {
        OscParams *params = &g_s.osc_arr[i].params;
        pthread_rwlock_wrlock(&params->rw);

        params->cents = RAND_RANGE(-OSC_CENTS_RANGE, OSC_CENTS_RANGE);
        params->detune = RAND_RANGE(OSC_DETUNE_MIN, OSC_DETUNE_MAX);
        params->unison = RAND_RANGE(OSC_UNISON_MIN, OSC_UNISON_MAX);
        params->semi = RAND_RANGE(-OSC_SEMI_RANGE, OSC_SEMI_RANGE);
        params->volume = RAND_RANGEF(0, 1);
        params->type = RAND_RANGE(0, OT_MAX);
        params->cents_mul = calc_cents_mul(params->cents);

        pthread_rwlock_unlock(&params->rw);

        prepare_osc_display_buffer(&g_s.osc_arr[i]);
    }

    for (size_t i = 0; i < ARRAY_SIZE(g_s.env_arr); i++) {
        Env *params = &g_s.env_arr[i];
        pthread_rwlock_wrlock(&params->rw);

        params->attack = RAND_RANGEF(ENV_A_MIN, ENV_A_MAX);
        params->decay = RAND_RANGEF(ENV_D_MIN, ENV_D_MAX);
        params->sustain = RAND_RANGEF(0, 1);
        params->release = RAND_RANGEF(ENV_R_MIN, ENV_R_MAX);

        pthread_rwlock_unlock(&params->rw);
    }

    {
        FilterParams *params = &g_s.flt.params;
        pthread_rwlock_wrlock(&params->rw);

        params->type = RAND_RANGE(0, FT_MAX);
        params->cutoff = RAND_RANGEF(FLT_CUTOFF_MIN, FLT_CUTOFF_MAX);
        params->gain = RAND_RANGEF(FLT_GAIN_MIN, FLT_GAIN_MAX);

        pthread_rwlock_unlock(&params->rw);

        prepare_filter(&g_s.flt);
    }

#undef RAND_RANGE
#undef RAND_RANGEF
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

void init_key_positions() {
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

void init_keys() {
    init_key_positions();

    for (int i = 0; i < ARRAY_SIZE(g_s.key_arr); i++) {
        g_s.key_arr[i].freq = powf(2.0f, (float)(i - KEY_A4_IDX) / (float)KEY_OCTAVE) * KEY_A4_FREQ;
    }
}

void audio_callback(void *_buffer, unsigned int frames);

void init_audio() {
    InitAudioDevice();

    // Set the number of samples the stream will keep in memory at a time to BUFFER_SIZE
    SetAudioStreamBufferSizeDefault(BUFFER_SIZE);
    // Init raw audio stream (sample rate: 44100, sample size: 32bit-float, channels: 1-mono)
    g_s.stream = LoadAudioStream(SAMPLE_RATE, 32, 1);
    SetAudioStreamPan(g_s.stream, g_s.params.pan);
    PlayAudioStream(g_s.stream);
    SetAudioStreamCallback(g_s.stream, audio_callback);
}

void init_synth() {
    int e = 0;
    g_s.screen_w = GetScreenWidth();
    g_s.screen_h = GetScreenHeight();

    g_s.cur_tab = TAB_KEYS;
    g_s.cur_octave = 4;

    g_s.buffer_fft_idx = 0;
    for (size_t i = 0; i < ARRAY_SIZE(g_s.buffer_fft_w); i++) {
        g_s.buffer_fft_w[i] = 0.5f * (1 - cos(2 * PI * i / (float)(FFT_BUFFER_SIZE - 1)));
    }

    e = pthread_rwlock_init(&g_s.params.rw, NULL);
    if (e != 0) {
        return;
    }
    strncpy(g_s.program_name, PROGRAM_NAME_INIT, PROGRAM_NAME_CAPACITY);
    g_s.params.amp = 0.2;
    g_s.params.pan = 0.5f;
    voice_arr_init(&g_s.voice_arr);

    for (size_t i = 0; i < ARRAY_SIZE(g_s.env_arr); i++) {
        init_env(&g_s.env_arr[i]);
    }

    for (size_t i = 0; i < ARRAY_SIZE(g_s.osc_arr); i++) {
        init_osc(&g_s.osc_arr[i]);
    }

    init_filter(&g_s.flt);

    init_program_selection();
    init_keys();
    init_audio();
}

void deinit_synth() {
    pthread_rwlock_destroy(&g_s.params.rw);

    voice_arr_deinit(&g_s.voice_arr);

    for (size_t i = 0; i < ARRAY_SIZE(g_s.env_arr); i++) {
        deinit_env(&g_s.env_arr[i]);
    }

    for (size_t i = 0; i < ARRAY_SIZE(g_s.osc_arr); i++) {
        deinit_osc(&g_s.osc_arr[i]);
    }

    deinit_filter(&g_s.flt);

    UnloadAudioStream(g_s.stream);
    CloseAudioDevice();
}

void process_screen() {
    g_s.screen_w = GetScreenWidth();
    g_s.screen_h = GetScreenHeight();
}

void osc_arr_add_voice(Voice new_voice) {
    for (size_t i = 0; i < ARRAY_SIZE(g_s.osc_arr); i++) {
        osc_add_voice(&g_s.osc_arr[i], new_voice, GetTime());
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
            Rectangle key_rec = {
                key.pos.x, key.pos.y,
                key.size.x, key.size.y,
            };

            if (CheckCollisionPointRec(touch_pos[i], key_rec)) {
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
        Rectangle key_rec = {
            key.pos.x, key.pos.y,
            key.size.x, key.size.y,
        };

        bool found = false;
        for (int i = 0; i < t_count; i++) {
            if (CheckCollisionPointRec(touch_pos[i], key_rec)) {
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

void update_osc_voice(const Osc* osc, Env* env, OscVoice* osc_voice, float wave_freq, float* buffer, size_t n) {
    Voice voice = osc_voice->voice;

    float vel_mul = voice.velocity / MAX_VELOCITY;
    float time = GetTime() - osc_voice->start_time;
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

void update_osc(Osc* osc, Env* env, float* buffer, size_t n) {
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
            key_idx = KEY_COUNT;
        }

        wave_freq = g_s.key_arr[key_idx].freq * osc->params.cents_mul * osc_voice->detune_mul;
        update_osc_voice(osc, env, osc_voice, wave_freq, buffer, n);
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

void update_stream(float* buffer, size_t n) {
    int e = pthread_rwlock_rdlock(&g_s.params.rw);
    if (e != 0) {
        // TODO: Log
        return;
    }

    for (int i = 0; i < n; i++) {
        buffer[i] = g_s.params.amp * buffer[i];
    }

    for (int i = 0; i < n; i++) {
        g_s.buffer_fft[g_s.buffer_fft_idx] = buffer[i];
        g_s.buffer_fft_idx++;
        g_s.buffer_fft_idx %= FFT_BUFFER_SIZE;
    }

    e = pthread_rwlock_unlock(&g_s.params.rw);
    if (e != 0) {
        // TODO: Log
        return;
    }
}

void update_fft() {
    size_t buffer_fft_idx = g_s.buffer_fft_idx;
    for (size_t i = 0; i < FFT_BUFFER_SIZE; i++) {
        // This is unsafe for now...
        g_s.buffer_fft_r[i] = g_s.buffer_fft[(i + buffer_fft_idx) % FFT_BUFFER_SIZE] * g_s.buffer_fft_w[i];
        g_s.buffer_fft_i[i] = 0.0f;
    }

    fft(g_s.buffer_fft_r, g_s.buffer_fft_i, FFT_BUFFER_SIZE);

    for (size_t i = 0; i < FFT_BUFFER_SIZE / 2; i++) {
        g_s.buffer_fft_r[i] = sqrt(powf(g_s.buffer_fft_r[i], 2) + powf(g_s.buffer_fft_i[i], 2)) / (BUFFER_SIZE / 16.0);
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

void update_synth(float* buffer, size_t n) {
    timespec_t start_time = {0};
    timespec_t section_time = {0};

    for (size_t i = 0; i < n; i++) {
        buffer[i] = 0.0f;
    }

    timespec_get(&start_time, TIME_UTC);

    timespec_get(&section_time, TIME_UTC);
    update_osc(&g_s.osc_arr[0], &g_s.env_arr[0], buffer, n);
    g_s.prof.osc_time[0] = duration_from(section_time);

    timespec_get(&section_time, TIME_UTC);
    update_osc(&g_s.osc_arr[1], &g_s.env_arr[1], buffer, n);
    g_s.prof.osc_time[1] = duration_from(section_time);

    timespec_get(&section_time, TIME_UTC);
    update_filter(&g_s.flt, buffer, n);
    g_s.prof.flt_time = duration_from(section_time);

    timespec_get(&section_time, TIME_UTC);
    update_stream(buffer, n);
    g_s.prof.total_time = duration_from(start_time);
}

void audio_callback(void *_buffer, unsigned int frames) {
    float *out_buffer = (float *)_buffer;
    update_synth(out_buffer, frames);
}

void draw_profiling_stats() {
    DrawText(
            TextFormat(
                "osc0: %dms\n"
                "osc1: %dms\n"
                "flt0: %dms\n"
                "total: %dms\n",
                NS_TO_MS(g_s.prof.osc_time[0].tv_nsec),
                NS_TO_MS(g_s.prof.osc_time[1].tv_nsec),
                NS_TO_MS(g_s.prof.flt_time.tv_nsec),
                NS_TO_MS(g_s.prof.total_time.tv_nsec)),
            GUI_GAP, GUI_GAP + FONT_SIZE,
            FONT_SIZE, RED);
}

void draw_voice_arr() {
    Osc* osc = &g_s.osc_arr[0];
    pthread_rwlock_rdlock(&osc->voice_arr.rw);

    for (int i = 0; i < osc->voice_arr.osc_voice_count; i++) {
        int k_idx = osc->voice_arr.osc_voice_arr[i].voice.key_idx;
        float detune = osc->voice_arr.osc_voice_arr[i].detune_mul;
        float start_time = osc->voice_arr.osc_voice_arr[i].start_time;
        float release_time = osc->voice_arr.osc_voice_arr[i].release_time;
        float env = osc->voice_arr.osc_voice_arr[i].last_env;
        DrawText(
                TextFormat(
                    "key: %d, d: %.2f, t: %.2f, r: %.2f, env: %.2f",
                    k_idx, detune, GetTime() - start_time, release_time - start_time, env),
                GUI_GAP, GUI_GAP + FONT_SIZE * i,
                FONT_SIZE, RED);
    }

    pthread_rwlock_unlock(&osc->voice_arr.rw);
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

        if (g_s.show_debug) {
            DrawText(
                    TextFormat("%d", i),
                    g_s.key_arr[i].pos.x + 5,
                    g_s.key_arr[i].pos.y + g_s.key_arr[i].size.y / 2,
                    FONT_SIZE, t_color);
        }
    }
}

void draw_fps() {
    DrawText(
            TextFormat("FPS: %d", GetFPS()),
            g_s.screen_w - 100, 10,
            FONT_SIZE, GREEN);
}

void draw_tab_synth(Vector2* cursor_p) {
    Vector2 tfield_s = { TEXT_FIELD_SIZE_W, TEXT_FIELD_SIZE_H };

    const char* program_opt_arr[PROGRAM_COUNT_MAX] = {};
    for (size_t i = 0; i < g_s.program_selection.program_count; i++) {
        program_opt_arr[i] = g_s.program_selection.program_name_arr[i];
    }

    Vector2 cursor_p_r1 = *cursor_p;
    set_gui_dir(GD_HORIZONTAL);
    draw_text_field("Program name", &cursor_p_r1, tfield_s, g_s.program_name, PROGRAM_NAME_CAPACITY);

    if (draw_dropdown(
                "Open", &cursor_p_r1,
                program_opt_arr, g_s.program_selection.program_count,
                &g_s.program_selection.selected_program_idx)) {
        open_program();
    }

    if (draw_button("Save", &cursor_p_r1)) {
        save_program();
    }

    cursor_p->y += BUTTON_SIZE_H + GUI_GAP;
    Vector2 cursor_p_r2 = *cursor_p;
    set_gui_dir(GD_HORIZONTAL);
    draw_knob("Amp", &cursor_p_r2, &g_s.params.amp, 0.0f, 1.0f, &g_s.params.rw);
    draw_knob("Pan", &cursor_p_r2, &g_s.params.pan, 0.0f, 1.0f, &g_s.params.rw);

    cursor_p->y += KNOB_SIZE_H + GUI_GAP;
    Vector2 cursor_p_r3 = *cursor_p;
    set_gui_dir(GD_HORIZONTAL);
    draw_toggle("Debug", &cursor_p_r3, &g_s.show_debug, NULL);
    draw_toggle("Profiling", &cursor_p_r3, &g_s.show_prof, NULL);
}

void draw_tab_osc(Vector2* cursor_p) {
    Vector2 wave_s = {(g_s.screen_w - 2 * GUI_GAP) / 2.0f - GUI_GAP, WAVE_SIZE_H - 50};

    for (size_t i = 0; i < ARRAY_SIZE(g_s.osc_arr); i++) {
        Vector2 split_cursor_p = {cursor_p->x + i * (wave_s.x + GUI_GAP), cursor_p->y};
        Osc *osc = &g_s.osc_arr[i];
        OscParams *params = &osc->params;
        int e = 0;

        push_gui_id_i(i);

        set_gui_dir(GD_VERTICAL);

        if (draw_wave("Oscillator wave", &split_cursor_p, wave_s, osc->disp_buffer, ARRAY_SIZE(osc->disp_buffer), 0)) {
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

        // 2nd Row
        Vector2 cursor_p_r2 = split_cursor_p;
        cursor_p_r2.y += KNOB_SIZE_H + GUI_GAP;

        set_gui_dir(GD_HORIZONTAL);

        draw_knob_i("Semitones", &split_cursor_p, &params->semi, -OSC_SEMI_RANGE, OSC_SEMI_RANGE, &params->rw);

        bool c_cents = draw_knob_i("Cents", &split_cursor_p, &params->cents, -OSC_CENTS_RANGE, OSC_CENTS_RANGE, &params->rw);
        if (c_cents) {
            e = pthread_rwlock_wrlock(&params->rw);
            if (e != 0) {
                // TODO: Log
                return;
            }

            params->cents_mul = calc_cents_mul(params->cents);

            e = pthread_rwlock_unlock(&params->rw);
            if (e != 0) {
                // TODO: Log
                return;
            }
        }

        split_cursor_p = cursor_p_r2;
        draw_knob_i("Unison", &split_cursor_p, &params->unison, OSC_UNISON_MIN, OSC_UNISON_MAX, &params->rw);

        draw_knob_i("Detune", &split_cursor_p, &params->detune, OSC_DETUNE_MIN, OSC_DETUNE_MAX, &params->rw);

        draw_knob("Volume", &split_cursor_p, &params->volume, 0.0f, 1.0f, &params->rw);

        pop_gui_id();
    }
}

void draw_tab_env(Vector2* cursor_p) {
    Vector2 wave_s = {(g_s.screen_w - 2 * GUI_GAP) / 2.0f - GUI_GAP, WAVE_SIZE_H};

    for (size_t i = 0; i < ARRAY_SIZE(g_s.env_arr); i++) {
        Env *env = &g_s.env_arr[i];
        Vector2 split_cursor_p = {cursor_p->x + i * (wave_s.x + GUI_GAP), cursor_p->y};

        push_gui_id_i(i);

        set_gui_dir(GD_HORIZONTAL);
        // TODO: Env wave
        draw_knob("Attack", &split_cursor_p, &env->attack, ENV_A_MIN, ENV_A_MAX, &env->rw);
        draw_knob("Decay", &split_cursor_p, &env->decay, ENV_D_MIN, ENV_D_MAX, &env->rw);
        draw_knob("Sustain", &split_cursor_p, &env->sustain, 0.0f, 1.0f, &env->rw);
        draw_knob("Release", &split_cursor_p, &env->release, ENV_R_MIN, ENV_R_MAX, &env->rw);

        pop_gui_id();
    }
}

void draw_tab_filter(Vector2* cursor_p) {
    bool changed_type = false;
    bool changed = false;
    FilterParams *params = &g_s.flt.params;
    Vector2 wave_s = {g_s.screen_w - GUI_GAP, WAVE_SIZE_H};
    Vector2 slider_s = {wave_s.x, SLIDER_SIZE_H};

    set_gui_dir(GD_VERTICAL);
    changed_type = draw_wave("Filter frequency response", cursor_p, wave_s, g_s.flt.disp_buffer, DISPLAY_BUFFER_SIZE, 0);
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

    changed |= draw_slider("Cutoff", cursor_p, slider_s, &params->cutoff, FLT_CUTOFF_MIN, FLT_CUTOFF_MAX, GS_LOG, &params->rw);

    set_gui_dir(GD_HORIZONTAL);
    changed |= draw_knob("Resonance", cursor_p, &params->resonance, FLT_RESONANCE_MIN, FLT_RESONANCE_MAX, &params->rw);
    changed |= draw_knob("Gain", cursor_p, &params->gain, FLT_GAIN_MIN, FLT_GAIN_MAX, &params->rw);

    if (g_s.show_debug) {
        DrawText(TextFormat("a0:%f b0:%f\na1:%f b1:%f\na2:%f b2:%f",
                params->a[0], params->b[0],
                params->a[1], params->b[1],
                params->a[2], params->b[2]),
                cursor_p->x, cursor_p->y, FONT_SIZE, TEXT_COLOR);
        cursor_p->y += FONT_SIZE + GUI_GAP;
    }

    // Right now this is a bit dumb, IMO there should be a copied struct.
    if (changed) {
        prepare_filter(&g_s.flt);
    }
}

void draw_tab_keys(Vector2* cursor_p) {
    Vector2 wave_s = {g_s.screen_w - GUI_GAP, WAVE_SIZE_H};
    bool clicked = false;

    set_gui_dir(GD_VERTICAL);
    if (g_s.show_fft) {
        clicked = draw_wave("FFT", cursor_p, wave_s, g_s.buffer_fft_r, ARRAY_SIZE(g_s.buffer_fft_r) / 2, 0);
    } else {
        clicked = draw_wave("Wave", cursor_p, wave_s, g_s.buffer_fft, ARRAY_SIZE(g_s.buffer_fft), g_s.buffer_fft_idx);
    }

    if (clicked) {
        g_s.show_fft ^= true;
    }

    draw_keys();

    cursor_p->x = g_s.screen_w  - KNOB_SIZE_W;
    if (draw_knob_i("Octave", cursor_p, &g_s.cur_octave, 0, OCTAVE_COUNT - 2, NULL)) {
        init_key_positions();
    }

    if (g_s.show_debug) {
        draw_voice_arr();
    }

    if (g_s.show_prof) {
        draw_profiling_stats();
    }
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
    Vector2 cursor_p = {GUI_GAP, GUI_GAP};
    Vector2 tab_s = {g_s.screen_w - 2 * GUI_GAP, TAB_H};
    const char* tab_l[] = {
        TAB_X(X_STR_ARR)
    };

    start_gui_ctx();

    set_gui_dir(GD_VERTICAL);

    draw_tab_menu("Tabs", &cursor_p, tab_s, tab_l, ARRAY_SIZE(tab_l), (int*)&g_s.cur_tab);
    switch (g_s.cur_tab) {
    case TAB_SYNTH:
        draw_tab_synth(&cursor_p);
        break;
    case TAB_OSC:
        draw_tab_osc(&cursor_p);
        break;
    case TAB_ENV:
        draw_tab_env(&cursor_p);
        break;
    case TAB_FLT:
        draw_tab_filter(&cursor_p);
        break;
    case TAB_KEYS:
        draw_tab_keys(&cursor_p);
        break;
    }
    end_tab_menu();

    finish_gui_ctx();
}

int main(void) {
    InitWindow(WIN_SIZE_W, WIN_SIZE_H, "Synth");
    SetTargetFPS(60);

    init_synth();

#ifndef PLATFORM_ANDROID
	ChangeDirectory("assets");
#endif

    while (!WindowShouldClose()) {
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

#ifndef PLATFORM_ANDROID
	ChangeDirectory("..");
#endif

    deinit_synth();

    CloseWindow();

    return 0;
}
