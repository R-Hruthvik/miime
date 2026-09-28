#include "../src/image.h"
#include <assert.h>
#include <stdio.h>

static void test_load_valid_image(void) {
    /* Create a tiny 2x2 test PNG using GdkPixbuf */
    GdkPixbuf *pb = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 2, 2);
    assert(pb != NULL);
    gdk_pixbuf_fill(pb, 0xFF0000FF); /* red */
    GError *err = NULL;
    gboolean ok = gdk_pixbuf_save(pb, "/tmp/miime_test.png", "png", &err, NULL);
    assert(ok && err == NULL);
    g_object_unref(pb);

    MiimeImage *img = miime_image_load("/tmp/miime_test.png");
    assert(img != NULL);
    assert(miime_image_width(img) == 2);
    assert(miime_image_height(img) == 2);
    assert(miime_image_get_pixbuf(img) != NULL);
    miime_image_free(img);

    printf("PASS: test_load_valid_image\n");
}

static void test_load_nonexistent(void) {
    MiimeImage *img = miime_image_load("/tmp/miime_no_such_file.png");
    assert(img == NULL);
    printf("PASS: test_load_nonexistent\n");
}

int main(void) {
    test_load_valid_image();
    test_load_nonexistent();
    printf("All image tests passed.\n");
    return 0;
}
