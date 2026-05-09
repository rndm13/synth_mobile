#include "gui_elements.h"
#include "raylib.h"
#include <pthread.h>
#include <raymath.h>
#include <math.h>
#include <string.h>

#include "utils.h"

#define MAX_GUI_ID_STACK_DEPTH     10
#define MAX_STR_LENGTH_TO_HASH     8

typedef struct DropdownData {
    bool draw_dropdown;
    const char **opt_arr;
    int opt_count;
    int *active_idx;
    Vector2 pos;

    float scroll;
} DropdownData;

typedef struct TKeyboardData {
    bool draw_tkeyboard;
} TKeyboardData;

typedef struct GuiID {
    size_t stack[MAX_GUI_ID_STACK_DEPTH];
    size_t size;
} GuiID;

typedef struct GuiContext {
    GuiID sel_id;
    float mouse_hold_time;
    Vector2 mouse_p;

    // Reset every frame
    GuiID cur_id;
    GuiDirection dir;

    TKeyboardData active_tkeyboard;
    DropdownData active_dropdown;
} GuiContext;

static GuiContext g_ctx;

void start_gui_ctx(void) {
    g_ctx.dir = GD_VERTICAL;

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        g_ctx.mouse_hold_time += GetFrameTime();
        g_ctx.mouse_p = GetMousePosition();
    } else {
        g_ctx.mouse_hold_time = 0;
    }
}

bool mouse_clicked(void) {
    bool mouse_clicked = 0 < g_ctx.mouse_hold_time && g_ctx.mouse_hold_time <= CLICK_MAX_TIME_S;
    return mouse_clicked;
}

static void draw_tkeyboard(void);
static void draw_dropdown_menu(void);

static bool process_tkeyboard(int* c);
static bool process_dropdown_menu(void);

void set_gui_dir(GuiDirection dir) {
    g_ctx.dir = dir;
}

static size_t hash_label(const char* label) {
    size_t hash = 0;

    for (size_t i = 0; label[i] != '\0' && i < MAX_STR_LENGTH_TO_HASH; i++) {
        hash += label[i] * i;
    }

    return hash;
}

static void push_gui_id(const char* label) {
    size_t id = hash_label(label);

    g_ctx.cur_id.stack[g_ctx.cur_id.size++] = id;
}

static void pop_gui_id() {
    g_ctx.cur_id.stack[--g_ctx.cur_id.size] = 0;
}

static void select_top_gui_id() {
    g_ctx.sel_id = g_ctx.cur_id;
}

static void reset_selected_gui_id() {
    memset(&g_ctx.sel_id, 0, sizeof(g_ctx.sel_id));
}

static bool is_selected_top_gui_id() {
    if (g_ctx.cur_id.size != g_ctx.sel_id.size) {
        return false;
    }

    for (size_t i = 0; i < g_ctx.cur_id.size; i++) {
        if (g_ctx.cur_id.stack[i] != g_ctx.sel_id.stack[i]) {
            return false;
        }
    }

    return true;
}


static void update_selected_gui_id(bool selected, bool set, bool reset) {
    if (set) {
        select_top_gui_id();
    } else if (selected && reset) {
        reset_selected_gui_id();
    }
}

#define UPDATE_VALUE(updated, v, new_v, rw)                   \
    do {                                                      \
        if (*(v) != (new_v)) {                                \
            if ((rw) != NULL) {                               \
                pthread_rwlock_wrlock(rw);                    \
            }                                                 \
            *(v) = (new_v);                                   \
            if ((rw) != NULL) {                               \
                pthread_rwlock_unlock(rw);                    \
            }                                                 \
            (updated) = true;                                 \
        }                                                     \
    } while (0);                                              \

static void update_cursor(Vector2* cursor, float width, float height) {
    if (g_ctx.dir == GD_HORIZONTAL) {
        cursor->x += width;
    } else {
        cursor->y += height;
    }
}

static float scale_value(GuiScaling scale, float percent, float min_v, float max_v) {
    if (max_v <= min_v) {
        return NAN;
    }

    switch (scale) {
        case GS_LINEAR:
            return Lerp(min_v, max_v, percent);
        case GS_LOG:
            return min_v * powf(max_v / min_v, percent);
    }
}

