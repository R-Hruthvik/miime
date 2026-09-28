#include "image.h"
#include <stdlib.h>

struct MiimeImage {
    GdkPixbuf *pixbuf;
    int        width;
    int        height;
};

MiimeImage *miime_image_load(const char *path) {
    if (!path) return NULL;

    GError *err = NULL;
    GdkPixbuf *pb = gdk_pixbuf_new_from_file(path, &err);
    if (!pb) {
        if (err) {
            g_printerr("miime: failed to load '%s': %s\n", path, err->message);
            g_error_free(err);
        }
        return NULL;
    }

    MiimeImage *img = malloc(sizeof(MiimeImage));
    if (!img) {
        g_object_unref(pb);
        return NULL;
    }

    img->pixbuf = pb;
    img->width  = gdk_pixbuf_get_width(pb);
    img->height = gdk_pixbuf_get_height(pb);
    return img;
}

void miime_image_free(MiimeImage *img) {
    if (img) {
        if (img->pixbuf) {
            g_object_unref(img->pixbuf);
        }
        free(img);
    }
}

int miime_image_width(const MiimeImage *img)  { return img ? img->width  : 0; }
int miime_image_height(const MiimeImage *img) { return img ? img->height : 0; }
GdkPixbuf *miime_image_get_pixbuf(const MiimeImage *img) { return img ? img->pixbuf : NULL; }
