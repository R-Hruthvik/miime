#ifndef MIIME_SKETCH_H
#define MIIME_SKETCH_H

#include <cairo.h>
#include "annotate.h"

typedef struct {
    uint32_t state;
} MiimeSketchRng;

/* Hash an annotation id (or any string) into a 32-bit seed. */
unsigned int miime_sketch_hash_id(const char *id);

/* Initialize PRNG from id (empty/NULL uses a fallback seed). */
void miime_sketch_rng_init(MiimeSketchRng *rng, const char *id);

/* Deterministic rand in [min, max]. */
double miime_sketch_rand(MiimeSketchRng *rng, double min, double max);

/* Path builders: append a (possibly perturbed) primitive path to cr.
 * When rough==0 they fall back to clean geometry. */
void miime_sketch_line(cairo_t *cr, MiimeSketchRng *rng,
                       double x1, double y1, double x2, double y2, int rough);
void miime_sketch_line_dash(cairo_t *cr, MiimeSketchRng *rng,
                            double x1, double y1, double x2, double y2,
                            int rough, int dash);

void miime_sketch_rect(cairo_t *cr, MiimeSketchRng *rng,
                       double x, double y, double w, double h, int rough);

/* Rounded rectangle path; radius is clamped to half the shorter side. */
void miime_sketch_rounded_rect(cairo_t *cr, MiimeSketchRng *rng,
                               double x, double y, double w, double h,
                               double radius, int rough);

void miime_sketch_ellipse(cairo_t *cr, MiimeSketchRng *rng,
                          double cx, double cy, double rx, double ry, int rough);

void miime_sketch_arrow(cairo_t *cr, MiimeSketchRng *rng,
                        double x1, double y1, double x2, double y2, int rough);

/* Hachure fill: strokes diagonal scanlines inside rect (caller sets alpha/width). */
void miime_sketch_hachure(cairo_t *cr, MiimeSketchRng *rng,
                          double x, double y, double w, double h);

/* Variable-width stroke along a polyline: builds filled polygon (fills, not strokes). */
void miime_sketch_var_stroke(cairo_t *cr, MiimeSketchRng *rng,
                             const double *pts, int count, double base_width, int rough);

/* Linear taper between explicit start and end widths. */
void miime_sketch_var_stroke_taper(cairo_t *cr, MiimeSketchRng *rng,
                                   const double *pts, int count,
                                   double start_width, double end_width, int rough);

#endif /* MIIME_SKETCH_H */
