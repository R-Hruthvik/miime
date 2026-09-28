#include "render.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void render_one_annotation(cairo_t *cr, const MiimeAnnotation *a) {
    if (!a) return;

    cairo_set_source_rgba(cr, a->color_r, a->color_g, a->color_b, a->color_a);
    cairo_set_line_width(cr, a->stroke_width);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

    switch (a->type) {
        case MIIME_ANNOT_PEN:
            if (a->point_count < 4) break;
            cairo_move_to(cr, a->points[0], a->points[1]);
            for (int i = 2; i < a->point_count; i += 2) {
                cairo_line_to(cr, a->points[i], a->points[i + 1]);
            }
            cairo_stroke(cr);
            break;

        case MIIME_ANNOT_LINE:
            if (a->point_count < 4) break;
            cairo_move_to(cr, a->points[0], a->points[1]);
            cairo_line_to(cr, a->points[2], a->points[3]);
            cairo_stroke(cr);
            break;

        case MIIME_ANNOT_RECT: {
            if (a->point_count < 4) break;
            double x = (a->points[0] < a->points[2]) ? a->points[0] : a->points[2];
            double y = (a->points[1] < a->points[3]) ? a->points[1] : a->points[3];
            double w = fabs(a->points[2] - a->points[0]);
            double h = fabs(a->points[3] - a->points[1]);
            cairo_rectangle(cr, x, y, w, h);
            cairo_stroke(cr);
            break;
        }

        case MIIME_ANNOT_ELLIPSE: {
            if (a->point_count < 4) break;
            double cx = (a->points[0] + a->points[2]) / 2.0;
            double cy = (a->points[1] + a->points[3]) / 2.0;
            double rx = fabs(a->points[2] - a->points[0]) / 2.0;
            double ry = fabs(a->points[3] - a->points[1]) / 2.0;
            if (rx < 0.5 || ry < 0.5) break;
            cairo_save(cr);
            cairo_translate(cr, cx, cy);
            cairo_scale(cr, rx, ry);
            cairo_arc(cr, 0, 0, 1.0, 0, 2 * M_PI);
            cairo_restore(cr);
            cairo_stroke(cr);
            break;
        }

        case MIIME_ANNOT_ARROW: {
            if (a->point_count < 4) break;
            double x1 = a->points[0], y1 = a->points[1];
            double x2 = a->points[2], y2 = a->points[3];
            /* Main shaft */
            cairo_move_to(cr, x1, y1);
            cairo_line_to(cr, x2, y2);
            cairo_stroke(cr);

            /* Arrowhead */
            double angle = atan2(y2 - y1, x2 - x1);
            double head_len = a->stroke_width * 4.0;
            if (head_len < 10.0) head_len = 10.0;
            double head_angle = 0.4; /* ~23 degrees */
            double ax = x2 - head_len * cos(angle - head_angle);
            double ay = y2 - head_len * sin(angle - head_angle);
            double bx = x2 - head_len * cos(angle + head_angle);
            double by = y2 - head_len * sin(angle + head_angle);

            cairo_move_to(cr, x2, y2);
            cairo_line_to(cr, ax, ay);
            cairo_move_to(cr, x2, y2);
            cairo_line_to(cr, bx, by);
            cairo_stroke(cr);
            break;
        }

        case MIIME_ANNOT_TEXT:
        case MIIME_ANNOT_BLUR:
        case MIIME_ANNOT_CROP:
            /* Handled in later tasks */
            break;
    }
}

void miime_render_annotations(cairo_t *cr,
                              const MiimeAnnotationStack *stack,
                              const MiimeAnnotation *preview) {
    if (!cr) return;

    if (stack) {
        int count = miime_annot_stack_count(stack);
        for (int i = 0; i < count; i++) {
            render_one_annotation(cr, miime_annot_stack_get(stack, i));
        }
    }

    if (preview) {
        render_one_annotation(cr, preview);
    }
}

void miime_render_on_screen(cairo_t *cr,
                            MiimeImage *img,
                            const MiimeAnnotationStack *stack,
                            const MiimeAnnotation *preview,
                            double zoom,
                            double pan_x, double pan_y,
                            int widget_w, int widget_h) {
    (void)widget_w;
    (void)widget_h;

    if (!cr || !img) return;

    cairo_surface_t *surface = miime_image_get_surface(img);
    if (!surface) return;

    /* Clear background to dark gray */
    cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
    cairo_paint(cr);

    /* Apply viewport transform */
    cairo_save(cr);
    cairo_translate(cr, pan_x, pan_y);
    cairo_scale(cr, zoom, zoom);

    cairo_set_source_surface(cr, surface, 0, 0);

    /* Use appropriate filtering */
    if (zoom > 2.0) {
        cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST);
    } else {
        cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BILINEAR);
    }

    cairo_paint(cr);

    /* Render annotations on top of the image */
    miime_render_annotations(cr, stack, preview);

    cairo_restore(cr);
}
