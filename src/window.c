#include "window.h"
#include "canvas.h"
#include "tools.h"
#include "annotate.h"
#include "clipboard.h"
#include "render.h"
#include <stdlib.h>
#include <string.h>

struct MiimeWindow {
    GtkWidget            *gtk_window;
    GtkWidget            *main_box;
    GtkWidget            *toolbar;
    MiimeCanvas          *canvas;
    MiimeToolState       *tool_state;
    MiimeAnnotationStack *annot_stack;
    GtkWidget            *tool_buttons[9];
    char                 *current_path;
};

static void on_tool_btn_toggled(GtkToggleButton *btn, gpointer user_data) {
    if (!gtk_toggle_button_get_active(btn)) return;
    MiimeWindow *win = (MiimeWindow *)user_data;
    MiimeToolType type = (MiimeToolType)GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "tool-type"));
    miime_tool_set_type(win->tool_state, type);
}

static void on_color_notify(GObject *gobject, GParamSpec *pspec, gpointer user_data) {
    (void)pspec;
    MiimeWindow *win = (MiimeWindow *)user_data;
    const GdkRGBA *rgba = gtk_color_dialog_button_get_rgba(GTK_COLOR_DIALOG_BUTTON(gobject));
    if (rgba) {
        miime_tool_set_color(win->tool_state, rgba->red, rgba->green, rgba->blue, rgba->alpha);
    }
}

static void on_stroke_width_changed(GtkSpinButton *spin, gpointer user_data) {
    MiimeWindow *win = (MiimeWindow *)user_data;
    double w = gtk_spin_button_get_value(spin);
    miime_tool_set_stroke_width(win->tool_state, w);
}

static void on_undo_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    MiimeWindow *win = (MiimeWindow *)user_data;
    miime_annot_stack_undo(win->annot_stack);
    gtk_widget_queue_draw(miime_canvas_get_widget(win->canvas));
}

static void on_redo_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    MiimeWindow *win = (MiimeWindow *)user_data;
    miime_annot_stack_redo(win->annot_stack);
    gtk_widget_queue_draw(miime_canvas_get_widget(win->canvas));
}

static void on_clear_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    MiimeWindow *win = (MiimeWindow *)user_data;
    miime_annot_stack_clear(win->annot_stack);
    gtk_widget_queue_draw(miime_canvas_get_widget(win->canvas));
}

static void on_save_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    MiimeWindow *win = (MiimeWindow *)user_data;
    miime_window_save(win);
}

static void on_copy_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    MiimeWindow *win = (MiimeWindow *)user_data;
    miime_window_copy_clipboard(win);
}

static void on_toggle_toolbar(gpointer user_data) {
    MiimeWindow *win = (MiimeWindow *)user_data;
    miime_window_toggle_toolbar(win);
}

static void on_save_cb(gpointer user_data) {
    MiimeWindow *win = (MiimeWindow *)user_data;
    miime_window_save(win);
}

static void on_save_as_cb(gpointer user_data) {
    MiimeWindow *win = (MiimeWindow *)user_data;
    miime_window_save_as(win);
}

static void on_copy_cb(gpointer user_data) {
    MiimeWindow *win = (MiimeWindow *)user_data;
    miime_window_copy_clipboard(win);
}

static void save_file_dialog_finish(GObject *source, GAsyncResult *res, gpointer user_data) {
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source);
    MiimeWindow *win = (MiimeWindow *)user_data;

    GError *err = NULL;
    GFile *file = gtk_file_dialog_save_finish(dialog, res, &err);
    if (!file) {
        if (err) g_error_free(err);
        return;
    }

    char *path = g_file_get_path(file);
    g_object_unref(file);
    if (!path) return;

    MiimeImage *img = miime_canvas_get_image(win->canvas);
    if (img) {
        int w, h;
        cairo_surface_t *surf = miime_render_export(img, win->annot_stack, &w, &h);
        if (surf) {
            miime_render_save_to_file(surf, path);
            cairo_surface_destroy(surf);
            if (win->current_path) g_free(win->current_path);
            win->current_path = path;
            return;
        }
    }
    g_free(path);
}

