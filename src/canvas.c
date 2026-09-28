#include "canvas.h"
#include "render.h"
#include <gdk/gdk.h>
#include <stdlib.h>

struct MiimeCanvas {
    GtkWidget        *drawing_area;
    MiimeImage       *image;      /* borrowed, not owned */
    double            zoom;
    double            pan_x;
    double            pan_y;
    gboolean          fit_mode;   /* TRUE = fit-to-window */
    double            last_mouse_x;
    double            last_mouse_y;
    double            drag_start_pan_x;
    double            drag_start_pan_y;
    MiimeNavCallback  nav_cb;
    gpointer          nav_data;
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

static gboolean canvas_scroll_cb(GtkEventControllerScroll *ctrl,
                                 double dx, double dy,
                                 gpointer user_data) {
    MiimeCanvas *c = (MiimeCanvas *)user_data;
    (void)ctrl;
    (void)dx;

    if (!c->image) return FALSE;

    if (c->fit_mode) {
        int widget_w = gtk_widget_get_width(c->drawing_area);
        int widget_h = gtk_widget_get_height(c->drawing_area);
        int img_w = miime_image_width(c->image);
        int img_h = miime_image_height(c->image);
        if (img_w > 0 && img_h > 0) {
            double sx = (double)widget_w / img_w;
            double sy = (double)widget_h / img_h;
            c->zoom = (sx < sy) ? sx : sy;
            c->pan_x = (widget_w - img_w * c->zoom) / 2.0;
            c->pan_y = (widget_h - img_h * c->zoom) / 2.0;
        }
        c->fit_mode = FALSE;
    }

    double mouse_x = c->last_mouse_x;
    double mouse_y = c->last_mouse_y;

    double factor = (dy < 0) ? 1.1 : (1.0 / 1.1);
    double new_zoom = c->zoom * factor;

    if (new_zoom < 0.01) new_zoom = 0.01;
    if (new_zoom > 100.0) new_zoom = 100.0;

    c->pan_x = mouse_x - (mouse_x - c->pan_x) * (new_zoom / c->zoom);
    c->pan_y = mouse_y - (mouse_y - c->pan_y) * (new_zoom / c->zoom);
    c->zoom = new_zoom;

    gtk_widget_queue_draw(c->drawing_area);
    return TRUE;
}

static void canvas_motion_cb(GtkEventControllerMotion *ctrl,
                             double x, double y,
                             gpointer user_data) {
    MiimeCanvas *c = (MiimeCanvas *)user_data;
    (void)ctrl;
    c->last_mouse_x = x;
    c->last_mouse_y = y;
}

static void canvas_drag_begin_cb(GtkGestureDrag *gesture,
                                 double x, double y,
                                 gpointer user_data) {
    MiimeCanvas *c = (MiimeCanvas *)user_data;
    guint button = gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture));
    (void)x; (void)y;

    if (button != 2 && button != 3) return;

    if (c->fit_mode) {
        int widget_w = gtk_widget_get_width(c->drawing_area);
        int widget_h = gtk_widget_get_height(c->drawing_area);
        int img_w = miime_image_width(c->image);
        int img_h = miime_image_height(c->image);
        if (img_w > 0 && img_h > 0) {
            double sx = (double)widget_w / img_w;
            double sy = (double)widget_h / img_h;
            c->zoom = (sx < sy) ? sx : sy;
            c->pan_x = (widget_w - img_w * c->zoom) / 2.0;
            c->pan_y = (widget_h - img_h * c->zoom) / 2.0;
        }
        c->fit_mode = FALSE;
    }

    c->drag_start_pan_x = c->pan_x;
    c->drag_start_pan_y = c->pan_y;
}

static void canvas_drag_update_cb(GtkGestureDrag *gesture,
                                  double offset_x, double offset_y,
                                  gpointer user_data) {
    MiimeCanvas *c = (MiimeCanvas *)user_data;
    guint button = gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture));

    if (button != 2 && button != 3) return;

    c->pan_x = c->drag_start_pan_x + offset_x;
    c->pan_y = c->drag_start_pan_y + offset_y;
    gtk_widget_queue_draw(c->drawing_area);
}

