#include "tools.h"
#include "image.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

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

void miime_tool_compute_blur(MiimeToolState *ts, MiimeImage *img) {
    if (!ts || !ts->active || ts->type != MIIME_TOOL_BLUR || !img) return;

    MiimeAnnotation *a = &ts->current;
    double x = (a->points[0] < a->points[2]) ? a->points[0] : a->points[2];
    double y = (a->points[1] < a->points[3]) ? a->points[1] : a->points[3];
    double w = fabs(a->points[2] - a->points[0]);
    double h = fabs(a->points[3] - a->points[1]);

    int bx = (int)x, by = (int)y, bw = (int)w, bh = (int)h;
    if (bw < 1 || bh < 1) return;

    int img_w = miime_image_width(img);
    int img_h = miime_image_height(img);
    if (bx < 0) { bw += bx; bx = 0; }
    if (by < 0) { bh += by; by = 0; }
    if (bx + bw > img_w) bw = img_w - bx;
    if (by + bh > img_h) bh = img_h - by;
    if (bw < 1 || bh < 1) return;

    cairo_surface_t *surf = miime_image_get_surface(img);
    if (!surf) return;

    cairo_surface_flush(surf);
    const unsigned char *pixels = cairo_image_surface_get_data(surf);
    int stride = cairo_image_surface_get_stride(surf);

    int factor = 8;
    a->blur_width = bw;
    a->blur_height = bh;
    a->blur_stride = bw * 4;
    a->blur_data = malloc(bw * bh * 4);
    if (!a->blur_data) return;

    /* Pixelate: for each factor x factor block, average colors in ARGB32 */
    for (int block_y = 0; block_y < bh; block_y += factor) {
        for (int block_x = 0; block_x < bw; block_x += factor) {
            int cur_bw = (block_x + factor <= bw) ? factor : (bw - block_x);
            int cur_bh = (block_y + factor <= bh) ? factor : (bh - block_y);

            long long sum_b = 0, sum_g = 0, sum_r = 0, sum_a = 0;
            int count = 0;

            for (int dy = 0; dy < cur_bh; dy++) {
                const unsigned char *row = pixels + (by + block_y + dy) * stride;
                for (int dx = 0; dx < cur_bw; dx++) {
                    int sx = bx + block_x + dx;
                    const unsigned char *p = row + sx * 4;
                    sum_b += p[0];
                    sum_g += p[1];
                    sum_r += p[2];
                    sum_a += p[3];
                    count++;
                }
            }

            unsigned char avg_b = (count > 0) ? (unsigned char)(sum_b / count) : 0;
            unsigned char avg_g = (count > 0) ? (unsigned char)(sum_g / count) : 0;
            unsigned char avg_r = (count > 0) ? (unsigned char)(sum_r / count) : 0;
            unsigned char avg_a = (count > 0) ? (unsigned char)(sum_a / count) : 255;

            for (int dy = 0; dy < cur_bh; dy++) {
                unsigned char *dst_row = a->blur_data + (block_y + dy) * a->blur_stride;
                for (int dx = 0; dx < cur_bw; dx++) {
                    int px_idx = (block_x + dx) * 4;
                    dst_row[px_idx + 0] = avg_b;
                    dst_row[px_idx + 1] = avg_g;
                    dst_row[px_idx + 2] = avg_r;
                    dst_row[px_idx + 3] = avg_a;
                }
            }
        }
    }

    a->points[0] = bx;
    a->points[1] = by;
    a->points[2] = bx + bw;
    a->points[3] = by + bh;
}

const MiimeAnnotation *miime_tool_get_preview(const MiimeToolState *ts) {
    if (!ts || !ts->active) return NULL;
    return &ts->current;
}
