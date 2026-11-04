#include "../src/render.h"
#include "../src/annotate.h"
#include "../src/image.h"
#include "../src/sketch.h"
#include "../src/style.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pango/pangocairo.h>

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

static void test_render_annotation_vocabulary(void) {
    const MiimeAnnotationType types[] = {
        MIIME_ANNOT_HIGHLIGHT_BOX, MIIME_ANNOT_SPACING_RULER,
        MIIME_ANNOT_ALIGNMENT_REDLINE, MIIME_ANNOT_CALLOUT, MIIME_ANNOT_PIN,
        MIIME_ANNOT_FREEHAND, MIIME_ANNOT_CIRCLE, MIIME_ANNOT_BRACKET,
        MIIME_ANNOT_UNDERLINE, MIIME_ANNOT_STRIKE
    };
    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
        cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 64, 64);
        cairo_t *cr = cairo_create(surf);
        MiimeAnnotationStack *stack = miime_annot_stack_new();
        MiimeAnnotation a;
        memset(&a, 0, sizeof(a));
        a.type = types[i];
        a.points[0] = 5; a.points[1] = 6; a.points[2] = 40; a.points[3] = 45;
        a.point_count = 4; a.color_r = 1.0; a.color_a = 1.0; a.stroke_width = 2.0;
        miime_annot_stack_push(stack, a);
        miime_render_annotations(cr, stack, NULL);
        assert(cairo_status(cr) == CAIRO_STATUS_SUCCESS);
        miime_annot_stack_free(stack);
        cairo_destroy(cr);
        cairo_surface_destroy(surf);
    }
}

static unsigned pixel_alpha(cairo_surface_t *surface, int x, int y) {
    cairo_surface_flush(surface);
    uint32_t *pixels = (uint32_t *)cairo_image_surface_get_data(surface);
    int stride = cairo_image_surface_get_stride(surface) / (int)sizeof(uint32_t);
    return pixels[y * stride + x] >> 24;
}

static uint32_t pixel_value(cairo_surface_t *surface, int x, int y) {
    cairo_surface_flush(surface);
    uint32_t *pixels = (uint32_t *)cairo_image_surface_get_data(surface);
    int stride = cairo_image_surface_get_stride(surface) / (int)sizeof(uint32_t);
    return pixels[y * stride + x];
}

static char *surface_sha256(cairo_surface_t *surface) {
    cairo_surface_flush(surface);
    int height=cairo_image_surface_get_height(surface);
    int stride=cairo_image_surface_get_stride(surface);
    GChecksum *sum=g_checksum_new(G_CHECKSUM_SHA256);
    g_checksum_update(sum,cairo_image_surface_get_data(surface),(gssize)stride*height);
    char *hash=g_strdup(g_checksum_get_string(sum));
    g_checksum_free(sum);
    return hash;
}

static void test_render_legacy_hashes_without_options(void) {
    GdkPixbuf *pb=gdk_pixbuf_new(GDK_COLORSPACE_RGB,TRUE,8,96,72);
    gdk_pixbuf_fill(pb,0x9ab4c8ff); gdk_pixbuf_save(pb,"/tmp/miime_legacy_hash.png","png",NULL,NULL); g_object_unref(pb);
    MiimeImage *img=miime_image_load("/tmp/miime_legacy_hash.png"); assert(img);
    MiimeAnnotationStack *stack=miime_annot_stack_new();
    MiimeAnnotation a; memset(&a,0,sizeof(a)); a.type=MIIME_ANNOT_RECT;
    a.points[0]=10; a.points[1]=12; a.points[2]=48; a.points[3]=50; a.point_count=4;
    a.color_r=.9; a.color_g=.1; a.color_b=.2; a.color_a=.85;
    strcpy(a.label_text,"Legacy label"); miime_annot_stack_push(stack,a);
    miime_render_set_aesthetic(MIIME_AESTHETIC_ROUGH_SKETCH);
    cairo_surface_t *exported=miime_render_export(img,stack,NULL,NULL); assert(exported);
    char *hash=surface_sha256(exported);
    assert(strcmp(hash,"85eed7a48c2db46b65cf95ec9dc694f8828cdcd7c5b6730afd5837a06586866e")==0);
    g_free(hash); cairo_surface_destroy(exported);
    cairo_surface_t *laid_out=NULL;
    char *json=miime_render_annotations_with_dry_run(img,stack,1,MIIME_AESTHETIC_ROUGH_SKETCH,
                                                       FALSE,&laid_out,NULL,NULL);
    assert(json==NULL && laid_out);
    hash=surface_sha256(laid_out);
    assert(strcmp(hash,"266fc1d39c2799982b51f5733378e0896e0b0ee4389545f388509438ae3d9c3c")==0);
    g_free(hash); cairo_surface_destroy(laid_out);
    miime_annot_stack_free(stack); miime_image_free(img); remove("/tmp/miime_legacy_hash.png");
}

static void test_options_renderer_applies_resolved_appearance_without_mutation(void) {
    GdkPixbuf *pb = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 80, 60);
    gdk_pixbuf_fill(pb, 0x202020ff);
    gdk_pixbuf_save(pb, "/tmp/miime_test_options_base.png", "png", NULL, NULL);
    g_object_unref(pb);
    MiimeImage *img = miime_image_load("/tmp/miime_test_options_base.png");
    assert(img);
    cairo_surface_t *source = miime_image_get_surface(img);
    cairo_surface_flush(source);
    uint32_t source_before = pixel_value(source, 30, 20);

    MiimeAnnotationStack *stack = miime_annot_stack_new();
    MiimeAnnotation a; memset(&a, 0, sizeof(a));
    a.type = MIIME_ANNOT_RECT;
    a.points[0] = 20; a.points[1] = 15; a.points[2] = 55; a.points[3] = 45;
    a.point_count = 4;
    a.has_stroke_width = FALSE;
    a.json_id[0] = 'r'; a.json_id[1] = '\0';
    miime_annot_stack_push(stack, a);
    MiimeAnnotation original = *miime_annot_stack_get(stack, 0);
    MiimeAnnotation blur; memset(&blur,0,sizeof(blur));
    blur.type=MIIME_ANNOT_BLUR; blur.points[0]=2; blur.points[1]=2;
    blur.point_count=4; blur.blur_width=4; blur.blur_height=4; blur.blur_stride=16;
    blur.blur_data=calloc(4*4,4); assert(blur.blur_data);
    memset(blur.blur_data,0x5a,4*4*4);
    unsigned char blur_snapshot[4*4*4]; memcpy(blur_snapshot,blur.blur_data,sizeof(blur_snapshot));
    unsigned char *blur_owner=blur.blur_data;
    miime_annot_stack_push(stack,blur);

    MiimeRenderOptions options; memset(&options, 0, sizeof(options));
    options.requested.has_theme = TRUE;
    options.requested.theme = MIIME_THEME_DARK;
    options.requested.has_drawing_style = TRUE;
    options.requested.drawing_style = MIIME_DRAW_EDITORIAL;
    int w = 0, h = 0;
    cairo_surface_t *rendered = miime_render_export_with_options(img, stack, &options, &w, &h);
    assert(rendered && w == 80 && h == 60);
    /* Dark-theme ink is #F1F3F4; resolved colors must reach actual geometry. */
    uint32_t edge = pixel_value(rendered, 37, 15);
    assert(((edge >> 16) & 255) > 225 && ((edge >> 8) & 255) > 225 && (edge & 255) > 225);
    assert(pixel_value(source, 30, 20) == source_before);
    const MiimeAnnotation *after = miime_annot_stack_get(stack, 0);
    assert(memcmp(&original, after, sizeof(original)) == 0);
    assert(miime_annot_stack_get(stack,1)->blur_data==blur_owner);
    assert(memcmp(miime_annot_stack_get(stack,1)->blur_data,blur_snapshot,sizeof(blur_snapshot))==0);
    cairo_surface_destroy(rendered);

    /* Explicit per-annotation color remains authoritative under a theme. */
    a.has_color = TRUE; a.color_r = 1.0; a.color_g = 0.0; a.color_b = 0.0; a.color_a = 1.0;
    miime_annot_stack_clear(stack); miime_annot_stack_push(stack, a);
    rendered = miime_render_export_with_options(img, stack, &options, &w, &h);
    assert(rendered);
    edge = pixel_value(rendered, 37, 15);
    assert(((edge >> 16) & 255) > 220 && ((edge >> 8) & 255) < 40 && (edge & 255) < 40);
    cairo_surface_destroy(rendered);
    miime_annot_stack_free(stack); miime_image_free(img);
    remove("/tmp/miime_test_options_base.png");
}

