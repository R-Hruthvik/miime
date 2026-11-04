#include "../src/headless.h"
#include "../src/image.h"
#include "../src/annotate.h"
#include "../src/schema.h"
#include "../src/render.h"
#include <cairo.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

int main(void) {
    /* Create a small test PNG */
    const char *inpath = "/tmp/opencode/test_headless_in.png";
    const char *annpath = "/tmp/opencode/test_headless_a.json";
    const char *outpath = "/tmp/opencode/test_headless_out.png";
    mkdir("/tmp/opencode", 0777);

    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 64, 64);
    cairo_t *cr = cairo_create(s);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);
    cairo_destroy(cr);
    cairo_surface_write_to_png(s, inpath);
    cairo_surface_destroy(s);

    FILE *f = fopen(annpath, "w");
    fputs("{\"drawing_style\":\"editorial\",\"theme\":\"auto\",\"annotations\":[{\"id\":\"x\",\"type\":\"rect\",\"points\":[10,10,50,50],\"text\":\"\"}]}", f);
    fclose(f);

    int rc = miime_headless_run(inpath, annpath, outpath);
    assert(rc == 0);

    /* Verify output exists and is PNG */
    FILE *o = fopen(outpath, "rb");
    assert(o != NULL);
    unsigned char magic[8];
    fread(magic, 1, 8, o);
    fclose(o);
    assert(magic[0] == 0x89 && magic[1] == 'P' && magic[2] == 'N' && magic[3] == 'G');

    /* The headless JSON route and the shared options-aware renderer must
     * produce identical PNG bytes for the same requested config. */
    MiimeImage *img = miime_image_load(inpath);
    assert(img != NULL);
    GError *err = NULL;
    MiimeStyleConfig config = {0};
    assert(miime_schema_deserialize_render_config("{\"drawing_style\":\"editorial\",\"theme\":\"auto\"}", &config, &err));
    assert(err == NULL);
    MiimeAnnotationStack *stack = miime_schema_deserialize("{\"annotations\":[{\"id\":\"x\",\"type\":\"rect\",\"points\":[10,10,50,50],\"text\":\"\"}]}", &err);
    assert(stack != NULL && err == NULL);
    MiimeRenderOptions options = {.requested=config, .resolution=NULL};
    cairo_surface_t *expected = NULL;
    int w = 0, h = 0;
    char *placement = miime_render_annotations_with_options(img, stack, 0, MIIME_AESTHETIC_ROUGH_SKETCH, &options, FALSE, &expected, &w, &h);
    assert(placement == NULL && expected != NULL);
    const char *expected_path = "/tmp/opencode/test_headless_expected.png";
    assert(miime_render_save_to_file(expected, expected_path));
    cairo_surface_destroy(expected);
    miime_annot_stack_free(stack);
    miime_image_free(img);

    FILE *a = fopen(outpath, "rb"), *b = fopen(expected_path, "rb");
    assert(a && b);
    fseek(a, 0, SEEK_END); long asz=ftell(a); rewind(a);
    fseek(b, 0, SEEK_END); long bsz=ftell(b); rewind(b);
    assert(asz == bsz);
    unsigned char *abytes=malloc((size_t)asz), *bbytes=malloc((size_t)bsz);
    assert(abytes && bbytes);
    assert(fread(abytes,1,(size_t)asz,a)==(size_t)asz);
    assert(fread(bbytes,1,(size_t)bsz,b)==(size_t)bsz);
    fclose(a); fclose(b);
    assert(memcmp(abytes,bbytes,(size_t)asz)==0);
    free(abytes); free(bbytes);

    f = fopen(annpath, "w");
    fputs("{\"theme\":\"ultraviolet\",\"annotations\":[]}", f);
    fclose(f);
    assert(miime_headless_run(inpath, annpath, outpath) != 0);
    printf("All headless tests passed.\n");
    return 0;
}
