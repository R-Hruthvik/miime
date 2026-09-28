#include "render.h"
#include <math.h>
#include <string.h>
#include <pango/pangocairo.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void render_one_annotation(cairo_t *cr, const MiimeAnnotation *a, gboolean is_preview) {
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

        case MIIME_ANNOT_TEXT: {
            double size = (a->font_size > 0.0) ? a->font_size : 18.0;
            PangoLayout *layout = pango_cairo_create_layout(cr);
            char font_desc_str[64];
            snprintf(font_desc_str, sizeof(font_desc_str), "Sans %.0f", size);
            PangoFontDescription *font = pango_font_description_from_string(font_desc_str);
            pango_layout_set_font_description(layout, font);
            pango_layout_set_text(layout, a->text, -1);

            if (a->text[0] != '\0') {
                cairo_move_to(cr, a->points[0], a->points[1]);
                cairo_set_source_rgba(cr, a->color_r, a->color_g, a->color_b, a->color_a);
                pango_cairo_show_layout(cr, layout);
            }

            if (is_preview) {
                int text_w = 0, text_h = 0;
                pango_layout_get_pixel_size(layout, &text_w, &text_h);
                if (text_h <= 0) text_h = (int)(size * 1.3);
                if (text_h < 12) text_h = 12;

                /* Draw dashed text bounding box */
                cairo_save(cr);
                double dashes[] = {3.0, 3.0};
                cairo_set_dash(cr, dashes, 2, 0.0);
                cairo_set_source_rgba(cr, a->color_r, a->color_g, a->color_b, 0.7);
                cairo_set_line_width(cr, 1.0);
                int box_w = (text_w > 0) ? text_w + 4 : (int)(size * 0.8);
                cairo_rectangle(cr, a->points[0] - 2, a->points[1] - 2, box_w + 4, text_h + 4);
                cairo_stroke(cr);

                /* Draw insertion cursor caret */
                cairo_set_dash(cr, NULL, 0, 0.0);
                cairo_set_source_rgba(cr, a->color_r, a->color_g, a->color_b, 1.0);
                cairo_set_line_width(cr, 2.0);
                double caret_x = a->points[0] + text_w;
                cairo_move_to(cr, caret_x, a->points[1]);
                cairo_line_to(cr, caret_x, a->points[1] + text_h);
                cairo_stroke(cr);
                cairo_restore(cr);
            }

            pango_font_description_free(font);
            g_object_unref(layout);
            break;
        }

        case MIIME_ANNOT_BLUR: {
            if (a->blur_data && a->blur_width >= 1 && a->blur_height >= 1) {
                cairo_surface_t *patch = cairo_image_surface_create_for_data(
                    a->blur_data, CAIRO_FORMAT_ARGB32,
                    a->blur_width, a->blur_height, a->blur_stride);
                cairo_set_source_surface(cr, patch, a->points[0], a->points[1]);
                cairo_paint(cr);
                cairo_surface_destroy(patch);
            } else if (a->point_count >= 4) {
                double x = (a->points[0] < a->points[2]) ? a->points[0] : a->points[2];
                double y = (a->points[1] < a->points[3]) ? a->points[1] : a->points[3];
                double w = fabs(a->points[2] - a->points[0]);
                double h = fabs(a->points[3] - a->points[1]);
                cairo_set_source_rgba(cr, 0.3, 0.6, 1.0, 0.3);
                cairo_rectangle(cr, x, y, w, h);
                cairo_fill(cr);
                cairo_set_source_rgba(cr, 0.3, 0.6, 1.0, 0.8);
                cairo_set_line_width(cr, 1.5);
                cairo_rectangle(cr, x, y, w, h);
                cairo_stroke(cr);
            }
            break;
        }

        case MIIME_ANNOT_CROP: {
            if (a->point_count < 4) break;
            double x = (a->points[0] < a->points[2]) ? a->points[0] : a->points[2];
            double y = (a->points[1] < a->points[3]) ? a->points[1] : a->points[3];
            double w = fabs(a->points[2] - a->points[0]);
            double h = fabs(a->points[3] - a->points[1]);
            if (w < 1 || h < 1) break;

            cairo_save(cr);
            double dashes[] = {6.0, 4.0};
            cairo_set_dash(cr, dashes, 2, 0.0);
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.9);
            cairo_set_line_width(cr, 2.0);
            cairo_rectangle(cr, x, y, w, h);
            cairo_stroke(cr);

            cairo_set_dash(cr, dashes, 2, 5.0);
            cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.9);
            cairo_rectangle(cr, x, y, w, h);
            cairo_stroke(cr);
            cairo_restore(cr);
            break;
        }
    }
}

