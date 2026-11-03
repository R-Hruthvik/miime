#include "../src/schema.h"
#include "../src/annotate.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_annotation_vocabulary_round_trip(void) {
    const char *types[] = {"highlight_box", "spacing_ruler", "alignment_redline", "callout", "pin", "freehand", "circle", "bracket", "underline", "strike"};
    const MiimeAnnotationType expected[] = {
        MIIME_ANNOT_HIGHLIGHT_BOX, MIIME_ANNOT_SPACING_RULER,
        MIIME_ANNOT_ALIGNMENT_REDLINE, MIIME_ANNOT_CALLOUT, MIIME_ANNOT_PIN,
        MIIME_ANNOT_FREEHAND, MIIME_ANNOT_CIRCLE, MIIME_ANNOT_BRACKET,
        MIIME_ANNOT_UNDERLINE, MIIME_ANNOT_STRIKE
    };
    MiimeAnnotationStack *stack = miime_annot_stack_new();
    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
        char json[256];
        snprintf(json, sizeof(json), "{\"annotations\":[{\"type\":\"%s\",\"points\":[1,2,30,40]}]}", types[i]);
        MiimeAnnotationStack *one = miime_schema_deserialize(json, NULL);
        assert(one && miime_annot_stack_count(one) == 1);
        const MiimeAnnotation *annotation = miime_annot_stack_get(one, 0);
        assert(annotation->type == expected[i]);
        miime_annot_stack_push(stack, *annotation);
        miime_annot_stack_free(one);
    }
    char *encoded = miime_schema_serialize(stack);
    MiimeAnnotationStack *decoded = miime_schema_deserialize(encoded, NULL);
    assert(decoded && miime_annot_stack_count(decoded) == 10);
    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
        char serialized_type[64];
        snprintf(serialized_type, sizeof(serialized_type), "\"type\":\"%s\"", types[i]);
        assert(miime_annot_stack_get(stack, (int)i)->type == expected[i]);
        assert(miime_annot_stack_get(decoded, (int)i)->type == expected[i]);
        assert(strstr(encoded, serialized_type) != NULL);
    }
    g_free(encoded);
    miime_annot_stack_free(stack);
    miime_annot_stack_free(decoded);
}

