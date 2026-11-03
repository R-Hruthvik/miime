#include "window.h"
#include "image.h"
#include "headless.h"
#include "annotate.h"
#include "schema.h"
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>

static MiimeWindow  *g_win = NULL;
static MiimeImage   *g_img = NULL;
static MiimeDirList *g_dir_list = NULL;
static char         *g_image_path = NULL;
static char         *g_annotations_path = NULL;
static char         *g_annotations_out_path = NULL;
static gboolean      g_annotations_loaded = FALSE;

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

    if (g_annotations_path && g_win && !g_annotations_loaded) {
        FILE *f = fopen(g_annotations_path, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fseek(f, 0, SEEK_SET);
            char *buf = malloc(sz + 1);
            if (buf) {
                fread(buf, 1, sz, f);
                buf[sz] = '\0';
                GError *err = NULL;
                MiimeAnnotationStack *loaded = miime_schema_deserialize(buf, &err);
                if (loaded) {
                    MiimeAnnotationStack *st = miime_window_get_annot_stack(g_win);
                    if (st) {
                        miime_annot_stack_clear(st);
                        int n = miime_annot_stack_count(loaded);
                        for (int i = 0; i < n; i++) {
                            const MiimeAnnotation *a = miime_annot_stack_get(loaded, i);
                            if (a) miime_annot_stack_push(st, *a);
                        }
                    }
                    miime_annot_stack_free(loaded);
                }
                if (err) { g_error_free(err); }
                free(buf);
            }
            fclose(f);
        }
        g_annotations_loaded = TRUE;
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
    if (g_annotations_out_path && g_win) {
        MiimeAnnotationStack *st = miime_window_get_annot_stack(g_win);
        char *json = miime_schema_serialize(st);
        if (json) {
            g_file_set_contents(g_annotations_out_path, json, -1, NULL);
            g_free(json);
        }
    }
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
    /* Parse miime-specific options (--annotations, --annotations-out) */
    int fargc = 0;
    char **fargv = malloc((argc + 1) * sizeof(char *));
    if (!fargv) return 1;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--annotations") == 0 && i + 1 < argc) {
            g_annotations_path = g_strdup(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "--annotations-out") == 0 && i + 1 < argc) {
            g_annotations_out_path = g_strdup(argv[++i]);
            continue;
        }
        fargv[fargc++] = argv[i];
    }
    fargv[fargc] = NULL;

    /* Headless mode detection */
    for (int i = 1; i < fargc; i++) {
        if (strcmp(fargv[i], "--headless") == 0) {
            const char *input = NULL;
            const char *annotations = NULL;
            const char *output = NULL;
            for (int j = i + 1; j < fargc; j++) {
                if (strcmp(fargv[j], "--annotations") == 0 && j + 1 < fargc) {
                    annotations = fargv[++j];
                } else if (strcmp(fargv[j], "-o") == 0 && j + 1 < fargc) {
                    output = fargv[++j];
                } else if (fargv[j][0] != '-') {
                    if (!input) input = fargv[j];
                }
            }
            if (!input || !output) {
                g_printerr("usage: miime --headless in.png [--annotations a.json] -o out.png\n");
                return 1;
            }
            return miime_headless_run(input, annotations, output);
        }
    }

    g_setenv("GSK_RENDERER", "cairo", FALSE);

    GtkApplication *app = gtk_application_new("com.miime.app",
                                               G_APPLICATION_HANDLES_OPEN | G_APPLICATION_NON_UNIQUE);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    g_signal_connect(app, "open", G_CALLBACK(on_open), NULL);
    g_signal_connect(app, "shutdown", G_CALLBACK(on_shutdown), NULL);

    int status = g_application_run(G_APPLICATION(app), fargc, fargv);
    g_object_unref(app);
    return status;
}
