#ifndef MIIME_IMAGE_H
#define MIIME_IMAGE_H

#include <gdk-pixbuf/gdk-pixbuf.h>

typedef struct MiimeImage MiimeImage;
typedef struct MiimeDirList MiimeDirList;

/* Load an image from a file path. Returns NULL on failure. */
MiimeImage *miime_image_load(const char *path);

/* Free the image and its pixel buffer. */
void miime_image_free(MiimeImage *img);

/* Get pixel dimensions. */
int miime_image_width(const MiimeImage *img);
int miime_image_height(const MiimeImage *img);

/* Get the underlying GdkPixbuf (borrowed — do not unref). */
GdkPixbuf *miime_image_get_pixbuf(const MiimeImage *img);

/* Scan the directory containing image_path for image files, sorted alphabetically.
 * Sets current index to the position of image_path in the list. */
MiimeDirList *miime_dir_list_new(const char *image_path);
void miime_dir_list_free(MiimeDirList *dl);
const char *miime_dir_list_current(MiimeDirList *dl);
const char *miime_dir_list_next(MiimeDirList *dl);
const char *miime_dir_list_prev(MiimeDirList *dl);

#endif /* MIIME_IMAGE_H */