static void test_options_layout_uses_crop_rail_and_shared_resolution(void) {
    GdkPixbuf *pb=gdk_pixbuf_new(GDK_COLORSPACE_RGB,FALSE,8,80,60);
    gdk_pixbuf_fill(pb,0xffffffff);
    for (int y=10;y<50;y++) for (int x=20;x<60;x++) {
        guchar *p=gdk_pixbuf_get_pixels(pb)+y*gdk_pixbuf_get_rowstride(pb)+x*3;
        p[0]=p[1]=p[2]=0x18;
    }
    gdk_pixbuf_save(pb,"/tmp/miime_test_options_crop.png","png",NULL,NULL); g_object_unref(pb);
    MiimeImage *img=miime_image_load("/tmp/miime_test_options_crop.png"); assert(img);
    MiimeAnnotationStack *stack=miime_annot_stack_new();
    MiimeAnnotation crop; memset(&crop,0,sizeof(crop)); crop.type=MIIME_ANNOT_CROP;
    crop.points[0]=20; crop.points[1]=10; crop.points[2]=60; crop.points[3]=50; crop.point_count=4;
    miime_annot_stack_push(stack,crop);
    MiimeAnnotation label; memset(&label,0,sizeof(label)); label.type=MIIME_ANNOT_RECT;
    label.points[0]=30; label.points[1]=20; label.points[2]=45; label.points[3]=35; label.point_count=4;
    strcpy(label.label_text,"crop rail label");
    miime_annot_stack_push(stack,label);

    MiimeRenderOptions options; memset(&options,0,sizeof(options));
    options.requested.has_theme=TRUE; options.requested.theme=MIIME_THEME_AUTO;
    options.requested.has_drawing_style=TRUE; options.requested.drawing_style=MIIME_DRAW_MINIMAL;
    cairo_surface_t *resolution_source=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,80,60);
    cairo_t *copy=cairo_create(resolution_source);
    cairo_set_source_surface(copy,miime_image_get_surface(img),0,0); cairo_paint(copy); cairo_destroy(copy);
    MiimeCrop source_crop={20,10,40,40,TRUE};
    MiimeStyleResolution *resolution=miime_style_resolve(resolution_source,&options.requested,stack,source_crop);
    assert(resolution && resolution->resolved_theme==MIIME_THEME_DARK);
    options.resolution=resolution;

    cairo_surface_t *out=NULL; int w=0,h=0;
    char *json=miime_render_annotations_with_options(img,stack,1,MIIME_AESTHETIC_CLEAN_TECHNICAL,
                                                       &options,TRUE,&out,&w,&h);
    assert(json && !out && strstr(json,"\"theme\":\"dark\"") && strstr(json,"\"drawing_style\":\"minimal\""));
    g_free(json);
    json=miime_render_annotations_with_options(img,stack,1,MIIME_AESTHETIC_CLEAN_TECHNICAL,
                                                &options,FALSE,&out,&w,&h);
    assert(!json && out && w==640 && h==40);
    /* Source crop begins at output x=300; rail surfaces are separate opaque pixels. */
    assert(pixel_value(out,300,0)==pixel_value(miime_image_get_surface(img),20,10));
    json=miime_render_annotations_with_options(img,stack,1,MIIME_AESTHETIC_CLEAN_TECHNICAL,
                                                &options,TRUE,NULL,NULL,NULL);
    assert(json && strstr(json,"\"resolution\":{\"requested\":{\"drawing_style\":\"minimal\",\"theme\":\"auto\""));
    assert(strstr(json,"\"resolved\":{\"drawing_style\":\"minimal\",\"theme\":\"dark\"}"));
    assert(strstr(json,"\"coordinate_space\":\"source_image\",\"crop_x\":20,\"crop_y\":10,\"crop_width\":40,\"crop_height\":40,\"rail_offset_x\":300"));
    const char *placement=strstr(json,"\"label_x\":");
    assert(placement);
    double lx=0,ly=0,lw=0,lh=0;
    assert(sscanf(placement,"\"label_x\":%lf,\"label_y\":%lf,\"label_w\":%lf,\"label_h\":%lf",&lx,&ly,&lw,&lh)==4);
    assert(lw>16 && lh>16);
    int sx=(int)lx+1, sy=(int)ly+1;
    assert(pixel_alpha(out,sx,sy)==255);
    g_free(json); cairo_surface_destroy(out);
    miime_style_resolution_free(resolution); cairo_surface_destroy(resolution_source);
    miime_annot_stack_free(stack); miime_image_free(img); remove("/tmp/miime_test_options_crop.png");
}

static void test_style_tokens_change_geometry_and_pango_metrics(void) {
    static const double widths[]={2.0,2.5,3.0,1.5};
    static const double radii[]={4.0,8.0,10.0,2.0};
    static const double fonts[]={18.0,18.0,20.0,16.0};
    GdkPixbuf *pb=gdk_pixbuf_new(GDK_COLORSPACE_RGB,FALSE,8,80,60);
    gdk_pixbuf_fill(pb,0xffffffff); gdk_pixbuf_save(pb,"/tmp/miime_test_style_geometry.png","png",NULL,NULL); g_object_unref(pb);
    MiimeImage *img=miime_image_load("/tmp/miime_test_style_geometry.png"); assert(img);
    MiimeAnnotationStack *stack=miime_annot_stack_new();
    MiimeAnnotation a; memset(&a,0,sizeof(a)); a.type=MIIME_ANNOT_LINE;
    a.points[0]=20; a.points[1]=20; a.points[2]=60; a.points[3]=20; a.point_count=4;
    a.label_text[0]='x'; a.label_text[1]='\0';
    strcpy(a.label_text,"A long label whose wrapped Pango metrics respond to the requested absolute font size");
    miime_annot_stack_push(stack,a);
    MiimeAnnotationStack *shape_stack=miime_annot_stack_new();
    MiimeAnnotation shape=a; shape.label_text[0]='\0';
    miime_annot_stack_push(shape_stack,shape);
    int changed_counts[4]={0}; double label_widths[4]={0},label_heights[4]={0};
    for(int s=0;s<4;s++) {
        MiimeRenderOptions options; memset(&options,0,sizeof(options));
        options.requested.has_theme=TRUE; options.requested.theme=MIIME_THEME_LIGHT;
        options.requested.has_drawing_style=TRUE; options.requested.drawing_style=(MiimeDrawingStyle)s;
        MiimeCrop full={0,0,80,60,FALSE};
        MiimeStyleResolution *resolved=miime_style_resolve(miime_image_get_surface(img),&options.requested,shape_stack,full);
        assert(resolved && resolved->annotation_count==1);
        assert(resolved->annotations[0].stroke_width==widths[s]);
        assert(resolved->annotations[0].radius==radii[s]);
        assert(resolved->annotations[0].font_size==fonts[s]);
        options.resolution=resolved;
        int w=0,h=0; cairo_surface_t *out=miime_render_export_with_options(img,shape_stack,&options,&w,&h);
        assert(out && w==80 && h==60);
        for(int y=15;y<=25;y++) if(pixel_value(out,40,y)!=0xffffffffu) changed_counts[s]++;
        cairo_surface_destroy(out);
        char *json=miime_render_annotations_with_options(img,stack,2,MIIME_AESTHETIC_CLEAN_TECHNICAL,
                                                          &options,TRUE,NULL,NULL,NULL);
        assert(json);
        const char *placement=strstr(json,"\"label_x\":");
        double lx=0,ly=0;
        assert(placement && sscanf(placement,"\"label_x\":%lf,\"label_y\":%lf,\"label_w\":%lf,\"label_h\":%lf",
                                   &lx,&ly,&label_widths[s],&label_heights[s])==4);
        cairo_surface_t *measure_surface=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,1,1);
        cairo_t *measure_cr=cairo_create(measure_surface);
        PangoLayout *expected_layout=pango_cairo_create_layout(measure_cr);
        PangoFontDescription *expected_font=pango_font_description_new();
        pango_font_description_set_family(expected_font,"DejaVu Sans");
        pango_font_description_set_absolute_size(expected_font,fonts[s]*PANGO_SCALE);
        pango_layout_set_font_description(expected_layout,expected_font);
        pango_layout_set_width(expected_layout,240*PANGO_SCALE);
        pango_layout_set_wrap(expected_layout,PANGO_WRAP_WORD_CHAR);
        pango_layout_set_text(expected_layout,a.label_text,-1);
        int expected_width=0,expected_height=0;
        pango_layout_get_pixel_size(expected_layout,&expected_width,&expected_height);
        assert(label_widths[s]==MIN(expected_width,240)+16.0);
        assert(label_heights[s]==expected_height+16.0);
        pango_font_description_free(expected_font); g_object_unref(expected_layout);
        cairo_destroy(measure_cr); cairo_surface_destroy(measure_surface);
        g_free(json); miime_style_resolution_free(resolved);
    }
    assert(changed_counts[2]>changed_counts[3]);
    assert(label_widths[2]>label_widths[3] && label_heights[2]>label_heights[3]);
    miime_annot_stack_free(shape_stack); miime_annot_stack_free(stack); miime_image_free(img); remove("/tmp/miime_test_style_geometry.png");
}

