#pragma once

#include <raylib.h>
#include <stdbool.h>

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

#define FONT_SIZE         20
#define MAX_TOUCH_POINTS  10
#define KNOB_RADIUS       25
#define KNOB_SIZE_H       FONT_SIZE + 2 * GUI_GAP + 2 * KNOB_RADIUS

bool DrawKnob(const char* label, Vector2 center, float radius, float *value, float minValue, float maxValue);
bool DrawTabMenu(Rectangle bounds, const char **labels, int count, int *activeIndex);
