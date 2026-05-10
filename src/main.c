#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <stddef.h>
#include <complex.h>
#include <pthread.h>
#include <string.h>
#include <time.h>

#include "raylib.h"

#include "synth.h"
#include "utils.h"
#include "settings.h"
#include "gui_elements.h"
#include "voice.h"
#include "fft.h"
#include "env.h"
#include "osc.h"
#include "filter.h"
#include "file.h"

#define TAB_X(X)                \
    X(TAB_SYNTH, "Synth")       \
    X(TAB_OSC,   "Oscillators") \
    X(TAB_ENV,   "Envelopes")   \
    X(TAB_FLT,   "Filters")     \
    X(TAB_KEYS,  "Keys")        \

typedef enum Tab {
    TAB_X(X_ENUM)
} Tab;

typedef struct SynthKeyboard {
    int cur_octave;
    Rectangle key_rect_arr[KEY_COUNT];
} SynthKeyboard;

typedef struct FFTStream {
    size_t buffer_idx;
    float buffer_audio[FFT_BUFFER_SIZE];
    float buffer_r[FFT_BUFFER_SIZE];
    float buffer_i[FFT_BUFFER_SIZE];
    float buffer_w[FFT_BUFFER_SIZE];
} FFTStream;

typedef struct App {
    SynthKeyboard skeyboard;

    Tab cur_tab;

    FFTStream fft_stream;
    AudioStream audio_stream;

    bool show_fft;

    bool show_debug;
    bool show_prof;

    ProgramSelection program_selection;

    Synth s;
} App;

App g_a;

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

void update_synth_keyboard() {
    SynthKeyboard* sk = &g_a.skeyboard;
    const float k_w = KEY_WIDTH;
    const float k_ws = GUI_GAP;
    const float k_h = KEY_HEIGHT;
    const float k_hs = GUI_GAP;
    float k_x = k_ws;

    const float black_y = GetScreenHeight() - 2 * (k_hs + k_h);
    const float white_y = GetScreenHeight() - 1 * (k_hs + k_h);

    for (int i = 0; i < ARRAY_SIZE(sk->key_rect_arr); i++) {
        sk->key_rect_arr[i].x = 0;
        sk->key_rect_arr[i].y = 0;
        sk->key_rect_arr[i].width = 0;
        sk->key_rect_arr[i].height = 0;
    }

    for (int i = sk->cur_octave * KEY_OCTAVE + KEY_C_OFF; i < sk->cur_octave * KEY_OCTAVE + KEY_C_OFF + 2 * KEY_OCTAVE; i++) {
        sk->key_rect_arr[i].width = k_w;
        sk->key_rect_arr[i].height = k_h;

        if (key_is_black(i)) {
            sk->key_rect_arr[i].x = k_x - (k_w + k_ws) / 2;
            sk->key_rect_arr[i].y = black_y;
        } else {
            sk->key_rect_arr[i].x = k_x;
            sk->key_rect_arr[i].y = white_y;

            k_x += k_w;
            k_x += k_ws;
        }
    }
}

void audio_callback(void *_buffer, unsigned int frames);

void init_audio() {
    InitAudioDevice();

    // Set the number of samples the stream will keep in memory at a time to BUFFER_SIZE
    SetAudioStreamBufferSizeDefault(BUFFER_SIZE);
    // Init raw audio stream (sample rate: 44100, sample size: 32bit-float, channels: 1-mono)
    g_a.audio_stream = LoadAudioStream(SAMPLE_RATE, 32, 1);
    SetAudioStreamPan(g_a.audio_stream, g_a.s.params.pan);
    PlayAudioStream(g_a.audio_stream);
    SetAudioStreamCallback(g_a.audio_stream, audio_callback);
}

void deinit_audio() {
    UnloadAudioStream(g_a.audio_stream);
    CloseAudioDevice();
}

