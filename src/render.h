#ifndef MIIME_RENDER_H
#define MIIME_RENDER_H

#include <cairo.h>
#include "image.h"

/*
 * Render the base image to a Cairo context with viewport transforms.
 * zoom: scale factor (1.0 = original size)
 * pan_x, pan_y: pixel offset of the viewport
 * widget_w, widget_h: dimensions of the drawing area
 */
void miime_render_on_screen(cairo_t *cr,
                            MiimeImage *img,
                            double zoom,
                            double pan_x, double pan_y,
                            int widget_w, int widget_h);

#endif /* MIIME_RENDER_H */
