#ifndef MIIME_CANVAS_H
#define MIIME_CANVAS_H

#include <gtk/gtk.h>
#include "image.h"

typedef struct MiimeCanvas MiimeCanvas;
typedef void (*MiimeNavCallback)(int direction, gpointer user_data);

/* Create a new canvas (wraps a GtkDrawingArea). */
MiimeCanvas *miime_canvas_new(void);

/* Free canvas resources. */
void miime_canvas_destroy(MiimeCanvas *c);

/* Set the image to display. Canvas takes a borrowed reference (caller owns lifetime). */
void miime_canvas_set_image(MiimeCanvas *c, MiimeImage *img);

/* Get the underlying GtkWidget for embedding in a container. */
GtkWidget *miime_canvas_get_widget(MiimeCanvas *c);

/* Register navigation callback (triggered on Space/Backspace). */
void miime_canvas_set_nav_callback(MiimeCanvas *c, MiimeNavCallback cb, gpointer data);

#endif /* MIIME_CANVAS_H */
