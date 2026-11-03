#include "sketch.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

unsigned int miime_sketch_hash_id(const char *id) {
    /* cyrb128-ish simple hash */
    uint32_t h = 2166136261u;
    if (!id) return h;
    for (const char *p = id; *p; p++) {
        h ^= (unsigned char)*p;
        h *= 16777619u;
        h ^= h >> 13;
        h *= 5;
    }
    if (h == 0) h = 0x9e3779b9u;
    return h;
}

void miime_sketch_rng_init(MiimeSketchRng *rng, const char *id) {
    if (!rng) return;
    unsigned int seed = miime_sketch_hash_id(id);
    /* mix */
    seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
    if (seed == 0) seed = 1;
    rng->state = seed;
}

double miime_sketch_rand(MiimeSketchRng *rng, double min, double max) {
    if (!rng) return (min + max) * 0.5;
    rng->state += 0x6D2B79F5u;
    uint32_t t = rng->state;
    t = (t ^ (t >> 15)) * (t | 1u);
    t ^= t + (t ^ (t >> 7)) * (t | 61u);
    double u = (double)(t ^ (t >> 14)) / 4294967296.0;
    return min + (max - min) * u;
}

/* --- line --- */
void miime_sketch_line(cairo_t *cr, MiimeSketchRng *rng,
                       double x1, double y1, double x2, double y2, int rough) {
    if (!cr) return;
    if (!rough) {
        cairo_move_to(cr, x1, y1);
        cairo_line_to(cr, x2, y2);
        return;
    }
    double dx = x2 - x1, dy = y2 - y1;
    double len = hypot(dx, dy);
    if (len < 1e-9) { cairo_move_to(cr, x1, y1); return; }
    double nx = -dy / len, ny = dx / len;
    double j1 = miime_sketch_rand(rng, -2.0, 2.0);
    double j2 = miime_sketch_rand(rng, -2.0, 2.0);
    double sx = x1 + nx * j1;
    double sy = y1 + ny * j1;
    double ex = x2 + nx * j2;
    double ey = y2 + ny * j2;
    double over = miime_sketch_rand(rng, 0.0, 3.0);
    ex += (dx / len) * over;
    ey += (dy / len) * over;
    double bow = miime_sketch_rand(rng, -8.0, 8.0);
    if (bow > 8) bow = 8;
    if (bow < -8) bow = -8;
    double c1x = sx + dx / 3.0 + nx * bow * 0.5 + miime_sketch_rand(rng, -1.0, 1.0);
    double c1y = sy + dy / 3.0 + ny * bow * 0.5 + miime_sketch_rand(rng, -1.0, 1.0);
    double c2x = sx + 2.0 * dx / 3.0 + nx * bow * 0.5 + miime_sketch_rand(rng, -1.0, 1.0);
    double c2y = sy + 2.0 * dy / 3.0 + ny * bow * 0.5 + miime_sketch_rand(rng, -1.0, 1.0);
    cairo_move_to(cr, sx, sy);
    cairo_curve_to(cr, c1x, c1y, c2x, c2y, ex, ey);
}

void miime_sketch_line_dash(cairo_t *cr, MiimeSketchRng *rng,
                            double x1, double y1, double x2, double y2,
                            int rough, int dash) {
    if (!cr) return;
    if (dash == 1) {
        const double pattern[] = {8.0, 5.0};
        cairo_set_dash(cr, pattern, 2, 0.0);
    } else if (dash == 2) {
        const double pattern[] = {2.0, 4.0};
        cairo_set_dash(cr, pattern, 2, 0.0);
    } else {
        cairo_set_dash(cr, NULL, 0, 0.0);
    }
    miime_sketch_line(cr, rng, x1, y1, x2, y2, rough);
    cairo_stroke(cr);
    cairo_set_dash(cr, NULL, 0, 0.0);
}