static float scale_percent(GuiScaling scale, float v, float min_v, float max_v) {
    if (max_v <= min_v) {
        return NAN;
    }

    switch (scale) {
        case GS_LINEAR:
            return (v - min_v) / (max_v - min_v);
        case GS_LOG:
            if (v > 0) {
                return logf(v / min_v) / logf(max_v / min_v);
            }
            return 0.0f;
    }
}


bool draw_knob(const char *label, Vector2* cursor, float *v, float min_v, float max_v, pthread_rwlock_t *rw) {
    push_gui_id(label);
    float new_v = *v;

    Vector2 m_pos = GetMousePosition();
    Vector2 center_pos = {cursor->x + KNOB_RADIUS, cursor->y + FONT_SIZE + GUI_GAP + KNOB_RADIUS};

    float cur_scaled_percent = (*v - min_v) / (max_v - min_v);
    float cur_angle = KNOB_START_ANGLE + (cur_scaled_percent * KNOB_ANGLE_RANGE);
    float cur_rad_angle = cur_angle * DEG2RAD;
    Vector2 indicator_pos = {
        center_pos.x + cosf(cur_rad_angle) * (KNOB_RADIUS - KNOB_INDICATOR_OFF),
        center_pos.y + sinf(cur_rad_angle) * (KNOB_RADIUS - KNOB_INDICATOR_OFF)
    };
    int text_w = MeasureText(label, FONT_SIZE);

    bool updated = false;
    bool hovered = CheckCollisionPointCircle(m_pos, center_pos, KNOB_RADIUS);
    bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hovered;
    bool reset = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    bool selected = is_selected_top_gui_id();

    update_selected_gui_id(selected, clicked, reset);

    if (selected) {
        float dy = -(m_pos.y - center_pos.y);

        dy = Clamp(dy, KNOB_START_DRAG, KNOB_END_DRAG);

        float percent = (dy - KNOB_START_DRAG) / KNOB_DRAG_RANGE;

        new_v = Lerp(min_v, max_v, percent);
    }

    DrawText(label, cursor->x, cursor->y, FONT_SIZE, TEXT_COLOR);
    DrawText(TextFormat("%.2f", *v), cursor->x, center_pos.y + KNOB_RADIUS + GUI_GAP, FONT_SIZE, TEXT_COLOR);

    // Background
    DrawCircleV(center_pos, KNOB_RADIUS, KNOB_INNER_COLOR);
    // Outer lines
    DrawCircleLines(center_pos.x, center_pos.y, KNOB_RADIUS, KNOB_OUTER_COLOR);
    // TODO: change indicator to a rectangle
    DrawCircleV(indicator_pos, KNOB_INDICATOR_SIZE, KNOB_INDICATOR_COLOR);

    update_cursor(cursor, fmax(text_w + GUI_GAP, KNOB_SIZE_W), KNOB_SIZE_H);

    UPDATE_VALUE(updated, v, new_v, rw);

    pop_gui_id();

    return updated;
}

bool draw_knob_i(const char *label, Vector2* cursor, int *v, int min_v, int max_v, pthread_rwlock_t *rw) {
    float f_value = *v;

    bool updated = draw_knob(label, cursor, &f_value, min_v, max_v, NULL);

    if (updated) {
        UPDATE_VALUE(updated, v, round(f_value), rw);
    }

    return updated;
}

