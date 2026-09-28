#include "canvas.h"
#include "render.h"
#include <gdk/gdk.h>
#include <stdlib.h>

struct MiimeCanvas {
    GtkWidget            *drawing_area;
    MiimeImage           *image;        /* borrowed, not owned */
    double                zoom;
    double                pan_x;
    double                pan_y;
    gboolean              fit_mode;     /* TRUE = fit-to-window */
    double                last_mouse_x;
    double                last_mouse_y;
    double                drag_start_pan_x;
    double                drag_start_pan_y;
    double                draw_start_x;
    double                draw_start_y;
    MiimeNavCallback      nav_cb;
    gpointer              nav_data;
    MiimeSimpleCallback   toggle_tb_cb;
    gpointer              toggle_tb_data;
    MiimeSimpleCallback   save_cb;
    gpointer              save_data;
    MiimeSimpleCallback   save_as_cb;
    gpointer              save_as_data;
    MiimeSimpleCallback   copy_cb;
    gpointer              copy_data;
    MiimeToolState       *tool_state;   /* borrowed */
    MiimeAnnotationStack *annot_stack;  /* borrowed */
};

static void widget_to_image(MiimeCanvas *c, double wx, double wy,
                            double *ix, double *iy) {
    if (c->zoom <= 0.0) {
        *ix = 0; *iy = 0;
        return;
    }
    *ix = (wx - c->pan_x) / c->zoom;
    *iy = (wy - c->pan_y) / c->zoom;
}

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
            c->zoom = zoom;
            c->pan_x = px;
            c->pan_y = py;
        }
    }

    const MiimeAnnotation *preview = c->tool_state ? miime_tool_get_preview(c->tool_state) : NULL;
    miime_render_on_screen(cr, c->image, c->annot_stack, preview, zoom, px, py, width, height);
}

static gboolean canvas_scroll_cb(GtkEventControllerScroll *ctrl,
                                 double dx, double dy,
                                 gpointer user_data) {
    MiimeCanvas *c = (MiimeCanvas *)user_data;
    (void)ctrl;
    (void)dx;

    if (!c->image) return FALSE;

    if (c->fit_mode) {
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

static void canvas_pan_drag_begin_cb(GtkGestureDrag *gesture,
                                     double x, double y,
                                     gpointer user_data) {
    MiimeCanvas *c = (MiimeCanvas *)user_data;
    guint button = gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture));
    (void)x; (void)y;

    if (button != 2 && button != 3) return;

    if (c->fit_mode) {
        c->fit_mode = FALSE;
    }

    c->drag_start_pan_x = c->pan_x;
    c->drag_start_pan_y = c->pan_y;
}

static void canvas_pan_drag_update_cb(GtkGestureDrag *gesture,
                                      double offset_x, double offset_y,
                                      gpointer user_data) {
    MiimeCanvas *c = (MiimeCanvas *)user_data;
    guint button = gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture));

    if (button != 2 && button != 3) return;

    c->pan_x = c->drag_start_pan_x + offset_x;
    c->pan_y = c->drag_start_pan_y + offset_y;
    gtk_widget_queue_draw(c->drawing_area);
}

static void canvas_draw_drag_begin_cb(GtkGestureDrag *gesture,
                                      double start_x, double start_y,
                                      gpointer user_data) {
    MiimeCanvas *c = (MiimeCanvas *)user_data;
    guint button = gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture));

    if (button != 1 || !c->image || !c->tool_state) return;

    c->draw_start_x = start_x;
    c->draw_start_y = start_y;

    double ix, iy;
    widget_to_image(c, start_x, start_y, &ix, &iy);
    miime_tool_on_press(c->tool_state, ix, iy, c->annot_stack);
    gtk_widget_queue_draw(c->drawing_area);
}

