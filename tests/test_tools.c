#include "../src/tools.h"
#include "../src/annotate.h"
#include "../src/image.h"
#include <gtk/gtk.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_text_tool_typing_and_commit(void) {
    MiimeAnnotationStack *stack = miime_annot_stack_new();
    MiimeToolState *ts = miime_tool_state_new();

    miime_tool_set_type(ts, MIIME_TOOL_TEXT);
    assert(miime_tool_get_type(ts) == MIIME_TOOL_TEXT);

    /* 1. Click on canvas at (100, 200) */
    miime_tool_on_press(ts, 100, 200, stack);
    const MiimeAnnotation *prev = miime_tool_get_preview(ts);
    assert(prev != NULL);
    assert(prev->type == MIIME_ANNOT_TEXT);
    assert(prev->points[0] == 100);
    assert(prev->points[1] == 200);
    assert(prev->text[0] == '\0');

    /* 2. Type "Test 1" */
    assert(miime_tool_text_key(ts, GDK_KEY_T, stack) == TRUE);
    assert(miime_tool_text_key(ts, GDK_KEY_e, stack) == TRUE);
    assert(miime_tool_text_key(ts, GDK_KEY_s, stack) == TRUE);
    assert(miime_tool_text_key(ts, GDK_KEY_t, stack) == TRUE);
    assert(miime_tool_text_key(ts, GDK_KEY_space, stack) == TRUE);
    assert(miime_tool_text_key(ts, GDK_KEY_1, stack) == TRUE);
    assert(strcmp(prev->text, "Test 1") == 0);

    /* 3. Backspace */
    assert(miime_tool_text_key(ts, GDK_KEY_BackSpace, stack) == TRUE);
    assert(strcmp(prev->text, "Test ") == 0);
    assert(miime_tool_text_key(ts, GDK_KEY_2, stack) == TRUE);
    assert(strcmp(prev->text, "Test 2") == 0);

    /* 4. Commit with Enter */
    assert(miime_tool_text_key(ts, GDK_KEY_Return, stack) == TRUE);
    assert(miime_tool_get_preview(ts) == NULL);
    assert(miime_annot_stack_count(stack) == 1);

    const MiimeAnnotation *comm = miime_annot_stack_get(stack, 0);
    assert(comm->type == MIIME_ANNOT_TEXT);
    assert(strcmp(comm->text, "Test 2") == 0);
    assert(comm->points[0] == 100);
    assert(comm->points[1] == 200);

    miime_tool_state_free(ts);
    miime_annot_stack_free(stack);
    printf("PASS: test_text_tool_typing_and_commit\n");
}

static void test_text_tool_cancel_escape(void) {
    MiimeAnnotationStack *stack = miime_annot_stack_new();
    MiimeToolState *ts = miime_tool_state_new();

    miime_tool_set_type(ts, MIIME_TOOL_TEXT);
    miime_tool_on_press(ts, 50, 50, stack);
    assert(miime_tool_text_key(ts, GDK_KEY_A, stack) == TRUE);
    assert(miime_tool_text_key(ts, GDK_KEY_b, stack) == TRUE);

    /* Cancel with Escape */
    assert(miime_tool_text_key(ts, GDK_KEY_Escape, stack) == TRUE);
    assert(miime_tool_get_preview(ts) == NULL);
    assert(miime_annot_stack_count(stack) == 0);

    miime_tool_state_free(ts);
    miime_annot_stack_free(stack);
    printf("PASS: test_text_tool_cancel_escape\n");
}

static void test_text_tool_click_commits_previous(void) {
    MiimeAnnotationStack *stack = miime_annot_stack_new();
    MiimeToolState *ts = miime_tool_state_new();

    miime_tool_set_type(ts, MIIME_TOOL_TEXT);
    miime_tool_on_press(ts, 10, 10, stack);
    miime_tool_text_key(ts, GDK_KEY_F, stack);
    miime_tool_text_key(ts, GDK_KEY_i, stack);
    miime_tool_text_key(ts, GDK_KEY_r, stack);
    miime_tool_text_key(ts, GDK_KEY_s, stack);
    miime_tool_text_key(ts, GDK_KEY_t, stack);

    /* Click second location without pressing Enter */
    miime_tool_on_press(ts, 200, 200, stack);

    /* First text should now be committed to stack */
    assert(miime_annot_stack_count(stack) == 1);
    assert(strcmp(miime_annot_stack_get(stack, 0)->text, "First") == 0);

    /* Second text is now active preview */
    const MiimeAnnotation *prev = miime_tool_get_preview(ts);
    assert(prev != NULL);
    assert(prev->points[0] == 200);
    assert(prev->points[1] == 200);
    assert(prev->text[0] == '\0');

    miime_tool_text_key(ts, GDK_KEY_S, stack);
    miime_tool_text_key(ts, GDK_KEY_Return, stack);
    assert(miime_annot_stack_count(stack) == 2);
    assert(strcmp(miime_annot_stack_get(stack, 1)->text, "S") == 0);

    miime_tool_state_free(ts);
    miime_annot_stack_free(stack);
    printf("PASS: test_text_tool_click_commits_previous\n");
}

int main(void) {
    test_text_tool_typing_and_commit();
    test_text_tool_cancel_escape();
    test_text_tool_click_commits_previous();
    printf("All text tool tests passed.\n");
    return 0;
}
