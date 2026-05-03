#pragma once

#include <raylib.h>
#include <stdbool.h>
#include <stddef.h>
#include <pthread.h>

#define WIN_SIZE_W             800
#define WIN_SIZE_H             450

#define KEY_WIDTH              40
#define KEY_HEIGHT             100
#define KEY_WHITE_COLOR        WHITE
#define KEY_BLACK_COLOR        BLACK
#define KEY_OUTER_COLOR        BLACK

#define GUI_LABEL_MAX_LEN      128
#define GUI_GAP                10
#define TAB_H                  50

#define BG_COLOR               RAYWHITE
#define TEXT_COLOR             BLACK
#define TEXT_INACTIVE_COLOR    GRAY

#define KNOB_INNER_COLOR       LIGHTGRAY
#define KNOB_OUTER_COLOR       GRAY
#define KNOB_INDICATOR_COLOR   DARKBLUE
#define KNOB_INDICATOR_SIZE    5.0f
#define KNOB_INDICATOR_OFF     10.0f
#define KNOB_START_ANGLE       135.0f
#define KNOB_END_ANGLE         405.0f
#define KNOB_ANGLE_RANGE       (KNOB_END_ANGLE - KNOB_START_ANGLE)

#define KNOB_START_DRAG        -75.0f
#define KNOB_END_DRAG          75.0f
#define KNOB_DRAG_RANGE        (KNOB_END_DRAG - KNOB_START_DRAG)

#define TAB_INACTIVE_COLOR     LIGHTGRAY
#define TAB_ACTIVE_COLOR       RAYWHITE
#define TAB_HOVERING_COLOR     GRAY
#define TAB_OUTER_COLOR        DARKGRAY
#define TAB_ACCENT_LINE_COLOR  SKYBLUE

#define WAVE_INNER_COLOR       LIGHTGRAY
#define WAVE_OUTER_COLOR       BLACK
#define WAVE_LINE_COLOR        RED
#define WAVE_SIZE_W            500
#define WAVE_SIZE_H            150

#define SLIDER_INNER_COLOR     LIGHTGRAY
#define SLIDER_OUTER_COLOR     BLACK
#define SLIDER_FILLED_COLOR    DARKBLUE
#define SLIDER_HANDLE_COLOR    SKYBLUE
#define SLIDER_HANDLE_SIZE_W   10
#define SLIDER_SIZE_W          500
#define SLIDER_SIZE_H          50
#define SLIDER_OUTER_THICKNESS 1.0f

#define FONT_SIZE              20
#define MAX_TOUCH_POINTS       10
#define KNOB_RADIUS            25
#define KNOB_SIZE_W            (2 * KNOB_RADIUS + GUI_GAP)
#define KNOB_SIZE_H            (2 * FONT_SIZE + 2 * KNOB_RADIUS + 3 * GUI_GAP)

typedef enum GuiScaling {
    GS_LINEAR,
    GS_LOG,
} GuiScaling;

typedef enum GuiDirection {
    GD_VERTICAL,
    GD_HORIZONTAL
} GuiDirection;

void reset_gui_ctx(void);
void set_gui_dir(GuiDirection dir);

bool draw_knob(const char* label, Vector2 *cursor, float *value, float min_value, float max_value, pthread_rwlock_t *rw);
bool draw_knob_i(const char* label, Vector2 *cursor, int *value, int min_value, int max_value, pthread_rwlock_t *rw);
bool draw_slider(Vector2 *cursor, Vector2 size, float *value, float min_value, float max_value, GuiScaling scale, pthread_rwlock_t *rw);

bool draw_tab_menu(Vector2 *cursor, Vector2 size, const char **label_arr, int label_count, int *active_index);
bool draw_wave(Vector2 *cursor, Vector2 size, float *buffer, size_t buf_size, size_t off);
