#include "gui_elements.h"
#include <pthread.h>
#include <raymath.h>

static GuiDirection g_dir = GD_VERTICAL;

void SetDir(GuiDirection dir) {
    g_dir = dir;
}

// Draws a rotary knob and updates the value if the user interacts with it.
// Returns true if the value was modified this frame.
bool DrawKnob(const char *label, Vector2* cursor, float radius, float *value, float minValue, float maxValue, pthread_rwlock_t *rw) {
    // Define the visual arc limits in degrees.
    // In raylib, 0 is right, 90 is down.
    // 135 is bottom-left. 405 is bottom-right (360 + 45).
    const float startAngle = 135.0f;
    const float endAngle = 405.0f;
    const float angleRange = endAngle - startAngle;
    float newValue = *value;
    bool valueChanged = false;

    // 1. Handle Input
    Vector2 mousePos = GetMousePosition();
    Vector2 center = {cursor->x + KNOB_RADIUS, cursor->y + FONT_SIZE + GUI_GAP + KNOB_RADIUS};

    // Check if mouse is held down and within the knob's radius
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && CheckCollisionPointCircle(mousePos, center, radius)) {

        // Calculate angle between center and mouse
        float dx = mousePos.x - center.x;
        float dy = mousePos.y - center.y;
        float angle = atan2f(dy, dx) * RAD2DEG; // Returns -180 to +180

        // Unwrap the angle so the "dead zone" seam is at 90 degrees (straight down)
        // This maps the -180...180 range to 90...450 smoothly.
        if (angle < 90.0f) {
            angle += 360.0f;
        }

        // Clamp the angle to our visual limits so it doesn't spin freely through the bottom
        if (angle < startAngle) angle = startAngle;
        if (angle > endAngle) angle = endAngle;

        // Map the angle back to a percentage (0.0 to 1.0) and then to the value range
        float percent = (angle - startAngle) / angleRange;
        newValue = minValue + percent * (maxValue - minValue);
    }

    // 2. Draw the visual components
    // Draw the background base
    int textWidth = MeasureText(label, FONT_SIZE);
    DrawText(label, cursor->x, cursor->y, FONT_SIZE, TEXT_COLOR);
    DrawText(TextFormat("%.2f", *value), cursor->x, center.y + radius + GUI_GAP , FONT_SIZE, TEXT_COLOR);
    DrawCircleV(center, radius, KNOB_INNER_COLOR);
    DrawCircleLines(center.x, center.y, radius, KNOB_OUTER_COLOR);

    // Calculate where the indicator should point based on the CURRENT value
    float currentPercent = 0.0f;
    if (maxValue > minValue) {
        currentPercent = (*value - minValue) / (maxValue - minValue);
    }

    // Convert the percentage back into radians for drawing
    float currentAngle = startAngle + (currentPercent * angleRange);
    float radianAngle = currentAngle * DEG2RAD;

    // Calculate the position of the indicator dot using sine and cosine
    Vector2 indicatorPos = {
        center.x + cosf(radianAngle) * (radius - KNOB_INDICATOR_OFF), // 10 pixels inset from the edge
        center.y + sinf(radianAngle) * (radius - KNOB_INDICATOR_OFF)
    };

    // Draw the indicator dot
    DrawCircleV(indicatorPos, KNOB_INDICATOR_SIZE, KNOB_INDICATOR_COLOR);

    if (g_dir == GD_HORIZONTAL) {
        cursor->x += fmax(textWidth + GUI_GAP, KNOB_SIZE_W);
    } else {
        cursor->y += KNOB_SIZE_H;
    }

    if (*value != newValue) {
        int e = 0;
        if (rw != NULL) {
            e = pthread_rwlock_wrlock(rw);
            if (e != 0) {
                // TODO: Log
                return false;
            }
        }

        *value = newValue;

        if (rw != NULL) {
            e = pthread_rwlock_unlock(rw);
            if (e != 0) {
                // TODO: Log
                return true;
            }
        }

        valueChanged = true;
    }

    return valueChanged;
}

bool DrawKnobI(const char *label, Vector2* cursor, float radius, int *value, int minValue, int maxValue, pthread_rwlock_t *rw) {
    float f_value = *value;

    bool changed = DrawKnob(label, cursor, radius, &f_value, minValue, maxValue, NULL);
    if (changed) {
        int e = 0;
        if (rw != NULL) {
            e = pthread_rwlock_wrlock(rw);
            if (e != 0) {
                // TODO: Log
                return false;
            }
        }

        *value = round(f_value);

        if (rw != NULL) {
            e = pthread_rwlock_unlock(rw);
            if (e != 0) {
                // TODO: Log
                return true;
            }
        }
    }

    return changed;
}