static void test_all_themes_preserve_source_and_report_local_coverage(void) {
    GdkPixbuf *pb=gdk_pixbuf_new(GDK_COLORSPACE_RGB,TRUE,8,64,48);
    gdk_pixbuf_fill(pb,0x8090a080); gdk_pixbuf_save(pb,"/tmp/miime_test_all_themes.png","png",NULL,NULL); g_object_unref(pb);
    MiimeImage *img=miime_image_load("/tmp/miime_test_all_themes.png"); assert(img);
    MiimeAnnotationStack *stack=miime_annot_stack_new();
    MiimeAnnotation a; memset(&a,0,sizeof(a)); a.type=MIIME_ANNOT_LINE;
    a.points[0]=20; a.points[1]=20; a.points[2]=44; a.points[3]=20; a.point_count=4;
    miime_annot_stack_push(stack,a);
    uint32_t untouched=pixel_value(miime_image_get_surface(img),2,2);
    const MiimeTheme themes[]={MIIME_THEME_AUTO,MIIME_THEME_LIGHT,MIIME_THEME_DARK,MIIME_THEME_WARM,MIIME_THEME_NEUTRAL};
    for(size_t i=0;i<sizeof(themes)/sizeof(themes[0]);i++) {
        MiimeRenderOptions options; memset(&options,0,sizeof(options));
        options.requested.has_theme=TRUE; options.requested.theme=themes[i];
        int w=0,h=0; cairo_surface_t *out=miime_render_export_with_options(img,stack,&options,&w,&h);
        assert(out && w==64 && h==48);
        assert(pixel_value(out,2,2)==untouched && pixel_alpha(out,2,2)==0x80);
        cairo_surface_t *clean=miime_image_get_surface(img); MiimeCrop full={0,0,64,48,FALSE};
        MiimeStyleResolution *r=miime_style_resolve(clean,&options.requested,stack,full);
        assert(r && r->contrast_hits<=r->contrast_total && r->annotation_count==1);
        assert(r->annotations[0].contrast_hits<=r->annotations[0].local_sample_count);
        if(r->annotations[0].contrast_hits<r->annotations[0].local_sample_count)
            assert(r->annotations[0].contrast_unresolved);
        miime_style_resolution_free(r); cairo_surface_destroy(out);
    }
    miime_annot_stack_free(stack); miime_image_free(img); remove("/tmp/miime_test_all_themes.png");
}

static void test_contrast_fallback_draws_dual_halo_on_path(void) {
    GdkPixbuf *pb=gdk_pixbuf_new(GDK_COLORSPACE_RGB,FALSE,8,64,48);
    gdk_pixbuf_fill(pb,0x202020ff); gdk_pixbuf_save(pb,"/tmp/miime_test_dual_halo.png","png",NULL,NULL); g_object_unref(pb);
    MiimeImage *img=miime_image_load("/tmp/miime_test_dual_halo.png"); assert(img);
    MiimeAnnotationStack *stack=miime_annot_stack_new();
    MiimeAnnotation line; memset(&line,0,sizeof(line)); line.type=MIIME_ANNOT_LINE;
    line.points[0]=12; line.points[1]=20; line.points[2]=52; line.points[3]=20; line.point_count=4;
    miime_annot_stack_push(stack,line);
    MiimeRenderOptions options; memset(&options,0,sizeof(options));
    options.requested.has_theme=TRUE; options.requested.theme=MIIME_THEME_LIGHT;
    options.requested.has_drawing_style=TRUE; options.requested.drawing_style=MIIME_DRAW_EDITORIAL;
    MiimeCrop full={0,0,64,48,FALSE};
    MiimeStyleResolution *r=miime_style_resolve(miime_image_get_surface(img),&options.requested,stack,full);
    assert(r && r->annotations[0].halo_applied && r->annotations[0].contrast_hits>0);
    options.resolution=r;
    cairo_surface_t *out=miime_render_export_with_options(img,stack,&options,NULL,NULL); assert(out);
    uint32_t outer=pixel_value(out,32,17), inner=pixel_value(out,32,19);
    assert(((outer>>16)&255)>0x70 && ((outer>>8)&255)>0x70 && (outer&255)>0x70);
    assert(((inner>>16)&255)<0x60 && ((inner>>8)&255)<0x60 && (inner&255)<0x60);
    cairo_surface_destroy(out); miime_style_resolution_free(r); miime_annot_stack_free(stack);
    miime_image_free(img); remove("/tmp/miime_test_dual_halo.png");
}

typedef struct { unsigned char *data; size_t len, cap; } PngBytes;

static cairo_status_t collect_png_bytes(void *closure, const unsigned char *data, unsigned int length) {
    PngBytes *bytes = closure;
    if (bytes->len + length > bytes->cap) {
        size_t cap = bytes->cap ? bytes->cap : 1024;
        while (cap < bytes->len + length) cap *= 2;
        unsigned char *grown = realloc(bytes->data, cap);
        if (!grown) return CAIRO_STATUS_NO_MEMORY;
        bytes->data = grown;
        bytes->cap = cap;
    }
    memcpy(bytes->data + bytes->len, data, length);
    bytes->len += length;
    return CAIRO_STATUS_SUCCESS;
}

static PngBytes render_freehand_png(void) {
    static const double points[] = {10, 60, 25, 20, 45, 55, 65, 15, 85, 50, 110, 25};
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 128, 80);
    cairo_t *cr = cairo_create(surface);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);
    MiimeAnnotationStack *stack = miime_annot_stack_new();
    MiimeAnnotation a;
    memset(&a, 0, sizeof(a));
    a.type = MIIME_ANNOT_FREEHAND;
    memcpy(a.points, points, sizeof(points));
    a.point_count = (int)(sizeof(points) / sizeof(points[0]));
    a.color_r = 0; a.color_g = 0; a.color_b = 0; a.color_a = 1;
    a.stroke_width = 5;
    strcpy(a.json_id, "freehand-smoothing-test");
    miime_annot_stack_push(stack, a);
    miime_render_set_aesthetic(MIIME_AESTHETIC_CLEAN_TECHNICAL);
    miime_render_annotations(cr, stack, NULL);
    PngBytes bytes = {0};
    assert(cairo_surface_write_to_png_stream(surface, collect_png_bytes, &bytes) == CAIRO_STATUS_SUCCESS);
    miime_annot_stack_free(stack);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    return bytes;
}

static void test_freehand_is_smoothed_and_deterministic(void) {
    PngBytes first = render_freehand_png();
    PngBytes second = render_freehand_png();
    assert(first.len == second.len);
    assert(memcmp(first.data, second.data, first.len) == 0);

    /* The rendered freehand path should differ from its raw waypoint polygon. */
    static const double points[] = {10, 60, 25, 20, 45, 55, 65, 15, 85, 50, 110, 25};
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 128, 80);
    cairo_t *cr = cairo_create(surface);
    cairo_set_source_rgb(cr, 1, 1, 1); cairo_paint(cr);
    cairo_set_source_rgb(cr, 0, 0, 0); cairo_set_line_width(cr, 5);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    MiimeSketchRng raw_rng;
    miime_sketch_rng_init(&raw_rng, "freehand-smoothing-test");
    miime_sketch_var_stroke(cr, &raw_rng, points,
                            (int)(sizeof(points) / sizeof(points[0])), 5, 0);
    PngBytes polygon = {0};
    assert(cairo_surface_write_to_png_stream(surface, collect_png_bytes, &polygon) == CAIRO_STATUS_SUCCESS);
    assert(first.len != polygon.len || memcmp(first.data, polygon.data, first.len) != 0);
    free(first.data); free(second.data); free(polygon.data);
    cairo_destroy(cr); cairo_surface_destroy(surface);
}

