#include "image.h"
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

struct MiimeImage {
    GdkPixbuf       *pixbuf;
    cairo_surface_t *surface;
    int              width;
    int              height;
};

struct MiimeDirList {
    char **paths;       /* array of full paths */
    int    count;
    int    current;     /* index of current image */
};

static cairo_surface_t *pixbuf_to_cairo_surface(GdkPixbuf *pb, int w, int h) {
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    if (!surface || cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        return NULL;
    }

    unsigned char *surf_data = cairo_image_surface_get_data(surface);
    int surf_stride = cairo_image_surface_get_stride(surface);

    cairo_surface_flush(surface);

    const guchar *px = gdk_pixbuf_get_pixels(pb);
    int rowstride = gdk_pixbuf_get_rowstride(pb);
    int n_channels = gdk_pixbuf_get_n_channels(pb);
    gboolean has_alpha = gdk_pixbuf_get_has_alpha(pb);

    for (int y = 0; y < h; y++) {
        const guchar *src_row = px + y * rowstride;
        unsigned char *dst_row = surf_data + y * surf_stride;
        for (int x = 0; x < w; x++) {
            guchar r = src_row[x * n_channels + 0];
            guchar g = src_row[x * n_channels + 1];
            guchar b = src_row[x * n_channels + 2];
            guchar a = has_alpha ? src_row[x * n_channels + 3] : 255;
            dst_row[x * 4 + 0] = (guchar)((b * a) / 255); /* B */
            dst_row[x * 4 + 1] = (guchar)((g * a) / 255); /* G */
            dst_row[x * 4 + 2] = (guchar)((r * a) / 255); /* R */
            dst_row[x * 4 + 3] = a;                        /* A */
        }
    }

    cairo_surface_mark_dirty(surface);
    return surface;
}

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

    img->width  = gdk_pixbuf_get_width(pb);
    img->height = gdk_pixbuf_get_height(pb);
    img->surface = pixbuf_to_cairo_surface(pb, img->width, img->height);
    img->pixbuf = pb;

    return img;
}

void miime_image_free(MiimeImage *img) {
    if (img) {
        if (img->surface) {
            cairo_surface_destroy(img->surface);
        }
        if (img->pixbuf) {
            g_object_unref(img->pixbuf);
        }
        free(img);
    }
}

int miime_image_width(const MiimeImage *img)  { return img ? img->width  : 0; }
int miime_image_height(const MiimeImage *img) { return img ? img->height : 0; }
cairo_surface_t *miime_image_get_surface(const MiimeImage *img) { return img ? img->surface : NULL; }
GdkPixbuf *miime_image_get_pixbuf(const MiimeImage *img) { return img ? img->pixbuf : NULL; }

static int str_compare(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

static gboolean is_image_extension(const char *name) {
    const char *ext = strrchr(name, '.');
    if (!ext) return FALSE;
    ext++; /* skip the dot */
    const char *supported[] = {"png", "jpg", "jpeg", "webp", "bmp", "gif", "tiff", "tif", "svg", NULL};
    for (int i = 0; supported[i]; i++) {
        if (g_ascii_strcasecmp(ext, supported[i]) == 0) return TRUE;
    }
    return FALSE;
}

MiimeDirList *miime_dir_list_new(const char *image_path) {
    if (!image_path) return NULL;

    char *path_copy = g_strdup(image_path);
    char *dir = g_path_get_dirname(path_copy);
    char *basename = g_path_get_basename(path_copy);

    DIR *d = opendir(dir);
    if (!d) {
        g_free(path_copy);
        g_free(dir);
        g_free(basename);
        return NULL;
    }

    MiimeDirList *dl = malloc(sizeof(MiimeDirList));
    if (!dl) {
        closedir(d);
        g_free(path_copy);
        g_free(dir);
        g_free(basename);
        return NULL;
    }
    dl->paths = NULL;
    dl->count = 0;
    dl->current = 0;

    int capacity = 64;
    dl->paths = malloc(sizeof(char *) * capacity);
    if (!dl->paths) {
        closedir(d);
        free(dl);
        g_free(path_copy);
        g_free(dir);
        g_free(basename);
        return NULL;
    }

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        if (!is_image_extension(entry->d_name)) continue;

        if (dl->count >= capacity) {
            capacity *= 2;
            char **new_paths = realloc(dl->paths, sizeof(char *) * capacity);
            if (!new_paths) break;
            dl->paths = new_paths;
        }
        dl->paths[dl->count] = g_build_filename(dir, entry->d_name, NULL);
        dl->count++;
    }
    closedir(d);

    /* Sort alphabetically */
    if (dl->count > 0) {
        qsort(dl->paths, dl->count, sizeof(char *), str_compare);
    }

    /* Find current image index */
    char *full_path = g_build_filename(dir, basename, NULL);
    for (int i = 0; i < dl->count; i++) {
        if (strcmp(dl->paths[i], full_path) == 0) {
            dl->current = i;
            break;
        }
    }
    g_free(full_path);
    g_free(path_copy);
    g_free(dir);
    g_free(basename);

    return dl;
}

void miime_dir_list_free(MiimeDirList *dl) {
    if (!dl) return;
    for (int i = 0; i < dl->count; i++) {
        g_free(dl->paths[i]);
    }
    free(dl->paths);
    free(dl);
}

const char *miime_dir_list_current(MiimeDirList *dl) {
    if (!dl || dl->count == 0) return NULL;
    return dl->paths[dl->current];
}

const char *miime_dir_list_next(MiimeDirList *dl) {
    if (!dl || dl->count == 0) return NULL;
    dl->current = (dl->current + 1) % dl->count;
    return dl->paths[dl->current];
}

const char *miime_dir_list_prev(MiimeDirList *dl) {
    if (!dl || dl->count == 0) return NULL;
    dl->current = (dl->current - 1 + dl->count) % dl->count;
    return dl->paths[dl->current];
}