/* --- rect --- */
void miime_sketch_rect(cairo_t *cr, MiimeSketchRng *rng,
                       double x, double y, double w, double h, int rough) {
    if (!cr) return;
    if (!rough) {
        cairo_rectangle(cr, x, y, w, h);
        return;
    }
    /* four perturbed edges */
    double x2 = x + w, y2 = y + h;
    miime_sketch_line(cr, rng, x, y, x2, y, 1);
    miime_sketch_line(cr, rng, x2, y, x2, y2, 1);
    miime_sketch_line(cr, rng, x2, y2, x, y2, 1);
    miime_sketch_line(cr, rng, x, y2, x, y, 1);
    /* close-ish */
}

void miime_sketch_rounded_rect(cairo_t *cr, MiimeSketchRng *rng,
                               double x, double y, double w, double h,
                               double radius, int rough) {
    if (!cr || w <= 0 || h <= 0) return;
    double r = fmax(0.0, fmin(radius, fmin(w, h) * 0.5));
    if (r < 0.01) {
        miime_sketch_rect(cr, rng, x, y, w, h, rough);
        return;
    }
    /* Keep the four corners tangent and use the sketch line builder only on
       the straight spans, so the fill and stroke share one closed outline. */
    double k = 0.5522847498307936;
    cairo_new_sub_path(cr);
    cairo_move_to(cr, x + r, y);
    cairo_line_to(cr, x + w - r, y);
    cairo_curve_to(cr, x + w - r + k*r, y, x+w, y+r-k*r, x+w, y+r);
    cairo_line_to(cr, x+w, y+h-r);
    cairo_curve_to(cr, x+w, y+h-r+k*r, x+w-r+k*r, y+h, x+w-r, y+h);
    cairo_line_to(cr, x+r, y+h);
    cairo_curve_to(cr, x+r-k*r, y+h, x, y+h-r+k*r, x, y+h-r);
    cairo_line_to(cr, x, y+r);
    cairo_curve_to(cr, x, y+r-k*r, x+r-k*r, y, x+r, y);
    cairo_close_path(cr);
}

/* --- ellipse --- */
void miime_sketch_ellipse(cairo_t *cr, MiimeSketchRng *rng,
                          double cx, double cy, double rx, double ry, int rough) {
    if (!cr) return;
    if (rx < 0.5 || ry < 0.5) return;
    if (!rough) {
        cairo_save(cr);
        cairo_translate(cr, cx, cy);
        cairo_scale(cr, rx, ry);
        cairo_arc(cr, 0, 0, 1.0, 0, 2 * M_PI);
        cairo_restore(cr);
        return;
    }
    double overlap = miime_sketch_rand(rng, 0.25, 0.55);
    double span = 2 * M_PI + overlap;
    int n = 48;
    double phase = miime_sketch_rand(rng, 0, 2 * M_PI);
    double phase2 = miime_sketch_rand(rng, 0, 2 * M_PI);
    /* collect points */
    double *xs = malloc(n * sizeof(double));
    double *ys = malloc(n * sizeof(double));
    if (!xs || !ys) { free(xs); free(ys); return; }
    for (int i = 0; i < n; i++) {
        double t = (double)i / (n - 1);
        double ang = t * span;
        double r = 1.0
            + 0.03 * sin(3 * ang + phase)
            + 0.02 * cos(5 * ang + phase2)
            + (ang / span) * 0.04;
        xs[i] = cx + rx * r * cos(ang);
        ys[i] = cy + ry * r * sin(ang);
    }
    /* Catmull-Rom -> cubic bezier */
    cairo_move_to(cr, xs[0], ys[0]);
    for (int i = 0; i < n - 1; i++) {
        int i0 = (i == 0) ? 0 : i - 1;
        int i1 = i;
        int i2 = i + 1;
        int i3 = (i + 2 < n) ? i + 2 : n - 1;
        double cp1x = xs[i1] + (xs[i2] - xs[i0]) / 6.0;
        double cp1y = ys[i1] + (ys[i2] - ys[i0]) / 6.0;
        double cp2x = xs[i2] - (xs[i3] - xs[i1]) / 6.0;
        double cp2y = ys[i2] - (ys[i3] - ys[i1]) / 6.0;
        cairo_curve_to(cr, cp1x, cp1y, cp2x, cp2y, xs[i2], ys[i2]);
    }
    free(xs); free(ys);
}

