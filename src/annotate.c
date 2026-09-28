#include "annotate.h"
#include <stdlib.h>
#include <string.h>

struct MiimeAnnotationStack {
    MiimeAnnotation *items;
    int capacity;
    int total;    /* total items in buffer (including undone) */
    int active;   /* number of active (non-undone) items, active <= total */
};

MiimeAnnotationStack *miime_annot_stack_new(void) {
    MiimeAnnotationStack *s = malloc(sizeof(MiimeAnnotationStack));
    if (!s) return NULL;
    s->capacity = 64;
    s->items = malloc(sizeof(MiimeAnnotation) * s->capacity);
    if (!s->items) {
        free(s);
        return NULL;
    }
    s->total = 0;
    s->active = 0;
    return s;
}

void miime_annot_stack_free(MiimeAnnotationStack *s) {
    if (!s) return;
    /* Free any blur_data in annotations */
    for (int i = 0; i < s->total; i++) {
        if (s->items[i].blur_data) {
            free(s->items[i].blur_data);
        }
    }
    free(s->items);
    free(s);
}

void miime_annot_stack_push(MiimeAnnotationStack *s, MiimeAnnotation annot) {
    if (!s) return;

    /* Discard any undone items (clear redo stack) */
    for (int i = s->active; i < s->total; i++) {
        if (s->items[i].blur_data) {
            free(s->items[i].blur_data);
            s->items[i].blur_data = NULL;
        }
    }
    s->total = s->active;

    /* Grow if needed */
    if (s->total >= s->capacity) {
        s->capacity *= 2;
        MiimeAnnotation *new_items = realloc(s->items, sizeof(MiimeAnnotation) * s->capacity);
        if (!new_items) return;
        s->items = new_items;
    }

    s->items[s->total] = annot;
    s->total++;
    s->active = s->total;
}

gboolean miime_annot_stack_undo(MiimeAnnotationStack *s) {
    if (!s || s->active == 0) return FALSE;
    s->active--;
    return TRUE;
}

gboolean miime_annot_stack_redo(MiimeAnnotationStack *s) {
    if (!s || s->active >= s->total) return FALSE;
    s->active++;
    return TRUE;
}

int miime_annot_stack_count(const MiimeAnnotationStack *s) {
    return s ? s->active : 0;
}

const MiimeAnnotation *miime_annot_stack_get(const MiimeAnnotationStack *s, int index) {
    if (!s || index < 0 || index >= s->active) return NULL;
    return &s->items[index];
}

void miime_annot_stack_clear(MiimeAnnotationStack *s) {
    if (!s) return;
    for (int i = 0; i < s->total; i++) {
        if (s->items[i].blur_data) {
            free(s->items[i].blur_data);
            s->items[i].blur_data = NULL;
        }
    }
    s->total = 0;
    s->active = 0;
}
