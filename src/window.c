#include "window.h"
#include "canvas.h"
#include <stdlib.h>

struct MiimeWindow {
    GtkWidget   *gtk_window;
    MiimeCanvas *canvas;
};

MiimeWindow *miime_window_new(GtkApplication *app) {
    MiimeWindow *win = malloc(sizeof(MiimeWindow));
    if (!win) return NULL;

    win->gtk_window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(win->gtk_window), "MIIME");
    gtk_window_set_default_size(GTK_WINDOW(win->gtk_window), 800, 600);

    win->canvas = miime_canvas_new();
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
        free(win);
    }
}

GtkWidget *miime_window_get_widget(MiimeWindow *win) {
    return win ? win->gtk_window : NULL;
}

MiimeCanvas *miime_window_get_canvas(MiimeWindow *win) {
    return win ? win->canvas : NULL;
}
