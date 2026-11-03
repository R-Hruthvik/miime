#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "grounding.h"
#include <cairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib.h>

int main(void) {
    /* Create a synthetic UI screenshot */
    int w=800,h=600;
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,w,h);
    cairo_t *cr = cairo_create(s);
    cairo_set_source_rgb(cr,0.95,0.95,0.97); cairo_paint(cr);
    /* draw many rectangles */
    for (int i=0;i<30;i++){
        cairo_set_source_rgb(cr, 0.2+0.01*i, 0.4, 0.7);
        cairo_rectangle(cr, 20+(i%6)*120, 20+(i/6)*90, 100, 60);
        cairo_fill(cr);
    }
    cairo_destroy(cr);
    const char *img="/tmp/opencode/test_ui.png";
    cairo_surface_write_to_png(s, img);
    cairo_surface_destroy(s);

    MiimeViewport vp={w,h};
    char *png=NULL, *json=NULL;
    int rc = miime_grounding_run(1, img, vp, 1, &png, &json);
    printf("rc=%d png=%s\n", rc, png?png:"(null)");
    if (rc != 0) { printf("FAIL run\n"); return 1; }

    /* count elements in json (simple count of "id") */
    int cnt=0; char *p=json;
    while ((p=strstr(p,"\"id\""))) { cnt++; p+=4; }
    printf("elements=%d\n", cnt);
    if (cnt < 20) { printf("FAIL <20\n"); return 1; }
    if (!png || !g_file_test(png, G_FILE_TEST_EXISTS)) { printf("FAIL png missing\n"); return 1; }

    g_free(png); g_free(json);
    printf("OK\n");
    return 0;
}