/* --- arrow --- */
void miime_sketch_arrow(cairo_t *cr, MiimeSketchRng *rng,
                        double x1, double y1, double x2, double y2, int rough) {
    if (!cr) return;
    double angle = atan2(y2 - y1, x2 - x1);
    /* shaft */
    miime_sketch_line(cr, rng, x1, y1, x2, y2, rough);
    /* wings */
    double angA = miime_sketch_rand(rng, 23.0, 33.0) * M_PI / 180.0;
    double angB = miime_sketch_rand(rng, 23.0, 33.0) * M_PI / 180.0;
    double lenA = miime_sketch_rand(rng, 12.0, 16.0);
    double lenB = miime_sketch_rand(rng, 12.0, 16.0);
    /* asymmetric: one side uses + angle, other - angle with independent magnitudes */
    double w1x = x2 - lenA * cos(angle - angA);
    double w1y = y2 - lenA * sin(angle - angA);
    double w2x = x2 - lenB * cos(angle + angB);
    double w2y = y2 - lenB * sin(angle + angB);

    /* wing as quadratic bezier bowed 1.5 */
    if (!rough) {
        cairo_move_to(cr, x2, y2);
        cairo_line_to(cr, w1x, w1y);
        cairo_move_to(cr, x2, y2);
        cairo_line_to(cr, w2x, w2y);
        return;
    }
    /* first wing */
    {
        double mx = (x2 + w1x) * 0.5, my = (y2 + w1y) * 0.5;
        double px = -sin(angle), py = cos(angle);
        double bx = mx + px * 1.5, by = my + py * 1.5;
        cairo_move_to(cr, x2, y2);
        cairo_curve_to(cr, bx, by, bx, by, w1x, w1y);
    }
    /* second wing */
    {
        double mx = (x2 + w2x) * 0.5, my = (y2 + w2y) * 0.5;
        double px = sin(angle), py = -cos(angle);
        double bx = mx + px * 1.5, by = my + py * 1.5;
        cairo_move_to(cr, x2, y2);
        cairo_curve_to(cr, bx, by, bx, by, w2x, w2y);
    }
}

/* --- hachure --- */
void miime_sketch_hachure(cairo_t *cr, MiimeSketchRng *rng,
                          double x, double y, double w, double h) {
    if (!cr || w <= 0 || h <= 0) return;
    double angle = -41.0 * M_PI / 180.0;
    double gap = 8.0;
    double ca = cos(angle), sa = sin(angle);
    /* transform to rotated space: project corners */
    double minv = 1e18, maxv = -1e18;
    /* Keep perimeter order clockwise; the former TL,TR,BL,BR order
       crossed the scan polygon and could leave diagonal fill gaps. */
    double corners[4][2] = {{x,y},{x+w,y},{x+w,y+h},{x,y+h}};
    for (int i = 0; i < 4; i++) {
        double v = corners[i][0] * ca + corners[i][1] * sa; /* not exact but ok */
        if (v < minv) minv = v;
        if (v > maxv) maxv = v;
    }
    /* perpendicular axis for scan offset */
    double perp_x = -sa, perp_y = ca;
    for (double off = minv; off <= maxv; off += gap) {
        int first = 1;
        double tmin2=0, tmax2=0;
        for (int e = 0; e < 4; e++) {
            double ax = corners[e][0], ay = corners[e][1];
            double bx = corners[(e+1)%4][0], by = corners[(e+1)%4][1];
            double va = ax*ca + ay*sa;
            double vb = bx*ca + by*sa;
            if ((va <= off && vb >= off) || (vb <= off && va >= off)) {
                double t;
                if (fabs(vb - va) > 1e-12) {
                    double u = (off - va) / (vb - va);
                    double ix = ax + u*(bx-ax);
                    double iy = ay + u*(by-ay);
                    t = ix*perp_x + iy*perp_y;
                } else {
                    t = ax*perp_x + ay*perp_y;
                }
                if (first) { tmin2 = tmax2 = t; first = 0; }
                else { if (t < tmin2) tmin2 = t; if (t > tmax2) tmax2 = t; }
            }
        }
        if (first) continue;
        double inset = miime_sketch_rand(rng, 2.0, 6.0);
        double total = tmax2 - tmin2;
        if (total <= 2*inset) continue;
        tmin2 += inset; tmax2 -= inset;
        double p1x = off * ca + tmin2 * perp_x;
        double p1y = off * sa + tmin2 * perp_y;
        double p2x = off * ca + tmax2 * perp_x;
        double p2y = off * sa + tmax2 * perp_y;
        /* zig-zag: alternate direction handled by order; just line */
        cairo_move_to(cr, p1x, p1y);
        cairo_line_to(cr, p2x, p2y);
        cairo_stroke(cr);
    }
}

