#include "render.h"

void miime_render_on_screen(cairo_t *cr,
                            MiimeImage *img,
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
    cairo_restore(cr);
}
