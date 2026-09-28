#include "../src/image.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void test_dir_list(void) {
    /* Create dummy test directory with 3 images */
    g_mkdir_with_parents("/tmp/miime_test_dir", 0755);

    GdkPixbuf *pb = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 2, 2);
    assert(pb != NULL);
    gdk_pixbuf_save(pb, "/tmp/miime_test_dir/a.png", "png", NULL, NULL);
    gdk_pixbuf_save(pb, "/tmp/miime_test_dir/b.jpg", "jpeg", NULL, NULL);
    gdk_pixbuf_save(pb, "/tmp/miime_test_dir/c.png", "png", NULL, NULL);
    g_object_unref(pb);

    MiimeDirList *dl = miime_dir_list_new("/tmp/miime_test_dir/b.jpg");
    assert(dl != NULL);

    const char *cur = miime_dir_list_current(dl);
    assert(cur != NULL && strstr(cur, "b.jpg") != NULL);

    const char *next = miime_dir_list_next(dl);
    assert(next != NULL && strstr(next, "c.png") != NULL);

    /* Wraps around to a.png */
    next = miime_dir_list_next(dl);
    assert(next != NULL && strstr(next, "a.png") != NULL);

    /* Previous wraps back to c.png */
    const char *prev = miime_dir_list_prev(dl);
    assert(prev != NULL && strstr(prev, "c.png") != NULL);

    miime_dir_list_free(dl);

    /* Clean up */
    remove("/tmp/miime_test_dir/a.png");
    remove("/tmp/miime_test_dir/b.jpg");
    remove("/tmp/miime_test_dir/c.png");
    remove("/tmp/miime_test_dir");

    printf("PASS: test_dir_list\n");
}

int main(void) {
    test_load_valid_image();
    test_load_nonexistent();
    test_dir_list();
    printf("All image tests passed.\n");
    return 0;
}