static gboolean canvas_key_cb(GtkEventControllerKey *ctrl,
                              guint keyval, guint keycode,
                              GdkModifierType state,
                              gpointer user_data) {
    MiimeCanvas *c = (MiimeCanvas *)user_data;
    (void)ctrl; (void)keycode; (void)state;

    if (keyval == GDK_KEY_1) {
        if (c->fit_mode) {
            int widget_w = gtk_widget_get_width(c->drawing_area);
            int widget_h = gtk_widget_get_height(c->drawing_area);
            int img_w = miime_image_width(c->image);
            int img_h = miime_image_height(c->image);
            c->zoom = 1.0;
            c->pan_x = (widget_w - img_w) / 2.0;
            c->pan_y = (widget_h - img_h) / 2.0;
            c->fit_mode = FALSE;
        } else {
            c->fit_mode = TRUE;
        }
        gtk_widget_queue_draw(c->drawing_area);
        return TRUE;
    }

    if (keyval == GDK_KEY_space) {
        if (c->nav_cb) c->nav_cb(+1, c->nav_data);
        return TRUE;
    }

    if (keyval == GDK_KEY_BackSpace) {
        if (c->nav_cb) c->nav_cb(-1, c->nav_data);
        return TRUE;
    }

    if (keyval == GDK_KEY_q || keyval == GDK_KEY_Q) {
        GtkRoot *root = gtk_widget_get_root(c->drawing_area);
        if (root && GTK_IS_WINDOW(root)) {
            gtk_window_close(GTK_WINDOW(root));
        }
        return TRUE;
    }

    return FALSE;
}

MiimeCanvas *miime_canvas_new(void) {
    MiimeCanvas *c = malloc(sizeof(MiimeCanvas));
    if (!c) return NULL;

    c->image    = NULL;
    c->zoom     = 1.0;
    c->pan_x    = 0.0;
    c->pan_y    = 0.0;
    c->fit_mode = TRUE;
    c->last_mouse_x = 0.0;
    c->last_mouse_y = 0.0;
    c->drag_start_pan_x = 0.0;
    c->drag_start_pan_y = 0.0;
    c->nav_cb   = NULL;
    c->nav_data = NULL;

    c->drawing_area = gtk_drawing_area_new();
    gtk_widget_set_hexpand(c->drawing_area, TRUE);
    gtk_widget_set_vexpand(c->drawing_area, TRUE);
    gtk_widget_set_focusable(c->drawing_area, TRUE);

    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(c->drawing_area),
                                   canvas_draw_cb, c, NULL);

    /* Scroll controller for zoom */
    GtkEventController *scroll = gtk_event_controller_scroll_new(
        GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
    g_signal_connect(scroll, "scroll", G_CALLBACK(canvas_scroll_cb), c);
    gtk_widget_add_controller(c->drawing_area, scroll);

    /* Motion controller to track cursor */
    GtkEventController *motion = gtk_event_controller_motion_new();
    g_signal_connect(motion, "motion", G_CALLBACK(canvas_motion_cb), c);
    gtk_widget_add_controller(c->drawing_area, motion);

    /* Drag gesture for panning (middle or right button) */
    GtkGesture *drag = gtk_gesture_drag_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(drag), 0); /* accept any button */
    g_signal_connect(drag, "drag-begin", G_CALLBACK(canvas_drag_begin_cb), c);
    g_signal_connect(drag, "drag-update", G_CALLBACK(canvas_drag_update_cb), c);
    gtk_widget_add_controller(c->drawing_area, GTK_EVENT_CONTROLLER(drag));

    /* Key controller for shortcuts */
    GtkEventController *key = gtk_event_controller_key_new();
    g_signal_connect(key, "key-pressed", G_CALLBACK(canvas_key_cb), c);
    gtk_widget_add_controller(c->drawing_area, key);

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

void miime_canvas_set_nav_callback(MiimeCanvas *c, MiimeNavCallback cb, gpointer data) {
    if (!c) return;
    c->nav_cb = cb;
    c->nav_data = data;
}