bool draw_slider(const char* label, Vector2 *cursor, Vector2 size, float *v, float min_v, float max_v, GuiScaling scale, pthread_rwlock_t *rw) {
    push_gui_id(label);

    float new_v = *v;
    Rectangle bounds = {cursor->x, cursor->y, size.x, size.y};

    Vector2 m_pos = GetMousePosition();
    float cur_scaled_percent = scale_percent(scale, *v, min_v, max_v);
    const char* v_text = TextFormat("%.2f", *v);
    int v_text_w = MeasureText(v_text, FONT_SIZE);

    Rectangle fill_rec = {
        bounds.x, bounds.y,
        bounds.width * cur_scaled_percent, bounds.height
    };
    Rectangle handle_rec = {
        bounds.x + fill_rec.width - SLIDER_HANDLE_SIZE_W / 2.0f, bounds.y,
        SLIDER_HANDLE_SIZE_W, bounds.height
    };

    bool updated = false;
    bool hovered = CheckCollisionPointRec(m_pos, bounds);
    bool clicked = hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    bool reset = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);

    bool selected = is_selected_top_gui_id();
    update_selected_gui_id(selected, clicked, reset);

    if (selected) {
        float percent = (m_pos.x - bounds.x) / bounds.width;

        percent = Clamp(percent, 0.0f, 1.0f);

        new_v = scale_value(scale, percent, min_v, max_v);
    }

    DrawRectangleRec(bounds, SLIDER_INNER_COLOR);
    DrawRectangleLinesEx(bounds, SLIDER_LINE_THICKNESS, SLIDER_LINE_COLOR);
    DrawRectangleRec(fill_rec, SLIDER_FILLED_COLOR);
    DrawRectangleRec(handle_rec, SLIDER_HANDLE_COLOR);

    DrawText(v_text,
            bounds.x + (bounds.width - v_text_w) / 2.0,
            bounds.y + (bounds.height - FONT_SIZE) / 2.0,
            FONT_SIZE, TEXT_COLOR);

    update_cursor(cursor, size.x + GUI_GAP, size.y + GUI_GAP);

    UPDATE_VALUE(updated, v, new_v, rw);

    pop_gui_id();

    return updated;
}

bool draw_tab_menu(const char* label, Vector2 *cursor, Vector2 size, const char **label_arr, int label_count, int *idx) {
    push_gui_id(label);
    bool updated = false;
    int new_idx = *idx;
    Rectangle bounds = {cursor->x, cursor->y, size.x, size.y};

    float tab_w = size.x / label_count;
    Vector2 m_pos = GetMousePosition();

    for (int i = 0; i < label_count; i++) {
        int text_w = MeasureText(label_arr[i], FONT_SIZE);
        Rectangle tab_rec = {
            bounds.x + (i * tab_w),
            bounds.y,
            tab_w,
            bounds.height
        };

        bool hovered = CheckCollisionPointRec(m_pos, tab_rec);
        bool active = (*idx == i);
        bool clicked = hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

        // Update index on click
        if (clicked) {
            new_idx = i;
        }

        Color bg_col = TAB_INACTIVE_COLOR;
        if (active) {
            bg_col = TAB_ACTIVE_COLOR;
        } else if (hovered) {
            bg_col = TAB_HOVERING_COLOR;
        }

        // Background
        DrawRectangleRec(tab_rec, bg_col);
        // Outer line
        DrawRectangleLinesEx(tab_rec, 1.0f, TAB_OUTER_COLOR);
        // Active accent
        if (active) {
            DrawRectangle(tab_rec.x, tab_rec.y, tab_rec.width, 3, TAB_ACCENT_LINE_COLOR);
        }

        DrawText(label_arr[i],
                 tab_rec.x + (tab_w / 2.0f) - (text_w / 2.0f),
                 tab_rec.y + (tab_rec.height / 2.0f) - (FONT_SIZE / 2.0f),
                 FONT_SIZE,
                 active ? TEXT_COLOR : TEXT_INACTIVE_COLOR);
    }

    update_cursor(cursor, size.x + GUI_GAP, size.y + GUI_GAP);

    UPDATE_VALUE(updated, idx, new_idx, NULL);

    push_gui_id(label_arr[new_idx]);

    return updated;
}

void end_tab_menu(void) {
    pop_gui_id(); // Inner tab
    pop_gui_id(); // The tab menu
}