static int vertical_ink_pixels(cairo_surface_t *surface, int x, int y0, int y1) {
    int count = 0;
    for (int y = y0; y <= y1; y++) if (pixel_alpha(surface, x, y) > 0) count++;
    return count;
}

static void test_ink_marker_and_taper_pixels(void) {
    miime_render_set_aesthetic(MIIME_AESTHETIC_CLEAN_TECHNICAL);
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 110, 50);
    cairo_t *cr = cairo_create(surface);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);
    MiimeAnnotation marker;
    memset(&marker, 0, sizeof(marker));
    marker.type = MIIME_ANNOT_LINE;
    marker.points[0] = 10; marker.points[1] = 10; marker.points[2] = 50; marker.points[3] = 10;
    marker.point_count = 4; marker.color_r = 1; marker.color_a = 1;
    marker.ink = 3; marker.has_ink = TRUE;
    MiimeAnnotationStack *stack = miime_annot_stack_new();
    miime_annot_stack_push(stack, marker);
    miime_render_annotations(cr, stack, NULL);
    /* Marker composites at 35% opacity and reaches the square-cap edge. */
    uint32_t composited_marker = pixel_value(surface, 30, 10);
    assert(((composited_marker >> 16) & 255) > 245);
    assert(((composited_marker >> 8) & 255) > 150 && ((composited_marker >> 8) & 255) < 190);
    assert(pixel_alpha(surface, 5, 4) > 0); /* 14px square-cap stroke reaches its edge. */
    miime_annot_stack_free(stack);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 64, 40);
    cr = cairo_create(surface);
    stack = miime_annot_stack_new();
    marker.points[0] = 8; marker.points[1] = 20; marker.points[2] = 56; marker.points[3] = 20;
    marker.color_r = 1; marker.color_g = 0; marker.color_b = 0;
    MiimeAnnotation normal;
    memset(&normal, 0, sizeof(normal));
    normal.type = MIIME_ANNOT_LINE;
    normal.points[0] = 32; normal.points[1] = 4; normal.points[2] = 32; normal.points[3] = 36;
    normal.point_count = 4; normal.color_g = 1; normal.color_a = 1; normal.stroke_width = 4;
    miime_annot_stack_push(stack, normal);
    miime_annot_stack_push(stack, marker);
    miime_render_annotations(cr, stack, NULL);
    uint32_t crossing = pixel_value(surface, 32, 20);
    assert(((crossing >> 8) & 255) > ((crossing >> 16) & 255));
    miime_annot_stack_free(stack);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 64, 40);
    cr = cairo_create(surface);
    MiimeAnnotation bold;
    memset(&bold, 0, sizeof(bold));
    bold.type = MIIME_ANNOT_LINE;
    bold.points[0] = 8; bold.points[1] = 20; bold.points[2] = 56; bold.points[3] = 20;
    bold.point_count = 4; bold.color_r = 1; bold.color_a = 1;
    bold.ink = MIIME_INK_BOLD; bold.has_ink = TRUE;
    stack = miime_annot_stack_new();
    miime_annot_stack_push(stack, bold);
    miime_render_annotations(cr, stack, NULL);
    assert(vertical_ink_pixels(surface, 32, 10, 30) >= 4);
    miime_annot_stack_free(stack);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 110, 50);
    cr = cairo_create(surface);
    MiimeAnnotation taper;
    memset(&taper, 0, sizeof(taper));
    taper.type = MIIME_ANNOT_LINE;
    taper.points[0] = 10; taper.points[1] = 25; taper.points[2] = 90; taper.points[3] = 25;
    taper.point_count = 4; taper.color_r = 1; taper.color_a = 1;
    taper.stroke_width = 2; taper.stroke_width_end = 10; taper.has_stroke_width = TRUE;
    taper.has_stroke_width_pair = TRUE;
    stack = miime_annot_stack_new();
    miime_annot_stack_push(stack, taper);
    miime_render_annotations(cr, stack, NULL);
    assert(vertical_ink_pixels(surface, 20, 10, 40) <= 4);
    assert(vertical_ink_pixels(surface, 80, 10, 40) >= 8);
    miime_annot_stack_free(stack);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 110, 50);
    cr = cairo_create(surface);
    MiimeAnnotation uneven;
    memset(&uneven, 0, sizeof(uneven));
    uneven.type = MIIME_ANNOT_PEN;
    uneven.points[0] = 10; uneven.points[1] = 25;
    uneven.points[2] = 18; uneven.points[3] = 25;
    uneven.points[4] = 90; uneven.points[5] = 25;
    uneven.point_count = 6; uneven.color_r = 1; uneven.color_a = 1;
    uneven.stroke_width = 2; uneven.stroke_width_end = 10;
    uneven.has_stroke_width = TRUE; uneven.has_stroke_width_pair = TRUE;
    stack = miime_annot_stack_new();
    miime_annot_stack_push(stack, uneven);
    miime_render_annotations(cr, stack, NULL);
    assert(vertical_ink_pixels(surface, 18, 10, 40) <= 4);
    miime_annot_stack_free(stack);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 64, 48);
    cr = cairo_create(surface);
    MiimeAnnotation marker_pen;
    memset(&marker_pen, 0, sizeof(marker_pen));
    marker_pen.type = MIIME_ANNOT_PEN;
    marker_pen.points[0] = 20; marker_pen.points[1] = 20;
    marker_pen.points[2] = 40; marker_pen.points[3] = 20;
    marker_pen.point_count = 4; marker_pen.color_r = 1; marker_pen.color_a = 1;
    marker_pen.ink = MIIME_INK_MARKER; marker_pen.has_ink = TRUE;
    marker_pen.stroke_width = 14; marker_pen.stroke_width_end = 4;
    marker_pen.has_stroke_width = TRUE; marker_pen.has_stroke_width_pair = TRUE;
    stack = miime_annot_stack_new();
    miime_annot_stack_push(stack, marker_pen);
    miime_render_annotations(cr, stack, NULL);
    assert(pixel_alpha(surface, 14, 14) > 0);
    miime_annot_stack_free(stack);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    miime_render_set_aesthetic(MIIME_AESTHETIC_ROUGH_SKETCH);
}

static void test_legacy_alias_pixels(void) {
    /* Highlight remains byte-identical to the old rect alias. The ruler and
       redline now intentionally add ticks and a default dash pattern. */
    const MiimeAnnotationType aliases[] = { MIIME_ANNOT_HIGHLIGHT_BOX };
    const MiimeAnnotationType bases[] = { MIIME_ANNOT_RECT };
    for (size_t i = 0; i < sizeof(aliases) / sizeof(aliases[0]); i++) {
        cairo_surface_t *alias_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 64, 64);
        cairo_surface_t *base_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 64, 64);
        cairo_t *alias_cr = cairo_create(alias_surface);
        cairo_t *base_cr = cairo_create(base_surface);
        MiimeAnnotation alias, base;
        memset(&alias, 0, sizeof(alias));
        alias.type = aliases[i];
        alias.points[0] = 5; alias.points[1] = 6; alias.points[2] = 40; alias.points[3] = 45;
        alias.point_count = 4; alias.color_r = 1.0; alias.color_a = 1.0; alias.stroke_width = 2.0;
        base = alias;
        base.type = bases[i];
        MiimeAnnotationStack *alias_stack = miime_annot_stack_new();
        MiimeAnnotationStack *base_stack = miime_annot_stack_new();
        miime_annot_stack_push(alias_stack, alias);
        miime_annot_stack_push(base_stack, base);
        miime_render_annotations(alias_cr, alias_stack, NULL);
        miime_render_annotations(base_cr, base_stack, NULL);
        cairo_surface_flush(alias_surface);
        cairo_surface_flush(base_surface);
        assert(cairo_image_surface_get_stride(alias_surface) == cairo_image_surface_get_stride(base_surface));
        size_t bytes = (size_t)cairo_image_surface_get_stride(alias_surface) * 64;
        assert(memcmp(cairo_image_surface_get_data(alias_surface), cairo_image_surface_get_data(base_surface), bytes) == 0);
        miime_annot_stack_free(alias_stack);
        miime_annot_stack_free(base_stack);
        cairo_destroy(alias_cr); cairo_destroy(base_cr);
        cairo_surface_destroy(alias_surface); cairo_surface_destroy(base_surface);
    }
}