/* --- variable width stroke --- */
void miime_sketch_var_stroke(cairo_t *cr, MiimeSketchRng *rng,
                             const double *pts, int count, double base_width, int rough) {
    if (!cr || !pts || count < 4) return;
    int n = count / 2;
    if (n < 2) return;
    if (!rough) {
        /* simple stroke */
        cairo_move_to(cr, pts[0], pts[1]);
        for (int i = 1; i < n; i++) cairo_line_to(cr, pts[i*2], pts[i*2+1]);
        cairo_set_line_width(cr, base_width);
        cairo_stroke(cr);
        return;
    }
    /* build left/right polygons */
    /* compute normals and widths */
    /* We'll create arrays of left and right points */
    double *lx = malloc(n * sizeof(double));
    double *ly = malloc(n * sizeof(double));
    double *rx = malloc(n * sizeof(double));
    double *ry = malloc(n * sizeof(double));
    if (!lx||!ly||!rx||!ry) { free(lx);free(ly);free(rx);free(ry); return; }

    for (int i = 0; i < n; i++) {
        double t = (n > 1) ? (double)i / (double)(n - 1) : 0.5;
        /* pressure profile */
        double w = base_width * (1.0 - 0.25 * sin(M_PI * t)) *
                   (t < 0.125 ? 8*t : 1.0) * ((1-t) < 0.125 ? 8*(1-t) : 1.0);
        if (w < 0.5) w = 0.5;
        /* tangent */
        double tx, ty;
        if (i == 0) { tx = pts[2]-pts[0]; ty = pts[3]-pts[1]; }
        else if (i == n-1) { tx = pts[i*2]-pts[(i-1)*2]; ty = pts[i*2+1]-pts[(i-1)*2+1]; }
        else { tx = pts[(i+1)*2]-pts[(i-1)*2]; ty = pts[(i+1)*2+1]-pts[(i-1)*2+1]; }
        double tlen = hypot(tx, ty);
        if (tlen < 1e-9) { tx = 1; ty = 0; tlen = 1; }
        tx /= tlen; ty /= tlen;
        double nx = -ty, ny = tx;
        double half = w * 0.5;
        double j = miime_sketch_rand(rng, -0.5, 0.5);
        lx[i] = pts[i*2] + nx * (half + j);
        ly[i] = pts[i*2+1] + ny * (half + j);
        rx[i] = pts[i*2] - nx * (half + j);
        ry[i] = pts[i*2+1] - ny * (half + j);
    }

    cairo_move_to(cr, lx[0], ly[0]);
    for (int i = 1; i < n; i++) cairo_line_to(cr, lx[i], ly[i]);
    for (int i = n - 1; i >= 0; i--) cairo_line_to(cr, rx[i], ry[i]);
    cairo_close_path(cr);
    cairo_fill(cr);

    free(lx); free(ly); free(rx); free(ry);
}

