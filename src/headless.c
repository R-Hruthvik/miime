#include "headless.h"
#include "image.h"
#include "annotate.h"
#include "schema.h"
#include "render.h"
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_layout_mode(const char *json) {
    const char *key=strstr(json,"\"layout_mode\"");
    if (!key) return 0;
    const char *value=strchr(key,':');
    if (!value) return 0;
    value++;
    while (*value==' ' || *value=='\t' || *value=='\r' || *value=='\n') value++;
    if (*value=='"') {
        value++;
        if (strncmp(value,"margin_rail",11)==0) return 1;
        if (strncmp(value,"in_canvas_floating",18)==0 || strncmp(value,"in_canvas",9)==0) return 2;
        return 0;
    }
    return *value>='0' && *value<='2' ? *value-'0' : 0;
}

int miime_headless_run(const char *input_path, const char *annotations_path, const char *output_path) {
    if (!input_path || !output_path) return 1;

    MiimeImage *img = miime_image_load(input_path);
    if (!img) return 1;

    MiimeAnnotationStack *stack = NULL;
    MiimeStyleConfig requested_config={0};
    int layout_mode=0;
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
                if (!miime_schema_deserialize_render_config(buf,&requested_config,&err)) {
                    g_printerr("miime: %s\n",err ? err->message : "invalid render config");
                    if (err) g_error_free(err);
                    free(buf);
                    fclose(f);
                    miime_image_free(img);
                    return 1;
                }
                layout_mode=parse_layout_mode(buf);
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
    cairo_surface_t *surf=NULL;
    if (requested_config.has_drawing_style || requested_config.has_theme) {
        MiimeRenderOptions options={.requested=requested_config,.resolution=NULL};
        char *placement=miime_render_annotations_with_options(img,stack,layout_mode,
            requested_config.has_aesthetic_style ? requested_config.aesthetic_style : MIIME_AESTHETIC_ROUGH_SKETCH,
            &options,FALSE,&surf,&w,&h);
        g_free(placement);
    } else {
        surf = miime_render_export(img, stack, &w, &h);
    }
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
