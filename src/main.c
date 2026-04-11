#include "raylib.h"
#include <math.h>

#include "gui_elements.h"

#define ARRAY_SIZE(X) (sizeof(X) / sizeof(*(X)))

#define ENV_A_MIN 0.05f
#define ENV_R_MIN 0.05f

#define KEY_COUNT 88
#define KEY_OCTAVE 12

#define KEY_A4_IDX  49
#define KEY_A4_FREQ 440.0f
#define KEY_C4_IDX  (KEY_A4_IDX - 9)
#define KEY_C_OFF   4
#define KEY_OFF     KEY_C4_IDX

#define KEY_MAX_VOICES 8

#define KEY_IDX_INVALID -1

#define BUFFER_SIZE 4096
#define SAMPLE_RATE 44100

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

typedef struct Voice {
    int key_idx;
    int wave_idx;
    int velocity;
    float time;
    float release_time;
    bool released;
} Voice;

typedef struct Env {
    float attack;
    float decay;
    float sustain;
    float release;
} Env;

typedef struct Osc {
    float buffer[BUFFER_SIZE];
} Osc;

#define TAB_X(X)                \
    X(TAB_SYNTH, "Synth")       \
    X(TAB_OSC,   "Oscillators") \
    X(TAB_ENV,   "Envelopes")   \
    X(TAB_FLT,   "Filters")     \
    X(TAB_KEYS,  "Keys")        \

typedef enum Tab {
    TAB_X(X_ENUM)
} Tab;

typedef struct Synth {
    int screen_w;
    int screen_h;
    Tab cur_tab;

    Key key_arr[KEY_COUNT];

    Voice voice_arr[KEY_MAX_VOICES];

    Osc osc;
    Env env;

    float amp;
    float pan;

    float buffer[BUFFER_SIZE];
    AudioStream stream;
} Synth;

Synth g_s;

