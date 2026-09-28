#ifndef MIIME_ANNOTATE_H
#define MIIME_ANNOTATE_H

#include <glib.h>

#define MIIME_MAX_POINTS 4096  /* max points in a freehand stroke */

typedef enum {
    MIIME_ANNOT_PEN,
    MIIME_ANNOT_LINE,
    MIIME_ANNOT_RECT,
    MIIME_ANNOT_ELLIPSE,
    MIIME_ANNOT_ARROW,
    MIIME_ANNOT_TEXT,
    MIIME_ANNOT_BLUR,
    MIIME_ANNOT_CROP
} MiimeAnnotationType;

typedef struct {
    MiimeAnnotationType type;
    double points[MIIME_MAX_POINTS]; /* x0,y0,x1,y1,... in image coordinates */
    int    point_count;              /* number of doubles used (pairs * 2) */
    double color_r, color_g, color_b, color_a;
    double stroke_width;
    char   text[256];                /* for MIIME_ANNOT_TEXT */
    double font_size;                /* for MIIME_ANNOT_TEXT */
    /* For MIIME_ANNOT_BLUR: points[0..3] = x,y,w,h bounding box */
    unsigned char *blur_data;        /* raster patch for blur (malloc'd, may be NULL) */
    int blur_width, blur_height, blur_stride;
} MiimeAnnotation;

typedef struct MiimeAnnotationStack MiimeAnnotationStack;

MiimeAnnotationStack *miime_annot_stack_new(void);
void miime_annot_stack_free(MiimeAnnotationStack *s);
void miime_annot_stack_push(MiimeAnnotationStack *s, MiimeAnnotation annot);
gboolean miime_annot_stack_undo(MiimeAnnotationStack *s);
gboolean miime_annot_stack_redo(MiimeAnnotationStack *s);
int miime_annot_stack_count(const MiimeAnnotationStack *s);
const MiimeAnnotation *miime_annot_stack_get(const MiimeAnnotationStack *s, int index);
void miime_annot_stack_clear(MiimeAnnotationStack *s);

#endif /* MIIME_ANNOTATE_H */
