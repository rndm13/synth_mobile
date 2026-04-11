#include "raylib.h"
#include <math.h>
#include <string.h>

#define ARRAY_SIZE(X) (sizeof(X) / sizeof(*(X)))

#define KEY_COUNT 88
#define KEY_OCTAVE 12

#define KEY_A4_IDX  49
#define KEY_A4_FREQ 440.0f
#define KEY_C4_IDX  (KEY_A4_IDX - 9)
#define KEY_OFF     KEY_C4_IDX

#define KEY_MAX_VOICES 8

#define KEY_IDX_INVALID -1

#define BUFFER_SIZE 4096
#define SAMPLE_RATE 44100

#define FONT_SIZE 20
#define MAX_TOUCH_POINTS 10

typedef struct Key {
    Vector2 pos;
    Vector2 size;

    float freq;
} Key;

typedef struct Osc {
    int voice_key_idx[KEY_MAX_VOICES];

    int wave_idx;

    float buffer[BUFFER_SIZE];
} Osc;

typedef struct Synth {
    int screen_w;
    int screen_h;

    Key keys[KEY_COUNT];

    int voice_key_idx[KEY_MAX_VOICES];

    Osc osc;

    float pan;
    float buffer[BUFFER_SIZE];
    AudioStream stream;
} Synth;

Synth g_s;

void prepare_keys() {
    for (int i = 0; i < ARRAY_SIZE(g_s.keys); i++) {
        g_s.keys[i].freq = powf(2.0f, (float)(i - KEY_A4_IDX) / (float)KEY_OCTAVE) * KEY_A4_FREQ;
    }
}

void prepare_voices() {
    for (int i = 0; i < ARRAY_SIZE(g_s.voice_key_idx); i++) {
        g_s.voice_key_idx[i] = KEY_IDX_INVALID;
    }

    for (int i = 0; i < ARRAY_SIZE(g_s.osc.voice_key_idx); i++) {
        g_s.osc.voice_key_idx[i] = KEY_IDX_INVALID;
    }
}

void prepare_audio() {
    InitAudioDevice();

    // Set the number of samples the stream will keep in memory at a time to BUFFER_SIZE
    SetAudioStreamBufferSizeDefault(BUFFER_SIZE);
    // Init raw audio stream (sample rate: 44100, sample size: 32bit-float, channels: 1-mono)
    g_s.stream = LoadAudioStream(SAMPLE_RATE, 32, 1);
    g_s.pan = 0.0f;
    SetAudioStreamPan(g_s.stream, g_s.pan);
    PlayAudioStream(g_s.stream);
}

void init_synth() {
    g_s.screen_w = GetScreenWidth();
    g_s.screen_h = GetScreenHeight();

    prepare_keys();
    prepare_voices();
    prepare_audio();
}

void deinit_synth() {
    UnloadAudioStream(g_s.stream);
    CloseAudioDevice();
}

void hold_voice(int idx) {
    for (int i = 0; i < ARRAY_SIZE(g_s.voice_key_idx); i++) {
        if (g_s.voice_key_idx[i] == idx) {
            return;
        }

        if (g_s.voice_key_idx[i] == KEY_IDX_INVALID) {
            g_s.voice_key_idx[i] = idx;
            return;
        }
    }

    g_s.voice_key_idx[0] = idx;
}

void release_all_voices() {
    for (int i = 0; i < ARRAY_SIZE(g_s.voice_key_idx); i++) {
        g_s.voice_key_idx[i] = KEY_IDX_INVALID;
    }
}

void release_voice(int idx) {
    for (int i = 0; i < ARRAY_SIZE(g_s.voice_key_idx); i++) {
        if (g_s.voice_key_idx[i] == idx) {
            g_s.voice_key_idx[i] = KEY_IDX_INVALID;
        }
    }
}

void process_screen() {
    g_s.screen_w = GetScreenWidth();
    g_s.screen_h = GetScreenHeight();
}

void process_keys() {
    for (int i = 0; i < 2 * KEY_OCTAVE; i++) {
        const float k_w = 40;
        const float k_ws = 10;
        const float k_h = 100;
        const float k_hs = 10;

        g_s.keys[i + KEY_OFF].size.x = k_w;
        g_s.keys[i + KEY_OFF].size.y = k_h;

        g_s.keys[i + KEY_OFF].pos.x = i * k_w + (i + 1) * k_ws;
        g_s.keys[i + KEY_OFF].pos.y = g_s.screen_h - k_hs - k_h;
    }

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
        for (int j = KEY_OFF; j < KEY_OFF + 2 * KEY_OCTAVE; j++) {
            if (touch_pos[i].x >= g_s.keys[j].pos.x && touch_pos[i].x <= g_s.keys[j].pos.x + g_s.keys[j].size.x &&
                touch_pos[i].y >= g_s.keys[j].pos.y && touch_pos[i].y <= g_s.keys[j].pos.y + g_s.keys[j].size.y) {
                hold_voice(j);
                break; // Found a key for this touch. See next.
            }
        }
    }
}

