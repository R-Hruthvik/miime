#include "../src/headless.h"
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
    fputs("{\"annotations\":[{\"id\":\"x\",\"type\":\"rect\",\"points\":[10,10,50,50],\"color\":[1,0,0,1],\"stroke_width\":2,\"text\":\"\",\"font_size\":18}]}", f);
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
    printf("All headless tests passed.\n");
    return 0;
}