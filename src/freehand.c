#include "freehand.h"
#include <math.h>
#include <stdlib.h>

#define FREEHAND_MAX_SEGMENTS 16384
#define FREEHAND_SPACING 4.0

static void catmull_rom(const double *pts, int n, int segment, double t,
                        double *x, double *y) {
    int i1 = segment, i2 = segment + 1;
    int i0 = i1 > 0 ? i1 - 1 : i1;
    int i3 = i2 + 1 < n ? i2 + 1 : i2;
    double t2 = t * t, t3 = t2 * t;
    for (int axis = 0; axis < 2; axis++) {
        double p0 = pts[i0 * 2 + axis], p1 = pts[i1 * 2 + axis];
        double p2 = pts[i2 * 2 + axis], p3 = pts[i3 * 2 + axis];
        double value = 0.5 * ((2.0 * p1) + (-p0 + p2) * t +
                              (2.0*p0 - 5.0*p1 + 4.0*p2 - p3) * t2 +
                              (-p0 + 3.0*p1 - 3.0*p2 + p3) * t3);
        if (axis == 0) *x = value; else *y = value;
    }
}

void miime_freehand_build_path(cairo_t *cr, MiimeSketchRng *rng,
                               const double *pts, int count,
                               double start_width, double end_width, int rough) {
    if (!cr || !pts || count < 4 || count > MIIME_MAX_POINTS || (count & 1) ||
        !isfinite(start_width) || start_width <= 0)
        return;
    int n = count / 2;
    for (int i = 0; i < count; i++) if (!isfinite(pts[i])) return;
    int has_distinct_point = 0;
    for (int i = 1; i < n; i++)
        if (pts[i * 2] != pts[0] || pts[i * 2 + 1] != pts[1]) {
            has_distinct_point = 1;
            break;
        }
    if (!has_distinct_point) return;

    /* Estimate all segment lengths first. Control-polygon length safely
       bounds curve length closely enough to choose roughly 4px spacing. */
    double *estimates = malloc((size_t)(n - 1) * sizeof(double));
    int *steps = malloc((size_t)(n - 1) * sizeof(int));
    if (!estimates || !steps) { free(estimates); free(steps); return; }
    for (int i = 0; i < n - 1; i++) {
        int i0 = i > 0 ? i - 1 : i, i1 = i, i2 = i + 1;
        int i3 = i + 2 < n ? i + 2 : i + 1;
        double x1 = pts[i1*2], y1 = pts[i1*2+1];
        double x2 = pts[i2*2], y2 = pts[i2*2+1];
        double c1x = x1 + (pts[i2*2] - pts[i0*2]) / 6.0;
        double c1y = y1 + (pts[i2*2+1] - pts[i0*2+1]) / 6.0;
        double c2x = x2 - (pts[i3*2] - pts[i1*2]) / 6.0;
        double c2y = y2 - (pts[i3*2+1] - pts[i1*2+1]) / 6.0;
        estimates[i] = hypot(c1x-x1, c1y-y1) + hypot(c2x-c1x, c2y-c1y) + hypot(x2-c2x, y2-c2y);
        if (!isfinite(estimates[i])) estimates[i] = FREEHAND_MAX_SEGMENTS * FREEHAND_SPACING;
    }
    int segment_count = n - 1;
    int remaining = FREEHAND_MAX_SEGMENTS;
    for (int i = 0; i < segment_count; i++) {
        int min_reserved = segment_count - i - 1;
        double wanted = ceil(estimates[i] / FREEHAND_SPACING);
        if (wanted < 1) wanted = 1;
        int available = remaining - min_reserved;
        steps[i] = wanted > available ? available : (int)wanted;
        remaining -= steps[i];
    }
    double *sampled = malloc((size_t)(FREEHAND_MAX_SEGMENTS + 1) * 2 * sizeof(double));
    if (!sampled) { free(estimates); free(steps); return; }
    int used = 0;
    sampled[used++] = pts[0]; sampled[used++] = pts[1];
    int sample_count = 1;
    for (int i = 0; i < segment_count; i++) {
        for (int j = 1; j <= steps[i]; j++) {
            double x, y;
            catmull_rom(pts, n, i, (double)j / steps[i], &x, &y);
            if (!isfinite(x) || !isfinite(y)) continue;
            sampled[used++] = x; sampled[used++] = y;
            sample_count++;
        }
    }
    free(estimates); free(steps);
    if (sample_count < 2) { free(sampled); return; }
    if (end_width > 0 && isfinite(end_width))
        miime_sketch_var_stroke_taper(cr, rng, sampled, used, start_width, end_width, rough);
    else
        miime_sketch_var_stroke(cr, rng, sampled, used, start_width, rough);
    free(sampled);
}