static void test_center_pin_renders_badge(void) {
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 64, 64);
    cairo_t *cr = cairo_create(surface);
    MiimeAnnotation pin;
    memset(&pin, 0, sizeof(pin));
    pin.type = MIIME_ANNOT_PIN;
    pin.points[0] = 32; pin.points[1] = 32;
    pin.point_count = 2;
    pin.color_r = 1.0; pin.color_a = 1.0; pin.stroke_width = 2.0;
    MiimeAnnotationStack *stack = miime_annot_stack_new();
    miime_annot_stack_push(stack, pin);
    MiimeAestheticStyle old_style = miime_render_get_aesthetic();
    miime_render_set_aesthetic(MIIME_AESTHETIC_CLEAN_TECHNICAL);
    miime_render_annotations(cr, stack, NULL);
    cairo_surface_flush(surface);
    uint32_t pixel = 0;
    const unsigned char *data = cairo_image_surface_get_data(surface);
    memcpy(&pixel, data + 32 * cairo_image_surface_get_stride(surface) + 36 * 4, sizeof(pixel));
    assert((pixel >> 24) != 0);
    miime_render_set_aesthetic(old_style);
    miime_annot_stack_free(stack);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
}

static void test_callout_tail_and_pin_sequence_pixels(void) {
    MiimeAestheticStyle old_style = miime_render_get_aesthetic();
    miime_render_set_aesthetic(MIIME_AESTHETIC_CLEAN_TECHNICAL);
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 100, 100);
    cairo_t *cr = cairo_create(surface);
    MiimeAnnotation callout;
    memset(&callout, 0, sizeof(callout));
    callout.type = MIIME_ANNOT_CALLOUT;
    callout.points[0] = 30; callout.points[1] = 30;
    callout.points[2] = 70; callout.points[3] = 70;
    callout.point_count = 4; callout.color_g = 1; callout.color_a = 1;
    callout.stroke_width = 2;
    MiimeAnnotationStack *stack = miime_annot_stack_new();
    miime_annot_stack_push(stack, callout);
    miime_render_annotations(cr, stack, NULL);
    /* Inset bubble tail reaches the top-edge midpoint of the target bounds. */
    assert(pixel_alpha(surface, 50, 33) > 0);
    assert(pixel_alpha(surface, 50, 50) >= 63 && pixel_alpha(surface, 50, 50) <= 65);
    assert(pixel_alpha(surface, 36, 36) == 0); /* default rounded corner */
    assert(cairo_status(cr) == CAIRO_STATUS_SUCCESS);
    miime_annot_stack_free(stack);
    cairo_destroy(cr); cairo_surface_destroy(surface);

    surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 100, 100);
    cr = cairo_create(surface);
    callout.has_fill = TRUE; callout.fill = 0;
    stack = miime_annot_stack_new();
    miime_annot_stack_push(stack, callout);
    miime_render_annotations(cr, stack, NULL);
    assert(pixel_alpha(surface, 50, 50) == 0); /* explicit none remains unfilled */
    assert(cairo_status(cr) == CAIRO_STATUS_SUCCESS);
    miime_annot_stack_free(stack); cairo_destroy(cr); cairo_surface_destroy(surface);

    cairo_surface_t *one = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 64, 64);
    cairo_surface_t *two = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 64, 64);
    cairo_t *one_cr = cairo_create(one), *two_cr = cairo_create(two);
    MiimeAnnotation pin;
    memset(&pin, 0, sizeof(pin));
    pin.type = MIIME_ANNOT_PIN; pin.points[0] = 32; pin.points[1] = 32;
    pin.point_count = 2; pin.color_r = 1; pin.color_a = 1; pin.stroke_width = 2;
    MiimeAnnotationStack *one_stack = miime_annot_stack_new();
    MiimeAnnotationStack *two_stack = miime_annot_stack_new();
    miime_annot_stack_push(one_stack, pin);
    miime_annot_stack_push(two_stack, pin);
    pin.points[0] = 32; pin.points[1] = 32;
    miime_annot_stack_push(two_stack, pin); /* second badge must display 2 */
    miime_render_annotations(one_cr, one_stack, NULL);
    miime_render_annotations(two_cr, two_stack, NULL);
    cairo_surface_flush(one); cairo_surface_flush(two);
    /* The first and second badge glyphs differ inside the same centered badge. */
    int glyph_difference = 0;
    for (int y = 25; y <= 39; y++) for (int x = 25; x <= 39; x++)
        glyph_difference += pixel_value(one, x, y) != pixel_value(two, x, y);
    assert(glyph_difference > 10);
    assert(cairo_status(one_cr) == CAIRO_STATUS_SUCCESS);
    assert(cairo_status(two_cr) == CAIRO_STATUS_SUCCESS);
    miime_annot_stack_free(one_stack); miime_annot_stack_free(two_stack);
    cairo_destroy(one_cr); cairo_destroy(two_cr);
    cairo_surface_destroy(one); cairo_surface_destroy(two);
    miime_render_set_aesthetic(old_style);
}

static void test_callout_chooses_nearest_target_edge(void) {
    MiimeAestheticStyle old_style = miime_render_get_aesthetic();
    miime_render_set_aesthetic(MIIME_AESTHETIC_CLEAN_TECHNICAL);
    const double boxes[][4] = {{10, 20, 90, 60}, {40, 10, 60, 90}};
    const int tail_x[] = {50, 41};
    const int tail_y[] = {33, 50};
    for (int i = 0; i < 2; i++) {
        cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 100, 100);
        cairo_t *cr = cairo_create(surface);
        MiimeAnnotation callout;
        memset(&callout, 0, sizeof(callout));
        callout.type = MIIME_ANNOT_CALLOUT;
        callout.points[0] = boxes[i][0]; callout.points[1] = boxes[i][1];
        callout.points[2] = boxes[i][2]; callout.points[3] = boxes[i][3];
        callout.point_count = 4; callout.color_b = 1; callout.color_a = 1;
        callout.stroke_width = 2;
        MiimeAnnotationStack *stack = miime_annot_stack_new();
        miime_annot_stack_push(stack, callout);
        miime_render_annotations(cr, stack, NULL);
        assert(pixel_alpha(surface, tail_x[i], tail_y[i]) > 0);
        assert(cairo_status(cr) == CAIRO_STATUS_SUCCESS);
        miime_annot_stack_free(stack); cairo_destroy(cr); cairo_surface_destroy(surface);
    }
    miime_render_set_aesthetic(old_style);
}

static void test_four_corner_box_uses_opposite_corner(void) {
    cairo_surface_t *four_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 64, 64);
    cairo_surface_t *legacy_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 64, 64);
    cairo_t *four_cr = cairo_create(four_surface);
    cairo_t *legacy_cr = cairo_create(legacy_surface);
    MiimeAnnotation four, legacy;
    memset(&four, 0, sizeof(four));
    four.type = MIIME_ANNOT_HIGHLIGHT_BOX;
    four.points[0] = 5; four.points[1] = 6;
    four.points[2] = 40; four.points[3] = 6;
    four.points[4] = 40; four.points[5] = 45;
    four.points[6] = 5; four.points[7] = 45;
    four.point_count = 8;
    four.color_r = 1.0; four.color_a = 1.0; four.stroke_width = 2.0;
    legacy = four;
    legacy.point_count = 4;
    legacy.points[2] = 40; legacy.points[3] = 45;
    MiimeAnnotationStack *four_stack = miime_annot_stack_new();
    MiimeAnnotationStack *legacy_stack = miime_annot_stack_new();
    miime_annot_stack_push(four_stack, four);
    miime_annot_stack_push(legacy_stack, legacy);
    miime_render_annotations(four_cr, four_stack, NULL);
    miime_render_annotations(legacy_cr, legacy_stack, NULL);
    cairo_surface_flush(four_surface);
    cairo_surface_flush(legacy_surface);
    size_t bytes = (size_t)cairo_image_surface_get_stride(four_surface) * 64;
    assert(memcmp(cairo_image_surface_get_data(four_surface),
                  cairo_image_surface_get_data(legacy_surface), bytes) == 0);
    miime_annot_stack_free(four_stack); miime_annot_stack_free(legacy_stack);
    cairo_destroy(four_cr); cairo_destroy(legacy_cr);
    cairo_surface_destroy(four_surface); cairo_surface_destroy(legacy_surface);
}