void miime_render_annotations(cairo_t *cr,
                              const MiimeAnnotationStack *stack,
                              const MiimeAnnotation *preview) {
    if (!cr) return;

    if (stack) {
        int count = miime_annot_stack_count(stack);
        for (int i = 0; i < count; i++) {
            render_one_annotation(cr, miime_annot_stack_get(stack, i), FALSE);
        }
    }

    if (preview) {
        render_one_annotation(cr, preview, TRUE);
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

cairo_surface_t *miime_render_export(MiimeImage *img,
                                     const MiimeAnnotationStack *stack,
                                     int *out_w, int *out_h) {
    if (!img) return NULL;

    int img_w = miime_image_width(img);
    int img_h = miime_image_height(img);
    if (img_w <= 0 || img_h <= 0) return NULL;

    /* Look for latest crop annotation on stack */
    gboolean has_crop = FALSE;
    double crop_x = 0, crop_y = 0, crop_w = img_w, crop_h = img_h;

    if (stack) {
        int count = miime_annot_stack_count(stack);
        for (int i = count - 1; i >= 0; i--) {
            const MiimeAnnotation *a = miime_annot_stack_get(stack, i);
            if (a && a->type == MIIME_ANNOT_CROP && a->point_count >= 4) {
                double x1 = (a->points[0] < a->points[2]) ? a->points[0] : a->points[2];
                double y1 = (a->points[1] < a->points[3]) ? a->points[1] : a->points[3];
                double w = fabs(a->points[2] - a->points[0]);
                double h = fabs(a->points[3] - a->points[1]);
                if (w >= 1 && h >= 1) {
                    crop_x = x1;
                    crop_y = y1;
                    crop_w = w;
                    crop_h = h;
                    has_crop = TRUE;
                    break;
                }
            }
        }
    }

    if (has_crop) {
        if (crop_x < 0) { crop_w += crop_x; crop_x = 0; }
        if (crop_y < 0) { crop_h += crop_y; crop_y = 0; }
        if (crop_x + crop_w > img_w) crop_w = img_w - crop_x;
        if (crop_y + crop_h > img_h) crop_h = img_h - crop_y;
        if (crop_w < 1) crop_w = 1;
        if (crop_h < 1) crop_h = 1;
    }

    int target_w = (int)crop_w;
    int target_h = (int)crop_h;

    cairo_surface_t *out_surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, target_w, target_h);
    cairo_t *cr = cairo_create(out_surf);

    if (has_crop) {
        cairo_translate(cr, -crop_x, -crop_y);
    }

    /* Paint base image */
    cairo_surface_t *base_surf = miime_image_get_surface(img);
    if (base_surf) {
        cairo_set_source_surface(cr, base_surf, 0, 0);
        cairo_paint(cr);
    }

    /* Paint annotations (skipping crop boxes from output render) */
    if (stack) {
        int count = miime_annot_stack_count(stack);
        for (int i = 0; i < count; i++) {
            const MiimeAnnotation *a = miime_annot_stack_get(stack, i);
            if (a && a->type != MIIME_ANNOT_CROP) {
                render_one_annotation(cr, a, FALSE);
            }
        }
    }

    cairo_destroy(cr);

    if (out_w) *out_w = target_w;
    if (out_h) *out_h = target_h;
    return out_surf;
}

gboolean miime_render_save_to_file(cairo_surface_t *surface, const char *filepath) {
    if (!surface || !filepath) return FALSE;

    const char *ext = strrchr(filepath, '.');
    if (ext && (g_ascii_strcasecmp(ext, ".jpg") == 0 || g_ascii_strcasecmp(ext, ".jpeg") == 0)) {
        cairo_surface_flush(surface);
        int w = cairo_image_surface_get_width(surface);
        int h = cairo_image_surface_get_height(surface);
        const unsigned char *data = cairo_image_surface_get_data(surface);
        int stride = cairo_image_surface_get_stride(surface);

        GdkPixbuf *pb = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, w, h);
        if (!pb) return FALSE;
        guchar *dst_pixels = gdk_pixbuf_get_pixels(pb);
        int dst_stride = gdk_pixbuf_get_rowstride(pb);

        for (int y = 0; y < h; y++) {
            const unsigned char *src_row = data + y * stride;
            guchar *dst_row = dst_pixels + y * dst_stride;
            for (int x = 0; x < w; x++) {
                dst_row[x * 3 + 0] = src_row[x * 4 + 2]; /* R */
                dst_row[x * 3 + 1] = src_row[x * 4 + 1]; /* G */
                dst_row[x * 3 + 2] = src_row[x * 4 + 0]; /* B */
            }
        }
        GError *err = NULL;
        gboolean ok = gdk_pixbuf_save(pb, filepath, "jpeg", &err, "quality", "95", NULL);
        g_object_unref(pb);
        if (err) {
            g_printerr("miime: error saving jpeg: %s\n", err->message);
            g_error_free(err);
        }
        return ok;
    } else {
        cairo_status_t st = cairo_surface_write_to_png(surface, filepath);
        return (st == CAIRO_STATUS_SUCCESS);
    }
}
