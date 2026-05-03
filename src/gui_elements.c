#include "gui_elements.h"
#include "raylib.h"
#include <pthread.h>
#include <raymath.h>
#include <math.h>
#include <string.h>

typedef struct GuiContext {
    // Persistent, 0 means that nothing is selected
    size_t selected_idx;

    size_t cur_idx;
    GuiDirection dir;
} GuiContext;

static GuiContext g_ctx;

void reset_gui_ctx(void) {
    g_ctx.cur_idx = 1;
    g_ctx.dir = GD_VERTICAL;
}

void set_gui_dir(GuiDirection dir) {
    g_ctx.dir = dir;
}

static size_t get_gui_idx(void) {
    return g_ctx.cur_idx++;
}

// Returns if the element is selected
static bool update_gui_selected_idx(bool clicked, bool reset, size_t idx) {
    if (clicked) {
        g_ctx.selected_idx = idx;
    } else if (reset) {
        g_ctx.selected_idx = 0;
    }

    return g_ctx.selected_idx == idx;
}

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

static bool update_value(float* v, float new_v, pthread_rwlock_t *rw) {
    bool updated = false;

    if (*v != new_v) {
        int e = 0;
        if (rw != NULL) {
            e = pthread_rwlock_wrlock(rw);
            if (e != 0) {
                // TODO: Log
                return false;
            }
        }

        *v = new_v;

        if (rw != NULL) {
            e = pthread_rwlock_unlock(rw);
            if (e != 0) {
                // TODO: Log
                return true;
            }
        }

        updated = true;
    }

    return updated;
}

static bool update_value_i(int* v, int new_v, pthread_rwlock_t *rw) {
    bool updated = false;

    if (*v != new_v) {
        int e = 0;
        if (rw != NULL) {
            e = pthread_rwlock_wrlock(rw);
            if (e != 0) {
                // TODO: Log
                return false;
            }
        }

        *v = new_v;

        if (rw != NULL) {
            e = pthread_rwlock_unlock(rw);
            if (e != 0) {
                // TODO: Log
                return true;
            }
        }

        updated = true;
    }

    return updated;
}

bool draw_knob(const char *label, Vector2* cursor, float *v, float min_v, float max_v, pthread_rwlock_t *rw) {
    size_t gui_idx = get_gui_idx();
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

    bool hovered = CheckCollisionPointCircle(m_pos, center_pos, KNOB_RADIUS);
    bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hovered;
    bool reset = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    bool selected = update_gui_selected_idx(clicked, reset, gui_idx);

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

    return update_value(v, new_v, rw);
}

bool draw_knob_i(const char *label, Vector2* cursor, int *v, int min_v, int max_v, pthread_rwlock_t *rw) {
    float f_value = *v;

    bool changed = draw_knob(label, cursor, &f_value, min_v, max_v, NULL);

    if (changed) {
        changed = update_value_i(v, round(f_value), rw);
    }

    return changed;
}

bool draw_slider(Vector2 *cursor, Vector2 size, float *v, float min_v, float max_v, GuiScaling scale, pthread_rwlock_t *rw) {
    size_t gui_idx = get_gui_idx();
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

    bool hovered = CheckCollisionPointRec(m_pos, bounds);
    bool clicked = hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    bool reset = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    bool selected = update_gui_selected_idx(clicked, reset, gui_idx);

    if (selected) {
        float percent = (m_pos.x - bounds.x) / bounds.width;

        percent = Clamp(percent, 0.0f, 1.0f);

        new_v = scale_value(scale, percent, min_v, max_v);
    }

    DrawRectangleRec(bounds, SLIDER_INNER_COLOR);
    DrawRectangleLinesEx(bounds, SLIDER_OUTER_THICKNESS, SLIDER_OUTER_COLOR);
    DrawRectangleRec(fill_rec, SLIDER_FILLED_COLOR);
    DrawRectangleRec(handle_rec, SLIDER_HANDLE_COLOR);

    DrawText(v_text,
            bounds.x + (bounds.width - v_text_w) / 2.0,
            bounds.y + (bounds.height - FONT_SIZE) / 2.0,
            FONT_SIZE, TEXT_COLOR);

    update_cursor(cursor, size.x + GUI_GAP, size.y + GUI_GAP);

    return update_value(v, new_v, rw);
}

// TODO: make this use GUI index
bool draw_tab_menu(Vector2 *cursor, Vector2 size, const char **label_arr, int label_count, int *idx) {
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

    return update_value_i(idx, new_idx, NULL);
}

bool draw_wave(Vector2 *cursor, Vector2 size, float *buffer, size_t buf_size, size_t off) {
    DrawRectangle(cursor->x, cursor->y, size.x, size.y, WAVE_INNER_COLOR);
    DrawRectangleLines(cursor->x, cursor->y, size.x, size.y, WAVE_OUTER_COLOR);
    Rectangle rec = {cursor->x, cursor->y, size.x, size.y};

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

    if (g_ctx.dir == GD_HORIZONTAL) {
        cursor->x += size.x + GUI_GAP;
    } else {
        cursor->y += size.y + GUI_GAP;
    }

    bool hovering = CheckCollisionPointRec(GetMousePosition(), rec);
    bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hovering;

    return clicked;
}
