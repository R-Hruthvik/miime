#ifndef MIIME_IMAGE_H
#define MIIME_IMAGE_H

#include <gdk-pixbuf/gdk-pixbuf.h>

typedef struct MiimeImage MiimeImage;

/* Load an image from a file path. Returns NULL on failure. */
MiimeImage *miime_image_load(const char *path);

/* Free the image and its pixel buffer. */
void miime_image_free(MiimeImage *img);

/* Get pixel dimensions. */
int miime_image_width(const MiimeImage *img);
int miime_image_height(const MiimeImage *img);

/* Get the underlying GdkPixbuf (borrowed — do not unref). */
GdkPixbuf *miime_image_get_pixbuf(const MiimeImage *img);

#endif /* MIIME_IMAGE_H */
