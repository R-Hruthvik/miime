#include "tools.h"
#include <stdlib.h>
#include <string.h>

struct MiimeToolState {
    MiimeToolType type;
    double color_r, color_g, color_b, color_a;
    double stroke_width;
    gboolean active;          /* TRUE while dragging */
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
    if (ts) ts->type = type;
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
    (void)stack;

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

    /* Final update */
    miime_tool_on_drag(ts, x, y);

    /* Push completed annotation to stack */
    if (stack && ts->current.point_count >= 4) {
        miime_annot_stack_push(stack, ts->current);
    }

    ts->active = FALSE;
    memset(&ts->current, 0, sizeof(MiimeAnnotation));
}

const MiimeAnnotation *miime_tool_get_preview(const MiimeToolState *ts) {
    if (!ts || !ts->active) return NULL;
    return &ts->current;
}
