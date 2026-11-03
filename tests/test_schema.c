#include "../src/schema.h"
#include "../src/annotate.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    MiimeStyleConfig cfg = {0};
    cfg.drawing_style = MIIME_DRAW_EDITORIAL; cfg.has_drawing_style = TRUE;
    cfg.theme = MIIME_THEME_AUTO; cfg.has_theme = TRUE;
    cfg.analysis_background_rgba = 0xABCDEFff; cfg.has_analysis_background = TRUE;
    char *cfg_json = miime_schema_serialize_render_config(&cfg);
    assert(cfg_json && strcmp(cfg_json, "{\"drawing_style\":\"editorial\",\"theme\":\"auto\",\"analysis_background\":\"#ABCDEF\"}") == 0);
    MiimeStyleConfig cfg2 = {0}; GError *cfg_err = NULL;
    assert(miime_schema_deserialize_render_config(cfg_json, &cfg2, &cfg_err) && cfg_err == NULL);
    assert(cfg2.has_drawing_style && cfg2.drawing_style == MIIME_DRAW_EDITORIAL && cfg2.has_theme && cfg2.theme == MIIME_THEME_AUTO);
    assert(cfg2.has_analysis_background && cfg2.analysis_background_rgba == 0xABCDEFFF);
    g_free(cfg_json);
    MiimeStyleConfig alias_only={0};
    assert(miime_schema_deserialize_render_config("{\"aesthetic_style\":\"clean_technical\"}",&alias_only,NULL));
    assert(alias_only.has_aesthetic_style && !alias_only.has_drawing_style && !alias_only.has_theme);
    const char *bad_config[] = {"{\"drawing_style\":\"scribble\"}", "{\"theme\":false}", "{\"analysis_background\":\"#11223344\"}", "{\"analysis_background\":\"#gg2233\"}", "{\"reset_render_config\":\"true\"}"};
    for (guint i=0; i<G_N_ELEMENTS(bad_config); i++) {
        MiimeStyleConfig unchanged = {0}; unchanged.theme = MIIME_THEME_WARM; unchanged.has_theme = TRUE;
        cfg_err = NULL;
        assert(!miime_schema_deserialize_render_config(bad_config[i], &unchanged, &cfg_err));
        assert(cfg_err && unchanged.theme == MIIME_THEME_WARM && unchanged.has_theme);
        g_error_free(cfg_err);
    }
    MiimeAnnotationStack *presence = miime_schema_deserialize("{\"annotations\":[{\"font_size\":0,\"color\":[0,0,0,0],\"role\":\"info\",\"label_surface\":[0,0.5,1,1]}]}", NULL);
    assert(presence && miime_annot_stack_get(presence,0)->has_font_size && miime_annot_stack_get(presence,0)->font_size==0);
    assert(miime_annot_stack_get(presence,0)->has_color && miime_annot_stack_get(presence,0)->has_role && miime_annot_stack_get(presence,0)->role==MIIME_ROLE_INFO);
    assert(miime_annot_stack_get(presence,0)->has_label_surface);
    MiimeStyleConfig active = {0}; active.has_theme=TRUE; active.theme=MIIME_THEME_LIGHT;
    char *opt_json = miime_schema_serialize_with_config(presence, &active);
    assert(opt_json && strstr(opt_json,"\"role\":\"info\"") && strstr(opt_json,"\"font_size\":0") && strstr(opt_json,"\"color\":[0,0,0,0]"));
    MiimeAnnotationStack *presence2=miime_schema_deserialize(opt_json,NULL);
    assert(presence2 && miime_annot_stack_get(presence2,0)->has_font_size && miime_annot_stack_get(presence2,0)->font_size==0);
    g_free(opt_json); miime_annot_stack_free(presence); miime_annot_stack_free(presence2);
    MiimeAnnotationStack *automatic=miime_schema_deserialize("{\"annotations\":[{\"type\":\"text\",\"text\":\"auto\"}]}",NULL);
    char *auto1=miime_schema_serialize_with_config(automatic,&active);
    MiimeAnnotationStack *automatic2=miime_schema_deserialize(auto1,NULL);
    char *auto2=miime_schema_serialize_with_config(automatic2,&active);
    assert(auto1 && auto2 && strstr(auto1,"\"color\"")==NULL && strstr(auto1,"\"font_size\"")==NULL && strstr(auto1,"\"stroke_width\"")==NULL);
    assert(strstr(auto2,"\"color\"")==NULL && strstr(auto2,"\"font_size\"")==NULL && strstr(auto2,"\"stroke_width\"")==NULL);
    g_free(auto1); g_free(auto2); miime_annot_stack_free(automatic); miime_annot_stack_free(automatic2);
    static const char *role_names[]={"none","error","warning","info"};
    for(guint i=0;i<G_N_ELEMENTS(role_names);i++) {
        char *role_input=g_strdup_printf("{\"annotations\":[{\"role\":\"%s\"}]}",role_names[i]);
        MiimeAnnotationStack *rs=miime_schema_deserialize(role_input,NULL);
        char *rout=miime_schema_serialize(rs);
        assert(rs && miime_annot_stack_get(rs,0)->has_role && miime_annot_stack_get(rs,0)->role==(MiimeSemanticRole)i);
        assert(rout && strstr(rout,role_names[i]));
        g_free(role_input); g_free(rout); miime_annot_stack_free(rs);
    }

    const char *bad_widths[] = {
        "{\"annotations\":[{\"stroke_width\":\"wide\"}]}",
        "{\"annotations\":[{\"stroke_width\":[1]}]}",
        "{\"annotations\":[{\"stroke_width\":[1,\"wide\"]}]}",
        "{\"annotations\":[{\"stroke_width\":0}]}",
        "{\"annotations\":[{\"stroke_width\":-1}]}",
        "{\"annotations\":[{\"stroke_width\":1e999}]}"
    };
    for (size_t i=0; i<sizeof(bad_widths)/sizeof(bad_widths[0]); i++) {
        GError *width_err=NULL;
        assert(miime_schema_deserialize(bad_widths[i], &width_err) == NULL);
        assert(width_err != NULL);
        g_error_free(width_err);
        assert(miime_schema_validate(bad_widths[i], NULL) == FALSE);
    }
    MiimeAnnotationStack *bad_anchor=miime_schema_deserialize("{\"annotations\":[{\"label\":{\"text\":\"x\",\"anchor\":\"bogus\"}}]}",NULL);
    assert(bad_anchor && !miime_annot_stack_get(bad_anchor,0)->has_label_anchor);
    miime_annot_stack_free(bad_anchor);

    /* Numeric label ports and their legacy numeric-string form are both valid. */
    MiimeAnnotationStack *numeric_anchor_stack=miime_schema_deserialize(
        "{\"annotations\":[{\"label\":{\"text\":\"integer port\",\"anchor\":4}},{\"label\":{\"text\":\"string port\",\"anchor\":\"4\"}}]}",NULL);
    assert(numeric_anchor_stack && miime_annot_stack_count(numeric_anchor_stack)==2);
    assert(miime_annot_stack_get(numeric_anchor_stack,0)->has_label_anchor && miime_annot_stack_get(numeric_anchor_stack,0)->label_anchor==4);
    assert(miime_annot_stack_get(numeric_anchor_stack,1)->has_label_anchor && miime_annot_stack_get(numeric_anchor_stack,1)->label_anchor==4);
    char *numeric_anchor_json=miime_schema_serialize(numeric_anchor_stack);
    assert(numeric_anchor_json && strstr(numeric_anchor_json,"\"anchor\":\"4\"")!=NULL);
    MiimeAnnotationStack *numeric_anchor_roundtrip=miime_schema_deserialize(numeric_anchor_json,NULL);
    assert(numeric_anchor_roundtrip && miime_annot_stack_get(numeric_anchor_roundtrip,0)->label_anchor==4 && miime_annot_stack_get(numeric_anchor_roundtrip,1)->label_anchor==4);
    g_free(numeric_anchor_json);
    miime_annot_stack_free(numeric_anchor_stack);
    miime_annot_stack_free(numeric_anchor_roundtrip);

    MiimeAnnotationStack *ink_stack=miime_schema_deserialize("{\"annotations\":[{\"ink\":\"bold\"},{\"ink\":\"regular\",\"stroke_width\":2}]}",NULL);
    assert(ink_stack && miime_annot_stack_count(ink_stack)==2);
    const MiimeAnnotation *ink0=miime_annot_stack_get(ink_stack,0);
    const MiimeAnnotation *ink1=miime_annot_stack_get(ink_stack,1);
    assert(ink0->stroke_width==4 && !ink0->has_stroke_width);
    assert(ink1->stroke_width==2 && ink1->has_stroke_width);
    char *ink_json=miime_schema_serialize(ink_stack);
    MiimeAnnotationStack *ink_stack2=miime_schema_deserialize(ink_json,NULL);
    assert(ink_stack2 && miime_annot_stack_get(ink_stack2,0)->stroke_width==4);
    assert(miime_annot_stack_get(ink_stack2,0)->has_ink && miime_annot_stack_get(ink_stack2,0)->has_stroke_width);
    assert(miime_annot_stack_get(ink_stack2,1)->stroke_width==2 && miime_annot_stack_get(ink_stack2,1)->has_stroke_width);
    MiimeAnnotation invalid_enum=*ink0; invalid_enum.ink=99; invalid_enum.has_ink=TRUE;
    MiimeAnnotationStack *invalid_stack=miime_annot_stack_new(); miime_annot_stack_push(invalid_stack,invalid_enum);
    char *invalid_json=miime_schema_serialize(invalid_stack);
    assert(invalid_json && strstr(invalid_json,"\"ink\"")==NULL);
    g_free(invalid_json); miime_annot_stack_free(invalid_stack);
    g_free(ink_json); miime_annot_stack_free(ink_stack); miime_annot_stack_free(ink_stack2);

    const char *registry = "[{\"id\":40,\"tag\":\"x\",\"bounds\":[20,715,1875,147]}]";
    GError *target_err = NULL;
    MiimeAnnotationStack *target_stack = miime_schema_deserialize_ex(
        "{\"annotations\":[{\"type\":\"highlight_box\",\"som_id\":40}]}",
        &target_err, registry);
    assert(target_err == NULL && target_stack != NULL);
    const MiimeAnnotation *target = miime_annot_stack_get(target_stack, 0);
    assert(target->has_som_id && target->som_id == 40);
    assert(target->point_count == 8);
    assert(target->points[0] == 20 && target->points[1] == 715);
    assert(target->points[2] == 1895 && target->points[3] == 715);
    assert(target->points[4] == 1895 && target->points[5] == 862);
    assert(target->points[6] == 20 && target->points[7] == 862);
    miime_annot_stack_free(target_stack);

    target_err = NULL;
    target_stack = miime_schema_deserialize_ex(
        "{\"annotations\":[{\"type\":\"line\",\"som_id\":40,\"points\":[[0,0]]}]}",
        &target_err, registry);
    assert(target_err == NULL && target_stack != NULL);
    target = miime_annot_stack_get(target_stack, 0);
    assert(target->point_count == 4 && target->points[2] == 0 && target->points[3] == 0);
    miime_annot_stack_free(target_stack);

    target_stack = miime_schema_deserialize_ex(
        "{\"annotations\":[{\"type\":\"pin\",\"som_id\":40}]}",
        NULL, registry);
    assert(target_stack != NULL);
    target = miime_annot_stack_get(target_stack, 0);
    assert(target->point_count == 2 && target->points[0] == 957.5 && target->points[1] == 788.5);
    miime_annot_stack_free(target_stack);

    const char *bad_registries[] = {"[", "[{\"id\":40,\"bounds\":[20,715,1875]}]", "[{\"id\":40,\"bounds\":null}]"};
    for (size_t i = 0; i < sizeof(bad_registries)/sizeof(bad_registries[0]); i++) {
        target_err = NULL;
        assert(miime_schema_deserialize_ex(
            "{\"annotations\":[{\"type\":\"highlight_box\",\"som_id\":40}]}",
            &target_err, bad_registries[i]) == NULL);
        assert(target_err != NULL);
        g_error_free(target_err);
    }
    target_err = NULL;
    assert(miime_schema_deserialize_ex(
        "{\"annotations\":[{\"type\":\"highlight_box\",\"som_id\":41}]}",
        &target_err, registry) == NULL);
    assert(target_err != NULL);
    g_error_free(target_err);
    target_err = NULL;
    assert(miime_schema_deserialize_ex(
        "{\"annotations\":[{\"type\":\"highlight_box\",\"som_id\":40}]}",
        &target_err, NULL) == NULL);
    assert(target_err != NULL);
    g_error_free(target_err);

    const char *legacy = "{\"annotations\":[{\"id\":\"l\",\"type\":\"pen\",\"source\":\"human\",\"points\":[1,2],\"color\":[1,0,0,1],\"stroke_width\":2,\"text\":\"\",\"font_size\":0}]}";
    const char *legacy_expected = "{\"annotations\":[{\"id\":\"l\",\"type\":\"pen\",\"source\":\"human\",\"points\":[1,2],\"color\":[1,0,0,1],\"stroke_width\":2,\"text\":\"\",\"font_size\":0}]}";
    GError *legacy_err = NULL;
    MiimeAnnotationStack *legacy_stack = miime_schema_deserialize(legacy, &legacy_err);
    assert(legacy_err == NULL && legacy_stack != NULL);
    char *legacy_out = miime_schema_serialize(legacy_stack);
    assert(strcmp(legacy_out, legacy_expected) == 0);
    MiimeAnnotationStack *legacy_stack2 = miime_schema_deserialize(legacy_out, NULL);
    char *legacy_out2 = miime_schema_serialize(legacy_stack2);
    assert(strcmp(legacy_out2, legacy_out) == 0);
    g_free(legacy_out); g_free(legacy_out2);
    miime_annot_stack_free(legacy_stack); miime_annot_stack_free(legacy_stack2);

    const char *extended = "{\"annotations\":[{\"id\":\"x\",\"type\":\"highlight_box\",\"points\":[1,2],\"color\":[1,0,0,1],\"stroke_width\":[4,2],\"ink\":\"bold\",\"dash\":\"dashed\",\"fill\":\"hachure\",\"fill_color\":[1,0.5,0,0.2],\"radius\":12,\"arrow\":\"end\",\"z\":\"under\",\"som_id\":40,\"label\":{\"text\":\"hi\",\"anchor\":\"auto\",\"leader\":\"elbow\"}}]}";
    MiimeAnnotationStack *ext_stack = miime_schema_deserialize_ex(extended, NULL, registry);
    assert(ext_stack != NULL);
    const MiimeAnnotation *ext = miime_annot_stack_get(ext_stack, 0);
    assert(ext->ink == 2 && ext->dash == 1 && ext->fill == 1);
    assert(ext->stroke_width == 4 && ext->stroke_width_end == 2);
    assert(ext->fill_r == 1 && ext->fill_g == 0.5 && ext->fill_b == 0 && ext->fill_a == 0.2);
    assert(ext->radius == 12 && ext->arrow == 2 && ext->z == 0 && ext->som_id == 40);
    assert(strcmp(ext->label_text, "hi") == 0 && ext->label_anchor == -1 && ext->label_leader == 1);
    char *ext_out = miime_schema_serialize(ext_stack);
    MiimeAnnotationStack *ext_stack2 = miime_schema_deserialize_ex(ext_out, NULL, registry);
    const MiimeAnnotation *ext2 = miime_annot_stack_get(ext_stack2, 0);
    assert(ext2->ink == 2 && ext2->dash == 1 && ext2->fill == 1 && ext2->stroke_width_end == 2);
    assert(ext2->fill_a == 0.2 && ext2->radius == 12 && ext2->arrow == 2 && ext2->z == 0 && ext2->som_id == 40);
    assert(strcmp(ext2->label_text, "hi") == 0 && ext2->label_anchor == -1 && ext2->label_leader == 1);
    g_free(ext_out); miime_annot_stack_free(ext_stack); miime_annot_stack_free(ext_stack2);

    MiimeAnnotationStack *explicit_defaults=miime_schema_deserialize(
        "{\"annotations\":[{\"type\":\"callout\",\"points\":[10,10,90,70],\"fill\":\"none\",\"radius\":0},{\"type\":\"arrow\",\"points\":[10,10,90,70],\"arrow\":\"none\"},{\"type\":\"alignment_redline\",\"points\":[10,10,90,70],\"dash\":\"solid\"}]}",NULL);
    assert(explicit_defaults && miime_annot_stack_count(explicit_defaults)==3);
    char *explicit_json=miime_schema_serialize(explicit_defaults);
    assert(explicit_json && strstr(explicit_json,"\"fill\":\"none\"") && strstr(explicit_json,"\"radius\":0"));
    assert(strstr(explicit_json,"\"arrow\":\"none\"") && strstr(explicit_json,"\"dash\":\"solid\""));
    MiimeAnnotationStack *explicit_roundtrip=miime_schema_deserialize(explicit_json,NULL);
    assert(explicit_roundtrip && miime_annot_stack_count(explicit_roundtrip)==3);
    const MiimeAnnotation *round_callout=miime_annot_stack_get(explicit_roundtrip,0);
    const MiimeAnnotation *round_arrow=miime_annot_stack_get(explicit_roundtrip,1);
    const MiimeAnnotation *round_redline=miime_annot_stack_get(explicit_roundtrip,2);
    assert(round_callout->has_fill && round_callout->fill==0 && round_callout->has_radius && round_callout->radius==0);
    assert(round_arrow->has_arrow && round_arrow->arrow==0);
    assert(round_redline->has_dash && round_redline->dash==0);
    g_free(explicit_json); miime_annot_stack_free(explicit_defaults); miime_annot_stack_free(explicit_roundtrip);

    MiimeAnnotationStack *unicode_stack=miime_schema_deserialize(
        "{\"annotations\":[{\"label\":{\"text\":\"A\\u20ac\\uD83D\\uDE00\"}}]}",NULL);
    assert(unicode_stack);
    const MiimeAnnotation *unicode=miime_annot_stack_get(unicode_stack,0);
    assert(strcmp(unicode->label_text,"A\xe2\x82\xac\xf0\x9f\x98\x80")==0);
    miime_annot_stack_free(unicode_stack);
    GString *long_unicode_json=g_string_new("{\"annotations\":[{\"label\":{\"text\":\"");
    for (int i=0;i<86;i++) g_string_append(long_unicode_json,"\\u20ac");
    g_string_append(long_unicode_json,"\"}}]}");
    unicode_stack=miime_schema_deserialize(long_unicode_json->str,NULL);
    assert(unicode_stack);
    unicode=miime_annot_stack_get(unicode_stack,0);
    assert(strlen(unicode->label_text)==255);
    assert(g_utf8_validate(unicode->label_text,-1,NULL));
    miime_annot_stack_free(unicode_stack); g_string_free(long_unicode_json,TRUE);

    MiimeAnnotationStack *s = miime_annot_stack_new();
    MiimeAnnotation a;
    memset(&a, 0, sizeof(a));
    a.type = MIIME_ANNOT_PEN;
    strcpy(a.json_id, "abc");
    a.points[0]=1; a.points[1]=2; a.points[2]=3; a.points[3]=4;
    a.point_count=4;
    a.color_r=1; a.color_g=0; a.color_b=0; a.color_a=1;
    a.stroke_width=2;
    miime_annot_stack_push(s, a);

    char *json = miime_schema_serialize(s);
    printf("%s\n", json);
    assert(json != NULL);

    GError *err = NULL;
    MiimeAnnotationStack *s2 = miime_schema_deserialize(json, &err);
    assert(err == NULL);
    assert(s2 != NULL);
    assert(miime_annot_stack_count(s2) == 1);
    const MiimeAnnotation *g = miime_annot_stack_get(s2, 0);
    assert(strcmp(g->json_id, "abc") == 0);
    assert(g->points[0] == 1);

    assert(miime_schema_validate(json, NULL) == TRUE);

    g_free(json);
    miime_annot_stack_free(s);
    miime_annot_stack_free(s2);
    printf("All schema tests passed.\n");
    return 0;
}
