#ifndef MIIME_WINDOW_H
#define MIIME_WINDOW_H

#include <gtk/gtk.h>
#include "canvas.h"
#include "tools.h"
#include "annotate.h"

typedef struct MiimeWindow MiimeWindow;

/* Create the main application window. Returns NULL on failure. */
MiimeWindow *miime_window_new(GtkApplication *app);

/* Free all resources owned by the window. */
void miime_window_destroy(MiimeWindow *win);

/* Get the underlying GtkWindow pointer (for GTK calls). */
GtkWidget *miime_window_get_widget(MiimeWindow *win);

/* Get the embedded canvas. */
MiimeCanvas *miime_window_get_canvas(MiimeWindow *win);

/* Get tool state. */
MiimeToolState *miime_window_get_tool_state(MiimeWindow *win);

/* Get annotation stack. */
MiimeAnnotationStack *miime_window_get_annot_stack(MiimeWindow *win);

/* Toggle toolbar visibility (Tab key). */
void miime_window_toggle_toolbar(MiimeWindow *win);

/* Current image path tracking */
void miime_window_set_image_path(MiimeWindow *win, const char *path);
const char *miime_window_get_image_path(const MiimeWindow *win);

/* Export actions */
void miime_window_save(MiimeWindow *win);
void miime_window_save_as(MiimeWindow *win);
void miime_window_copy_clipboard(MiimeWindow *win);

#endif /* MIIME_WINDOW_H */
