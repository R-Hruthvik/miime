#ifndef MIIME_RENDER_H
#define MIIME_RENDER_H

#include <cairo.h>
#include "image.h"
#include "annotate.h"

/*
 * Render the base image and annotations to a Cairo context with viewport transforms.
 * zoom: scale factor (1.0 = original size)
 * pan_x, pan_y: pixel offset of the viewport
 * widget_w, widget_h: dimensions of the drawing area
 */
void miime_render_on_screen(cairo_t *cr,
                            MiimeImage *img,
                            const MiimeAnnotationStack *stack,
                            const MiimeAnnotation *preview,
                            double zoom,
                            double pan_x, double pan_y,
                            int widget_w, int widget_h);

/* Render annotations from stack + optional live preview on a Cairo context.
 * The context should already have the image-space transform applied. */
void miime_render_annotations(cairo_t *cr,
                              const MiimeAnnotationStack *stack,
                              const MiimeAnnotation *preview);

/*
 * Bake base image + all annotations into a flat Cairo ARGB32 surface.
 * If a crop annotation exists, crops the output surface to that region.
 * Returns newly created cairo_surface_t (caller must cairo_surface_destroy).
 * Sets out_w and out_h if non-NULL.
 */
cairo_surface_t *miime_render_export(MiimeImage *img,
                                     const MiimeAnnotationStack *stack,
                                     int *out_w, int *out_h);

/* Save exported surface to a file (PNG or JPEG based on extension). Returns TRUE on success. */
gboolean miime_render_save_to_file(cairo_surface_t *surface, const char *filepath);

#endif /* MIIME_RENDER_H */
