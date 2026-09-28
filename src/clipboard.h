#ifndef MIIME_CLIPBOARD_H
#define MIIME_CLIPBOARD_H

#include <gtk/gtk.h>
#include <cairo.h>

/* Copy a Cairo image surface to the system clipboard as a texture. */
void miime_clipboard_copy_surface(GdkClipboard *clipboard, cairo_surface_t *surface);

#endif /* MIIME_CLIPBOARD_H */
