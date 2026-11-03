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
    MIIME_ANNOT_CROP,
    MIIME_ANNOT_HIGHLIGHT_BOX,
    MIIME_ANNOT_SPACING_RULER,
    MIIME_ANNOT_ALIGNMENT_REDLINE,
    MIIME_ANNOT_CALLOUT,
    MIIME_ANNOT_PIN,
    MIIME_ANNOT_FREEHAND,
    MIIME_ANNOT_CIRCLE,
    MIIME_ANNOT_BRACKET,
    MIIME_ANNOT_UNDERLINE,
    MIIME_ANNOT_STRIKE
} MiimeAnnotationType;

typedef enum {
    MIIME_AESTHETIC_ROUGH_SKETCH = 0,
    MIIME_AESTHETIC_CLEAN_TECHNICAL
} MiimeAestheticStyle;

typedef enum {
    MIIME_SOURCE_HUMAN = 0,
    MIIME_SOURCE_AGENT
} MiimeAnnotationSource;

typedef enum {
    MIIME_INK_THIN = 0,
    MIIME_INK_REGULAR,
    MIIME_INK_BOLD,
    /* Marker ink always renders under other annotations, with a square cap. */
    MIIME_INK_MARKER
} MiimeInk;

typedef enum { MIIME_DRAW_TECHNICAL, MIIME_DRAW_SKETCH, MIIME_DRAW_EDITORIAL, MIIME_DRAW_MINIMAL } MiimeDrawingStyle;
typedef enum { MIIME_THEME_AUTO, MIIME_THEME_LIGHT, MIIME_THEME_DARK, MIIME_THEME_WARM, MIIME_THEME_NEUTRAL } MiimeTheme;
typedef enum { MIIME_ROLE_NONE, MIIME_ROLE_ERROR, MIIME_ROLE_WARNING, MIIME_ROLE_INFO } MiimeSemanticRole;
typedef struct {
    MiimeDrawingStyle drawing_style;
    MiimeTheme theme;
    guint32 analysis_background_rgba;
    gboolean has_drawing_style, has_theme, has_analysis_background;
    /* Retained request provenance for the MCP/state layer. */
    MiimeAestheticStyle aesthetic_style;
    gboolean has_aesthetic_style, has_reset_render_config, reset_render_config;
} MiimeStyleConfig;

/* Backward-compatible aliases. */
#define MIIME_ANNOT_SOURCE_HUMAN MIIME_SOURCE_HUMAN
#define MIIME_ANNOT_SOURCE_AGENT MIIME_SOURCE_AGENT

typedef struct {
    int layout_mode; /* 0 auto,1 margin_rail,2 in_canvas_floating */
    MiimeAestheticStyle aesthetic_style;
    gboolean spotlight_dimming;
} MiimeCanvasConfig;

typedef struct {
    MiimeAnnotationType type;
    char   json_id[32];          /* stable id for JSON round-trip (may be empty) */
    MiimeAnnotationSource source; /* human (default) or agent */
    double points[MIIME_MAX_POINTS]; /* x0,y0,x1,y1,... in image coordinates */
    int    point_count;              /* number of doubles used (pairs * 2) */
    double color_r, color_g, color_b, color_a;
    MiimeSemanticRole role;
    double label_surface_rgba[4];
    double stroke_width;
    char   text[256];                /* for MIIME_ANNOT_TEXT */
    double font_size;                /* for MIIME_ANNOT_TEXT */
    /* For MIIME_ANNOT_BLUR: points[0..3] = x,y,w,h bounding box */
    unsigned char *blur_data;        /* raster patch for blur (malloc'd, may be NULL) */
    int blur_width, blur_height, blur_stride;
    int    som_id;              /* -1 unset */
    MiimeInk ink;
    int    fill;                /* 0 none, 1 hachure, 2 solid */
    int    dash;                /* 0 solid, 1 dashed, 2 dotted */
    double fill_r, fill_g, fill_b, fill_a;
    double stroke_width_end;    /* taper end; 0 = no taper */
    double radius;
    int    arrow;               /* 0 none, 1 start, 2 end, 3 both */
    int    z;                   /* 0 under, 1 normal, 2 over */
    char   label_text[256];
    int    label_anchor;        /* -1 auto, 0..7 */
    int    label_leader;        /* 0 curved, 1 elbow, 2 none */
    /* Explicit-presence bits preserve meaningful enum/numeric zero values. */
    gboolean has_som_id, has_ink, has_fill, has_dash, has_fill_color;
    gboolean has_stroke_width, has_stroke_width_pair, has_radius, has_arrow, has_z;
    gboolean has_label, has_label_anchor, has_label_leader;
    gboolean has_color, has_font_size, has_role, has_label_surface;
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
