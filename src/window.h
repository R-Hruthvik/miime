#ifndef MIIME_WINDOW_H
#define MIIME_WINDOW_H

#include <gtk/gtk.h>

typedef struct MiimeWindow MiimeWindow;

/* Create the main application window. Returns NULL on failure. */
MiimeWindow *miime_window_new(GtkApplication *app);

/* Free all resources owned by the window. */
void miime_window_destroy(MiimeWindow *win);

/* Get the underlying GtkWindow pointer (for GTK calls). */
GtkWidget *miime_window_get_widget(MiimeWindow *win);

#endif /* MIIME_WINDOW_H */