// Draws a slider and updates the value if the user interacts with it.
// Returns true if the value was modified this frame.
bool DrawSlider(Vector2 *cursor, Vector2 size, float *value, float minValue, float maxValue, pthread_rwlock_t *rw) {
    bool valueChanged = false;
    float newValue = *value;
    Rectangle bounds = {cursor->x, cursor->y, size.x, size.y};

    // 1. Handle Input
    Vector2 mousePos = GetMousePosition();
    bool isHovering = CheckCollisionPointRec(mousePos, bounds);

    // If the mouse is pressed or held down while over the slider bounds
    if (isHovering && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        // Calculate where the mouse is relative to the width of the slider (0.0 to 1.0)
        float percent = (mousePos.x - bounds.x) / bounds.width;

        // Clamp the percentage to prevent the value from exceeding min/max limits
        if (percent < 0.0f) percent = 0.0f;
        if (percent > 1.0f) percent = 1.0f;

        // Map the percentage back to the value range
        newValue = minValue + percent * (maxValue - minValue);
    }

    // 2. Draw the visual components
    // Draw the background track
    DrawRectangleRec(bounds, SLIDER_INNER_COLOR);
    DrawRectangleLinesEx(bounds, SLIDER_OUTER_THIKNESS, SLIDER_OUTER_COLOR);

    // Calculate how much of the slider should be "filled"
    float percentFilled = 0.0f;
    if (maxValue > minValue) { // Prevent division by zero
        percentFilled = (*value - minValue) / (maxValue - minValue);
    }

    // Draw the filled portion
    Rectangle fillRec = { bounds.x, bounds.y, bounds.width * percentFilled, bounds.height };
    DrawRectangleRec(fillRec, SLIDER_FILLED_COLOR);

    // Draw the handle (thumb)
    Rectangle handleRec = { bounds.x + fillRec.width - 5, bounds.y - 2, 10, bounds.height + 4 };
    DrawRectangleRec(handleRec, SLIDER_THUMB_COLOR);

    if (g_dir == GD_HORIZONTAL) {
        cursor->x += size.x + GUI_GAP;
    } else {
        cursor->y += size.y + GUI_GAP;
    }

    if (*value != newValue) {
        int e = 0;
        if (rw != NULL) {
            e = pthread_rwlock_wrlock(rw);
            if (e != 0) {
                // TODO: Log
                return false;
            }
        }

        *value = newValue;

        if (rw != NULL) {
            e = pthread_rwlock_unlock(rw);
            if (e != 0) {
                // TODO: Log
                return true;
            }
        }

        valueChanged = true;
    }

    return valueChanged;
}

bool DrawTabMenu(Rectangle bounds, const char **labels, int count, int *activeIndex) {
    bool indexChanged = false;
    float tabWidth = bounds.width / count;
    Vector2 mousePos = GetMousePosition();

    for (int i = 0; i < count; i++) {
        // Calculate the rectangle for this specific tab
        Rectangle tabRec = {
            bounds.x + (i * tabWidth),
            bounds.y,
            tabWidth,
            bounds.height
        };

        bool isHovering = CheckCollisionPointRec(mousePos, tabRec);
        bool isActive = (*activeIndex == i);

        // Logic: Update index on click
        if (isHovering && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (*activeIndex != i) {
                *activeIndex = i;
                indexChanged = true;
            }
        }

        // --- Visuals ---
        // Draw background: Use a darker color for inactive, lighter for active
        Color bgColor = isActive ? TAB_ACTIVE_COLOR : TAB_INACTIVE_COLOR;
        if (isHovering && !isActive) bgColor = TAB_HOVERING_COLOR;

        DrawRectangleRec(tabRec, bgColor);

        // Draw an outline for the tab
        DrawRectangleLinesEx(tabRec, 1.0f, TAB_OUTER_COLOR);

        // Draw an accent line at the top for the active tab
        if (isActive) {
            DrawRectangle(tabRec.x, tabRec.y, tabRec.width, 3, TAB_ACCENT_LINE_COLOR);
        }

        // Center the text inside the tab
        int textWidth = MeasureText(labels[i], FONT_SIZE);
        DrawText(labels[i],
                 tabRec.x + (tabWidth / 2) - (textWidth / 2),
                 tabRec.y + (tabRec.height / 2) - (FONT_SIZE / 2),
                 FONT_SIZE,
                 isActive ? TEXT_COLOR : TEXT_INACTIVE_COLOR);
    }

    return indexChanged;
}

bool DrawWave(Vector2 *cursor, Vector2 size, float *buffer, size_t buf_size) {
    DrawRectangle(cursor->x, cursor->y, size.x, size.y, WAVE_INNER_COLOR);
    DrawRectangleLines(cursor->x, cursor->y, size.x, size.y, WAVE_OUTER_COLOR);
    Rectangle rec = {cursor->x, cursor->y, size.x, size.y};

    for (int i = 0; i < size.x - 1; i++) {
        int si = i * buf_size / size.x;
        int ei = (i + 1) * buf_size / size.x;

        Vector2 s_pos = { cursor->x + i, cursor->y + size.y / 2 - size.y * buffer[si] / 2.1f };
        Vector2 e_pos = { cursor->x + i + 1, cursor->y + size.y / 2 - size.y * buffer[ei] / 2.1f };

        DrawLineV(s_pos, e_pos, WAVE_LINE_COLOR);
    }

    if (g_dir == GD_HORIZONTAL) {
        cursor->x += size.x + GUI_GAP;
    } else {
        cursor->y += size.y + GUI_GAP;
    }

    bool hovering = CheckCollisionPointRec(GetMousePosition(), rec);
    bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hovering;

    return clicked;
}
