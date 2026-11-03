#include "headless.h"
#include "image.h"
#include "annotate.h"
#include "schema.h"
#include "render.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int miime_headless_run(const char *input_path, const char *annotations_path, const char *output_path) {
    if (!input_path || !output_path) return 1;

    MiimeImage *img = miime_image_load(input_path);
    if (!img) return 1;

    MiimeAnnotationStack *stack = NULL;
    if (annotations_path) {
        FILE *f = fopen(annotations_path, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fseek(f, 0, SEEK_SET);
            char *buf = malloc(sz + 1);
            if (buf) {
                fread(buf, 1, sz, f);
                buf[sz] = '\0';
                GError *err = NULL;
                stack = miime_schema_deserialize(buf, &err);
                if (err) {
                    g_printerr("miime: %s\n", err->message);
                    g_error_free(err);
                }
                free(buf);
            }
            fclose(f);
        }
    }
    if (!stack) stack = miime_annot_stack_new();

    int w = 0, h = 0;
    cairo_surface_t *surf = miime_render_export(img, stack, &w, &h);
    if (!surf) {
        miime_annot_stack_free(stack);
        miime_image_free(img);
        return 1;
    }

    gboolean ok = miime_render_save_to_file(surf, output_path);
    cairo_surface_destroy(surf);
    miime_annot_stack_free(stack);
    miime_image_free(img);
    return ok ? 0 : 1;
}