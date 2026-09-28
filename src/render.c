#include "render.h"
#include <gdk-pixbuf/gdk-pixbuf.h>

void miime_render_on_screen(cairo_t *cr,
                            MiimeImage *img,
                            double zoom,
                            double pan_x, double pan_y,
                            int widget_w, int widget_h) {
    (void)widget_w;
    (void)widget_h;

    if (!cr || !img) return;

    GdkPixbuf *pb = miime_image_get_pixbuf(img);
    if (!pb) return;

    /* Clear background to dark gray */
    cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
    cairo_paint(cr);

    /* Apply viewport transform */
    cairo_save(cr);
    cairo_translate(cr, pan_x, pan_y);
    cairo_scale(cr, zoom, zoom);

    int img_w = miime_image_width(img);
    int img_h = miime_image_height(img);

    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, img_w, img_h);
    unsigned char *surf_data = cairo_image_surface_get_data(surface);
    int surf_stride = cairo_image_surface_get_stride(surface);

    cairo_surface_flush(surface);

    const guchar *px = gdk_pixbuf_get_pixels(pb);
    int rowstride = gdk_pixbuf_get_rowstride(pb);
    int n_channels = gdk_pixbuf_get_n_channels(pb);
    gboolean has_alpha = gdk_pixbuf_get_has_alpha(pb);

    for (int y = 0; y < img_h; y++) {
        const guchar *src_row = px + y * rowstride;
        unsigned char *dst_row = surf_data + y * surf_stride;
        for (int x = 0; x < img_w; x++) {
            guchar r = src_row[x * n_channels + 0];
            guchar g = src_row[x * n_channels + 1];
            guchar b = src_row[x * n_channels + 2];
            guchar a = has_alpha ? src_row[x * n_channels + 3] : 255;
            /* Cairo ARGB32 is premultiplied, native byte order (B, G, R, A on little-endian) */
            dst_row[x * 4 + 0] = (guchar)((b * a) / 255); /* B */
            dst_row[x * 4 + 1] = (guchar)((g * a) / 255); /* G */
            dst_row[x * 4 + 2] = (guchar)((r * a) / 255); /* R */
            dst_row[x * 4 + 3] = a;                        /* A */
        }
    }

    cairo_surface_mark_dirty(surface);

    cairo_set_source_surface(cr, surface, 0, 0);

    /* Use appropriate filtering */
    if (zoom > 2.0) {
        cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST);
    } else {
        cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BILINEAR);
    }

    cairo_paint(cr);
    cairo_surface_destroy(surface);

    cairo_restore(cr);
}
