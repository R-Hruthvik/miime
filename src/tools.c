#include "tools.h"
#include <stdlib.h>
#include <string.h>

struct MiimeToolState {
    MiimeToolType type;
    double color_r, color_g, color_b, color_a;
    double stroke_width;
    gboolean active;          /* TRUE while dragging or typing */
    MiimeAnnotation current;  /* in-progress annotation */
};

MiimeToolState *miime_tool_state_new(void) {
    MiimeToolState *ts = malloc(sizeof(MiimeToolState));
    if (!ts) return NULL;
    ts->type = MIIME_TOOL_PEN;
    ts->color_r = 1.0; ts->color_g = 0.0; ts->color_b = 0.0; ts->color_a = 1.0;
    ts->stroke_width = 3.0;
    ts->active = FALSE;
    memset(&ts->current, 0, sizeof(MiimeAnnotation));
    return ts;
}

void miime_tool_state_free(MiimeToolState *ts) {
    if (ts) {
        if (ts->current.blur_data) {
            free(ts->current.blur_data);
        }
        free(ts);
    }
}

void miime_tool_set_type(MiimeToolState *ts, MiimeToolType type) {
    if (ts) {
        if (ts->active && ts->type == MIIME_TOOL_TEXT) {
            ts->active = FALSE;
            memset(&ts->current, 0, sizeof(MiimeAnnotation));
        }
        ts->type = type;
    }
}

MiimeToolType miime_tool_get_type(const MiimeToolState *ts) {
    return ts ? ts->type : MIIME_TOOL_PEN;
}

void miime_tool_set_color(MiimeToolState *ts, double r, double g, double b, double a) {
    if (!ts) return;
    ts->color_r = r; ts->color_g = g; ts->color_b = b; ts->color_a = a;
}

void miime_tool_set_stroke_width(MiimeToolState *ts, double w) {
    if (ts) ts->stroke_width = w;
}

static void init_annotation(MiimeToolState *ts) {
    if (ts->current.blur_data) {
        free(ts->current.blur_data);
        ts->current.blur_data = NULL;
    }
    memset(&ts->current, 0, sizeof(MiimeAnnotation));
    ts->current.color_r = ts->color_r;
    ts->current.color_g = ts->color_g;
    ts->current.color_b = ts->color_b;
    ts->current.color_a = ts->color_a;
    ts->current.stroke_width = ts->stroke_width;

    switch (ts->type) {
        case MIIME_TOOL_PEN:
            ts->current.type = MIIME_ANNOT_PEN;
            break;
        case MIIME_TOOL_HIGHLIGHTER:
            ts->current.type = MIIME_ANNOT_PEN;
            ts->current.color_a = 0.3; /* forced 30% opacity */
            break;
        case MIIME_TOOL_LINE:
            ts->current.type = MIIME_ANNOT_LINE;
            break;
        case MIIME_TOOL_RECT:
            ts->current.type = MIIME_ANNOT_RECT;
            break;
        case MIIME_TOOL_ELLIPSE:
            ts->current.type = MIIME_ANNOT_ELLIPSE;
            break;
        case MIIME_TOOL_ARROW:
            ts->current.type = MIIME_ANNOT_ARROW;
            break;
        case MIIME_TOOL_BLUR:
            ts->current.type = MIIME_ANNOT_BLUR;
            break;
        case MIIME_TOOL_CROP:
            ts->current.type = MIIME_ANNOT_CROP;
            break;
        case MIIME_TOOL_TEXT:
            ts->current.type = MIIME_ANNOT_TEXT;
            break;
    }
}

void miime_tool_on_press(MiimeToolState *ts, double x, double y,
                         MiimeAnnotationStack *stack) {
    if (!ts) return;

    if (ts->type == MIIME_TOOL_TEXT) {
        if (ts->active && ts->current.type == MIIME_ANNOT_TEXT && ts->current.text[0] != '\0') {
            if (stack) {
                miime_annot_stack_push(stack, ts->current);
            }
        }
        init_annotation(ts);
        ts->current.type = MIIME_ANNOT_TEXT;
        ts->current.points[0] = x;
        ts->current.points[1] = y;
        ts->current.point_count = 2;
        ts->current.font_size = 18.0;
        ts->current.text[0] = '\0';
        ts->active = TRUE;
        return;
    }

    init_annotation(ts);
    ts->active = TRUE;

    if (ts->type == MIIME_TOOL_PEN || ts->type == MIIME_TOOL_HIGHLIGHTER) {
        ts->current.points[0] = x;
        ts->current.points[1] = y;
        ts->current.point_count = 2;
    } else {
        /* Shapes: store start point and initial end point */
        ts->current.points[0] = x;
        ts->current.points[1] = y;
        ts->current.points[2] = x;
        ts->current.points[3] = y;
        ts->current.point_count = 4;
    }
}

void miime_tool_on_drag(MiimeToolState *ts, double x, double y) {
    if (!ts || !ts->active) return;
    if (ts->type == MIIME_TOOL_TEXT) return;

    if (ts->type == MIIME_TOOL_PEN || ts->type == MIIME_TOOL_HIGHLIGHTER) {
        if (ts->current.point_count + 2 <= MIIME_MAX_POINTS) {
            ts->current.points[ts->current.point_count]     = x;
            ts->current.points[ts->current.point_count + 1] = y;
            ts->current.point_count += 2;
        }
    } else {
        /* Shapes: update end point */
        ts->current.points[2] = x;
        ts->current.points[3] = y;
    }
}

void miime_tool_on_release(MiimeToolState *ts, double x, double y,
                           MiimeAnnotationStack *stack) {
    if (!ts || !ts->active) return;

    if (ts->type == MIIME_TOOL_TEXT) {
        /* Don't commit text on mouse release — wait for Enter */
        return;
    }

    /* Final update */
    miime_tool_on_drag(ts, x, y);

    /* Push completed annotation to stack */
    if (stack && ts->current.point_count >= 4) {
        miime_annot_stack_push(stack, ts->current);
    }

    ts->active = FALSE;
    memset(&ts->current, 0, sizeof(MiimeAnnotation));
}

gboolean miime_tool_text_key(MiimeToolState *ts, guint keyval,
                             MiimeAnnotationStack *stack) {
    if (!ts || !ts->active || ts->type != MIIME_TOOL_TEXT) return FALSE;

    if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) {
        if (ts->current.text[0] != '\0') {
            if (stack) {
                miime_annot_stack_push(stack, ts->current);
            }
        }
        ts->active = FALSE;
        memset(&ts->current, 0, sizeof(MiimeAnnotation));
        return TRUE;
    }

    if (keyval == GDK_KEY_Escape) {
        ts->active = FALSE;
        memset(&ts->current, 0, sizeof(MiimeAnnotation));
        return TRUE;
    }

    if (keyval == GDK_KEY_BackSpace) {
        size_t len = strlen(ts->current.text);
        if (len > 0) {
            ts->current.text[len - 1] = '\0';
        }
        return TRUE;
    }

    gunichar ch = gdk_keyval_to_unicode(keyval);
    if (ch && g_unichar_isprint(ch)) {
        size_t len = strlen(ts->current.text);
        if (len < sizeof(ts->current.text) - 6) {
            int written = g_unichar_to_utf8(ch, &ts->current.text[len]);
            ts->current.text[len + written] = '\0';
        }
        return TRUE;
    }

    return FALSE;
}

const MiimeAnnotation *miime_tool_get_preview(const MiimeToolState *ts) {
    if (!ts || !ts->active) return NULL;
    return &ts->current;
}
