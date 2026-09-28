#ifndef MIIME_CANVAS_H
#define MIIME_CANVAS_H

#include <gtk/gtk.h>
#include "image.h"
#include "annotate.h"
#include "tools.h"

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

/* Set tools and annotation stack (borrowed from window). */
void miime_canvas_set_tool_state(MiimeCanvas *c, MiimeToolState *ts);
void miime_canvas_set_annot_stack(MiimeCanvas *c, MiimeAnnotationStack *stack);

#endif /* MIIME_CANVAS_H */
