#pragma once

#include <raylib.h>
#include <stdbool.h>
#include <stddef.h>
#include <pthread.h>

#define COL_BG1                              CLITERAL(Color){0x07, 0x02, 0x0D, 0xFF}
#define COL_BG2                              CLITERAL(Color){0x2E, 0x29, 0x24, 0xFF}
#define COL_FG1                              CLITERAL(Color){0xF2, 0xF4, 0xF3, 0xFF}
#define COL_FG2                              CLITERAL(Color){0xD3, 0xD9, 0xD6, 0xFF}
#define COL_AC1                              CLITERAL(Color){0x76, 0x2C, 0x77, 0xFF}
#define COL_AC2                              CLITERAL(Color){0x4A, 0x1C, 0x4A, 0xFF}

#define WIN_SIZE_W                           800
#define WIN_SIZE_H                           450

#define KEY_WIDTH                            40
#define KEY_HEIGHT                           100
#define KEY_WHITE_COLOR                      COL_AC1
#define KEY_BLACK_COLOR                      COL_AC2
#define KEY_DOWN_COLOR                       COL_BG2
#define KEY_LINE_COLOR                       COL_BG2
#define KEY_LINE_THICKNESS                   2.0f

#define GUI_LABEL_MAX_LEN                    128
#define GUI_GAP                              10

#define BG_COLOR                             COL_BG1
#define TEXT_COLOR                           COL_FG1
#define TEXT_INACTIVE_COLOR                  COL_FG2

#define KNOB_INNER_COLOR                     COL_BG2
#define KNOB_LINE_COLOR                      COL_AC2
#define KNOB_LINE_THICKNESS                  2.0f
#define KNOB_INDICATOR_COLOR                 COL_FG1
#define KNOB_START_ANGLE                     135.0f
#define KNOB_END_ANGLE                       405.0f
#define KNOB_ANGLE_RANGE                     (KNOB_END_ANGLE - KNOB_START_ANGLE)
#define KNOB_RADIUS                          25.0f
#define KNOB_SIZE_W                          (2 * KNOB_RADIUS)
#define KNOB_SIZE_H                          (2 * FONT_SIZE + 2 * KNOB_RADIUS + 2 * GUI_GAP)
#define KNOB_INDICATOR_RADIUS                20.0f
#define KNOB_INDICATOR_SIZE_H                3.0f
#define KNOB_INDICATOR_SIZE_W                7.0f

#define KNOB_START_DRAG                      -75.0f
#define KNOB_END_DRAG                        75.0f
#define KNOB_DRAG_RANGE                      (KNOB_END_DRAG - KNOB_START_DRAG)

#define TAB_INACTIVE_COLOR                   COL_BG1
#define TAB_ACTIVE_COLOR                     COL_BG2
#define TAB_HOVERING_COLOR                   COL_AC2
#define TAB_OUTER_COLOR                      COL_AC2
#define TAB_ACCENT_LINE_COLOR                COL_AC1
#define TAB_LINE_THICKNESS                   2.0f
#define TAB_SIZE_H                           50

#define BUTTON_INNER_COLOR                   COL_BG1
#define BUTTON_ACTIVE_COLOR                  COL_AC2
#define BUTTON_LINE_COLOR                    COL_BG2
#define BUTTON_LINE_THICKNESS                2.0f
#define BUTTON_SIZE_H                        (FONT_SIZE + GUI_GAP * 2)

#define TOGGLE_INACTIVE_COLOR                COL_BG1
#define TOGGLE_ACTIVE_COLOR                  COL_AC2
#define TOGGLE_LINE_COLOR                    COL_BG2
#define TOGGLE_LINE_THICKNESS                2.0f

#define WAVE_INNER_COLOR                     COL_BG2
#define WAVE_OUTER_COLOR                     COL_AC2
#define WAVE_LINE_COLOR                      COL_AC1
#define WAVE_LINE_THICKNESS                  2.0f
#define WAVE_SIZE_W                          500
#define WAVE_SIZE_H                          150

#define SLIDER_INNER_COLOR                   COL_AC2
#define SLIDER_LINE_COLOR                    COL_BG2
#define SLIDER_LINE_THICKNESS                2.0f
#define SLIDER_FILLED_COLOR                  COL_AC1
#define SLIDER_HANDLE_COLOR                  COL_FG2
#define SLIDER_HANDLE_SIZE_W                 10
#define SLIDER_SIZE_W                        500
#define SLIDER_SIZE_H                        50