bool draw_wave(const char* label, Vector2 *cursor, Vector2 size, float *buffer, size_t buf_size, size_t off) {
    push_gui_id(label);
    Rectangle rec = {cursor->x, cursor->y, size.x, size.y};

    DrawRectangle(cursor->x, cursor->y, size.x, size.y, WAVE_INNER_COLOR);
    DrawRectangleLines(cursor->x, cursor->y, size.x, size.y, WAVE_OUTER_COLOR);

    for (int i = 0; i < size.x - 1; i++) {
        int si = i * buf_size / size.x;
        int ei = (i + 1) * buf_size / size.x;

        float min_amp = INFINITY;
        float max_amp = -INFINITY;
        for (int j = si; j <= ei; j++) {
            max_amp = Clamp(fmax(buffer[(j + off) % buf_size], max_amp), -1, 1);
            min_amp = Clamp(fmin(buffer[(j + off) % buf_size], min_amp), -1, 1);
        }

        Vector2 s_pos = { cursor->x + i, cursor->y + size.y / 2 - size.y * min_amp / 2.05f };
        Vector2 e_pos = { cursor->x + i + 1, cursor->y + size.y / 2 - size.y * max_amp / 2.05f };

        DrawLineV(s_pos, e_pos, WAVE_LINE_COLOR);
    }

    update_cursor(cursor, size.x + GUI_GAP, size.y + GUI_GAP);

    bool hovering = CheckCollisionPointRec(GetMousePosition(), rec);
    bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hovering;

    pop_gui_id();

    return clicked;
}

bool draw_button(const char* label, Vector2 *cursor) {
    push_gui_id(label);

    int text_w = MeasureText(label, FONT_SIZE);
    Vector2 size = { GUI_GAP * 2 + text_w, BUTTON_SIZE_H };
    Rectangle rec = { cursor->x, cursor->y, size.x, size.y };

    bool hovering = CheckCollisionPointRec(GetMousePosition(), rec);
    bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hovering;

    DrawRectangle(cursor->x, cursor->y, size.x, size.y, BUTTON_INNER_COLOR);
    DrawRectangleLines(cursor->x, cursor->y, size.x, size.y, BUTTON_LINE_COLOR);
    DrawText(label, cursor->x + GUI_GAP, cursor->y + GUI_GAP, FONT_SIZE, TEXT_COLOR);

    update_cursor(cursor, size.x + GUI_GAP, size.y + GUI_GAP);

    pop_gui_id();

    return clicked;
}

bool draw_toggle(const char* label, Vector2 *cursor, bool *v, pthread_rwlock_t *rw) {
    push_gui_id(label);

    bool new_v = *v;

    int text_w = MeasureText(label, FONT_SIZE);
    Vector2 size = {GUI_GAP * 2 + text_w, GUI_GAP * 2 + FONT_SIZE};
    Rectangle rec = {cursor->x, cursor->y, size.x, size.y};

    bool updated = false;
    bool hovering = CheckCollisionPointRec(GetMousePosition(), rec);
    bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hovering;

    if (clicked) {
        new_v ^= true;
    }

    Color color = TOGGLE_INACTIVE_COLOR;
    if (new_v) {
        color = TOGGLE_ACTIVE_COLOR;
    }

    DrawRectangle(cursor->x, cursor->y, size.x, size.y, color);
    DrawRectangleLines(cursor->x, cursor->y, size.x, size.y, TOGGLE_LINE_COLOR);
    DrawText(label, cursor->x + GUI_GAP, cursor->y + GUI_GAP, FONT_SIZE, TEXT_COLOR);

    update_cursor(cursor, size.x + GUI_GAP, size.y + GUI_GAP);

    UPDATE_VALUE(updated, v, new_v, rw);

    pop_gui_id();

    return updated;
}

bool draw_dropdown(
    const char* label, Vector2 *cursor,
    const char **opt_arr, int opt_count,
    int *active_idx) {
    push_gui_id(label);

    int text_w = MeasureText(label, FONT_SIZE);
    Vector2 size = {GUI_GAP * 2 + text_w, GUI_GAP * 2 + FONT_SIZE};
    Rectangle rec = {cursor->x, cursor->y, size.x, size.y};

    Vector2 dd_pos = {
        cursor->x,
        cursor->y + size.y,
    };

    int old_idx = *active_idx;
    bool updated = false;
    bool hovering = CheckCollisionPointRec(GetMousePosition(), rec);
    bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hovering;
    bool prevent_reset = false;
    bool selected = is_selected_top_gui_id();

    if (clicked) {
        select_top_gui_id();
    }

    if (selected) {
        g_ctx.active_dropdown.opt_arr = opt_arr;
        g_ctx.active_dropdown.opt_count = opt_count;
        g_ctx.active_dropdown.active_idx = active_idx;
        g_ctx.active_dropdown.pos = dd_pos;

        prevent_reset = process_dropdown_menu();
    }

    bool reset = selected && !prevent_reset && !hovering && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    if (reset) {
        memset(&g_ctx.active_dropdown, 0, sizeof(g_ctx.active_dropdown));

        reset_selected_gui_id();
    }

    DrawRectangle(cursor->x, cursor->y, size.x, size.y, DROPDOWN_INNER_COLOR);
    DrawRectangleLines(cursor->x, cursor->y, size.x, size.y, DROPDOWN_LINE_COLOR);
    DrawText(label, cursor->x + GUI_GAP, cursor->y + GUI_GAP, FONT_SIZE, TEXT_COLOR);

    update_cursor(cursor, size.x + GUI_GAP, size.y + GUI_GAP);

    updated = old_idx != *active_idx;

    pop_gui_id();

    return updated;
}

