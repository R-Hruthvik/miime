#ifndef MIIME_STYLE_H
#define MIIME_STYLE_H
#include "annotate.h"
#include <cairo.h>

typedef struct { int x, y, width, height; gboolean enabled; } MiimeCrop;
typedef struct {
    char annotation_id[32];
    guint local_sample_count, contrast_hits;
    gboolean halo_applied, contrast_unresolved, override_warning;
    double ink_rgba[4], fill_rgba[4], label_surface_rgba[4];
    double stroke_width, stroke_width_end, font_size, radius, halo_width;
    gboolean has_fill, has_label_surface;
} MiimeAnnotationStyleResult;
typedef struct MiimeStyleResolution {
    MiimeStyleConfig requested;
    MiimeDrawingStyle resolved_drawing_style;
    MiimeTheme resolved_theme;
    guint image_sample_count, contrast_hits, contrast_total;
    double text_target, shape_target;
    char policy[64], warning_codes[256];
    MiimeAnnotationStyleResult *annotations;
    guint annotation_count;
} MiimeStyleResolution;
typedef struct { double ink[4], label_surface[4], secondary_surface[4], error[4], warning[4], info[4]; } MiimeStylePalette;
const MiimeStylePalette *miime_style_palette(MiimeTheme theme);
const char *miime_style_token(MiimeDrawingStyle style);
MiimeStyleResolution *miime_style_resolve(const cairo_surface_t *clean_source,
    const MiimeStyleConfig *requested, const MiimeAnnotationStack *stack, MiimeCrop crop);
void miime_style_resolution_free(MiimeStyleResolution *resolution);
char *miime_style_resolution_to_json(const MiimeStyleResolution *resolution,
                                     MiimeCrop crop, int rail_offset_x);
#endif
