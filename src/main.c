#include "window.h"
#include <gtk/gtk.h>
#include <stdlib.h>

static MiimeWindow *g_win = NULL;

static void on_activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;
    g_win = miime_window_new(app);
    if (!g_win) {
        g_printerr("miime: failed to create window\n");
        return;
    }
    gtk_window_present(GTK_WINDOW(miime_window_get_widget(g_win)));
}

static void on_shutdown(GtkApplication *app, gpointer user_data) {
    (void)app;
    (void)user_data;
    miime_window_destroy(g_win);
    g_win = NULL;
}

int main(int argc, char *argv[]) {
    GtkApplication *app = gtk_application_new("com.miime.app",
                                               G_APPLICATION_NON_UNIQUE);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    g_signal_connect(app, "shutdown", G_CALLBACK(on_shutdown), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