MiimeWindow *miime_window_new(GtkApplication *app) {
    MiimeWindow *win = malloc(sizeof(MiimeWindow));
    if (!win) return NULL;

    win->tool_state   = miime_tool_state_new();
    win->annot_stack  = miime_annot_stack_new();
    win->canvas       = miime_canvas_new();
    win->current_path = NULL;

    if (win->canvas) {
        miime_canvas_set_tool_state(win->canvas, win->tool_state);
        miime_canvas_set_annot_stack(win->canvas, win->annot_stack);
        miime_canvas_set_toggle_toolbar_callback(win->canvas, on_toggle_toolbar, win);
        miime_canvas_set_save_callback(win->canvas, on_save_cb, win);
        miime_canvas_set_save_as_callback(win->canvas, on_save_as_cb, win);
        miime_canvas_set_copy_callback(win->canvas, on_copy_cb, win);
    }

    win->gtk_window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(win->gtk_window), "MIIME");
    gtk_window_set_default_size(GTK_WINDOW(win->gtk_window), 980, 680);

    /* Main vertical container */
    win->main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(win->gtk_window), win->main_box);

    /* Horizontal toolbar */
    win->toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_margin_start(win->toolbar, 8);
    gtk_widget_set_margin_end(win->toolbar, 8);
    gtk_widget_set_margin_top(win->toolbar, 6);
    gtk_widget_set_margin_bottom(win->toolbar, 6);
    gtk_box_append(GTK_BOX(win->main_box), win->toolbar);

    /* Tool button definitions */
    struct {
        const char   *label;
        MiimeToolType type;
    } tools[] = {
        {"Pen",         MIIME_TOOL_PEN},
        {"Highlighter", MIIME_TOOL_HIGHLIGHTER},
        {"Line",        MIIME_TOOL_LINE},
        {"Rect",        MIIME_TOOL_RECT},
        {"Ellipse",     MIIME_TOOL_ELLIPSE},
        {"Arrow",       MIIME_TOOL_ARROW},
        {"Text",        MIIME_TOOL_TEXT},
        {"Blur",        MIIME_TOOL_BLUR},
        {"Crop",        MIIME_TOOL_CROP}
    };

    GtkToggleButton *first_btn = NULL;
    for (int i = 0; i < 9; i++) {
        GtkWidget *btn = gtk_toggle_button_new_with_label(tools[i].label);
        win->tool_buttons[i] = btn;
        g_object_set_data(G_OBJECT(btn), "tool-type", GINT_TO_POINTER(tools[i].type));
        if (i == 0) {
            first_btn = GTK_TOGGLE_BUTTON(btn);
            gtk_toggle_button_set_active(first_btn, TRUE);
        } else {
            gtk_toggle_button_set_group(GTK_TOGGLE_BUTTON(btn), first_btn);
        }
        g_signal_connect(btn, "toggled", G_CALLBACK(on_tool_btn_toggled), win);
        gtk_box_append(GTK_BOX(win->toolbar), btn);
    }

    /* Separator */
    gtk_box_append(GTK_BOX(win->toolbar),
                   gtk_separator_new(GTK_ORIENTATION_VERTICAL));

    /* Color picker button */
    GtkColorDialog *dlg = gtk_color_dialog_new();
    GtkWidget *color_btn = gtk_color_dialog_button_new(dlg);
    GdkRGBA red = {1.0f, 0.0f, 0.0f, 1.0f};
    gtk_color_dialog_button_set_rgba(GTK_COLOR_DIALOG_BUTTON(color_btn), &red);
    g_signal_connect(color_btn, "notify::rgba", G_CALLBACK(on_color_notify), win);
    gtk_box_append(GTK_BOX(win->toolbar), color_btn);

    /* Stroke width spin button */
    GtkWidget *spin = gtk_spin_button_new_with_range(1.0, 50.0, 1.0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), 3.0);
    g_signal_connect(spin, "value-changed", G_CALLBACK(on_stroke_width_changed), win);
    gtk_box_append(GTK_BOX(win->toolbar), spin);

    /* Separator */
    gtk_box_append(GTK_BOX(win->toolbar),
                   gtk_separator_new(GTK_ORIENTATION_VERTICAL));

    /* Export buttons: Save, Copy */
    GtkWidget *save_btn = gtk_button_new_with_label("Save");
    g_signal_connect(save_btn, "clicked", G_CALLBACK(on_save_clicked), win);
    gtk_box_append(GTK_BOX(win->toolbar), save_btn);

    GtkWidget *copy_btn = gtk_button_new_with_label("Copy");
    g_signal_connect(copy_btn, "clicked", G_CALLBACK(on_copy_clicked), win);
    gtk_box_append(GTK_BOX(win->toolbar), copy_btn);

    /* Separator */
    gtk_box_append(GTK_BOX(win->toolbar),
                   gtk_separator_new(GTK_ORIENTATION_VERTICAL));

    /* Action buttons: Undo, Redo, Clear */
    GtkWidget *undo_btn = gtk_button_new_with_label("Undo");
    g_signal_connect(undo_btn, "clicked", G_CALLBACK(on_undo_clicked), win);
    gtk_box_append(GTK_BOX(win->toolbar), undo_btn);

    GtkWidget *redo_btn = gtk_button_new_with_label("Redo");
    g_signal_connect(redo_btn, "clicked", G_CALLBACK(on_redo_clicked), win);
    gtk_box_append(GTK_BOX(win->toolbar), redo_btn);

    GtkWidget *clear_btn = gtk_button_new_with_label("Clear");
    g_signal_connect(clear_btn, "clicked", G_CALLBACK(on_clear_clicked), win);
    gtk_box_append(GTK_BOX(win->toolbar), clear_btn);

    /* Embed Canvas */
    if (win->canvas) {
        GtkWidget *canvas_widget = miime_canvas_get_widget(win->canvas);
        gtk_widget_set_hexpand(canvas_widget, TRUE);
        gtk_widget_set_vexpand(canvas_widget, TRUE);
        gtk_box_append(GTK_BOX(win->main_box), canvas_widget);
    }

    return win;
}

