#include "canvas.h"
#include "render.h"
#include <stdlib.h>

struct MiimeCanvas {
    GtkWidget   *drawing_area;
    MiimeImage  *image;      /* borrowed, not owned */
    double       zoom;
    double       pan_x;
    double       pan_y;
    gboolean     fit_mode;   /* TRUE = fit-to-window */
};

static void canvas_draw_cb(GtkDrawingArea *area, cairo_t *cr,
                           int width, int height, gpointer user_data) {
    MiimeCanvas *c = (MiimeCanvas *)user_data;
    (void)area;

    if (!c->image) {
        cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
        cairo_paint(cr);
        return;
    }

    double zoom = c->zoom;
    double px = c->pan_x;
    double py = c->pan_y;

    if (c->fit_mode) {
        int img_w = miime_image_width(c->image);
        int img_h = miime_image_height(c->image);
        if (img_w > 0 && img_h > 0) {
            double sx = (double)width / img_w;
            double sy = (double)height / img_h;
            zoom = (sx < sy) ? sx : sy;
            px = (width  - img_w * zoom) / 2.0;
            py = (height - img_h * zoom) / 2.0;
        }
    }

    miime_render_on_screen(cr, c->image, zoom, px, py, width, height);
}

MiimeCanvas *miime_canvas_new(void) {
    MiimeCanvas *c = malloc(sizeof(MiimeCanvas));
    if (!c) return NULL;

    c->image    = NULL;
    c->zoom     = 1.0;
    c->pan_x    = 0.0;
    c->pan_y    = 0.0;
    c->fit_mode = TRUE;

    c->drawing_area = gtk_drawing_area_new();
    gtk_widget_set_hexpand(c->drawing_area, TRUE);
    gtk_widget_set_vexpand(c->drawing_area, TRUE);

    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(c->drawing_area),
                                   canvas_draw_cb, c, NULL);

    return c;
}

void miime_canvas_destroy(MiimeCanvas *c) {
    if (c) {
        free(c);
    }
}

void miime_canvas_set_image(MiimeCanvas *c, MiimeImage *img) {
    if (!c) return;
    c->image = img;
    c->fit_mode = TRUE;
    gtk_widget_queue_draw(c->drawing_area);
}

GtkWidget *miime_canvas_get_widget(MiimeCanvas *c) {
    return c ? c->drawing_area : NULL;
}