static void test_annotation_fills_and_radius(void) {
    const MiimeAnnotationType types[] = {MIIME_ANNOT_RECT, MIIME_ANNOT_CALLOUT};
    for (size_t i = 0; i < sizeof(types)/sizeof(types[0]); i++) {
        cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 100, 80);
        cairo_t *cr = cairo_create(surface);
        MiimeAnnotation a;
        memset(&a, 0, sizeof(a));
        a.type = types[i];
        a.points[0] = 10; a.points[1] = 10; a.points[2] = 90; a.points[3] = 70;
        a.point_count = 4; a.color_r = 1; a.color_a = 1; a.stroke_width = 2;
        a.fill = 1; a.has_fill = TRUE;
        MiimeAnnotationStack *stack = miime_annot_stack_new();
        miime_annot_stack_push(stack, a);
        MiimeAestheticStyle old = miime_render_get_aesthetic();
        miime_render_set_aesthetic(MIIME_AESTHETIC_CLEAN_TECHNICAL);
        miime_render_annotations(cr, stack, NULL);
        assert(cairo_status(cr) == CAIRO_STATUS_SUCCESS);
        /* The diagonal hatch must cross the center of both shape types. */
        int center_ink = 0;
        for (int y = 20; y < 60; y++) for (int x = 20; x < 80; x++)
            center_ink += pixel_alpha(surface, x, y) > 0;
        assert(center_ink > 20);
        miime_render_set_aesthetic(old);
        miime_annot_stack_free(stack);
        cairo_destroy(cr); cairo_surface_destroy(surface);
    }

    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 100, 80);
    cairo_t *cr = cairo_create(surface);
    MiimeAnnotation a;
    memset(&a, 0, sizeof(a));
    a.type = MIIME_ANNOT_RECT;
    a.points[0] = 10; a.points[1] = 10; a.points[2] = 90; a.points[3] = 70;
    a.point_count = 4; a.color_b = 1; a.color_a = 1; a.stroke_width = 2;
    a.fill = 2; a.has_fill = TRUE; a.has_fill_color = TRUE; a.fill_a = 0;
    a.has_radius = TRUE; a.radius = 12;
    MiimeAnnotationStack *stack = miime_annot_stack_new();
    miime_annot_stack_push(stack, a);
    miime_render_set_aesthetic(MIIME_AESTHETIC_CLEAN_TECHNICAL);
    miime_render_annotations(cr, stack, NULL);
    assert(pixel_alpha(surface, 50, 40) == 0); /* explicit zero fill alpha is honored */
    assert(pixel_alpha(surface, 10, 10) == 0); /* rounded corner clips the square corner */
    cairo_path_t *path = cairo_copy_path(cr);
    int curve_count = 0;
    for (int i = 0; i < path->num_data;) {
        if (path->data[i].header.type == CAIRO_PATH_CURVE_TO) curve_count++;
        i += path->data[i].header.length;
    }
    /* Renderer strokes and consumes paths, so inspect the rounded primitive directly. */
    cairo_path_destroy(path);
    cairo_new_path(cr);
    miime_sketch_rounded_rect(cr, NULL, 10, 10, 80, 60, 12, 0);
    path = cairo_copy_path(cr); curve_count = 0;
    for (int i = 0; i < path->num_data;) {
        if (path->data[i].header.type == CAIRO_PATH_CURVE_TO) curve_count++;
        i += path->data[i].header.length;
    }
    assert(curve_count >= 4);
    cairo_path_destroy(path);
    miime_annot_stack_free(stack); cairo_destroy(cr); cairo_surface_destroy(surface);
    miime_render_set_aesthetic(MIIME_AESTHETIC_ROUGH_SKETCH);
}

static void test_callout_explicit_zero_radius_is_square(void) {
    miime_render_set_aesthetic(MIIME_AESTHETIC_CLEAN_TECHNICAL);
    cairo_surface_t *zero_surface=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,100,80);
    cairo_surface_t *default_surface=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,100,80);
    MiimeAnnotation zero; memset(&zero,0,sizeof(zero));
    zero.type=MIIME_ANNOT_CALLOUT; zero.points[0]=10; zero.points[1]=10;
    zero.points[2]=90; zero.points[3]=70; zero.point_count=4;
    zero.color_r=1; zero.color_a=1; zero.stroke_width=2;
    zero.fill=2; zero.has_fill=TRUE; zero.radius=0; zero.has_radius=TRUE;
    MiimeAnnotation absent=zero; absent.has_radius=FALSE;
    MiimeAnnotationStack *z=miime_annot_stack_new(), *d=miime_annot_stack_new();
    miime_annot_stack_push(z,zero); miime_annot_stack_push(d,absent);
    cairo_t *zcr=cairo_create(zero_surface), *dcr=cairo_create(default_surface);
    miime_render_annotations(zcr,z,NULL); miime_render_annotations(dcr,d,NULL);
    cairo_surface_flush(zero_surface); cairo_surface_flush(default_surface);
    size_t surface_bytes=(size_t)cairo_image_surface_get_stride(zero_surface)*80;
    assert(memcmp(cairo_image_surface_get_data(zero_surface),
                  cairo_image_surface_get_data(default_surface),surface_bytes)!=0);
    miime_annot_stack_free(z); miime_annot_stack_free(d);
    cairo_destroy(zcr); cairo_destroy(dcr);
    cairo_surface_destroy(zero_surface); cairo_surface_destroy(default_surface);
    miime_render_set_aesthetic(MIIME_AESTHETIC_ROUGH_SKETCH);
}

static void test_line_family_geometry_and_dash(void) {
    miime_render_set_aesthetic(MIIME_AESTHETIC_CLEAN_TECHNICAL);
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 120, 80);
    cairo_t *cr = cairo_create(surface);
    MiimeAnnotationStack *stack = miime_annot_stack_new();
    MiimeAnnotation a;
    memset(&a, 0, sizeof(a));
    a.type = MIIME_ANNOT_SPACING_RULER;
    a.points[0] = 10; a.points[1] = 20; a.points[2] = 70; a.points[3] = 20;
    a.point_count = 4; a.color_r = 1; a.color_a = 1; a.stroke_width = 2;
    miime_annot_stack_push(stack, a);
    a.type = MIIME_ANNOT_ALIGNMENT_REDLINE;
    a.points[0] = -20; a.points[1] = 40; a.points[2] = 140; a.points[3] = 40;
    miime_annot_stack_push(stack, a);
    a.type = MIIME_ANNOT_CIRCLE;
    a.points[0] = 80; a.points[1] = 5; a.points[2] = 110; a.points[3] = 25;
    a.point_count = 4; a.color_g = 1; a.color_r = 0;
    miime_annot_stack_push(stack, a);
    miime_render_annotations(cr, stack, NULL);
    assert(cairo_status(cr) == CAIRO_STATUS_SUCCESS);
    /* Ruler end tick extends five pixels perpendicular to its main line. */
    assert(pixel_alpha(surface, 10, 15) > 0 && pixel_alpha(surface, 10, 25) > 0);
    /* Dashed full-bleed redline reaches the clipped canvas edges. */
    int left_ink=0, right_ink=0;
    for (int x=0; x<10; x++) left_ink += pixel_alpha(surface,x,40)>0;
    for (int x=110; x<120; x++) right_ink += pixel_alpha(surface,x,40)>0;
    assert(left_ink > 0 && right_ink > 0);
    /* Circle uses max(width,height)/2: its horizontal extent is 20px. */
    assert(pixel_alpha(surface, 80, 15) > 0);
    assert(pixel_alpha(surface, 75, 15) == 0);
    miime_annot_stack_free(stack); cairo_destroy(cr); cairo_surface_destroy(surface);

    surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 120, 30);
    cr = cairo_create(surface);
    stack = miime_annot_stack_new();
    memset(&a, 0, sizeof(a));
    a.type = MIIME_ANNOT_LINE; a.points[0] = 5; a.points[1] = 10;
    a.points[2] = 115; a.points[3] = 10; a.point_count = 4;
    a.color_r = 1; a.color_a = 1; a.stroke_width = 3;
    a.has_dash = TRUE; a.dash = 1;
    miime_annot_stack_push(stack, a);
    miime_render_annotations(cr, stack, NULL);
    assert(pixel_alpha(surface, 8, 10) > 0);
    assert(pixel_alpha(surface, 15, 10) == 0);
    miime_annot_stack_free(stack); cairo_destroy(cr); cairo_surface_destroy(surface);
    miime_render_set_aesthetic(MIIME_AESTHETIC_ROUGH_SKETCH);
}