void init_fft_stream() {
    FFTStream* fft_stream = &g_a.fft_stream;

    fft_stream->buffer_idx = 0;
    for (size_t i = 0; i < ARRAY_SIZE(fft_stream->buffer_w); i++) {
        fft_stream->buffer_w[i] = 0.5f * (1 - cos(2 * PI * i / (float)(FFT_BUFFER_SIZE - 1)));
    }
}

void init_app() {
    g_a.cur_tab = TAB_KEYS;

    g_a.skeyboard.cur_octave = 3;
    update_synth_keyboard();

    init_program_selection(PROGRAM_PATH, &g_a.program_selection);

    init_fft_stream();
    init_synth(&g_a.s);
    init_audio();
}

void deinit_app() {
    deinit_synth(&g_a.s);
    deinit_audio();
}

void process_synth_keyboard() {
    static Vector2 touch_pos_arr[MAX_TOUCH_POINTS] = { 0 };
    VoiceArr *va = &g_a.s.voice_arr;
    int t_count = GetTouchPointCount();

    // Clamp touch points available ( set the maximum touch points allowed )
    if (t_count > MAX_TOUCH_POINTS) {
        t_count = MAX_TOUCH_POINTS;
    }

    // Get touch points positions
    for (int i = 0; i < t_count; i++) {
        touch_pos_arr[i] = GetTouchPosition(i);
    }

    for (int i = 0; i < t_count; i++) {
        for (int k = 0; k < KEY_COUNT; k++) {
            Rectangle key_rec = g_a.skeyboard.key_rect_arr[k];

            if (CheckCollisionPointRec(touch_pos_arr[i], key_rec)) {
                Voice new_voice = {0};

                bool hold = voice_hold_key(va, k, &new_voice);
                if (!hold) {
                    synth_add_voice(&g_a.s, new_voice, GetTime());
                }

                break; // Found a key for this touch. See next touch.
            }
        }
    }

    // Release unheld keys
    pthread_rwlock_rdlock(&va->rw);

    for (int v = 0; v < va->voice_count; v++) {
        int k = va->voice_arr[v].key_idx;
        Rectangle key_rec = g_a.skeyboard.key_rect_arr[k];

        bool found = false;
        for (int i = 0; i < t_count; i++) {
            if (CheckCollisionPointRec(touch_pos_arr[i], key_rec)) {
                found = true;
                break; // Found a touch for the held key.
            }
        }

        if (!found) {
            pthread_rwlock_unlock(&va->rw);

            voice_remove(&g_a.s.voice_arr, v);

            pthread_rwlock_rdlock(&va->rw);

            synth_release_voice(&g_a.s, k, GetTime());
        }
    }

    pthread_rwlock_unlock(&va->rw);

    synth_gc_voice(&g_a.s);
}

void update_fft() {
    FFTStream* fft_stream = &g_a.fft_stream;
    size_t buffer_fft_idx = fft_stream->buffer_idx;

    for (size_t i = 0; i < FFT_BUFFER_SIZE; i++) {
        fft_stream->buffer_r[i] = fft_stream->buffer_audio[(i + buffer_fft_idx) % FFT_BUFFER_SIZE] * fft_stream->buffer_w[i];
        fft_stream->buffer_i[i] = 0.0f;
    }

    fft(fft_stream->buffer_r, fft_stream->buffer_i, FFT_BUFFER_SIZE);

    for (size_t i = 0; i < FFT_BUFFER_SIZE / 2; i++) {
        fft_stream->buffer_r[i] = sqrt(powf(fft_stream->buffer_r[i], 2) + powf(fft_stream->buffer_i[i], 2)) / (BUFFER_SIZE / 16.0);
    }
}

void push_fft_stream(float* buffer, size_t n) {
    FFTStream* fft_stream = &g_a.fft_stream;

    for (int i = 0; i < n; i++) {
        fft_stream->buffer_audio[fft_stream->buffer_idx] = buffer[i];
        fft_stream->buffer_idx++;
        fft_stream->buffer_idx %= FFT_BUFFER_SIZE;
    }
}

