#ifndef MIIME_LAYOUT_H
#define MIIME_LAYOUT_H

#include <stddef.h>

#define MIIME_LAYOUT_MAX_ITEMS 64

typedef struct {
    double x, y, w, h;
} MiimeRect;

typedef struct {
    double target_x, target_y, target_w, target_h;
    char   text[512];
    int    preferred_anchor; /* 0..7, -1 auto */
} MiimeLayoutItem;

typedef struct {
    double label_x, label_y, label_w, label_h;
    double port_label_x, port_label_y;
    double port_target_x, port_target_y;
    int    curve_type; /* 0 S-curve (cubic), 1 elbow (orthogonal) */
    double cps[8];     /* 4 points: start, cp1, cp2, end (x,y each) */
    int    obstacle_detour;
    int    z_index;
    int    mode;       /* 0 margin_rail, 1 in_canvas */
} MiimeLayoutResult;

/* measure_line returns pixel width of a single-line string. */
typedef double (*MiimeMeasureLineFn)(const char *text, void *userdata);

int  miime_layout_run(int img_w, int img_h,
                      MiimeLayoutItem *items, int n,
                      int layout_mode,   /* 0 auto, 1 margin_rail, 2 in_canvas */
                      int aesthetic,     /* 0 rough_sketch, 1 clean_technical */
                      MiimeMeasureLineFn measure_line, void *userdata,
                      MiimeLayoutResult *out);

int  miime_layout_mode_select(int img_w, int img_h, int n, const MiimeLayoutItem *items);

#endif /* MIIME_LAYOUT_H */