bool process_dropdown_menu() {
    DropdownData *dropdown = &g_ctx.active_dropdown;

    Vector2 pos = dropdown->pos;
    pos.y += dropdown->scroll;

    Rectangle rec = {
        pos.x, pos.y,
        DROPDOWN_ITEM_SIZE_W, DROPDOWN_ITEM_SIZE_H * dropdown->opt_count,
    };

    bool hovering = CheckCollisionPointRec(g_ctx.mouse_p, rec);
    bool dragged = IsMouseButtonDown(MOUSE_BUTTON_LEFT) && hovering;
    bool prevent_reset = dragged;
    bool draw_outside_screen = (rec.y + rec.height) > GetScreenHeight() || rec.y < 0;

    if (dragged && draw_outside_screen) {
        dropdown->scroll += GetMouseDelta().y;
        dropdown->scroll = Clamp(dropdown->scroll, -rec.height, 0);
    }

    for (size_t i = 0; i < dropdown->opt_count; i++) {
        Rectangle item_rec = {
            pos.x, pos.y,
            DROPDOWN_ITEM_SIZE_W, DROPDOWN_ITEM_SIZE_H,
        };

        hovering = CheckCollisionPointRec(g_ctx.mouse_p, item_rec);
        bool clicked = mouse_clicked() && hovering;
        if (clicked) {
            *dropdown->active_idx = i;
        }

        pos.y += DROPDOWN_ITEM_SIZE_H;
    }

    g_ctx.active_dropdown.draw_dropdown = true;

    return prevent_reset;
}

void draw_dropdown_menu() {
    const DropdownData *dropdown = &g_ctx.active_dropdown;
    Vector2 pos = dropdown->pos;
    pos.y += dropdown->scroll;

    for (size_t i = 0; i < dropdown->opt_count; i++) {
        DrawRectangle(
                pos.x, pos.y,
                DROPDOWN_ITEM_SIZE_W, DROPDOWN_ITEM_SIZE_H,
                DROPDOWN_ITEM_INNER_COLOR);
        DrawRectangleLines(
                pos.x, pos.y,
                DROPDOWN_ITEM_SIZE_W, DROPDOWN_ITEM_SIZE_H,
                DROPDOWN_ITEM_LINE_COLOR);
        DrawText(dropdown->opt_arr[i],
                pos.x + DROPDOWN_ITEM_PAD, pos.y + DROPDOWN_ITEM_PAD, FONT_SIZE, TEXT_COLOR);

        pos.y += DROPDOWN_ITEM_SIZE_H;
    }
}