int main(void) {
    test_annotation_vocabulary_round_trip();
    MiimeAnnotationStack *preset=miime_schema_deserialize("{\"annotations\":[{\"ink\":\"bold\"},{\"ink\":\"regular\",\"stroke_width\":2}]}",NULL);
    assert(preset && miime_annot_stack_get(preset,0)->stroke_width==4 && !miime_annot_stack_get(preset,0)->has_stroke_width);
    assert(miime_annot_stack_get(preset,1)->stroke_width==2 && miime_annot_stack_get(preset,1)->has_stroke_width);
    char *preset_json=miime_schema_serialize(preset);
    MiimeAnnotationStack *preset2=miime_schema_deserialize(preset_json,NULL);
    assert(preset2 && miime_annot_stack_get(preset2,0)->stroke_width==4 && miime_annot_stack_get(preset2,1)->stroke_width==2);
    g_free(preset_json); miime_annot_stack_free(preset); miime_annot_stack_free(preset2);

    const char *vocab = "{\"annotations\":[{\"id\":\"v\",\"type\":\"arrow\",\"source\":\"agent\",\"points\":[[1,2],[3,4]],\"color\":[1,0,0,1],\"stroke_width\":[4,2],\"ink\":\"bold\",\"dash\":\"dashed\",\"fill\":\"hachure\",\"fill_color\":[1,0.5,0,0.2],\"radius\":12,\"arrow\":\"end\",\"z\":\"under\",\"som_id\":40,\"label\":{\"text\":\"hi\",\"anchor\":\"auto\",\"leader\":\"elbow\"}}]}";
    const char *registry = "[{\"id\":40,\"bounds\":[0,0,20,20]}]";
    MiimeAnnotationStack *vs = miime_schema_deserialize_ex(vocab, NULL, registry);
    assert(vs && miime_annot_stack_count(vs) == 1);
    const MiimeAnnotation *va = miime_annot_stack_get(vs, 0);
    assert(va->type == MIIME_ANNOT_ARROW && va->source == MIIME_SOURCE_AGENT);
    assert(va->ink == 2 && va->dash == 1 && va->fill == 1 && va->stroke_width_end == 2);
    assert(va->arrow == 2 && va->z == 0 && va->som_id == 40);
    char *vjson = miime_schema_serialize(vs);
    MiimeAnnotationStack *vs2 = miime_schema_deserialize_ex(vjson, NULL, registry);
    assert(vs2 && miime_annot_stack_count(vs2) == 1);
    va = miime_annot_stack_get(vs2, 0);
    assert(va->type == MIIME_ANNOT_ARROW && va->source == MIIME_SOURCE_AGENT);
    assert(va->ink == 2 && va->dash == 1 && va->fill == 1 && va->stroke_width_end == 2);
    assert(va->arrow == 2 && va->z == 0 && va->som_id == 40);
    assert(strcmp(va->label_text,"hi") == 0 && va->label_anchor == -1 && va->label_leader == 1);
    g_free(vjson); miime_annot_stack_free(vs); miime_annot_stack_free(vs2);

    MiimeAnnotationStack *s = miime_annot_stack_new();
    MiimeAnnotation a;
    memset(&a, 0, sizeof(a));
    a.type = MIIME_ANNOT_RECT;
    strcpy(a.json_id, "box1");
    a.source = MIIME_SOURCE_AGENT;
    a.points[0]=10; a.points[1]=10; a.points[2]=50; a.points[3]=50;
    a.point_count=4;
    a.color_r=1; a.color_g=0; a.color_b=0; a.color_a=1;
    a.stroke_width=2;
    miime_annot_stack_push(s, a);

    char *json = miime_schema_serialize(s);
    assert(json != NULL);
    assert(strstr(json, "\"source\":\"agent\"") != NULL);

    /* reload */
    GError *err = NULL;
    MiimeAnnotationStack *s2 = miime_schema_deserialize(json, &err);
    assert(err == NULL && s2 != NULL);
    assert(miime_annot_stack_count(s2) == 1);
    const MiimeAnnotation *g0 = miime_annot_stack_get(s2, 0);
    assert(strcmp(g0->json_id, "box1") == 0);
    assert(g0->source == MIIME_SOURCE_AGENT);

    /* simulate human move: push new version? Undo/redo: push modified annotation */
    MiimeAnnotation moved = *g0;
    moved.points[0] = 20; moved.points[1] = 20;
    moved.source = MIIME_SOURCE_HUMAN;
    miime_annot_stack_push(s2, moved);
    assert(miime_annot_stack_count(s2) == 2);

    /* undo then redo */
    assert(miime_annot_stack_undo(s2) == TRUE);
    assert(miime_annot_stack_count(s2) == 1);
    assert(miime_annot_stack_redo(s2) == TRUE);
    assert(miime_annot_stack_count(s2) == 2);

    /* export + reload */
    char *json2 = miime_schema_serialize(s2);
    assert(json2 != NULL);
    MiimeAnnotationStack *s3 = miime_schema_deserialize(json2, &err);
    assert(err == NULL && s3 != NULL);
    assert(miime_annot_stack_count(s3) == 2);
    const MiimeAnnotation *last = miime_annot_stack_get(s3, 1);
    assert(last->points[0] == 20);
    assert(last->source == MIIME_SOURCE_HUMAN);

    /* default source human for missing field */
    const char *legacy = "{\"annotations\":[{\"id\":\"l\",\"type\":\"pen\",\"points\":[1,2],\"color\":[1,0,0,1],\"stroke_width\":2,\"text\":\"\",\"font_size\":18}]}";
    MiimeAnnotationStack *sl = miime_schema_deserialize(legacy, &err);
    assert(sl != NULL);
    assert(miime_annot_stack_get(sl,0)->source == MIIME_SOURCE_HUMAN);

    g_free(json); g_free(json2);
    miime_annot_stack_free(s);
    miime_annot_stack_free(s2);
    miime_annot_stack_free(s3);
    miime_annot_stack_free(sl);
    printf("All bidir tests passed.\n");
    return 0;
}
