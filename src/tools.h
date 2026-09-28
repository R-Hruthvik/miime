#ifndef MIIME_TOOLS_H
#define MIIME_TOOLS_H

#include "annotate.h"

typedef enum {
    MIIME_TOOL_PEN,
    MIIME_TOOL_HIGHLIGHTER,
    MIIME_TOOL_LINE,
    MIIME_TOOL_RECT,
    MIIME_TOOL_ELLIPSE,
    MIIME_TOOL_ARROW,
    MIIME_TOOL_TEXT,
    MIIME_TOOL_BLUR,
    MIIME_TOOL_CROP
} MiimeToolType;

typedef struct MiimeToolState MiimeToolState;

MiimeToolState *miime_tool_state_new(void);
void miime_tool_state_free(MiimeToolState *ts);

void miime_tool_set_type(MiimeToolState *ts, MiimeToolType type);
MiimeToolType miime_tool_get_type(const MiimeToolState *ts);
void miime_tool_set_color(MiimeToolState *ts, double r, double g, double b, double a);
void miime_tool_set_stroke_width(MiimeToolState *ts, double w);

/* Mouse event handlers. Coordinates are in image space (not widget space). */
void miime_tool_on_press(MiimeToolState *ts, double x, double y,
                         MiimeAnnotationStack *stack);
void miime_tool_on_drag(MiimeToolState *ts, double x, double y);
void miime_tool_on_release(MiimeToolState *ts, double x, double y,
                           MiimeAnnotationStack *stack);

/* Get the in-progress annotation for live preview (NULL if idle). */
const MiimeAnnotation *miime_tool_get_preview(const MiimeToolState *ts);

#endif /* MIIME_TOOLS_H */
