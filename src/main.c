#include "window.h"
#include "image.h"
#include <gtk/gtk.h>
#include <stdlib.h>

static MiimeWindow *g_win = NULL;
static MiimeImage  *g_img = NULL;
static const char  *g_image_path = NULL;

static void on_activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;
    g_win = miime_window_new(app);
    if (!g_win) {
        g_printerr("miime: failed to create window\n");
        return;
    }

    if (g_image_path) {
        g_img = miime_image_load(g_image_path);
        if (g_img) {
            miime_canvas_set_image(miime_window_get_canvas(g_win), g_img);
        } else {
            g_printerr("miime: could not load '%s'\n", g_image_path);
        }
    }

    gtk_window_present(GTK_WINDOW(miime_window_get_widget(g_win)));
}

static void on_shutdown(GtkApplication *app, gpointer user_data) {
    (void)app;
    (void)user_data;
    miime_window_destroy(g_win);
    g_win = NULL;
    miime_image_free(g_img);
    g_img = NULL;
}

int main(int argc, char *argv[]) {
    /* Extract image path before GTK consumes args */
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            g_image_path = argv[i];
            break;
        }
    }

    GtkApplication *app = gtk_application_new("com.miime.app",
                                               G_APPLICATION_NON_UNIQUE);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    g_signal_connect(app, "shutdown", G_CALLBACK(on_shutdown), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