static void test_rough_bracket_halo_has_no_diagonal(void) {
    MiimeAestheticStyle old_style = miime_render_get_aesthetic();
    miime_render_set_aesthetic(MIIME_AESTHETIC_ROUGH_SKETCH);
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 100, 80);
    cairo_t *cr = cairo_create(surface);
    cairo_set_source_rgb(cr, 0.1, 0.2, 0.35);
    cairo_paint(cr);
    uint32_t untouched = pixel_value(surface, 50, 40);
    MiimeAnnotation bracket;
    memset(&bracket, 0, sizeof(bracket));
    bracket.type = MIIME_ANNOT_BRACKET;
    bracket.points[0] = 30; bracket.points[1] = 20;
    bracket.points[2] = 70; bracket.points[3] = 60;
    bracket.point_count = 4; bracket.color_r = 1; bracket.color_a = 1;
    bracket.stroke_width = 2;
    MiimeAnnotationStack *stack = miime_annot_stack_new();
    miime_annot_stack_push(stack, bracket);
    miime_render_annotations(cr, stack, NULL);
    assert(pixel_value(surface, 50, 40) == untouched);
    assert(pixel_alpha(surface, 30, 40) > 0);
    assert(cairo_status(cr) == CAIRO_STATUS_SUCCESS);
    miime_annot_stack_free(stack); cairo_destroy(cr); cairo_surface_destroy(surface);
    miime_render_set_aesthetic(old_style);
}

static void test_solid_fill_default_alpha_and_clamped_radius(void) {
    miime_render_set_aesthetic(MIIME_AESTHETIC_CLEAN_TECHNICAL);
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 100, 80);
    cairo_t *cr = cairo_create(surface);
    MiimeAnnotation a;
    memset(&a, 0, sizeof(a));
    a.type = MIIME_ANNOT_RECT; a.points[0] = 10; a.points[1] = 10;
    a.points[2] = 90; a.points[3] = 70; a.point_count = 4;
    a.color_r = 1; a.color_a = 1; a.fill = 2; a.has_fill = TRUE;
    a.has_radius = TRUE; a.radius = 1000;
    MiimeAnnotationStack *stack = miime_annot_stack_new();
    miime_annot_stack_push(stack, a);
    miime_render_annotations(cr, stack, NULL);
    uint32_t center = pixel_value(surface, 50, 40);
    assert((center >> 24) == 64 || (center >> 24) == 63);
    /* Radius is clamped to 30px; this square corner stays outside the path. */
    assert(pixel_alpha(surface, 10, 10) == 0);
    assert(pixel_alpha(surface, 40, 10) > 0);
    miime_annot_stack_free(stack); cairo_destroy(cr); cairo_surface_destroy(surface);
    miime_render_set_aesthetic(MIIME_AESTHETIC_ROUGH_SKETCH);
}

static void test_explicit_dash_overrides_and_arrowheads(void) {
    miime_render_set_aesthetic(MIIME_AESTHETIC_CLEAN_TECHNICAL);
    cairo_surface_t *surface=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,100,60);
    cairo_t *cr=cairo_create(surface);
    MiimeAnnotation a; memset(&a,0,sizeof(a));
    a.type=MIIME_ANNOT_ALIGNMENT_REDLINE;
    a.points[0]=5; a.points[1]=10; a.points[2]=95; a.points[3]=10;
    a.point_count=4; a.color_r=1; a.color_a=1; a.stroke_width=2;
    a.has_dash=TRUE; a.dash=0;
    MiimeAnnotationStack *stack=miime_annot_stack_new(); miime_annot_stack_push(stack,a);
    miime_render_annotations(cr,stack,NULL);
    assert(pixel_alpha(surface,30,10)>0); /* explicit solid overrides redline default */
    miime_annot_stack_free(stack); cairo_destroy(cr); cairo_surface_destroy(surface);

    for (int mode=0; mode<=3; mode++) {
        surface=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,100,60); cr=cairo_create(surface);
        memset(&a,0,sizeof(a)); a.type=MIIME_ANNOT_LINE;
        a.points[0]=10; a.points[1]=30; a.points[2]=90; a.points[3]=30; a.point_count=4;
        a.color_r=1; a.color_a=1; a.stroke_width=2; a.has_arrow=TRUE; a.arrow=mode;
        stack=miime_annot_stack_new(); miime_annot_stack_push(stack,a);
        miime_render_annotations(cr,stack,NULL);
        int start_head=0, end_head=0;
        for (int y=24; y<=28; y++) {
            for (int x=10; x<=20; x++) start_head += pixel_alpha(surface,x,y)>0;
            for (int x=80; x<=90; x++) end_head += pixel_alpha(surface,x,y)>0;
        }
        assert((start_head>0)==(mode==1 || mode==3));
        assert((end_head>0)==(mode==2 || mode==3));
        miime_annot_stack_free(stack); cairo_destroy(cr); cairo_surface_destroy(surface);
    }

    for (int mode=0; mode<=3; mode++) {
        surface=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,100,60); cr=cairo_create(surface);
        memset(&a,0,sizeof(a)); a.type=MIIME_ANNOT_ARROW;
        a.points[0]=10; a.points[1]=30; a.points[2]=90; a.points[3]=30; a.point_count=4;
        a.color_r=1; a.color_a=1; a.stroke_width=2; a.has_arrow=TRUE; a.arrow=mode;
        stack=miime_annot_stack_new(); miime_annot_stack_push(stack,a);
        miime_render_annotations(cr,stack,NULL);
        int start_head=0, end_head=0;
        for (int y=24; y<=28; y++) {
            for (int x=10; x<=20; x++) start_head += pixel_alpha(surface,x,y)>0;
            for (int x=80; x<=90; x++) end_head += pixel_alpha(surface,x,y)>0;
        }
        assert((start_head>0)==(mode==1 || mode==3));
        assert((end_head>0)==(mode==2 || mode==3));
        miime_annot_stack_free(stack); cairo_destroy(cr); cairo_surface_destroy(surface);
    }

    /* Explicit dash, taper, and both heads compose on the same LINE. */
    surface=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,110,50); cr=cairo_create(surface);
    memset(&a,0,sizeof(a)); a.type=MIIME_ANNOT_LINE;
    a.points[0]=10; a.points[1]=25; a.points[2]=100; a.points[3]=25; a.point_count=4;
    a.color_r=1; a.color_a=1; a.stroke_width=2; a.stroke_width_end=12;
    a.has_stroke_width=TRUE; a.has_stroke_width_pair=TRUE; a.has_dash=TRUE; a.dash=1;
    a.has_arrow=TRUE; a.arrow=3;
    stack=miime_annot_stack_new(); miime_annot_stack_push(stack,a);
    miime_render_annotations(cr,stack,NULL);
    assert(pixel_alpha(surface,20,25)==0); /* a real dash gap in the shaft */
    assert(vertical_ink_pixels(surface,80,10,40)>vertical_ink_pixels(surface,13,10,40));
    int left_head=0,right_head=0;
    for (int y=19;y<=24;y++) {
        for (int x=10;x<=20;x++) left_head += pixel_alpha(surface,x,y)>0;
        for (int x=90;x<=100;x++) right_head += pixel_alpha(surface,x,y)>0;
    }
    assert(left_head>0 && right_head>0);
    miime_annot_stack_free(stack); cairo_destroy(cr); cairo_surface_destroy(surface);

    /* ARROW also composes dash+taper with its explicit head selection. */
    surface=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,110,50); cr=cairo_create(surface);
    a.type=MIIME_ANNOT_ARROW;
    stack=miime_annot_stack_new(); miime_annot_stack_push(stack,a);
    miime_render_annotations(cr,stack,NULL);
    assert(pixel_alpha(surface,20,25)==0);
    assert(vertical_ink_pixels(surface,80,10,40)>vertical_ink_pixels(surface,13,10,40));
    miime_annot_stack_free(stack); cairo_destroy(cr); cairo_surface_destroy(surface);

    const MiimeAnnotationType dashed_shapes[]={MIIME_ANNOT_RECT,MIIME_ANNOT_ELLIPSE};
    for (size_t i=0;i<sizeof(dashed_shapes)/sizeof(dashed_shapes[0]);i++) {
        cairo_surface_t *dashed=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,100,60);
        cairo_surface_t *solid=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,100,60);
        cairo_t *dcr=cairo_create(dashed), *scr=cairo_create(solid);
        MiimeAnnotation d; memset(&d,0,sizeof(d));
        d.type=dashed_shapes[i]; d.points[0]=10; d.points[1]=8;
        d.points[2]=90; d.points[3]=52; d.point_count=4;
        d.color_b=1; d.color_a=1; d.stroke_width=2; d.has_dash=TRUE; d.dash=1;
        MiimeAnnotation s=d; s.has_dash=FALSE;
        MiimeAnnotationStack *ds=miime_annot_stack_new(), *ss=miime_annot_stack_new();
        miime_annot_stack_push(ds,d); miime_annot_stack_push(ss,s);
        miime_render_annotations(dcr,ds,NULL); miime_render_annotations(scr,ss,NULL);
        cairo_surface_flush(dashed); cairo_surface_flush(solid);
        size_t bytes=(size_t)cairo_image_surface_get_stride(dashed)*60;
        assert(memcmp(cairo_image_surface_get_data(dashed),cairo_image_surface_get_data(solid),bytes)!=0);
        miime_annot_stack_free(ds); miime_annot_stack_free(ss);
        cairo_destroy(dcr); cairo_destroy(scr); cairo_surface_destroy(dashed); cairo_surface_destroy(solid);
    }
    miime_render_set_aesthetic(MIIME_AESTHETIC_ROUGH_SKETCH);
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
    int option_w=0,option_h=0;
    cairo_surface_t *null_options=miime_render_export_with_options(img,stack,NULL,&option_w,&option_h);
    assert(null_options && option_w==w && option_h==h);
    cairo_surface_flush(surf); cairo_surface_flush(null_options);
    size_t surface_bytes=(size_t)cairo_image_surface_get_stride(surf)*h;
    assert(memcmp(cairo_image_surface_get_data(surf),cairo_image_surface_get_data(null_options),surface_bytes)==0);
    cairo_surface_destroy(null_options);

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

