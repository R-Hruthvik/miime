#include "../src/render.h"
#include "../src/annotate.h"
#include "../src/image.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_render_shapes_offscreen(void) {
    cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 200, 200);
    cairo_t *cr = cairo_create(surf);

    MiimeAnnotationStack *stack = miime_annot_stack_new();

    /* Pen */
    MiimeAnnotation a1;
    memset(&a1, 0, sizeof(a1));
    a1.type = MIIME_ANNOT_PEN;
    a1.points[0] = 10; a1.points[1] = 10;
    a1.points[2] = 20; a1.points[3] = 20;
    a1.point_count = 4;
    a1.color_r = 1.0; a1.color_a = 1.0;
    a1.stroke_width = 2.0;
    miime_annot_stack_push(stack, a1);

    /* Line */
    MiimeAnnotation a2;
    memset(&a2, 0, sizeof(a2));
    a2.type = MIIME_ANNOT_LINE;
    a2.points[0] = 30; a2.points[1] = 30;
    a2.points[2] = 60; a2.points[3] = 60;
    a2.point_count = 4;
    a2.color_g = 1.0; a2.color_a = 1.0;
    a2.stroke_width = 3.0;
    miime_annot_stack_push(stack, a2);

    /* Rect */
    MiimeAnnotation a3;
    memset(&a3, 0, sizeof(a3));
    a3.type = MIIME_ANNOT_RECT;
    a3.points[0] = 50; a3.points[1] = 50;
    a3.points[2] = 100; a3.points[3] = 80;
    a3.point_count = 4;
    a3.color_b = 1.0; a3.color_a = 1.0;
    a3.stroke_width = 2.0;
    miime_annot_stack_push(stack, a3);

    /* Ellipse */
    MiimeAnnotation a4;
    memset(&a4, 0, sizeof(a4));
    a4.type = MIIME_ANNOT_ELLIPSE;
    a4.points[0] = 80; a4.points[1] = 80;
    a4.points[2] = 140; a4.points[3] = 120;
    a4.point_count = 4;
    a4.color_r = 1.0; a4.color_g = 1.0; a4.color_a = 1.0;
    a4.stroke_width = 2.0;
    miime_annot_stack_push(stack, a4);

    /* Arrow */
    MiimeAnnotation a5;
    memset(&a5, 0, sizeof(a5));
    a5.type = MIIME_ANNOT_ARROW;
    a5.points[0] = 10; a5.points[1] = 100;
    a5.points[2] = 90; a5.points[3] = 150;
    a5.point_count = 4;
    a5.color_r = 1.0; a5.color_b = 1.0; a5.color_a = 1.0;
    a5.stroke_width = 4.0;
    miime_annot_stack_push(stack, a5);

    /* Text */
    MiimeAnnotation a6;
    memset(&a6, 0, sizeof(a6));
    a6.type = MIIME_ANNOT_TEXT;
    a6.points[0] = 20; a6.points[1] = 50;
    a6.point_count = 2;
    strncpy(a6.text, "Hello MIIME", sizeof(a6.text) - 1);
    a6.font_size = 14.0;
    a6.color_r = 1.0; a6.color_a = 1.0;
    miime_annot_stack_push(stack, a6);

    /* Blur */
    MiimeAnnotation a7;
    memset(&a7, 0, sizeof(a7));
    a7.type = MIIME_ANNOT_BLUR;
    a7.points[0] = 10; a7.points[1] = 10;
    a7.points[2] = 50; a7.points[3] = 50;
    a7.point_count = 4;
    a7.blur_width = 40;
    a7.blur_height = 40;
    a7.blur_stride = 40 * 4;
    a7.blur_data = calloc(40 * 40 * 4, 1);
    miime_annot_stack_push(stack, a7);

    /* Crop */
    MiimeAnnotation a8;
    memset(&a8, 0, sizeof(a8));
    a8.type = MIIME_ANNOT_CROP;
    a8.points[0] = 5; a8.points[1] = 5;
    a8.points[2] = 150; a8.points[3] = 150;
    a8.point_count = 4;
    miime_annot_stack_push(stack, a8);

    /* Preview shape */
    MiimeAnnotation preview;
    memset(&preview, 0, sizeof(preview));
    preview.type = MIIME_ANNOT_LINE;
    preview.points[0] = 0; preview.points[1] = 0;
    preview.points[2] = 10; preview.points[3] = 10;
    preview.point_count = 4;
    preview.color_r = 0.5; preview.color_a = 0.5;
    preview.stroke_width = 1.0;

    miime_render_annotations(cr, stack, &preview);
    assert(cairo_status(cr) == CAIRO_STATUS_SUCCESS);

    cairo_destroy(cr);
    cairo_surface_destroy(surf);
    miime_annot_stack_free(stack);

    printf("PASS: test_render_shapes_offscreen\n");
}

static void test_render_export(void) {
    /* Create a 100x100 dummy image */
    GdkPixbuf *pb = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 100, 100);
    gdk_pixbuf_save(pb, "/tmp/miime_test_export_base.png", "png", NULL, NULL);
    g_object_unref(pb);

    MiimeImage *img = miime_image_load("/tmp/miime_test_export_base.png");
    assert(img != NULL);

    MiimeAnnotationStack *stack = miime_annot_stack_new();

    /* Uncropped export */
    int w = 0, h = 0;
    cairo_surface_t *surf = miime_render_export(img, stack, &w, &h);
    assert(surf != NULL);
    assert(w == 100 && h == 100);

    gboolean saved_png = miime_render_save_to_file(surf, "/tmp/miime_test_out.png");
    assert(saved_png == TRUE);

    gboolean saved_jpg = miime_render_save_to_file(surf, "/tmp/miime_test_out.jpg");
    assert(saved_jpg == TRUE);
    cairo_surface_destroy(surf);

    /* Add crop annotation: 20,20 to 60,70 -> 40x50 */
    MiimeAnnotation crop;
    memset(&crop, 0, sizeof(crop));
    crop.type = MIIME_ANNOT_CROP;
    crop.points[0] = 20; crop.points[1] = 20;
    crop.points[2] = 60; crop.points[3] = 70;
    crop.point_count = 4;
    miime_annot_stack_push(stack, crop);

    cairo_surface_t *cropped_surf = miime_render_export(img, stack, &w, &h);
    assert(cropped_surf != NULL);
    assert(w == 40 && h == 50);
    cairo_surface_destroy(cropped_surf);

    miime_annot_stack_free(stack);
    miime_image_free(img);

    remove("/tmp/miime_test_export_base.png");
    remove("/tmp/miime_test_out.png");
    remove("/tmp/miime_test_out.jpg");

    printf("PASS: test_render_export\n");
}

int main(void) {
    test_render_shapes_offscreen();
    test_render_export();
    printf("All render tests passed.\n");
    return 0;
}