void miime_window_destroy(MiimeWindow *win) {
    if (win) {
        if (win->canvas) {
            miime_canvas_destroy(win->canvas);
        }
        if (win->tool_state) {
            miime_tool_state_free(win->tool_state);
        }
        if (win->annot_stack) {
            miime_annot_stack_free(win->annot_stack);
        }
        if (win->current_path) {
            g_free(win->current_path);
        }
        free(win);
    }
}

GtkWidget *miime_window_get_widget(MiimeWindow *win) {
    return win ? win->gtk_window : NULL;
}

MiimeCanvas *miime_window_get_canvas(MiimeWindow *win) {
    return win ? win->canvas : NULL;
}

MiimeToolState *miime_window_get_tool_state(MiimeWindow *win) {
    return win ? win->tool_state : NULL;
}

MiimeAnnotationStack *miime_window_get_annot_stack(MiimeWindow *win) {
    return win ? win->annot_stack : NULL;
}

void miime_window_toggle_toolbar(MiimeWindow *win) {
    if (!win || !win->toolbar) return;
    gboolean vis = gtk_widget_get_visible(win->toolbar);
    gtk_widget_set_visible(win->toolbar, !vis);
}

void miime_window_set_image_path(MiimeWindow *win, const char *path) {
    if (!win) return;
    if (win->current_path) g_free(win->current_path);
    win->current_path = path ? g_strdup(path) : NULL;
}

const char *miime_window_get_image_path(const MiimeWindow *win) {
    return win ? win->current_path : NULL;
}

void miime_window_save(MiimeWindow *win) {
    if (!win) return;

    if (!win->current_path) {
        miime_window_save_as(win);
        return;
    }

    MiimeImage *img = miime_canvas_get_image(win->canvas);
    if (!img) return;

    int w, h;
    cairo_surface_t *surf = miime_render_export(img, win->annot_stack, &w, &h);
    if (surf) {
        miime_render_save_to_file(surf, win->current_path);
        cairo_surface_destroy(surf);
    }
}

void miime_window_save_as(MiimeWindow *win) {
    if (!win) return;

    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Save Image As");
    if (win->current_path) {
        char *basename = g_path_get_basename(win->current_path);
        gtk_file_dialog_set_initial_name(dialog, basename);
        g_free(basename);
    } else {
        gtk_file_dialog_set_initial_name(dialog, "export.png");
    }

    gtk_file_dialog_save(dialog, GTK_WINDOW(win->gtk_window), NULL,
                         save_file_dialog_finish, win);
}

void miime_window_copy_clipboard(MiimeWindow *win) {
    if (!win) return;

    MiimeImage *img = miime_canvas_get_image(win->canvas);
    if (!img) return;

    int w, h;
    cairo_surface_t *surf = miime_render_export(img, win->annot_stack, &w, &h);
    if (surf) {
        GdkClipboard *cb = gdk_display_get_clipboard(gtk_widget_get_display(win->gtk_window));
        miime_clipboard_copy_surface(cb, surf);
        cairo_surface_destroy(surf);
    }
}