float get_env_value(float time, bool released, float release_time, Env env) {
    // 1. Handle Release Phase
    if (released) {
        float time_in_release = time - release_time;
        if (time_in_release >= env.release) {
            return 0.0f;
        }

        // We calculate the value starting from the sustain level down to 0
        // (Note: For a perfect implementation, you'd track the exact amplitude at the
        // moment of release, but using sustainLevel is the standard simplification).
        return env.sustain * (1.0f - (time_in_release / env.release));
    }

    // 2. Attack Phase
    if (time < env.attack) {
        return time / env.attack;
    }

    // 3. Decay Phase
    float timeInDecay = time - env.attack;
    if (timeInDecay < env.decay) {
        float decayProgress = timeInDecay / env.decay;
        return 1.0f - (decayProgress * (1.0f - env.sustain));
    }

    // 4. Sustain Phase
    return env.sustain;
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

void prepare_key_pos() {
    const float k_w = KEY_WIDTH;
    const float k_ws = GUI_GAP;
    const float k_h = KEY_HEIGHT;
    const float k_hs = GUI_GAP;
    float k_x = k_ws;

    const float black_y = g_s.screen_h - 2 * (k_hs + k_h);
    const float white_y = g_s.screen_h - 1 * (k_hs + k_h);

    for (int i = 0; i < 2 * KEY_OCTAVE; i++) {
        g_s.key_arr[i + KEY_OFF].size.x = k_w;
        g_s.key_arr[i + KEY_OFF].size.y = k_h;

        if (key_is_black(i + KEY_OFF)) {
            g_s.key_arr[i + KEY_OFF].pos.x = k_x - (k_w + k_ws) / 2;
            g_s.key_arr[i + KEY_OFF].pos.y = black_y;
        } else {
            g_s.key_arr[i + KEY_OFF].pos.x = k_x;
            g_s.key_arr[i + KEY_OFF].pos.y = white_y;

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

void prepare_voices() {
    for (int i = 0; i < ARRAY_SIZE(g_s.voice_arr); i++) {
        g_s.voice_arr[i].key_idx = KEY_IDX_INVALID;
        g_s.voice_arr[i].time = 0;
        g_s.voice_arr[i].velocity = 0;
        g_s.voice_arr[i].wave_idx = 0;
    }
}

void prepare_audio() {
    InitAudioDevice();

    // Set the number of samples the stream will keep in memory at a time to BUFFER_SIZE
    SetAudioStreamBufferSizeDefault(BUFFER_SIZE);
    // Init raw audio stream (sample rate: 44100, sample size: 32bit-float, channels: 1-mono)
    g_s.stream = LoadAudioStream(SAMPLE_RATE, 32, 1);
    SetAudioStreamPan(g_s.stream, g_s.pan);
    PlayAudioStream(g_s.stream);
}

void init_synth() {
    g_s.screen_w = GetScreenWidth();
    g_s.screen_h = GetScreenHeight();

    g_s.cur_tab = TAB_KEYS;
    g_s.amp = 0.2;
    g_s.pan = 0.0f;

    g_s.env.attack = ENV_A_MIN;
    g_s.env.decay = 0.00f;
    g_s.env.sustain = 1.0f;
    g_s.env.release = ENV_R_MIN;

    prepare_keys();
    prepare_voices();
    prepare_audio();
}

void deinit_synth() {
    UnloadAudioStream(g_s.stream);
    CloseAudioDevice();
}

void set_voice(int v_idx, int k_idx) {
    g_s.voice_arr[v_idx].key_idx = k_idx;
    g_s.voice_arr[v_idx].time = 0.0f;
    g_s.voice_arr[v_idx].velocity = 40;
    g_s.voice_arr[v_idx].wave_idx = 0;
    g_s.voice_arr[v_idx].released = false;
    g_s.voice_arr[v_idx].release_time = 0.0f;
}

void reset_voice(int v_idx) {
    g_s.voice_arr[v_idx].key_idx = KEY_IDX_INVALID;
    g_s.voice_arr[v_idx].time = 0.0f;
    g_s.voice_arr[v_idx].velocity = 0;
    g_s.voice_arr[v_idx].wave_idx = 0;
    g_s.voice_arr[v_idx].released = false;
    g_s.voice_arr[v_idx].release_time = 0.0f;
}

void clear_voice(int v_idx) {
    int size = 0;
    for (; size < ARRAY_SIZE(g_s.voice_arr); size++) {
        if (g_s.voice_arr[size].key_idx == KEY_IDX_INVALID) {
            break;
        }
    }

    // Replace with the last element
    if (size != 0) {
        g_s.voice_arr[v_idx] = g_s.voice_arr[size - 1];
        reset_voice(size - 1);
    }
}

void hold_voice(int idx) {
    for (int i = 0; i < ARRAY_SIZE(g_s.voice_arr); i++) {
        if (g_s.voice_arr[i].key_idx == KEY_IDX_INVALID) {
            set_voice(i, idx);
            return;
        }

        if (g_s.voice_arr[i].key_idx == idx && !g_s.voice_arr[i].released) {
            return;
        }
    }

    // Shift voices left first and replace with the last index?
    clear_voice(0);
    set_voice(ARRAY_SIZE(g_s.voice_arr) - 1, idx);
}

void release_voice(int idx) {
    for (int i = 0; i < ARRAY_SIZE(g_s.voice_arr); i++) {
        if (g_s.voice_arr[i].key_idx == idx && !g_s.voice_arr[i].released) {
            g_s.voice_arr[i].released = true;
            g_s.voice_arr[i].release_time = g_s.voice_arr[i].time;

            break;
        }
    }
}

void clear_released_voices() {
    for (int i = 0; i < ARRAY_SIZE(g_s.voice_arr); i++) {
        if (g_s.voice_arr[i].key_idx == KEY_IDX_INVALID) {
            return;
        }

        float env_mul = get_env_value(
                g_s.voice_arr[i].time,
                g_s.voice_arr[i].released,
                g_s.voice_arr[i].release_time,
                g_s.env);

        if (g_s.voice_arr[i].release_time && env_mul == 0) {
            clear_voice(i);
            return;
        }
    }
}

void process_voice_arr() {
    for (int i = 0; i < ARRAY_SIZE(g_s.voice_arr); i++) {
        g_s.voice_arr[i].time += GetFrameTime();
    }

    clear_released_voices();
}

void process_screen() {
    g_s.screen_w = GetScreenWidth();
    g_s.screen_h = GetScreenHeight();
}

bool point_rect_intersection(Vector2 p, Vector2 rp, Vector2 rs) {
    return p.x >= rp.x && p.x <= rp.x + rs.x && p.y >= rp.y && p.y <= rp.y + rs.y;
}

void process_keys() {
    static Vector2 touch_pos[MAX_TOUCH_POINTS] = { 0 };

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
        for (int k = KEY_OFF; k < KEY_OFF + 2 * KEY_OCTAVE; k++) {
            Key key = g_s.key_arr[k];
            if (point_rect_intersection(touch_pos[i], key.pos, key.size)) {
                hold_voice(k);
                break; // Found a key for this touch. See next.
            }
        }
    }

    // Release unheld keys
    for (int v = 0; v < ARRAY_SIZE(g_s.voice_arr); v++) {
        if (g_s.voice_arr[v].key_idx == KEY_IDX_INVALID) {
            break;
        }

        int k = g_s.voice_arr[v].key_idx;
        Key key = g_s.key_arr[k];
        bool found = false;
        for (int i = 0; i < t_count; i++) {
            if (point_rect_intersection(touch_pos[i], key.pos, key.size)) {
                found = true;
                break; // Found a touch for the held key.
            }
        }

        if (!found) {
            release_voice(k);
        }
    }
}

void update_osc() {
    if (!IsAudioStreamProcessed(g_s.stream)) {
        return;
    }

    int i = 0;

    for (int j = 0; j < BUFFER_SIZE; j++) {
        g_s.osc.buffer[j] = 0;
    }

    for (i = 0; i < ARRAY_SIZE(g_s.voice_arr); i++) {
        if (g_s.voice_arr[i].key_idx == KEY_IDX_INVALID) {
            break;
        }

        int key_idx = g_s.voice_arr[i].key_idx;
        float wave_freq = g_s.key_arr[key_idx].freq;
        float vel_mul = g_s.voice_arr[i].velocity / MAX_VELOCITY;
        float env_mul = 0.0f;

        for (int j = 0; j < BUFFER_SIZE; j++) {
            float dt = j / (float)SAMPLE_RATE;
            float wave_length = SAMPLE_RATE / wave_freq;
            env_mul = get_env_value(
                    g_s.voice_arr[i].time + dt,
                    g_s.voice_arr[i].released,
                    g_s.voice_arr[i].release_time,
                    g_s.env);

            // TODO: Calculate time based on sample rate?
            // TODO: Mixer
            g_s.osc.buffer[j] += env_mul * vel_mul * sin(2 * PI * g_s.voice_arr[i].wave_idx / wave_length);
            g_s.voice_arr[i].wave_idx++;
            if (g_s.voice_arr[i].wave_idx >= wave_length) {
                g_s.voice_arr[i].wave_idx = 0;
            }
        }
    }
}

void update_stream() {
    if (!IsAudioStreamProcessed(g_s.stream)) {
        return;
    }

    for (int i = 0; i < BUFFER_SIZE; i++) {
        g_s.buffer[i] = g_s.amp * g_s.osc.buffer[i];
    }

    UpdateAudioStream(g_s.stream, g_s.buffer, BUFFER_SIZE);
}

void draw_voice_arr() {
    for (int i = 0; i < ARRAY_SIZE(g_s.voice_arr); i++) {
        if (g_s.voice_arr[i].key_idx == KEY_IDX_INVALID) {
            break;
        }

        int key_idx = g_s.voice_arr[i].key_idx;
        float rel_time = g_s.voice_arr[i].release_time;
        float wave_freq = g_s.key_arr[key_idx].freq;
        DrawText(TextFormat(
                    "sine frequency: %.2f, key idx: %d, rel time:%.2f",
                    wave_freq, key_idx, rel_time),
                10, 10 + FONT_SIZE * i,
                FONT_SIZE, RED);
    }
}

void draw_wave() {
    for (int i = 0; i < g_s.screen_w - 1; i++) {
        int si = i * BUFFER_SIZE / g_s.screen_w;
        int ei = (i + 1) * BUFFER_SIZE / g_s.screen_w;
        if (si < 0 || si > BUFFER_SIZE) {
            continue;
        }
        if (ei < 0 || ei > BUFFER_SIZE) {
            continue;
        }

        Vector2 s_pos = { i, 200 - 50 * g_s.buffer[si] };
        Vector2 e_pos = { i + 1, 200 - 50 * g_s.buffer[ei] };

        DrawLineV(s_pos, e_pos, RED);
    }
}

void draw_keys() {
    for (int i = KEY_OFF; i < KEY_OFF + 2 * KEY_OCTAVE; i++) {
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
    DrawKnob("Amp", cursor_p, KNOB_RADIUS, &g_s.amp, 0.0f, 1.0f);
    cursor_p.y += KNOB_SIZE_H;
    if (DrawKnob("Pan", cursor_p, KNOB_RADIUS, &g_s.pan, 0.0f, 1.0f)) {
        SetAudioStreamPan(g_s.stream, g_s.pan);
    }
    cursor_p.y += KNOB_SIZE_H;
}

void draw_tab_keys() {
    draw_voice_arr();
    draw_wave();
    draw_keys();
}

void draw_tab_env() {
    Vector2 cursor_p = {GUI_GAP, TAB_H + 2 * GUI_GAP};

    DrawKnob("Attack", cursor_p, KNOB_RADIUS, &g_s.env.attack, ENV_A_MIN, 1.0f);
    cursor_p.y += KNOB_SIZE_H;
    DrawKnob("Decay", cursor_p, KNOB_RADIUS, &g_s.env.decay, 0.0f, 1.0f);
    cursor_p.y += KNOB_SIZE_H;
    DrawKnob("Sustain", cursor_p, KNOB_RADIUS, &g_s.env.sustain, ENV_R_MIN, 1.0f);
    cursor_p.y += KNOB_SIZE_H;
    DrawKnob("Release", cursor_p, KNOB_RADIUS, &g_s.env.release, 0.0f, 1.0f);
    cursor_p.y += KNOB_SIZE_H;
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
        break;
    case TAB_ENV:
        draw_tab_env();
        break;
    case TAB_FLT:
        break;
    case TAB_KEYS:
        draw_tab_keys();
        break;
    }
}

int main(void) {
    InitWindow(800, 450, "Synth");

    init_synth();

    SetTargetFPS(60);

    while (!WindowShouldClose())
    {
        // Process
        process_ui();
        process_voice_arr();

        // Update
        update_osc();
        update_stream();

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
