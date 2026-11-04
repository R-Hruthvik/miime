#include "style.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static cairo_surface_t *rgb_surface(int w,int h,double r,double g,double b) {
    cairo_surface_t *s=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,w,h);
    cairo_t *cr=cairo_create(s);cairo_set_source_rgb(cr,r,g,b);cairo_paint(cr);cairo_destroy(cr);return s;
}
static double srgb_for_lum(double l) { return l<=.0031308?12.92*l:1.055*pow(l,1.0/2.4)-.055; }
static MiimeStyleResolution *resolve(cairo_surface_t *s,MiimeStyleConfig *c) { return miime_style_resolve(s,c,NULL,(MiimeCrop){0}); }
static void test_auto_theme_thresholds_and_ties(void) {
    MiimeStyleConfig c={0};c.theme=MIIME_THEME_AUTO;c.has_theme=TRUE;
    cairo_surface_t *s=rgb_surface(9,9,srgb_for_lum(.34),srgb_for_lum(.34),srgb_for_lum(.34));MiimeStyleResolution*r=resolve(s,&c);assert(r&&r->resolved_theme==MIIME_THEME_DARK);miime_style_resolution_free(r);cairo_surface_destroy(s);
    s=rgb_surface(9,9,srgb_for_lum(.35),srgb_for_lum(.35),srgb_for_lum(.35));r=resolve(s,&c);assert(r&&r->resolved_theme==MIIME_THEME_NEUTRAL);miime_style_resolution_free(r);cairo_surface_destroy(s);
    s=rgb_surface(9,9,srgb_for_lum(.645),srgb_for_lum(.645),srgb_for_lum(.645));r=resolve(s,&c);assert(r&&r->resolved_theme==MIIME_THEME_NEUTRAL);miime_style_resolution_free(r);cairo_surface_destroy(s);
    s=rgb_surface(9,9,srgb_for_lum(.651),srgb_for_lum(.651),srgb_for_lum(.651));r=resolve(s,&c);assert(r&&r->resolved_theme==MIIME_THEME_LIGHT);miime_style_resolution_free(r);cairo_surface_destroy(s);
    /* Hues in coarse red/orange/yellow bins dominate the middle-luminance fixture. */
    s=rgb_surface(4,4,1,.7,0);r=resolve(s,&c);assert(r&&r->resolved_theme==MIIME_THEME_WARM);miime_style_resolution_free(r);cairo_surface_destroy(s);
    s=rgb_surface(4,4,0,.7,.7);r=resolve(s,&c);assert(r&&r->resolved_theme==MIIME_THEME_NEUTRAL);miime_style_resolution_free(r);cairo_surface_destroy(s);
}
static void test_auto_style_edge_threshold(void) {
    MiimeStyleConfig c={0};c.theme=MIIME_THEME_AUTO;c.has_theme=TRUE;
    cairo_surface_t *s=rgb_surface(8,8,.5,.5,.5);MiimeStyleResolution*r=resolve(s,&c);assert(r&&r->resolved_drawing_style==MIIME_DRAW_SKETCH);miime_style_resolution_free(r);cairo_surface_destroy(s);
    s=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,64,64);cairo_t*cr=cairo_create(s);for(int y=0;y<64;y++)for(int x=0;x<64;x++){cairo_set_source_rgb(cr,((x+y)&1)?1:0,((x+y)&1)?1:0,((x+y)&1)?1:0);cairo_rectangle(cr,x,y,1,1);cairo_fill(cr);}cairo_destroy(cr);
    r=resolve(s,&c);assert(r&&r->resolved_drawing_style==MIIME_DRAW_TECHNICAL);miime_style_resolution_free(r);cairo_surface_destroy(s);
}
static void test_sampler_caps_and_even_spacing(void) {
    cairo_surface_t *s=rgb_surface(1000,900,.5,.5,.5);MiimeStyleConfig c={0};c.theme=MIIME_THEME_AUTO;c.has_theme=TRUE;
    MiimeStyleResolution*a=resolve(s,&c),*b=resolve(s,&c);assert(a&&a->image_sample_count<=4096&&a->image_sample_count==4096);assert(b&&a->image_sample_count==b->image_sample_count&&a->resolved_theme==b->resolved_theme);miime_style_resolution_free(a);miime_style_resolution_free(b);cairo_surface_destroy(s);
    s=rgb_surface(100,100,.5,.5,.5);MiimeAnnotationStack*stack=miime_annot_stack_new();MiimeAnnotation an={0};strcpy(an.json_id,"path");an.type=MIIME_ANNOT_PEN;an.point_count=4;an.points[0]=0;an.points[1]=0;an.points[2]=99;an.points[3]=99;miime_annot_stack_push(stack,an);
    a=miime_style_resolve(s,&c,stack,(MiimeCrop){0});assert(a&&a->annotation_count==1&&a->annotations[0].local_sample_count>1&&a->annotations[0].local_sample_count<=64);miime_style_resolution_free(a);miime_annot_stack_free(stack);cairo_surface_destroy(s);
}
static void test_style_alpha_composite_uses_white_analysis_bg(void) {
    cairo_surface_t*s=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,1,1);cairo_t*cr=cairo_create(s);cairo_set_source_rgba(cr,0,0,0,0);cairo_paint(cr);cairo_destroy(cr);MiimeStyleConfig c={0};c.theme=MIIME_THEME_AUTO;c.has_theme=TRUE;MiimeStyleResolution*r=resolve(s,&c);assert(r&&r->resolved_theme==MIIME_THEME_LIGHT);miime_style_resolution_free(r);cairo_surface_destroy(s);
}
static void test_terminal_and_photo_fixtures(void) {
    MiimeStyleConfig c={0};c.has_theme=TRUE;c.theme=MIIME_THEME_AUTO;
    cairo_surface_t*s=rgb_surface(48,48,23.0/255,25.0/255,28.0/255);MiimeStyleResolution*r=resolve(s,&c);assert(r&&r->resolved_theme==MIIME_THEME_DARK);miime_style_resolution_free(r);cairo_surface_destroy(s);
    s=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,96,64);cairo_t*cr=cairo_create(s);
    for(int y=0;y<64;y++) { for(int x=0;x<96;x++){double t=(double)x/95.0;double shade=.18+.58*t; cairo_set_source_rgb(cr,shade*.96,shade*.91,shade*.82);cairo_rectangle(cr,x,y,1,1);cairo_fill(cr);} }
    cairo_destroy(cr);
    r=resolve(s,&c);MiimeStyleResolution*r2=resolve(s,&c);assert(r&&r2&&r->resolved_theme==r2->resolved_theme&&r->image_sample_count==r2->image_sample_count);assert(r->resolved_theme>=MIIME_THEME_LIGHT&&r->resolved_theme<=MIIME_THEME_NEUTRAL);miime_style_resolution_free(r);miime_style_resolution_free(r2);cairo_surface_destroy(s);
}
static void test_label_contrast_uses_surface_pair(void) {
    cairo_surface_t*s=rgb_surface(32,32,1,1,1);MiimeStyleConfig c={0};c.has_theme=TRUE;c.theme=MIIME_THEME_LIGHT;MiimeAnnotationStack*stack=miime_annot_stack_new();MiimeAnnotation a={0};a.type=MIIME_ANNOT_TEXT;a.point_count=2;a.points[0]=2;a.points[1]=2;strcpy(a.text,"label");miime_annot_stack_push(stack,a);
    MiimeStyleResolution*r=miime_style_resolve(s,&c,stack,(MiimeCrop){0});assert(r&&r->annotations[0].local_sample_count>0&&!r->annotations[0].contrast_unresolved);assert(!r->annotations[0].has_label_surface);miime_style_resolution_free(r);miime_annot_stack_free(stack);cairo_surface_destroy(s);
}
static void test_alpha_composite_and_text_surface_alpha(void) {
    cairo_surface_t*s=rgb_surface(8,8,1,1,1);MiimeStyleConfig c={0};c.theme=MIIME_THEME_LIGHT;c.has_theme=TRUE;MiimeAnnotationStack*stack=miime_annot_stack_new();
    MiimeAnnotation a={0};a.type=MIIME_ANNOT_LINE;a.point_count=4;a.points[0]=0;a.points[1]=4;a.points[2]=7;a.points[3]=4;a.has_color=TRUE;a.color_r=a.color_g=a.color_b=0;a.color_a=.2;miime_annot_stack_push(stack,a);
    MiimeStyleResolution*r=miime_style_resolve(s,&c,stack,(MiimeCrop){0});assert(r&&r->annotations[0].contrast_unresolved&&r->annotations[0].contrast_hits==0);miime_style_resolution_free(r);miime_annot_stack_free(stack);
    stack=miime_annot_stack_new();a=(MiimeAnnotation){0};a.type=MIIME_ANNOT_TEXT;strcpy(a.text,"label");a.has_color=TRUE;a.color_r=a.color_g=a.color_b=0;a.color_a=1;a.has_label_surface=TRUE;a.label_surface_rgba[0]=a.label_surface_rgba[1]=a.label_surface_rgba[2]=0;a.label_surface_rgba[3]=0;miime_annot_stack_push(stack,a);
    r=miime_style_resolve(s,&c,stack,(MiimeCrop){0});assert(r&&r->annotations[0].contrast_hits==r->annotations[0].local_sample_count&&!r->annotations[0].contrast_unresolved);miime_style_resolution_free(r);miime_annot_stack_free(stack);cairo_surface_destroy(s);
}
static void test_per_sample_coverage_and_path_dedup(void) {
    cairo_surface_t*s=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,8,4);cairo_t*cr=cairo_create(s);
    for(int x=0;x<8;x++){cairo_set_source_rgb(cr,x<4?1:0,x<4?1:0,x<4?1:0);cairo_rectangle(cr,x,0,1,4);cairo_fill(cr);}cairo_destroy(cr);
    MiimeStyleConfig c={0};c.theme=MIIME_THEME_LIGHT;c.has_theme=TRUE;MiimeAnnotationStack*stack=miime_annot_stack_new();MiimeAnnotation a={0};a.type=MIIME_ANNOT_LINE;a.point_count=4;a.points[0]=0;a.points[1]=2;a.points[2]=7;a.points[3]=2;a.has_color=TRUE;a.color_r=a.color_g=a.color_b=0;a.color_a=1;miime_annot_stack_push(stack,a);
    MiimeStyleResolution*r=miime_style_resolve(s,&c,stack,(MiimeCrop){0});assert(r&&r->annotations[0].contrast_hits==r->annotations[0].local_sample_count&&r->annotations[0].halo_applied&&!r->annotations[0].contrast_unresolved);miime_style_resolution_free(r);miime_annot_stack_free(stack);
    stack=miime_annot_stack_new();a=(MiimeAnnotation){0};a.type=MIIME_ANNOT_LINE;a.point_count=6;a.points[0]=1;a.points[1]=1;a.points[2]=0;a.points[3]=1;a.points[4]=1;a.points[5]=1;miime_annot_stack_push(stack,a);
    r=miime_style_resolve(s,&c,stack,(MiimeCrop){0});assert(r&&r->annotations[0].local_sample_count==1);miime_style_resolution_free(r);miime_annot_stack_free(stack);cairo_surface_destroy(s);
}
static void test_shape_halo_rescues_only_when_edge_contrast_passes(void) {
    cairo_surface_t*s=rgb_surface(16,8,.3,.3,.3);MiimeStyleConfig c={0};c.theme=MIIME_THEME_LIGHT;c.has_theme=TRUE;
    MiimeAnnotationStack*stack=miime_annot_stack_new();MiimeAnnotation a={0};
    a.type=MIIME_ANNOT_LINE;a.point_count=4;a.points[0]=1;a.points[1]=2;a.points[2]=14;a.points[3]=2;
    a.has_color=TRUE;a.color_r=a.color_g=a.color_b=0;a.color_a=1;miime_annot_stack_push(stack,a);
    a.points[1]=5;a.points[3]=5;a.color_a=.15;miime_annot_stack_push(stack,a);
    MiimeStyleResolution*r=miime_style_resolve(s,&c,stack,(MiimeCrop){0});assert(r&&r->annotation_count==2);
    MiimeAnnotationStyleResult*rescued=&r->annotations[0],*unresolved=&r->annotations[1];
    assert(rescued->local_sample_count>0&&rescued->contrast_hits==rescued->local_sample_count);
    assert(rescued->halo_applied&&!rescued->contrast_unresolved);
    assert(unresolved->local_sample_count>0&&unresolved->contrast_hits==0);
    assert(unresolved->halo_applied&&unresolved->contrast_unresolved&&unresolved->override_warning);
    assert(strstr(r->warning_codes,"contrast_unresolved")&&strstr(r->warning_codes,"override_warning"));
    assert(r->contrast_hits==rescued->local_sample_count&&r->contrast_total==rescued->local_sample_count+unresolved->local_sample_count);
    miime_style_resolution_free(r);miime_annot_stack_free(stack);
    stack=miime_annot_stack_new();a=(MiimeAnnotation){0};a.type=MIIME_ANNOT_TEXT;a.point_count=2;a.points[0]=2;a.points[1]=2;strcpy(a.text,"label");a.has_color=TRUE;a.color_r=a.color_g=a.color_b=0;a.color_a=.15;miime_annot_stack_push(stack,a);
    r=miime_style_resolve(s,&c,stack,(MiimeCrop){0});assert(r&&r->annotations[0].contrast_unresolved&&!r->annotations[0].halo_applied);
    miime_style_resolution_free(r);miime_annot_stack_free(stack);cairo_surface_destroy(s);
}
static void test_explicit_zero_font_size_uses_legacy_fallback(void) {
    cairo_surface_t*s=rgb_surface(8,8,1,1,1);MiimeAnnotationStack*stack=miime_annot_stack_new();MiimeAnnotation a={0};a.has_font_size=TRUE;a.font_size=0;miime_annot_stack_push(stack,a);MiimeStyleConfig c={0};c.theme=MIIME_THEME_LIGHT;c.has_theme=TRUE;
    MiimeStyleResolution*r=miime_style_resolve(s,&c,stack,(MiimeCrop){0});assert(r&&r->annotations[0].font_size==24);miime_style_resolution_free(r);miime_annot_stack_free(stack);cairo_surface_destroy(s);
}
static void test_exact_palettes_and_style_tokens(void) {
    const MiimeStylePalette*p=miime_style_palette(MIIME_THEME_LIGHT);assert(p&&fabs(p->ink[0]-32.0/255)<1e-9&&p->label_surface[0]==1&&fabs(p->secondary_surface[0]-241.0/255)<1e-9);
    p=miime_style_palette(MIIME_THEME_DARK);assert(p&&fabs(p->error[0]-1)<1e-9&&fabs(p->warning[1]-200.0/255)<1e-9);
    p=miime_style_palette(MIIME_THEME_WARM);assert(p&&fabs(p->label_surface[0]-1)<1e-9&&fabs(p->info[2]-143.0/255)<1e-9);
    p=miime_style_palette(MIIME_THEME_NEUTRAL);assert(p&&fabs(p->ink[0]-34.0/255)<1e-9&&fabs(p->secondary_surface[0]-232.0/255)<1e-9);
    assert(!miime_style_palette(MIIME_THEME_AUTO));assert(!strcmp(miime_style_token(MIIME_DRAW_EDITORIAL),"editorial"));
}
static void test_annotations_role_override_and_path_coverage(void) {
    cairo_surface_t*s=rgb_surface(80,50,.5,.5,.5);MiimeStyleConfig c={0};c.theme=MIIME_THEME_LIGHT;c.has_theme=TRUE;MiimeAnnotationStack*stack=miime_annot_stack_new();
    MiimeAnnotation a={0};strcpy(a.json_id,"warning");a.type=MIIME_ANNOT_LINE;a.role=MIIME_ROLE_WARNING;a.has_role=TRUE;a.point_count=4;a.points[0]=5;a.points[1]=25;a.points[2]=75;a.points[3]=25;miime_annot_stack_push(stack,a);
    a=(MiimeAnnotation){0};strcpy(a.json_id,"override");a.type=MIIME_ANNOT_RECT;a.point_count=4;a.points[0]=10;a.points[1]=10;a.points[2]=30;a.points[3]=30;a.has_color=TRUE;a.color_r=1;a.color_g=0;a.color_b=1;a.color_a=.01;a.has_label_surface=TRUE;a.label_surface_rgba[0]=1;a.label_surface_rgba[1]=1;a.label_surface_rgba[2]=1;a.label_surface_rgba[3]=1;miime_annot_stack_push(stack,a);
    MiimeStyleResolution*r=miime_style_resolve(s,&c,stack,(MiimeCrop){0});assert(r&&r->annotation_count==2);assert(r->annotations[0].ink_rgba[0]==miime_style_palette(MIIME_THEME_LIGHT)->warning[0]);assert(r->annotations[0].local_sample_count>1&&r->annotations[0].local_sample_count<=64);assert(r->annotations[1].ink_rgba[0]==1&&r->annotations[1].ink_rgba[2]==1&&r->annotations[1].override_warning);assert(strstr(r->warning_codes,"override_warning"));assert(r->contrast_total==r->contrast_hits+r->annotations[1].local_sample_count||r->contrast_total>=r->contrast_hits);miime_style_resolution_free(r);miime_annot_stack_free(stack);cairo_surface_destroy(s);
}
static void test_roles_require_active_resolver_and_alias_conflict(void) {
    cairo_surface_t*s=rgb_surface(4,4,.5,.5,.5);MiimeAnnotationStack*stack=miime_annot_stack_new();MiimeAnnotation a={0};a.role=MIIME_ROLE_ERROR;a.has_role=TRUE;miime_annot_stack_push(stack,a);MiimeStyleConfig c={0};MiimeStyleResolution*r=miime_style_resolve(s,&c,stack,(MiimeCrop){0});assert(r&&fabs(r->annotations[0].ink_rgba[0]-34.0/255)<1e-9);miime_style_resolution_free(r);
    c.has_theme=TRUE;c.theme=MIIME_THEME_LIGHT;c.has_drawing_style=TRUE;c.drawing_style=MIIME_DRAW_MINIMAL;c.has_aesthetic_style=TRUE;c.aesthetic_style=MIIME_AESTHETIC_CLEAN_TECHNICAL;r=miime_style_resolve(s,&c,stack,(MiimeCrop){0});assert(r&&strstr(r->warning_codes,"style_alias_conflict")&&r->resolved_drawing_style==MIIME_DRAW_MINIMAL);miime_style_resolution_free(r);miime_annot_stack_free(stack);cairo_surface_destroy(s);
}
static void test_aesthetic_alias_overrides_auto_theme_style(void) {
    cairo_surface_t*s=rgb_surface(8,8,.5,.5,.5);MiimeStyleConfig c={0};c.theme=MIIME_THEME_AUTO;c.has_theme=TRUE;c.has_aesthetic_style=TRUE;c.aesthetic_style=MIIME_AESTHETIC_CLEAN_TECHNICAL;
    MiimeStyleResolution*r=resolve(s,&c);assert(r&&r->resolved_drawing_style==MIIME_DRAW_TECHNICAL);miime_style_resolution_free(r);cairo_surface_destroy(s);
}
static void test_crop_and_empty_crop(void) {
    cairo_surface_t*s=rgb_surface(40,30,1,1,1);MiimeStyleConfig c={0};c.theme=MIIME_THEME_AUTO;c.has_theme=TRUE;MiimeStyleResolution*r=miime_style_resolve(s,&c,NULL,(MiimeCrop){10,8,4,3,TRUE});assert(r&&r->image_sample_count==12&&r->resolved_theme==MIIME_THEME_LIGHT);miime_style_resolution_free(r);r=miime_style_resolve(s,&c,NULL,(MiimeCrop){60,8,4,3,TRUE});assert(r&&!strcmp(r->policy,"empty_crop")&&r->image_sample_count==0);miime_style_resolution_free(r);cairo_surface_destroy(s);
}
static void test_diagnostics_serialize_requested_and_resolved_fields(void) {
    cairo_surface_t*s=rgb_surface(12,10,.25,.25,.25);MiimeAnnotationStack*stack=miime_annot_stack_new();
    MiimeAnnotation a={0};strcpy(a.json_id,"quote\"slash\\id");a.type=MIIME_ANNOT_LINE;a.point_count=4;a.points[0]=1;a.points[1]=4;a.points[2]=10;a.points[3]=4;miime_annot_stack_push(stack,a);
    MiimeStyleConfig c={0};c.has_theme=TRUE;c.theme=MIIME_THEME_AUTO;c.has_analysis_background=TRUE;c.analysis_background_rgba=0xDDEEFFff;
    MiimeStyleResolution*r=miime_style_resolve(s,&c,stack,(MiimeCrop){2,1,8,7,TRUE});assert(r);
    char*json=miime_style_resolution_to_json(r,(MiimeCrop){2,1,8,7,TRUE},300);assert(json);
    assert(strstr(json,"\"requested\":{\"drawing_style\":null,\"theme\":\"auto\",\"analysis_background\":\"#DDEEFF\""));
    assert(strstr(json,"\"resolved\":{\"drawing_style\":\"sketch\",\"theme\":\"dark\"}"));
    assert(strstr(json,"\"analysis_background\":\"#DDEEFF\""));
    assert(strstr(json,"\"count\":56,\"max_count\":4096"));
    assert(strstr(json,"\"text_target\":4.5,\"shape_target\":3.0"));
    assert(strstr(json,"quote\\\"slash\\\\id"));
    assert(strstr(json,"\"halo_applied\":"));assert(strstr(json,"\"shape_sample_count\":"));assert(strstr(json,"\"label_contrast_hits\":"));assert(strstr(json,"\"warnings\":["));
    g_free(json);miime_style_resolution_free(r);miime_annot_stack_free(stack);cairo_surface_destroy(s);
}
static void test_zero_sample_coverage_is_no_data(void) {
    cairo_surface_t*s=rgb_surface(20,20,.5,.5,.5);MiimeStyleConfig c={0};c.has_theme=TRUE;c.theme=MIIME_THEME_LIGHT;
    MiimeAnnotationStack*stack=miime_annot_stack_new();MiimeAnnotation a={0};strcpy(a.json_id,"outside");a.type=MIIME_ANNOT_LINE;a.point_count=4;a.points[0]=12;a.points[1]=12;a.points[2]=18;a.points[3]=18;miime_annot_stack_push(stack,a);
    MiimeCrop crop={0,0,5,5,TRUE};MiimeStyleResolution*r=miime_style_resolve(s,&c,stack,crop);assert(r&&r->contrast_total==0&&r->annotations[0].local_sample_count==0);
    char*json=miime_style_resolution_to_json(r,crop,0);assert(json);assert(strstr(json,"\"hits\":0,\"total\":0,\"coverage\":null"));assert(strstr(json,"\"local_sample_count\":0,\"contrast_hits\":0,\"coverage\":null"));g_free(json);miime_style_resolution_free(r);miime_annot_stack_free(stack);
    r=miime_style_resolve(s,&c,NULL,(MiimeCrop){30,30,5,5,TRUE});assert(r&&r->contrast_total==0&&r->image_sample_count==0&&!strcmp(r->policy,"empty_crop"));json=miime_style_resolution_to_json(r,(MiimeCrop){30,30,5,5,TRUE},0);assert(json&&strstr(json,"\"hits\":0,\"total\":0,\"coverage\":null"));g_free(json);miime_style_resolution_free(r);cairo_surface_destroy(s);
}
static void test_shape_samples_follow_geometry_not_all_segments(void) {
    cairo_surface_t*s=rgb_surface(64,64,1,1,1);cairo_t*cr=cairo_create(s);cairo_set_source_rgb(cr,0,0,0);cairo_rectangle(cr,28,28,8,8);cairo_fill(cr);cairo_destroy(cr);MiimeStyleConfig c={0};c.theme=MIIME_THEME_LIGHT;c.has_theme=TRUE;
    MiimeAnnotationStack*stack=miime_annot_stack_new();MiimeAnnotation a={0};
    a.type=MIIME_ANNOT_RECT;a.point_count=4;a.points[0]=8;a.points[1]=8;a.points[2]=56;a.points[3]=56;miime_annot_stack_push(stack,a);
    a=(MiimeAnnotation){0};a.type=MIIME_ANNOT_ELLIPSE;a.point_count=4;a.points[0]=8;a.points[1]=8;a.points[2]=56;a.points[3]=56;miime_annot_stack_push(stack,a);
    MiimeStyleResolution*r=miime_style_resolve(s,&c,stack,(MiimeCrop){0});assert(r&&r->annotation_count==2);
    for(guint i=0;i<2;i++){assert(r->annotations[i].local_sample_count>8);assert(r->annotations[i].contrast_hits==r->annotations[i].local_sample_count&&!r->annotations[i].contrast_unresolved&&!r->annotations[i].halo_applied);}
    miime_style_resolution_free(r);miime_annot_stack_free(stack);cairo_surface_destroy(s);
}
static void test_text_samples_use_text_bounds_and_source_surface(void) {
    cairo_surface_t*s=rgb_surface(64,64,0,0,0);MiimeStyleConfig c={0};c.theme=MIIME_THEME_LIGHT;c.has_theme=TRUE;MiimeAnnotationStack*stack=miime_annot_stack_new();MiimeAnnotation a={0};
    a.type=MIIME_ANNOT_TEXT;a.point_count=2;a.points[0]=10;a.points[1]=12;a.has_font_size=TRUE;a.font_size=8;strcpy(a.text,"i");miime_annot_stack_push(stack,a);
    MiimeStyleResolution*r=miime_style_resolve(s,&c,stack,(MiimeCrop){0});assert(r&&r->annotations[0].local_sample_count>0&&r->annotations[0].local_sample_count<64&&r->annotations[0].contrast_unresolved);
    miime_style_resolution_free(r);miime_annot_stack_free(stack);cairo_surface_destroy(s);
}
static void test_marker_alpha_matches_rendered_opacity_before_halo(void) {
    cairo_surface_t*s=rgb_surface(32,32,0,0,0);MiimeStyleConfig c={0};c.theme=MIIME_THEME_LIGHT;c.has_theme=TRUE;MiimeAnnotationStack*stack=miime_annot_stack_new();MiimeAnnotation a={0};
    a.type=MIIME_ANNOT_LINE;a.point_count=4;a.points[0]=2;a.points[1]=16;a.points[2]=30;a.points[3]=16;a.has_ink=TRUE;a.ink=MIIME_INK_MARKER;a.has_color=TRUE;a.color_r=a.color_g=a.color_b=1;a.color_a=.8;miime_annot_stack_push(stack,a);
    MiimeStyleResolution*r=miime_style_resolve(s,&c,stack,(MiimeCrop){0});assert(r&&r->annotations[0].halo_applied&&r->annotations[0].contrast_unresolved);
    miime_style_resolution_free(r);miime_annot_stack_free(stack);cairo_surface_destroy(s);
}
static void test_crop_annotation_is_excluded_from_contrast(void) {
    cairo_surface_t*s=rgb_surface(20,20,.5,.5,.5);MiimeStyleConfig c={0};c.theme=MIIME_THEME_LIGHT;c.has_theme=TRUE;MiimeAnnotationStack*stack=miime_annot_stack_new();MiimeAnnotation a={0};
    a.type=MIIME_ANNOT_CROP;a.point_count=4;a.points[0]=2;a.points[1]=2;a.points[2]=18;a.points[3]=18;miime_annot_stack_push(stack,a);
    MiimeStyleResolution*r=miime_style_resolve(s,&c,stack,(MiimeCrop){0});assert(r&&r->contrast_total==0&&r->annotations[0].local_sample_count==0);
    miime_style_resolution_free(r);miime_annot_stack_free(stack);cairo_surface_destroy(s);
}
static void test_labeled_shape_tracks_shape_and_label_contrast_independently(void) {
    cairo_surface_t*s=rgb_surface(64,64,1,1,1);MiimeStyleConfig c={0};c.theme=MIIME_THEME_LIGHT;c.has_theme=TRUE;MiimeAnnotationStack*stack=miime_annot_stack_new();MiimeAnnotation a={0};
    a.type=MIIME_ANNOT_RECT;a.point_count=4;a.points[0]=8;a.points[1]=8;a.points[2]=56;a.points[3]=56;strcpy(a.label_text,"label");
    a.has_color=TRUE;a.color_r=a.color_g=a.color_b=1;a.color_a=1;a.has_label_surface=TRUE;a.label_surface_rgba[0]=a.label_surface_rgba[1]=a.label_surface_rgba[2]=0;a.label_surface_rgba[3]=1;
    miime_annot_stack_push(stack,a);MiimeStyleResolution*r=miime_style_resolve(s,&c,stack,(MiimeCrop){0});assert(r&&r->annotation_count==1);
    MiimeAnnotationStyleResult*z=&r->annotations[0];assert(z->shape_sample_count>0&&z->shape_contrast_hits==0&&z->shape_halo_hits==z->shape_sample_count);assert(z->label_sample_count==1&&z->label_contrast_hits==1);assert(z->contrast_hits==z->shape_halo_hits+z->label_contrast_hits&&!z->contrast_unresolved);
    miime_style_resolution_free(r);a.label_surface_rgba[0]=a.label_surface_rgba[1]=a.label_surface_rgba[2]=1;miime_annot_stack_clear(stack);miime_annot_stack_push(stack,a);
    r=miime_style_resolve(s,&c,stack,(MiimeCrop){0});assert(r&&r->annotations[0].shape_halo_hits==r->annotations[0].shape_sample_count&&r->annotations[0].label_contrast_hits==0&&r->annotations[0].contrast_unresolved);
    miime_style_resolution_free(r);miime_annot_stack_free(stack);cairo_surface_destroy(s);
}
static void test_bracket_samples_three_drawn_segments_not_endpoint_diagonal(void) {
    cairo_surface_t*s=rgb_surface(64,64,1,1,1);cairo_t*cr=cairo_create(s);cairo_set_source_rgb(cr,0,0,0);
    for(int y=10;y<40;y++){int x=20+(y-10);cairo_rectangle(cr,x,y,1,1);}cairo_fill(cr);cairo_destroy(cr);
    MiimeStyleConfig c={0};c.theme=MIIME_THEME_LIGHT;c.has_theme=TRUE;MiimeAnnotationStack*stack=miime_annot_stack_new();MiimeAnnotation a={0};
    a.type=MIIME_ANNOT_BRACKET;a.point_count=4;a.points[0]=20;a.points[1]=10;a.points[2]=50;a.points[3]=40;a.has_color=TRUE;a.color_r=a.color_g=a.color_b=0;a.color_a=1;miime_annot_stack_push(stack,a);
    MiimeStyleResolution*r=miime_style_resolve(s,&c,stack,(MiimeCrop){0});assert(r&&r->annotation_count==1);
    assert(r->annotations[0].local_sample_count>8&&r->annotations[0].local_sample_count<=64);assert(r->annotations[0].shape_contrast_hits*5>=r->annotations[0].shape_sample_count*4&&!r->annotations[0].contrast_unresolved);
    miime_style_resolution_free(r);miime_annot_stack_free(stack);cairo_surface_destroy(s);
}
int main(void) {
    test_auto_theme_thresholds_and_ties();test_auto_style_edge_threshold();test_sampler_caps_and_even_spacing();test_style_alpha_composite_uses_white_analysis_bg();test_exact_palettes_and_style_tokens();test_terminal_and_photo_fixtures();test_label_contrast_uses_surface_pair();test_alpha_composite_and_text_surface_alpha();test_per_sample_coverage_and_path_dedup();test_shape_halo_rescues_only_when_edge_contrast_passes();test_explicit_zero_font_size_uses_legacy_fallback();test_annotations_role_override_and_path_coverage();test_roles_require_active_resolver_and_alias_conflict();test_aesthetic_alias_overrides_auto_theme_style();test_crop_and_empty_crop();test_diagnostics_serialize_requested_and_resolved_fields();test_zero_sample_coverage_is_no_data();test_shape_samples_follow_geometry_not_all_segments();test_text_samples_use_text_bounds_and_source_surface();test_marker_alpha_matches_rendered_opacity_before_halo();test_crop_annotation_is_excluded_from_contrast();test_labeled_shape_tracks_shape_and_label_contrast_independently();test_bracket_samples_three_drawn_segments_not_endpoint_diagonal();puts("style tests passed");return 0;
}