void audio_callback(void *_buffer, unsigned int frames) {
    float *out_buffer = (float *)_buffer;
    update_synth(&g_a.s, GetTime(), out_buffer, frames);
    push_fft_stream(out_buffer, frames);
}

void draw_profiling_stats() {
    DrawText(
            TextFormat(
                "osc0: %dms\n"
                "osc1: %dms\n"
                "flt0: %dms\n"
                "total: %dms\n",
                NS_TO_MS(g_a.s.prof.osc_time[0].tv_nsec),
                NS_TO_MS(g_a.s.prof.osc_time[1].tv_nsec),
                NS_TO_MS(g_a.s.prof.flt_time.tv_nsec),
                NS_TO_MS(g_a.s.prof.total_time.tv_nsec)),
            GUI_GAP, GUI_GAP + FONT_SIZE,
            FONT_SIZE, RED);
}

void draw_voice_arr() {
    Osc* osc = &g_a.s.osc_arr[0];
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
    for (int i = 0; i < KEY_COUNT; i++) {
        Rectangle key_rec = g_a.skeyboard.key_rect_arr[i];

        if (key_rec.width == 0 && key_rec.height == 0) {
            continue;
        }

        Color k_color = KEY_WHITE_COLOR;
        Color t_color = BLACK;
        if (key_is_black(i)) {
            k_color = KEY_BLACK_COLOR;
            t_color = WHITE;
        }

        DrawRectangleRec(key_rec, k_color);
        DrawRectangleLinesEx(key_rec, KEY_LINE_THICKNESS, KEY_OUTER_COLOR);

        if (g_a.show_debug) {
            DrawText(
                    TextFormat("%d", i),
                    key_rec.x + GUI_GAP,
                    key_rec.y + GUI_GAP,
                    FONT_SIZE, t_color);
        }
    }
}

void draw_fps() {
    DrawText(
            TextFormat("FPS: %d", GetFPS()),
            GetScreenWidth() - 100, 10,
            FONT_SIZE, GREEN);
}

void draw_tab_synth(Vector2* cursor_p) {
    Vector2 tfield_s = { TEXT_FIELD_SIZE_W, TEXT_FIELD_SIZE_H };

    const char* program_opt_arr[PROGRAM_COUNT_MAX] = {};
    for (size_t i = 0; i < g_a.program_selection.program_count; i++) {
        program_opt_arr[i] = g_a.program_selection.program_name_arr[i];
    }

    Vector2 cursor_p_r1 = *cursor_p;
    set_gui_dir(GD_HORIZONTAL);
    draw_text_field("Program name", &cursor_p_r1, tfield_s, g_a.s.program_name, PROGRAM_NAME_CAPACITY);

    if (draw_dropdown(
                "Open", &cursor_p_r1,
                program_opt_arr, g_a.program_selection.program_count,
                &g_a.program_selection.selected_program_idx)) {
        open_program(&g_a.s, &g_a.program_selection);
    }

    if (draw_button("Save", &cursor_p_r1)) {
        save_program(&g_a.s, &g_a.program_selection);
    }

    cursor_p->y += BUTTON_SIZE_H + GUI_GAP;
    Vector2 cursor_p_r2 = *cursor_p;
    set_gui_dir(GD_HORIZONTAL);
    draw_knob("Amp", &cursor_p_r2, &g_a.s.params.amp, 0.0f, 1.0f, &g_a.s.params.rw);
    draw_knob("Pan", &cursor_p_r2, &g_a.s.params.pan, 0.0f, 1.0f, &g_a.s.params.rw);

    cursor_p->y += KNOB_SIZE_H + GUI_GAP;
    Vector2 cursor_p_r3 = *cursor_p;
    set_gui_dir(GD_HORIZONTAL);
    draw_toggle("Debug", &cursor_p_r3, &g_a.show_debug, NULL);
    draw_toggle("Profiling", &cursor_p_r3, &g_a.show_prof, NULL);
}

