#include "window.h"
#include "canvas.h"
#include "tools.h"
#include "annotate.h"
#include <stdlib.h>

struct MiimeWindow {
    GtkWidget            *gtk_window;
    MiimeCanvas          *canvas;
    MiimeToolState       *tool_state;
    MiimeAnnotationStack *annot_stack;
};

MiimeWindow *miime_window_new(GtkApplication *app) {
    MiimeWindow *win = malloc(sizeof(MiimeWindow));
    if (!win) return NULL;

    win->tool_state = miime_tool_state_new();
    win->annot_stack = miime_annot_stack_new();
    win->canvas = miime_canvas_new();

    if (win->canvas) {
        miime_canvas_set_tool_state(win->canvas, win->tool_state);
        miime_canvas_set_annot_stack(win->canvas, win->annot_stack);
    }

    win->gtk_window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(win->gtk_window), "MIIME");
    gtk_window_set_default_size(GTK_WINDOW(win->gtk_window), 800, 600);

    if (win->canvas) {
        gtk_window_set_child(GTK_WINDOW(win->gtk_window),
                             miime_canvas_get_widget(win->canvas));
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