#define TKEY_SIZE_W                          55
#define TKEY_SPACE_SIZE_W                    (TKEY_SIZE_W * 5)
#define TKEY_BACKSPACE_SIZE_W                (TKEY_SIZE_W + 20)
#define TKEY_SIZE_H                          30
#define TKEY_GAP                             5

#define TKEY_ACTIVE_COLOR                    COL_AC1
#define TKEY_INACTIVE_COLOR                  COL_BG1
#define TKEY_LINE_COLOR                      COL_AC2
#define TKEY_LINE_THICKNESS                  2.0f
#define TKEY_INNER_COLOR                     COL_BG2

#define TEXT_FIELD_CURSOR_COLOR              TEXT_COLOR
#define TEXT_FIELD_CURSOR_INTERVAL           0.5
#define TEXT_FIELD_CURSOR_SIZE_W             10
#define TEXT_FIELD_CURSOR_SIZE_H             3
#define TEXT_FIELD_INNER_COLOR               COL_BG1
#define TEXT_FIELD_LINE_COLOR                COL_BG2
#define TEXT_FIELD_LINE_THICKNESS            2.0f
#define TEXT_FIELD_SIZE_W                    250
#define TEXT_FIELD_SIZE_H                    (FONT_SIZE + GUI_GAP * 2)

#define DROPDOWN_INNER_COLOR                 COL_BG1
#define DROPDOWN_LINE_COLOR                  COL_BG2
#define DROPDOWN_LINE_THICKNESS              2.0f

#define DROPDOWN_ITEM_INNER_COLOR            COL_BG1
#define DROPDOWN_ITEM_ACTIVE_COLOR           COL_AC2
#define DROPDOWN_ITEM_LINE_COLOR             COL_BG2
#define DROPDOWN_ITEM_LINE_THICKNESS         2.0f
#define DROPDOWN_ITEM_SIZE_W                 300
#define DROPDOWN_ITEM_SIZE_H                 30
#define DROPDOWN_ITEM_PAD                    5

#define TOAST_INNER_COLOR                    COL_BG1
#define TOAST_LINE_COLOR                     COL_BG2
#define TOAST_MAX_SIZE_W                     400
#define TOAST_LINE_THICKNESS                 2.0f
#define TOAST_INITIAL_TTL                    5.0f
#define TOAST_SPEED                          1000
#define TOAST_MAX_DRAG_X                     100

#define MAX_TOAST_COUNT                      5

#define FONT_SIZE                            20
#define MAX_TOUCH_POINTS                     10

#define MAX_CLICK_TIME_S                     0.1

typedef enum GuiScaling {
    GS_LINEAR,
    GS_LOG,
} GuiScaling;

typedef enum GuiDirection {
    GD_VERTICAL,
    GD_HORIZONTAL
} GuiDirection;

void start_gui_ctx(void);
void finish_gui_ctx(void);

void push_gui_id_i(int id);
void push_gui_id(const char* label);
void pop_gui_id();

void set_gui_dir(GuiDirection dir);

void draw_text(const char* label, Vector2 *cursor);

bool draw_knob(const char* label, Vector2 *cursor, float *value, float min_value, float max_value, pthread_rwlock_t *rw);
bool draw_knob_i(const char* label, Vector2 *cursor, int *value, int min_value, int max_value, pthread_rwlock_t *rw);

bool draw_slider(const char* label, Vector2 *cursor, Vector2 size, float *value, float min_value, float max_value, GuiScaling scale, pthread_rwlock_t *rw);

bool draw_tab_menu(const char* label, Vector2 *cursor, Vector2 size, const char **label_arr, int label_count, int *active_index);

void end_tab_menu(void);

bool draw_wave(const char* label, Vector2 *cursor, Vector2 size, float *buffer, size_t buf_size, size_t off);

bool draw_button(const char* label, Vector2 *cursor);

bool draw_toggle(const char* label, Vector2 *cursor, bool *v, pthread_rwlock_t *rw);

bool draw_text_field(const char* label, Vector2 *cursor, Vector2 size, char* v, size_t v_capacity);

bool draw_dropdown(
    const char* label, Vector2 *cursor,
    const char **opt_arr, int opt_count,
    int *active_idx);

void add_toast(const char* fmt, ...);