void draw_tab_osc(Vector2* cursor_p) {
    Vector2 wave_s = {(GetScreenWidth() - 2 * GUI_GAP) / 2.0f - GUI_GAP, WAVE_SIZE_H - 50};

    for (size_t i = 0; i < ARRAY_SIZE(g_a.s.osc_arr); i++) {
        Vector2 split_cursor_p = {
            cursor_p->x + i * (wave_s.x + GUI_GAP),
            cursor_p->y
        };

        Osc *osc = &g_a.s.osc_arr[i];
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
    Vector2 wave_s = {(GetScreenWidth() - 2 * GUI_GAP) / 2.0f - GUI_GAP, WAVE_SIZE_H};

    for (size_t i = 0; i < ARRAY_SIZE(g_a.s.env_arr); i++) {
        Env *env = &g_a.s.env_arr[i];
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
    FilterParams *params = &g_a.s.flt.params;
    Vector2 wave_s = {GetScreenWidth() - GUI_GAP, WAVE_SIZE_H};
    Vector2 slider_s = {wave_s.x, SLIDER_SIZE_H};

    set_gui_dir(GD_VERTICAL);
    changed_type = draw_wave("Filter frequency response", cursor_p, wave_s, g_a.s.flt.disp_buffer, DISPLAY_BUFFER_SIZE, 0);
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

    if (g_a.show_debug) {
        DrawText(TextFormat("a0:%f b0:%f\na1:%f b1:%f\na2:%f b2:%f",
                params->a[0], params->b[0],
                params->a[1], params->b[1],
                params->a[2], params->b[2]),
                cursor_p->x, cursor_p->y, FONT_SIZE, TEXT_COLOR);
        cursor_p->y += FONT_SIZE + GUI_GAP;
    }

    // Right now this is a bit dumb, IMO there should be a copied struct.
    if (changed) {
        prepare_filter(&g_a.s.flt);
    }
}

void draw_tab_keys(Vector2* cursor_p) {
    Vector2 wave_s = { GetScreenWidth() - GUI_GAP, WAVE_SIZE_H };
    bool clicked = false;

    set_gui_dir(GD_VERTICAL);
    if (g_a.show_fft) {
        clicked = draw_wave("FFT", cursor_p, wave_s, g_a.fft_stream.buffer_r, FFT_BUFFER_SIZE / 2, 0);
    } else {
        clicked = draw_wave("Wave", cursor_p, wave_s, g_a.fft_stream.buffer_audio, FFT_BUFFER_SIZE, g_a.fft_stream.buffer_idx);
    }

    if (clicked) {
        g_a.show_fft ^= true;
    }

    draw_keys();

    cursor_p->x = GetScreenWidth()  - KNOB_SIZE_W;
    if (draw_knob_i("Octave", cursor_p, &g_a.skeyboard.cur_octave, 0, OCTAVE_COUNT - 2, NULL)) {
        update_synth_keyboard();
    }

    if (g_a.show_debug) {
        draw_voice_arr();
    }
}

void process_ui() {
    switch (g_a.cur_tab) {
    case TAB_SYNTH:
        break;
    case TAB_OSC:
        break;
    case TAB_ENV:
        break;
    case TAB_FLT:
        break;
    case TAB_KEYS:
        process_synth_keyboard();
        break;
    }
}

void draw_ui() {
    Vector2 cursor_p = {GUI_GAP, GUI_GAP};
    Vector2 tab_s = {GetScreenWidth() - 2 * GUI_GAP, TAB_H};
    const char* tab_l[] = {
        TAB_X(X_STR_ARR)
    };

    start_gui_ctx();

    set_gui_dir(GD_VERTICAL);

    draw_tab_menu("Tabs", &cursor_p, tab_s, tab_l, ARRAY_SIZE(tab_l), (int*)&g_a.cur_tab);
    switch (g_a.cur_tab) {
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

    if (g_a.show_prof) {
        draw_profiling_stats();
    }
}

int main(void) {
    InitWindow(WIN_SIZE_W, WIN_SIZE_H, "Synth");
    SetTargetFPS(60);

    init_app();

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

    deinit_app();

    CloseWindow();

    return 0;
}
