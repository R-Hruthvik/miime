#include "clipboard.h"

static cairo_status_t write_to_byte_array(void *closure,
                                          const unsigned char *data,
                                          unsigned int length) {
    GByteArray *ba = (GByteArray *)closure;
    g_byte_array_append(ba, data, length);
    return CAIRO_STATUS_SUCCESS;
}

void miime_clipboard_copy_surface(GdkClipboard *clipboard, cairo_surface_t *surface) {
    if (!clipboard || !surface) return;

    GByteArray *ba = g_byte_array_new();
    cairo_status_t st = cairo_surface_write_to_png_stream(surface, write_to_byte_array, ba);
    if (st != CAIRO_STATUS_SUCCESS) {
        g_byte_array_free(ba, TRUE);
        return;
    }

    GBytes *bytes = g_byte_array_free_to_bytes(ba);
    GError *err = NULL;
    GdkTexture *texture = gdk_texture_new_from_bytes(bytes, &err);
    g_bytes_unref(bytes);

    if (texture) {
        gdk_clipboard_set_texture(clipboard, texture);
        g_object_unref(texture);
    } else if (err) {
        g_printerr("miime: clipboard copy failed: %s\n", err->message);
        g_error_free(err);
    }
}
