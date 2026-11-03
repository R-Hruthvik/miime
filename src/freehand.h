#ifndef MIIME_FREEHAND_H
#define MIIME_FREEHAND_H

#include "sketch.h"

/* Render a Catmull-Rom smoothed stroke for coordinate pairs in pts. count is
 * the number of doubles. A positive end_width selects linear taper from
 * start_width; otherwise the standard sketch pressure profile is used.
 * rough controls deterministic edge jitter through rng. Fewer than two
 * distinct finite waypoints are a no-op. Sampling is bounded to 16,384
 * segments even for extremely distant points. */
void miime_freehand_build_path(cairo_t *cr, MiimeSketchRng *rng,
                               const double *pts, int count,
                               double start_width, double end_width, int rough);

#endif