static void canvas_draw_drag_update_cb(GtkGestureDrag *gesture,
                                       double offset_x, double offset_y,
                                       gpointer user_data) {
    MiimeCanvas *c = (MiimeCanvas *)user_data;
    guint button = gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture));

    if (button != 1 || !c->image || !c->tool_state) return;

    double wx = c->draw_start_x + offset_x;
    double wy = c->draw_start_y + offset_y;
    double ix, iy;
    widget_to_image(c, wx, wy, &ix, &iy);
    miime_tool_on_drag(c->tool_state, ix, iy);
    gtk_widget_queue_draw(c->drawing_area);
}

static void canvas_draw_drag_end_cb(GtkGestureDrag *gesture,
                                    double offset_x, double offset_y,
                                    gpointer user_data) {
    MiimeCanvas *c = (MiimeCanvas *)user_data;
    guint button = gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture));

    if (button != 1 || !c->image || !c->tool_state) return;

    double wx = c->draw_start_x + offset_x;
    double wy = c->draw_start_y + offset_y;
    double ix, iy;
    widget_to_image(c, wx, wy, &ix, &iy);

    if (miime_tool_get_type(c->tool_state) == MIIME_TOOL_BLUR) {
        miime_tool_compute_blur(c->tool_state, c->image);
    }

    miime_tool_on_release(c->tool_state, ix, iy, c->annot_stack);
    gtk_widget_queue_draw(c->drawing_area);
}

static gboolean canvas_key_cb(GtkEventControllerKey *ctrl,
                              guint keyval, guint keycode,
                              GdkModifierType state,
                              gpointer user_data) {
    MiimeCanvas *c = (MiimeCanvas *)user_data;
    (void)ctrl; (void)keycode;

    if (miime_tool_text_key(c->tool_state, keyval, c->annot_stack)) {
        gtk_widget_queue_draw(c->drawing_area);
        return TRUE;
    }

    if (keyval == GDK_KEY_Tab) {
        if (c->toggle_tb_cb) c->toggle_tb_cb(c->toggle_tb_data);
        return TRUE;
    }

    if (!(state & (GDK_CONTROL_MASK | GDK_ALT_MASK))) {
        if (keyval == GDK_KEY_p || keyval == GDK_KEY_P) { miime_tool_set_type(c->tool_state, MIIME_TOOL_PEN); return TRUE; }
        if (keyval == GDK_KEY_h || keyval == GDK_KEY_H) { miime_tool_set_type(c->tool_state, MIIME_TOOL_HIGHLIGHTER); return TRUE; }
        if (keyval == GDK_KEY_l || keyval == GDK_KEY_L) { miime_tool_set_type(c->tool_state, MIIME_TOOL_LINE); return TRUE; }
        if (keyval == GDK_KEY_r || keyval == GDK_KEY_R) { miime_tool_set_type(c->tool_state, MIIME_TOOL_RECT); return TRUE; }
        if (keyval == GDK_KEY_e || keyval == GDK_KEY_E) { miime_tool_set_type(c->tool_state, MIIME_TOOL_ELLIPSE); return TRUE; }
        if (keyval == GDK_KEY_a || keyval == GDK_KEY_A) { miime_tool_set_type(c->tool_state, MIIME_TOOL_ARROW); return TRUE; }
        if (keyval == GDK_KEY_t || keyval == GDK_KEY_T) { miime_tool_set_type(c->tool_state, MIIME_TOOL_TEXT); return TRUE; }
        if (keyval == GDK_KEY_b || keyval == GDK_KEY_B) { miime_tool_set_type(c->tool_state, MIIME_TOOL_BLUR); return TRUE; }
        if (keyval == GDK_KEY_c || keyval == GDK_KEY_C) { miime_tool_set_type(c->tool_state, MIIME_TOOL_CROP); return TRUE; }
    }

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

    if ((keyval == GDK_KEY_z || keyval == GDK_KEY_Z) && (state & GDK_CONTROL_MASK)) {
        if (c->annot_stack) {
            if (state & GDK_SHIFT_MASK) {
                miime_annot_stack_redo(c->annot_stack);
            } else {
                miime_annot_stack_undo(c->annot_stack);
            }
            gtk_widget_queue_draw(c->drawing_area);
        }
        return TRUE;
    }

    if ((keyval == GDK_KEY_c || keyval == GDK_KEY_C) && (state & GDK_CONTROL_MASK)) {
        if (c->copy_cb) c->copy_cb(c->copy_data);
        return TRUE;
    }

    if ((keyval == GDK_KEY_s || keyval == GDK_KEY_S) && (state & GDK_CONTROL_MASK)) {
        if (state & GDK_SHIFT_MASK) {
            if (c->save_as_cb) c->save_as_cb(c->save_as_data);
        } else {
            if (c->save_cb) c->save_cb(c->save_data);
        }
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

    c->image        = NULL;
    c->zoom         = 1.0;
    c->pan_x        = 0.0;
    c->pan_y        = 0.0;
    c->fit_mode     = TRUE;
    c->last_mouse_x = 0.0;
    c->last_mouse_y = 0.0;
    c->drag_start_pan_x = 0.0;
    c->drag_start_pan_y = 0.0;
    c->draw_start_x = 0.0;
    c->draw_start_y = 0.0;
    c->nav_cb       = NULL;
    c->nav_data     = NULL;
    c->tool_state   = NULL;
    c->annot_stack  = NULL;

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

    /* Drag gesture for drawing (left button) */
    GtkGesture *draw_drag = gtk_gesture_drag_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(draw_drag), 1);
    g_signal_connect(draw_drag, "drag-begin", G_CALLBACK(canvas_draw_drag_begin_cb), c);
    g_signal_connect(draw_drag, "drag-update", G_CALLBACK(canvas_draw_drag_update_cb), c);
    g_signal_connect(draw_drag, "drag-end", G_CALLBACK(canvas_draw_drag_end_cb), c);
    gtk_widget_add_controller(c->drawing_area, GTK_EVENT_CONTROLLER(draw_drag));

    /* Drag gesture for panning (middle or right button) */
    GtkGesture *pan_drag = gtk_gesture_drag_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(pan_drag), 0);
    g_signal_connect(pan_drag, "drag-begin", G_CALLBACK(canvas_pan_drag_begin_cb), c);
    g_signal_connect(pan_drag, "drag-update", G_CALLBACK(canvas_pan_drag_update_cb), c);
    gtk_widget_add_controller(c->drawing_area, GTK_EVENT_CONTROLLER(pan_drag));

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

