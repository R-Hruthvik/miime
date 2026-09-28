#include "window.h"
#include <stdlib.h>

struct MiimeWindow {
    GtkWidget *gtk_window;
};

MiimeWindow *miime_window_new(GtkApplication *app) {
    MiimeWindow *win = malloc(sizeof(MiimeWindow));
    if (!win) return NULL;

    win->gtk_window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(win->gtk_window), "MIIME");
    gtk_window_set_default_size(GTK_WINDOW(win->gtk_window), 800, 600);

    return win;
}

void miime_window_destroy(MiimeWindow *win) {
    if (win) {
        free(win);
    }
}

GtkWidget *miime_window_get_widget(MiimeWindow *win) {
    return win ? win->gtk_window : NULL;
}
