#pragma once

#include <raylib.h>
#include <stdbool.h>
#include <stddef.h>

#define WIN_SIZE_W             800
#define WIN_SIZE_H             450

#define KEY_WIDTH              40
#define KEY_HEIGHT             100
#define KEY_WHITE_COLOR        WHITE
#define KEY_BLACK_COLOR        BLACK
#define KEY_OUTER_COLOR        BLACK

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

#define FONT_SIZE              20
#define MAX_TOUCH_POINTS       10
#define KNOB_RADIUS            25
#define KNOB_SIZE_W            (2 * KNOB_RADIUS + GUI_GAP)
#define KNOB_SIZE_H            (2 * FONT_SIZE + 2 * KNOB_RADIUS + 3 * GUI_GAP)

typedef enum GuiDirection {
    GD_VERTICAL,
    GD_HORIZONTAL
} GuiDirection;

void SetDir(GuiDirection dir);

bool DrawKnob(const char* label, Vector2 *cursor, float radius, float *value, float minValue, float maxValue);
bool DrawTabMenu(Rectangle bounds, const char **labels, int count, int *activeIndex);
bool DrawWave(Vector2 *cursor, Vector2 size, float *buffer, size_t buf_size);