void update_osc() {
    int i = 0;

    memcpy(g_s.osc.voice_key_idx, g_s.voice_key_idx, sizeof(g_s.osc.voice_key_idx));
    release_all_voices();

    for (int j = 0; j < BUFFER_SIZE; j++) {
        g_s.osc.buffer[j] = 0;
    }

    for (i = 0; i < ARRAY_SIZE(g_s.osc.voice_key_idx); i++) {
        if (g_s.osc.voice_key_idx[i] == KEY_IDX_INVALID) {
            break;
        }

        int key_idx = g_s.osc.voice_key_idx[i];
        float wave_freq = g_s.keys[key_idx].freq;

        for (int j = 0; j < BUFFER_SIZE; j++) {
            float wave_length = SAMPLE_RATE / wave_freq;
            g_s.osc.buffer[j] += 0.3 * sin(2 * PI * g_s.osc.wave_idx / wave_length);
            g_s.osc.wave_idx++;
            if (g_s.osc.wave_idx >= wave_length) {
                g_s.osc.wave_idx = 0;
            }
        }
    }
}

void update_stream() {
    if (!IsAudioStreamProcessed(g_s.stream)) {
        return;
    }

    for (int i = 0; i < BUFFER_SIZE; i++) {
        g_s.buffer[i] = 1 * g_s.osc.buffer[i];
    }

    UpdateAudioStream(g_s.stream, g_s.osc.buffer, BUFFER_SIZE);
}

void draw_freq() {
    for (int i = 0; i < ARRAY_SIZE(g_s.osc.voice_key_idx); i++) {
        if (g_s.osc.voice_key_idx[i] == KEY_IDX_INVALID) {
            break;
        }

        int key_idx = g_s.osc.voice_key_idx[i];
        float wave_freq = g_s.keys[key_idx].freq;
        DrawText(
                TextFormat("sine frequency: %f, key idx: %d", wave_freq, key_idx),
                10, 10 + FONT_SIZE * i,
                FONT_SIZE, RED);
    }
}

void draw_wave() {
    static float buffer[BUFFER_SIZE] = {0};
    for (int i = 0; i < g_s.screen_w / 2; i++) {
        int si = i * BUFFER_SIZE / g_s.screen_w / 2;
        int ei = (i + 1) * BUFFER_SIZE / g_s.screen_w / 2;
        if (si < 0 || si > BUFFER_SIZE) {
            continue;
        }
        if (ei < 0 || ei > BUFFER_SIZE) {
            continue;
        }

        Vector2 s_pos = { i, 250 - 50 * buffer[si] };
        Vector2 e_pos = { i + 1, 250 - 50 * buffer[ei] };

        DrawLineV(s_pos, e_pos, RED);
    }

    for (int i = 0; i < g_s.screen_w / 2; i++) {
        int si = i * BUFFER_SIZE / g_s.screen_w / 2;
        int ei = (i + 1) * BUFFER_SIZE / g_s.screen_w / 2;
        if (si < 0 || si > BUFFER_SIZE) {
            continue;
        }
        if (ei < 0 || ei > BUFFER_SIZE) {
            continue;
        }

        Vector2 s_pos = { i + g_s.screen_w / 2.0f, 250 - 50 * g_s.buffer[si] };
        Vector2 e_pos = { i + 1 + g_s.screen_w / 2.0f, 250 - 50 * g_s.buffer[ei] };

        DrawLineV(s_pos, e_pos, RED);
    }

    memcpy(buffer, g_s.buffer, sizeof(buffer));
}

void draw_keys() {
    for (int i = KEY_OFF; i < KEY_OFF + 2 * KEY_OCTAVE; i++) {
        Rectangle r;
        r.x = g_s.keys[i].pos.x;
        r.y = g_s.keys[i].pos.y;
        r.width = g_s.keys[i].size.x;
        r.height = g_s.keys[i].size.y;

        DrawRectangleLinesEx(r, 5, BLACK);
        DrawText(
                TextFormat("%d\n%.2f", i, g_s.keys[i].freq),
                g_s.keys[i].pos.x + 5,
                g_s.keys[i].pos.y + g_s.keys[i].size.y / 2,
                FONT_SIZE, BLACK);
    }
}

void draw_fps() {
        DrawText(
                TextFormat("FPS: %d", GetFPS()),
                g_s.screen_w - 100, 10,
                FONT_SIZE, GREEN);
}

int main(void) {
    InitWindow(800, 450, "Synth");

    init_synth();

    SetTargetFPS(60);

    while (!WindowShouldClose())
    {
        // Process
        process_screen();
        process_keys(); // Set new keys

        // Update
        update_osc();
        update_stream();

        // Draw
        BeginDrawing();
            ClearBackground(RAYWHITE);

            draw_wave();
            draw_freq();
            draw_keys();
            draw_fps();
        EndDrawing();
    }

    deinit_synth();

    CloseWindow();

    return 0;
}