bool draw_text_field(const char* label, Vector2 *cursor, Vector2 size, char* v, size_t v_capacity) {
    push_gui_id(label);
    size_t v_len = strnlen(v, v_capacity);

    if (v_len >= v_capacity) {
        v[v_capacity - 1] = '\0';
        v_len = v_capacity - 1;
    }

    Rectangle rec = {cursor->x, cursor->y, size.x, size.y};
    int text_w = MeasureText(v, FONT_SIZE);
    bool show_cursor = fmod(GetTime(), TEXT_FIELD_CURSOR_INTERVAL * 2) > TEXT_FIELD_CURSOR_INTERVAL;

    bool hovering = CheckCollisionPointRec(GetMousePosition(), rec);
    bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hovering;
    bool prevent_reset = false;
    bool selected = is_selected_top_gui_id();
    if (clicked) {
        select_top_gui_id();
    }

    if (selected) {
        int new_k = '\0';
        prevent_reset = process_tkeyboard(&new_k);

        if (new_k == KEY_BACKSPACE) {
            if (v_len > 0) {
                v[v_len - 1] = '\0';
                v_len--;
            }
        } else if (new_k != '\0' && v_len < v_capacity - 1) {
            v[v_len] = (char)new_k;
            v_len++;
            v[v_len] = '\0';
        }
    }

    // Not clicked on keyboard and clicked outside of input element
    bool reset = selected && !prevent_reset && !hovering && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    if (reset) {
        memset(&g_ctx.active_tkeyboard, 0, sizeof(g_ctx.active_tkeyboard));

        reset_selected_gui_id();
        selected = false;
    }

    DrawRectangle(cursor->x, cursor->y, size.x, size.y, TEXT_FIELD_INNER_COLOR);
    DrawRectangleLines(cursor->x, cursor->y, size.x, size.y, TEXT_FIELD_LINE_COLOR);
    DrawText(v, cursor->x + GUI_GAP, cursor->y + GUI_GAP, FONT_SIZE, TEXT_COLOR);

    if (selected && show_cursor) {
        Rectangle rec = {
                cursor->x + GUI_GAP + text_w, cursor->y + GUI_GAP + FONT_SIZE - TEXT_FIELD_CURSOR_SIZE_H,
                TEXT_FIELD_CURSOR_SIZE_W, TEXT_FIELD_CURSOR_SIZE_H,
        };
        DrawRectangleRec(rec, TEXT_FIELD_CURSOR_COLOR);
    }

    update_cursor(cursor, size.x + GUI_GAP, size.y + GUI_GAP);

    pop_gui_id();

    // Return true only when the user finishes typing.
    return prevent_reset;
}

static int tk_l1_key_arr[] = {
    KEY_ONE, KEY_TWO, KEY_THREE, KEY_FOUR, KEY_FIVE,
    KEY_SIX, KEY_SEVEN, KEY_EIGHT, KEY_NINE, KEY_ZERO,
    KEY_BACKSPACE
};
static int tk_l1_key_size_w_arr[ARRAY_SIZE(tk_l1_key_arr)] = { 0 };
static int tk_l1_size_w;

static int tk_l2_key_arr[] = {
    KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T, KEY_Y,
    KEY_U, KEY_I, KEY_O, KEY_P
};
static int tk_l2_key_size_w_arr[ARRAY_SIZE(tk_l2_key_arr)] = { 0 };

static int tk_l3_key_arr[] = {
    KEY_A, KEY_S, KEY_D, KEY_F, KEY_G,
    KEY_H, KEY_J, KEY_K, KEY_L,
};
static int tk_l3_key_size_w_arr[ARRAY_SIZE(tk_l3_key_arr)] = { 0 };

static int tk_l4_key_arr[] = {
    KEY_Z, KEY_X, KEY_C, KEY_V, KEY_B,
    KEY_N, KEY_M
};
static int tk_l4_key_size_w_arr[ARRAY_SIZE(tk_l4_key_arr)] = { 0 };

static int tk_l5_key_arr[] = {
    KEY_SPACE
};
static int tk_l5_key_size_w_arr[ARRAY_SIZE(tk_l5_key_arr)] = { TKEY_SPACE_SIZE_W };

static int *tk_key_matr[] = {
    tk_l1_key_arr,
    tk_l2_key_arr,
    tk_l3_key_arr,
    tk_l4_key_arr,
    tk_l5_key_arr,
};
static size_t tk_key_arr_size_arr[] = {
    ARRAY_SIZE(tk_l1_key_arr),
    ARRAY_SIZE(tk_l2_key_arr),
    ARRAY_SIZE(tk_l3_key_arr),
    ARRAY_SIZE(tk_l4_key_arr),
    ARRAY_SIZE(tk_l5_key_arr),
};
static int *tk_key_size_w_matr[ARRAY_SIZE(tk_key_matr)] = {
    tk_l1_key_size_w_arr,
    tk_l2_key_size_w_arr,
    tk_l3_key_size_w_arr,
    tk_l4_key_size_w_arr,
    tk_l5_key_size_w_arr,
};
static int tk_line_off_arr[ARRAY_SIZE(tk_key_matr)] = {
    0,
    (TKEY_SIZE_W + TKEY_GAP) * 1 / 2,
    (TKEY_SIZE_W + TKEY_GAP) * 3 / 5,
    (TKEY_SIZE_W + TKEY_GAP),
    (TKEY_SIZE_W + TKEY_GAP) * 3,
};

