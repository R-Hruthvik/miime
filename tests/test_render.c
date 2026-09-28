#include "../src/render.h"
#include "../src/annotate.h"
#include <assert.h>
#include <stdio.h>
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

int main(void) {
    test_render_shapes_offscreen();
    printf("All render shape tests passed.\n");
    return 0;
}