MiimeImage *miime_canvas_get_image(const MiimeCanvas *c) {
    return c ? c->image : NULL;
}

GtkWidget *miime_canvas_get_widget(MiimeCanvas *c) {
    return c ? c->drawing_area : NULL;
}

void miime_canvas_set_nav_callback(MiimeCanvas *c, MiimeNavCallback cb, gpointer data) {
    if (!c) return;
    c->nav_cb = cb;
    c->nav_data = data;
}

void miime_canvas_set_tool_state(MiimeCanvas *c, MiimeToolState *ts) {
    if (!c) return;
    c->tool_state = ts;
}

void miime_canvas_set_annot_stack(MiimeCanvas *c, MiimeAnnotationStack *stack) {
    if (!c) return;
    c->annot_stack = stack;
}

void miime_canvas_set_toggle_toolbar_callback(MiimeCanvas *c, MiimeSimpleCallback cb, gpointer data) {
    if (!c) return;
    c->toggle_tb_cb = cb;
    c->toggle_tb_data = data;
}

void miime_canvas_set_save_callback(MiimeCanvas *c, MiimeSimpleCallback cb, gpointer data) {
    if (!c) return;
    c->save_cb = cb;
    c->save_data = data;
}

void miime_canvas_set_save_as_callback(MiimeCanvas *c, MiimeSimpleCallback cb, gpointer data) {
    if (!c) return;
    c->save_as_cb = cb;
    c->save_as_data = data;
}

void miime_canvas_set_copy_callback(MiimeCanvas *c, MiimeSimpleCallback cb, gpointer data) {
    if (!c) return;
    c->copy_cb = cb;
    c->copy_data = data;
}