static void test_layout_export_and_dry_run_cap(void) {
    GdkPixbuf *pb=gdk_pixbuf_new(GDK_COLORSPACE_RGB,FALSE,8,100,80);
    gdk_pixbuf_fill(pb,0xffffffff);
    gdk_pixbuf_save(pb,"/tmp/miime_test_layout_base.png","png",NULL,NULL);
    g_object_unref(pb);
    MiimeImage *img=miime_image_load("/tmp/miime_test_layout_base.png");
    assert(img);
    MiimeAnnotationStack *stack=miime_annot_stack_new();
    for (int i=0;i<3;i++) {
        MiimeAnnotation a; memset(&a,0,sizeof(a));
        a.type=MIIME_ANNOT_RECT; a.points[0]=10+i*30; a.points[1]=10+i*15;
        a.points[2]=30+i*30; a.points[3]=30+i*15; a.point_count=4;
        a.color_r=1; a.color_a=1; a.label_text[0]='A'+i; a.label_text[1]='\0';
        a.has_label=TRUE; a.has_label_anchor=TRUE; a.label_anchor=1;
        a.has_label_leader=TRUE; a.label_leader=i==0?1:(i==1?2:0);
        miime_annot_stack_push(stack,a);
    }
    cairo_surface_t *surface=NULL; int w=0,h=0;
    char *placements=miime_render_annotations_with_dry_run(img,stack,1,
        MIIME_AESTHETIC_CLEAN_TECHNICAL,FALSE,&surface,&w,&h);
    assert(placements==NULL && surface && w==700 && h==80);
    assert(cairo_image_surface_get_width(surface)==700);
    cairo_surface_destroy(surface);
    placements=miime_render_annotations_with_dry_run(img,stack,1,
        MIIME_AESTHETIC_CLEAN_TECHNICAL,TRUE,&surface,&w,&h);
    assert(placements && surface==NULL && strstr(placements,"\"placements\":[")!=NULL);
    assert(strstr(placements,"\"port_target_x\":330.00,\"port_target_y\":20.00")!=NULL);
    assert(strstr(placements,"\"leader\":1")!=NULL);
    assert(strstr(placements,"\"leader\":2")!=NULL);
    g_free(placements); miime_annot_stack_free(stack);

    stack=miime_annot_stack_new();
    for (int i=0;i<65;i++) {
        MiimeAnnotation a; memset(&a,0,sizeof(a));
        a.type=MIIME_ANNOT_RECT; a.points[0]=i%90; a.points[1]=(i*3)%70;
        a.points[2]=a.points[0]+5; a.points[3]=a.points[1]+5; a.point_count=4;
        a.label_text[0]='x'; a.label_text[1]='\0';
        miime_annot_stack_push(stack,a);
    }
    placements=miime_render_annotations_with_dry_run(img,stack,2,
        MIIME_AESTHETIC_CLEAN_TECHNICAL,TRUE,&surface,&w,&h);
    assert(placements && surface==NULL);
    assert(strstr(placements,"\"annotation_index\":63")!=NULL);
    assert(strstr(placements,"\"annotation_index\":64,\"label_x\":0,\"label_y\":0,\"label_w\":0,\"label_h\":0,\"obstacle_detour\":1")!=NULL);
    g_free(placements); miime_annot_stack_free(stack);

    stack=miime_annot_stack_new();
    MiimeAnnotation crop; memset(&crop,0,sizeof(crop));
    crop.type=MIIME_ANNOT_CROP; crop.points[0]=20; crop.points[1]=10;
    crop.points[2]=60; crop.points[3]=50; crop.point_count=4;
    miime_annot_stack_push(stack,crop);
    MiimeAnnotation cropped_label; memset(&cropped_label,0,sizeof(cropped_label));
    cropped_label.type=MIIME_ANNOT_RECT; cropped_label.points[0]=30; cropped_label.points[1]=20;
    cropped_label.points[2]=45; cropped_label.points[3]=35; cropped_label.point_count=4;
    strcpy(cropped_label.label_text,"crop label");
    miime_annot_stack_push(stack,cropped_label);
    placements=miime_render_annotations_with_dry_run(img,stack,2,
        MIIME_AESTHETIC_CLEAN_TECHNICAL,FALSE,&surface,&w,&h);
    assert(placements==NULL && surface && w==40 && h==40);
    cairo_surface_destroy(surface); miime_annot_stack_free(stack);

    stack=miime_annot_stack_new();
    memset(&crop,0,sizeof(crop)); crop.type=MIIME_ANNOT_CROP;
    crop.points[0]=20; crop.points[1]=10; crop.points[2]=60; crop.points[3]=50; crop.point_count=4;
    miime_annot_stack_push(stack,crop);
    MiimeAnnotation ordered; memset(&ordered,0,sizeof(ordered));
    ordered.type=MIIME_ANNOT_RECT; ordered.points[0]=25; ordered.points[1]=15;
    ordered.points[2]=55; ordered.points[3]=45; ordered.point_count=4;
    ordered.color_r=1; ordered.color_a=1; ordered.has_z=TRUE; ordered.z=2;
    miime_annot_stack_push(stack,ordered);
    placements=miime_render_annotations_with_dry_run(img,stack,1,
        MIIME_AESTHETIC_CLEAN_TECHNICAL,FALSE,&surface,&w,&h);
    assert(placements==NULL && surface && w==40 && h==40);
    cairo_surface_destroy(surface); miime_annot_stack_free(stack);
    miime_image_free(img); remove("/tmp/miime_test_layout_base.png");
}

int main(void) {
    test_render_legacy_hashes_without_options();
    test_render_shapes_offscreen();
    test_render_annotation_vocabulary();
    test_freehand_is_smoothed_and_deterministic();
    test_ink_marker_and_taper_pixels();
    test_legacy_alias_pixels();
    test_four_corner_box_uses_opposite_corner();
    test_annotation_fills_and_radius();
    test_callout_explicit_zero_radius_is_square();
    test_line_family_geometry_and_dash();
    test_rough_bracket_halo_has_no_diagonal();
    test_solid_fill_default_alpha_and_clamped_radius();
    test_explicit_dash_overrides_and_arrowheads();
    test_center_pin_renders_badge();
    test_callout_tail_and_pin_sequence_pixels();
    test_callout_chooses_nearest_target_edge();
    test_render_export();
    test_layout_export_and_dry_run_cap();
    test_options_renderer_applies_resolved_appearance_without_mutation();
    test_options_layout_uses_crop_rail_and_shared_resolution();
    test_style_tokens_change_geometry_and_pango_metrics();
    test_all_themes_preserve_source_and_report_local_coverage();
    test_contrast_fallback_draws_dual_halo_on_path();
    printf("All render tests passed.\n");
    return 0;
}
