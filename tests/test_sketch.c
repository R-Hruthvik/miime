#include "../src/sketch.h"
#include "../src/render.h"
#include "../src/annotate.h"
#include "../src/image.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

static void make_base_image(const char *path) {
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 120, 120);
    cairo_t *cr = cairo_create(s);
    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    cairo_paint(cr);
    cairo_surface_write_to_png(s, path);
    cairo_destroy(cr);
    cairo_surface_destroy(s);
}

static MiimeAnnotationStack *make_stack(void) {
    MiimeAnnotationStack *st = miime_annot_stack_new();
    for (int i = 0; i < 8; i++) {
        MiimeAnnotation a;
        memset(&a, 0, sizeof(a));
        snprintf(a.json_id, sizeof(a.json_id), "ann-%d", i);
        if (i % 4 == 0) {
            a.type = MIIME_ANNOT_RECT;
            a.points[0]=10+i*5; a.points[1]=10+i*5;
            a.points[2]=60+i*5; a.points[3]=40+i*5;
            a.point_count=4;
        } else if (i % 4 == 1) {
            a.type = MIIME_ANNOT_ELLIPSE;
            a.points[0]=20+i*3; a.points[1]=20+i*3;
            a.points[2]=80+i*3; a.points[3]=60+i*3;
            a.point_count=4;
        } else if (i % 4 == 2) {
            a.type = MIIME_ANNOT_ARROW;
            a.points[0]=5; a.points[1]=5+i*10;
            a.points[2]=100; a.points[3]=20+i*10;
            a.point_count=4;
        } else {
            a.type = MIIME_ANNOT_LINE;
            a.points[0]=5; a.points[1]=90;
            a.points[2]=110; a.points[3]=100;
            a.point_count=4;
        }
        a.color_r=0.9; a.color_g=0.2; a.color_b=0.2; a.color_a=1.0;
        a.stroke_width=2.5;
        miime_annot_stack_push(st, a);
    }
    return st;
}

int main(void) {
    const char *base = "/tmp/miime_sketch_base.png";
    make_base_image(base);
    MiimeImage *img = miime_image_load(base);
    assert(img != NULL);

    miime_render_set_aesthetic(MIIME_AESTHETIC_ROUGH_SKETCH);

    /* determinism: 100 renders byte-identical */
    MiimeAnnotationStack *st = make_stack();
    char firstpath[256];
    snprintf(firstpath, sizeof(firstpath), "/tmp/miime_sketch_render_0.png");
    int w,h;
    cairo_surface_t *s0 = miime_render_export(img, st, &w, &h);
    assert(s0);
    cairo_surface_write_to_png(s0, firstpath);
    /* read bytes of first */
    FILE *f = fopen(firstpath, "rb");
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *buf0 = malloc(sz);
    fread(buf0, 1, sz, f);
    fclose(f);
    cairo_surface_destroy(s0);

    for (int i = 1; i < 100; i++) {
        cairo_surface_t *s = miime_render_export(img, st, &w, &h);
        assert(s);
        char path[256];
        snprintf(path, sizeof(path), "/tmp/miime_sketch_render_%d.png", i);
        cairo_surface_write_to_png(s, path);
        cairo_surface_destroy(s);
        FILE *fi = fopen(path, "rb");
        fseek(fi, 0, SEEK_END);
        long szi = ftell(fi);
        fseek(fi, 0, SEEK_SET);
        assert(szi == sz);
        unsigned char *bufi = malloc(szi);
        fread(bufi, 1, szi, fi);
        fclose(fi);
        assert(memcmp(buf0, bufi, sz) == 0);
        free(bufi);
        remove(path);
    }
    remove(firstpath);
    free(buf0);

    /* performance: 8 annotations < 100ms */
    struct timeval t0, t1;
    gettimeofday(&t0, NULL);
    for (int i = 0; i < 10; i++) {
        cairo_surface_t *s = miime_render_export(img, st, &w, &h);
        cairo_surface_destroy(s);
    }
    gettimeofday(&t1, NULL);
    double ms = (t1.tv_sec - t0.tv_sec) * 1000.0 + (t1.tv_usec - t0.tv_usec) / 1000.0;
    ms /= 10.0;
    printf("avg render 8 annotations: %.2f ms\n", ms);
    assert(ms < 100.0);

    /* PRNG determinism */
    MiimeSketchRng r1, r2;
    miime_sketch_rng_init(&r1, "abc");
    miime_sketch_rng_init(&r2, "abc");
    for (int i = 0; i < 100; i++) {
        assert(miime_sketch_rand(&r1, -5, 5) == miime_sketch_rand(&r2, -5, 5));
    }

    miime_annot_stack_free(st);
    miime_image_free(img);
    remove(base);

    printf("PASS: test_sketch\n");
    return 0;
}
