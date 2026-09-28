#include "window.h"
#include "image.h"
#include <gtk/gtk.h>
#include <stdlib.h>

static MiimeWindow  *g_win = NULL;
static MiimeImage   *g_img = NULL;
static MiimeDirList *g_dir_list = NULL;
static char         *g_image_path = NULL;

static void on_navigate(int direction, gpointer user_data) {
    (void)user_data;
    if (!g_dir_list) return;

    const char *path = (direction > 0)
        ? miime_dir_list_next(g_dir_list)
        : miime_dir_list_prev(g_dir_list);

    if (!path) return;

    miime_image_free(g_img);
    g_img = miime_image_load(path);
    if (g_img) {
        miime_canvas_set_image(miime_window_get_canvas(g_win), g_img);
        miime_window_set_image_path(g_win, path);
    }
}

static void on_activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;
    if (!g_win) {
        g_win = miime_window_new(app);
        if (!g_win) {
            g_printerr("miime: failed to create window\n");
            return;
        }
    }

    if (g_image_path && !g_img) {
        g_img = miime_image_load(g_image_path);
        if (g_img) {
            miime_canvas_set_image(miime_window_get_canvas(g_win), g_img);
            miime_window_set_image_path(g_win, g_image_path);
            if (!g_dir_list) {
                g_dir_list = miime_dir_list_new(g_image_path);
                miime_canvas_set_nav_callback(miime_window_get_canvas(g_win), on_navigate, NULL);
            }
        } else {
            g_printerr("miime: could not load '%s'\n", g_image_path);
        }
    }

    gtk_window_present(GTK_WINDOW(miime_window_get_widget(g_win)));
}

static void on_open(GApplication *app, GFile **files, gint n_files, const gchar *hint, gpointer user_data) {
    (void)hint;
    (void)user_data;
    if (n_files > 0) {
        if (g_image_path) g_free(g_image_path);
        g_image_path = g_file_get_path(files[0]);
    }
    on_activate(GTK_APPLICATION(app), user_data);
}

static void on_shutdown(GtkApplication *app, gpointer user_data) {
    (void)app;
    (void)user_data;
    miime_window_destroy(g_win);
    g_win = NULL;
    miime_image_free(g_img);
    g_img = NULL;
    miime_dir_list_free(g_dir_list);
    g_dir_list = NULL;
    if (g_image_path) {
        g_free(g_image_path);
        g_image_path = NULL;
    }
}

int main(int argc, char *argv[]) {
    g_setenv("GSK_RENDERER", "cairo", FALSE);

    GtkApplication *app = gtk_application_new("com.miime.app",
                                               G_APPLICATION_HANDLES_OPEN | G_APPLICATION_NON_UNIQUE);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    g_signal_connect(app, "open", G_CALLBACK(on_open), NULL);
    g_signal_connect(app, "shutdown", G_CALLBACK(on_shutdown), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