static const char *get_key_label(int key);

static bool process_tkeyboard(int* c) {
    static bool first_run = true;

    int screen_w = GetScreenWidth();
    int screen_h = GetScreenHeight();

    int size_w = screen_w;
    int size_h = ARRAY_SIZE(tk_key_matr) * (TKEY_SIZE_H + TKEY_GAP) + TKEY_GAP;

    if (first_run) {
        first_run = false;
        for (size_t i = 0; i < ARRAY_SIZE(tk_key_matr) - 1; i++) {
            for (size_t j = 0; j < tk_key_arr_size_arr[i]; j++) {
                tk_key_size_w_matr[i][j] = TKEY_SIZE_W;
            }
        }
        // Set backspace size
        tk_key_size_w_matr[0][tk_key_arr_size_arr[0] - 1] = TKEY_BACKSPACE_SIZE_W;

        for (size_t j = 0; j < tk_key_arr_size_arr[0]; j++) {
            tk_l1_size_w += tk_key_size_w_matr[0][j] + TKEY_GAP;
        }
        tk_l1_size_w -= TKEY_GAP;
    }

    int center_off_x = (screen_w - tk_l1_size_w) / 2;
    Vector2 m_pos = GetMousePosition();

    Rectangle rec = {0, screen_h - size_h, size_w, size_h};
    bool hovered = CheckCollisionPointRec(m_pos, rec);
    bool prevent_reset = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hovered;

    *c = '\0';
    for (size_t i = 0; i < ARRAY_SIZE(tk_key_matr); i++) {
        int x = center_off_x + tk_line_off_arr[i];
        int y = screen_h - size_h + i * (TKEY_SIZE_H + TKEY_GAP) + TKEY_GAP;
        for (size_t j = 0; j < tk_key_arr_size_arr[i]; j++) {
            Rectangle rec = {x, y, tk_key_size_w_matr[i][j], TKEY_SIZE_H};
            bool hovered = CheckCollisionPointRec(m_pos, rec);
            bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hovered;

            if (clicked) {
                *c = tk_key_matr[i][j];
            }

            x += tk_key_size_w_matr[i][j] + TKEY_GAP;
        }
    }

    g_ctx.active_tkeyboard.draw_tkeyboard = true;

    return prevent_reset;
}

void draw_tkeyboard() {
    int screen_w = GetScreenWidth();
    int screen_h = GetScreenHeight();

    int size_w = screen_w - TKEY_GAP * 2;
    int size_h = ARRAY_SIZE(tk_key_matr) * (TKEY_SIZE_H + TKEY_GAP) + TKEY_GAP;
    int l1_size_w = 0;

    for (size_t j = 0; j < tk_key_arr_size_arr[0]; j++) {
        l1_size_w += tk_key_size_w_matr[0][j] + TKEY_GAP;
    }
    l1_size_w -= TKEY_GAP;

    int center_off_x = (screen_w - l1_size_w) / 2;
    Vector2 m_pos = GetMousePosition();

    DrawRectangle(TKEY_GAP, screen_h - size_h, size_w, size_h, TKEY_INNER_COLOR);
    DrawRectangleLines(TKEY_GAP, screen_h - size_h, size_w, size_h, TKEY_LINE_COLOR);

    for (size_t i = 0; i < ARRAY_SIZE(tk_key_matr); i++) {
        int x = center_off_x + tk_line_off_arr[i];
        int y = screen_h - size_h + i * (TKEY_SIZE_H + TKEY_GAP) + TKEY_GAP;
        for (size_t j = 0; j < tk_key_arr_size_arr[i]; j++) {
            Rectangle rec = {x, y, tk_key_size_w_matr[i][j], TKEY_SIZE_H};
            bool hovered = CheckCollisionPointRec(m_pos, rec);
            bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hovered;
            Color color = clicked ? TKEY_ACTIVE_COLOR : TKEY_INACTIVE_COLOR;

            DrawRectangle(x, y, tk_key_size_w_matr[i][j], TKEY_SIZE_H, color);
            DrawRectangleLines(x, y, tk_key_size_w_matr[i][j], TKEY_SIZE_H, TKEY_LINE_COLOR);
            DrawText(get_key_label(tk_key_matr[i][j]), x + GUI_GAP, y + TKEY_GAP, FONT_SIZE, TEXT_COLOR);

            x += tk_key_size_w_matr[i][j] + TKEY_GAP;
        }
    }
}