void miime_sketch_var_stroke_taper(cairo_t *cr, MiimeSketchRng *rng,
                                   const double *pts, int count,
                                   double start_width, double end_width, int rough) {
    if (!cr || !pts || count < 4 || start_width <= 0 || end_width <= 0) return;
    int n = count / 2;
    double *lx = malloc((size_t)n * sizeof(double));
    double *ly = malloc((size_t)n * sizeof(double));
    double *rx = malloc((size_t)n * sizeof(double));
    double *ry = malloc((size_t)n * sizeof(double));
    double *distance = malloc((size_t)n * sizeof(double));
    if (!lx || !ly || !rx || !ry || !distance) {
        free(lx); free(ly); free(rx); free(ry); free(distance); return;
    }
    distance[0] = 0.0;
    for (int i = 1; i < n; i++) {
        distance[i] = distance[i - 1] + hypot(pts[i * 2] - pts[(i - 1) * 2],
                                              pts[i * 2 + 1] - pts[(i - 1) * 2 + 1]);
    }
    double total_distance = distance[n - 1];
    for (int i = 0; i < n; i++) {
        double tx, ty;
        if (i == 0) { tx = pts[2] - pts[0]; ty = pts[3] - pts[1]; }
        else if (i == n - 1) { tx = pts[i * 2] - pts[(i - 1) * 2]; ty = pts[i * 2 + 1] - pts[(i - 1) * 2 + 1]; }
        else { tx = pts[(i + 1) * 2] - pts[(i - 1) * 2]; ty = pts[(i + 1) * 2 + 1] - pts[(i - 1) * 2 + 1]; }
        double len = hypot(tx, ty);
        if (len < 1e-9) { tx = 1; ty = 0; len = 1; }
        double t = total_distance > 1e-9 ? distance[i] / total_distance : (double)i / (double)(n - 1);
        double half = (start_width + (end_width - start_width) * t) * 0.5;
        double jitter = rough && rng ? miime_sketch_rand(rng, -0.25, 0.25) : 0.0;
        double nx = -ty / len, ny = tx / len;
        lx[i] = pts[i * 2] + nx * (half + jitter);
        ly[i] = pts[i * 2 + 1] + ny * (half + jitter);
        rx[i] = pts[i * 2] - nx * (half + jitter);
        ry[i] = pts[i * 2 + 1] - ny * (half + jitter);
    }
    if (cairo_get_line_cap(cr) == CAIRO_LINE_CAP_SQUARE) {
        double start_tx = pts[2] - pts[0], start_ty = pts[3] - pts[1];
        double start_len = hypot(start_tx, start_ty);
        if (start_len > 1e-9) {
            start_tx = start_tx / start_len * start_width * 0.5;
            start_ty = start_ty / start_len * start_width * 0.5;
            lx[0] -= start_tx; ly[0] -= start_ty;
            rx[0] -= start_tx; ry[0] -= start_ty;
        }
        double end_tx = pts[(n - 1) * 2] - pts[(n - 2) * 2];
        double end_ty = pts[(n - 1) * 2 + 1] - pts[(n - 2) * 2 + 1];
        double end_len = hypot(end_tx, end_ty);
        if (end_len > 1e-9) {
            end_tx = end_tx / end_len * end_width * 0.5;
            end_ty = end_ty / end_len * end_width * 0.5;
            lx[n - 1] += end_tx; ly[n - 1] += end_ty;
            rx[n - 1] += end_tx; ry[n - 1] += end_ty;
        }
    }
    cairo_move_to(cr, lx[0], ly[0]);
    for (int i = 1; i < n; i++) cairo_line_to(cr, lx[i], ly[i]);
    for (int i = n - 1; i >= 0; i--) cairo_line_to(cr, rx[i], ry[i]);
    cairo_close_path(cr);
    cairo_fill(cr);
    free(lx); free(ly); free(rx); free(ry); free(distance);
}
