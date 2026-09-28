#include "../src/annotate.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static MiimeAnnotation make_pen(double x1, double y1, double x2, double y2) {
    MiimeAnnotation a;
    memset(&a, 0, sizeof(a));
    a.type = MIIME_ANNOT_PEN;
    a.points[0] = x1; a.points[1] = y1;
    a.points[2] = x2; a.points[3] = y2;
    a.point_count = 4;
    a.color_r = 1.0; a.color_g = 0.0; a.color_b = 0.0; a.color_a = 1.0;
    a.stroke_width = 2.0;
    return a;
}

static void test_push_and_count(void) {
    MiimeAnnotationStack *s = miime_annot_stack_new();
    assert(miime_annot_stack_count(s) == 0);
    miime_annot_stack_push(s, make_pen(0, 0, 10, 10));
    assert(miime_annot_stack_count(s) == 1);
    miime_annot_stack_push(s, make_pen(5, 5, 20, 20));
    assert(miime_annot_stack_count(s) == 2);
    miime_annot_stack_free(s);
    printf("PASS: test_push_and_count\n");
}

static void test_undo_redo(void) {
    MiimeAnnotationStack *s = miime_annot_stack_new();
    miime_annot_stack_push(s, make_pen(0, 0, 10, 10));
    miime_annot_stack_push(s, make_pen(5, 5, 20, 20));
    assert(miime_annot_stack_count(s) == 2);

    assert(miime_annot_stack_undo(s) == TRUE);
    assert(miime_annot_stack_count(s) == 1);

    assert(miime_annot_stack_redo(s) == TRUE);
    assert(miime_annot_stack_count(s) == 2);

    /* Undo both */
    assert(miime_annot_stack_undo(s) == TRUE);
    assert(miime_annot_stack_undo(s) == TRUE);
    assert(miime_annot_stack_count(s) == 0);

    /* Can't undo past empty */
    assert(miime_annot_stack_undo(s) == FALSE);

    /* Redo all */
    assert(miime_annot_stack_redo(s) == TRUE);
    assert(miime_annot_stack_redo(s) == TRUE);
    assert(miime_annot_stack_count(s) == 2);

    /* Can't redo past top */
    assert(miime_annot_stack_redo(s) == FALSE);

    miime_annot_stack_free(s);
    printf("PASS: test_undo_redo\n");
}

static void test_push_clears_redo(void) {
    MiimeAnnotationStack *s = miime_annot_stack_new();
    miime_annot_stack_push(s, make_pen(0, 0, 10, 10));
    miime_annot_stack_push(s, make_pen(5, 5, 20, 20));

    miime_annot_stack_undo(s); /* undo second */
    assert(miime_annot_stack_count(s) == 1);

    /* Push a new one — redo stack should be cleared */
    miime_annot_stack_push(s, make_pen(30, 30, 40, 40));
    assert(miime_annot_stack_count(s) == 2);
    assert(miime_annot_stack_redo(s) == FALSE); /* redo cleared */

    miime_annot_stack_free(s);
    printf("PASS: test_push_clears_redo\n");
}

static void test_get(void) {
    MiimeAnnotationStack *s = miime_annot_stack_new();
    MiimeAnnotation a = make_pen(42, 43, 44, 45);
    miime_annot_stack_push(s, a);

    const MiimeAnnotation *got = miime_annot_stack_get(s, 0);
    assert(got != NULL);
    assert(got->points[0] == 42);
    assert(got->type == MIIME_ANNOT_PEN);

    miime_annot_stack_free(s);
    printf("PASS: test_get\n");
}

int main(void) {
    test_push_and_count();
    test_undo_redo();
    test_push_clears_redo();
    test_get();
    printf("All annotate tests passed.\n");
    return 0;
}