static const char *get_key_label(int key) {
    switch (key) {
        case KEY_APOSTROPHE      : return "'";          // Key: '
        case KEY_COMMA           : return ",";          // Key: ,
        case KEY_MINUS           : return "-";          // Key: -
        case KEY_PERIOD          : return ".";          // Key: .
        case KEY_SLASH           : return "/";          // Key: /
        case KEY_ZERO            : return "0";          // Key: 0
        case KEY_ONE             : return "1";          // Key: 1
        case KEY_TWO             : return "2";          // Key: 2
        case KEY_THREE           : return "3";          // Key: 3
        case KEY_FOUR            : return "4";          // Key: 4
        case KEY_FIVE            : return "5";          // Key: 5
        case KEY_SIX             : return "6";          // Key: 6
        case KEY_SEVEN           : return "7";          // Key: 7
        case KEY_EIGHT           : return "8";          // Key: 8
        case KEY_NINE            : return "9";          // Key: 9
        case KEY_SEMICOLON       : return ";";          // Key: ;
        case KEY_EQUAL           : return "=";          // Key: =
        case KEY_A               : return "A";          // Key: A | a
        case KEY_B               : return "B";          // Key: B | b
        case KEY_C               : return "C";          // Key: C | c
        case KEY_D               : return "D";          // Key: D | d
        case KEY_E               : return "E";          // Key: E | e
        case KEY_F               : return "F";          // Key: F | f
        case KEY_G               : return "G";          // Key: G | g
        case KEY_H               : return "H";          // Key: H | h
        case KEY_I               : return "I";          // Key: I | i
        case KEY_J               : return "J";          // Key: J | j
        case KEY_K               : return "K";          // Key: K | k
        case KEY_L               : return "L";          // Key: L | l
        case KEY_M               : return "M";          // Key: M | m
        case KEY_N               : return "N";          // Key: N | n
        case KEY_O               : return "O";          // Key: O | o
        case KEY_P               : return "P";          // Key: P | p
        case KEY_Q               : return "Q";          // Key: Q | q
        case KEY_R               : return "R";          // Key: R | r
        case KEY_S               : return "S";          // Key: S | s
        case KEY_T               : return "T";          // Key: T | t
        case KEY_U               : return "U";          // Key: U | u
        case KEY_V               : return "V";          // Key: V | v
        case KEY_W               : return "W";          // Key: W | w
        case KEY_X               : return "X";          // Key: X | x
        case KEY_Y               : return "Y";          // Key: Y | y
        case KEY_Z               : return "Z";          // Key: Z | z
        case KEY_BACKSPACE       : return "BACK";       // Key: Backspace
        case KEY_SPACE           : return "SPACE";      // Key: Space
        default: return "";
    }
}

void finish_gui_ctx(void) {
    if (g_ctx.cur_id.size > 0) {
        // Log...
    };
    memset(&g_ctx.cur_id, 0, sizeof(g_ctx.cur_id));

    if (g_ctx.active_tkeyboard.draw_tkeyboard) {
        draw_tkeyboard();
        g_ctx.active_tkeyboard.draw_tkeyboard = false;
    }

    if (g_ctx.active_dropdown.draw_dropdown) {
        draw_dropdown_menu();
        g_ctx.active_dropdown.draw_dropdown = false;
    }
}
