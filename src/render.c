#include "render.h"
#include "freehand.h"
#include "layout.h"
#include <math.h>
#include <string.h>
#include <pango/pangocairo.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MIIME_OPTIONS_LABEL_FONT_FAMILY "DejaVu Sans"

static MiimeAestheticStyle g_aesthetic = MIIME_AESTHETIC_ROUGH_SKETCH;

static char *render_annotations_core(MiimeImage *img, const MiimeAnnotationStack *stack,
                                    int layout_mode, MiimeAestheticStyle aesthetic,
                                    const MiimeRenderOptions *options, gboolean dry_run,
                                    cairo_surface_t **out_surface, int *out_w, int *out_h);

void miime_render_set_aesthetic(MiimeAestheticStyle style) { g_aesthetic = style; }
MiimeAestheticStyle miime_render_get_aesthetic(void) { return g_aesthetic; }

static gboolean annotation_has_fill(const MiimeAnnotation *a) {
    return a->has_fill && a->fill != 0;
}

static void set_annotation_fill_source(cairo_t *cr, const MiimeAnnotation *a) {
    if (a->has_fill_color) {
        cairo_set_source_rgba(cr, a->fill_r, a->fill_g, a->fill_b, a->fill_a);
    } else {
        cairo_set_source_rgba(cr, a->color_r, a->color_g, a->color_b, a->color_a * 0.25);
    }
}

static void render_rect_fill(cairo_t *cr, const MiimeAnnotation *a,
                             MiimeSketchRng *rng, double x, double y,
                             double w, double h, double radius, int rough) {
    if (!annotation_has_fill(a)) return;
    cairo_save(cr);
    miime_sketch_rounded_rect(cr, rng, x, y, w, h, radius, 0);
    cairo_clip(cr);
    set_annotation_fill_source(cr, a);
    if (a->fill == 2) {
        miime_sketch_rounded_rect(cr, rng, x, y, w, h, radius, rough);
        cairo_fill(cr);
    } else if (a->fill == 1) {
        MiimeSketchRng fill_rng = *rng;
        miime_sketch_hachure(cr, &fill_rng, x, y, w, h);
    }
    cairo_restore(cr);
}

static void render_ellipse_fill(cairo_t *cr, const MiimeAnnotation *a,
                                MiimeSketchRng *rng, double cx, double cy,
                                double rx, double ry, int rough) {
    if (!annotation_has_fill(a)) return;
    cairo_save(cr);
    miime_sketch_ellipse(cr, rng, cx, cy, rx, ry, 0);
    cairo_clip(cr);
    set_annotation_fill_source(cr, a);
    if (a->fill == 2) {
        miime_sketch_ellipse(cr, rng, cx, cy, rx, ry, rough);
        cairo_fill(cr);
    } else if (a->fill == 1) {
        MiimeSketchRng fill_rng = *rng;
        miime_sketch_hachure(cr, &fill_rng, cx-rx, cy-ry, 2*rx, 2*ry);
    }
    cairo_restore(cr);
}

/* Callouts inset the bubble 15% on each side, sizing it to 70% of the target.
 * Pick the closest target edge midpoint to the bubble center; equal distances
 * resolve in top, left, right, bottom order. */
static void callout_path(cairo_t *cr, MiimeSketchRng *rng,
                         double x, double y, double w, double h,
                         double radius, int rough) {
    double bubble_x = x + w * 0.15;
    double bubble_y = y + h * 0.15;
    double bubble_w = w * 0.7;
    double bubble_h = h * 0.7;
    double cx = x + w * 0.5, cy = y + h * 0.5;
    double bubble_cy = bubble_y + bubble_h * 0.5;
    double ports[4][2] = {{cx, y}, {x, cy}, {x + w, cy}, {cx, y + h}};
    int nearest = 0;
    double best = HUGE_VAL;
    for (int i = 0; i < 4; i++) {
        double distance = hypot(ports[i][0] - cx, ports[i][1] - bubble_cy);
        if (distance < best) { best = distance; nearest = i; }
    }
    miime_sketch_rounded_rect(cr, rng, bubble_x, bubble_y, bubble_w, bubble_h, radius, rough);
    if (nearest == 0 || nearest == 3) {
        double base_y = nearest == 0 ? bubble_y : bubble_y + bubble_h;
        double half_base = fmin(8.0, bubble_w * 0.3);
        cairo_move_to(cr, cx - half_base, base_y);
        cairo_line_to(cr, ports[nearest][0], ports[nearest][1]);
        cairo_line_to(cr, cx + half_base, base_y);
    } else {
        double base_x = nearest == 1 ? bubble_x : bubble_x + bubble_w;
        double half_base = fmin(8.0, bubble_h * 0.3);
        cairo_move_to(cr, base_x, bubble_cy - half_base);
        cairo_line_to(cr, ports[nearest][0], ports[nearest][1]);
        cairo_line_to(cr, base_x, bubble_cy + half_base);
    }
    cairo_close_path(cr);
}

static void render_callout(cairo_t *cr, const MiimeAnnotation *a,
                           MiimeSketchRng *rng, double x, double y,
                           double w, double h, int rough, double width,
                           double alpha, gboolean legacy_halo) {
    if (w <= 0.0 || h <= 0.0) return;
    double radius = a->has_radius ? a->radius : 12.0;
    int fill = a->has_fill ? a->fill : 2;
    if (fill != 0) {
        cairo_save(cr);
        callout_path(cr, rng, x, y, w, h, radius, 0);
        cairo_clip(cr);
        set_annotation_fill_source(cr, a);
        if (fill == 2) {
            callout_path(cr, rng, x, y, w, h, radius, rough);
            cairo_fill(cr);
        } else if (fill == 1) {
            MiimeSketchRng fill_rng = *rng;
            miime_sketch_hachure(cr, &fill_rng, x, y, w, h);
        }
        cairo_restore(cr);
    }
    if (rough && legacy_halo) {
        cairo_save(cr);
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.85);
        cairo_set_line_width(cr, 5.5);
        callout_path(cr, rng, x, y, w, h, radius, 1);
        cairo_stroke(cr);
        cairo_restore(cr);
        cairo_set_source_rgba(cr, a->color_r, a->color_g, a->color_b, alpha);
        cairo_set_line_width(cr, width);
    }
    callout_path(cr, rng, x, y, w, h, radius, rough);
    if (a->has_dash && a->dash == 1) { double d[] = {8, 5}; cairo_set_dash(cr, d, 2, 0); }
    else if (a->has_dash && a->dash == 2) { double d[] = {2, 4}; cairo_set_dash(cr, d, 2, 0); }
    cairo_stroke(cr);
    cairo_set_dash(cr, NULL, 0, 0);
}

static int pin_number_before(const MiimeAnnotationStack *stack, int index) {
    int number = 1;
    for (int i = 0; stack && i < index; i++) {
        const MiimeAnnotation *earlier = miime_annot_stack_get(stack, i);
        if (earlier && earlier->type == MIIME_ANNOT_PIN) number++;
    }
    return number;
}

static void render_arrow_head(cairo_t *cr, double x, double y, double angle, double width) {
    double len = fmax(9.0, width * 3.5), spread = 0.48;
    cairo_move_to(cr, x, y);
    cairo_line_to(cr, x - len * cos(angle - spread), y - len * sin(angle - spread));
    cairo_move_to(cr, x, y);
    cairo_line_to(cr, x - len * cos(angle + spread), y - len * sin(angle + spread));
}

/* Render a shaft with any supported taper and dash combination. Dashed tapered
 * shafts are split by arc length and each dash uses the interpolated width. */
static void render_styled_shaft(cairo_t *cr, MiimeSketchRng *rng,
                                double x1, double y1, double x2, double y2,
                                int rough, int dash, int taper,
                                double start_width, double end_width) {
    double dx=x2-x1, dy=y2-y1, len=hypot(dx,dy);
    if (len < 1e-9) return;
    if (taper && dash == 0) {
        double pts[]={x1,y1,x2,y2};
        miime_sketch_var_stroke_taper(cr,rng,pts,4,start_width,end_width,rough);
        return;
    }
    if (!taper || dash == 0) {
        cairo_set_line_width(cr,start_width);
        miime_sketch_line_dash(cr,rng,x1,y1,x2,y2,rough,dash);
        return;
    }
    double on=dash==1 ? 8.0 : 2.0, off=dash==1 ? 5.0 : 4.0;
    for (double pos=0.0; pos<len; pos+=on+off) {
        double finish=fmin(len,pos+on), mid=(pos+finish)*0.5/len;
        double width=start_width+(end_width-start_width)*mid;
        cairo_set_line_width(cr,width);
        miime_sketch_line(cr,rng,x1+dx*(pos/len),y1+dy*(pos/len),
                          x1+dx*(finish/len),y1+dy*(finish/len),rough);
        cairo_stroke(cr);
    }
}

static void render_arrow_geometry(cairo_t *cr, MiimeSketchRng *rng,
                                  double x1, double y1, double x2, double y2,
                                  int rough, int heads, int dash, int taper,
                                  double start_width, double end_width) {
    double angle=atan2(y2-y1,x2-x1);
    render_styled_shaft(cr,rng,x1,y1,x2,y2,rough,dash,taper,start_width,end_width);
    if (heads == 1 || heads == 3) {
        cairo_set_line_width(cr,start_width);
        render_arrow_head(cr,x1,y1,angle+M_PI,start_width);
        cairo_stroke(cr);
    }
    if (heads == 2 || heads == 3) {
        cairo_set_line_width(cr,end_width);
        render_arrow_head(cr,x2,y2,angle,end_width);
        cairo_stroke(cr);
    }
}

static void render_one_annotation(cairo_t *cr, const MiimeAnnotation *input, gboolean is_preview,
                                  int pin_number, MiimeAestheticStyle aesthetic,
                                  const MiimeAnnotationStyleResult *appearance, int halo_layer,
                                  gboolean options_aware) {
    MiimeAnnotation styled;
    const MiimeAnnotation *a = input;
    if (!a) return;

    if (appearance) {
        styled = *a;
        styled.color_r=appearance->ink_rgba[0]; styled.color_g=appearance->ink_rgba[1];
        styled.color_b=appearance->ink_rgba[2]; styled.color_a=appearance->ink_rgba[3];
        styled.stroke_width=appearance->stroke_width; styled.has_stroke_width=TRUE;
        styled.stroke_width_end=appearance->stroke_width_end;
        styled.font_size=appearance->font_size; styled.has_font_size=TRUE;
        styled.radius=appearance->radius; styled.has_radius=TRUE;
        if (!styled.has_fill_color) {
            styled.has_fill_color=TRUE;
            styled.fill_r=appearance->fill_rgba[0]; styled.fill_g=appearance->fill_rgba[1];
            styled.fill_b=appearance->fill_rgba[2]; styled.fill_a=appearance->fill_rgba[3];
        }
        if (appearance->has_fill) { styled.fill=2; styled.has_fill=TRUE; }
        if (appearance->has_label_surface)
            memcpy(styled.label_surface_rgba, appearance->label_surface_rgba, sizeof(styled.label_surface_rgba));
        if (halo_layer >= 0) {
            /* Paint the white outer stroke first, then the black inner stroke. */
            static const double neutral[2][4]={{1.0,1.0,1.0,1.0},{0.0,0.0,0.0,1.0}};
            styled.color_r=neutral[halo_layer][0]; styled.color_g=neutral[halo_layer][1];
            styled.color_b=neutral[halo_layer][2]; styled.color_a=neutral[halo_layer][3];
            styled.stroke_width=appearance->stroke_width + appearance->halo_width * (halo_layer == 0 ? 2.0 : 1.0);
            styled.stroke_width_end=styled.stroke_width;
            styled.fill=0; styled.has_fill=FALSE;
        }
        a = &styled;
    }

    gboolean has_ink = a->has_ink && a->ink >= MIIME_INK_THIN && a->ink <= MIIME_INK_MARKER;
    gboolean marker = has_ink && a->ink == MIIME_INK_MARKER;
    static const double ink_widths[] = {1.5, 2.5, 4.0, 14.0};
    double width = a->stroke_width > 0 ? a->stroke_width : 2.5;
    if (has_ink && !(a->has_stroke_width || a->has_stroke_width_pair)) width = ink_widths[a->ink];
    if (a->has_stroke_width || a->has_stroke_width_pair) width = a->stroke_width;
    double alpha = halo_layer >= 0 ? 1.0 : (marker ? a->color_a * 0.35 : a->color_a);
    if (halo_layer >= 0 && appearance) width = a->stroke_width;
    int rough = (aesthetic == MIIME_AESTHETIC_ROUGH_SKETCH) && !marker;

    MiimeSketchRng rng;
    char idbuf[64];
    MiimeAnnotationType rng_type = a->type;
    if (a->type == MIIME_ANNOT_HIGHLIGHT_BOX) rng_type = MIIME_ANNOT_RECT;
    else if (a->type == MIIME_ANNOT_SPACING_RULER || a->type == MIIME_ANNOT_ALIGNMENT_REDLINE)
        rng_type = MIIME_ANNOT_LINE;
    if (a->json_id[0]) {
        miime_sketch_rng_init(&rng, a->json_id);
    } else {
        snprintf(idbuf, sizeof(idbuf), "annot-%d-%d", (int)rng_type, (int)(a->points[0]*10));
        miime_sketch_rng_init(&rng, idbuf);
    }

    cairo_set_source_rgba(cr, a->color_r, a->color_g, a->color_b, alpha);
    cairo_set_line_width(cr, width);
    cairo_set_line_cap(cr, marker ? CAIRO_LINE_CAP_SQUARE : CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

    switch (a->type) {
        case MIIME_ANNOT_PEN:
            if (a->point_count < 4) break;
            if (a->has_stroke_width_pair && a->stroke_width_end > 0)
                miime_sketch_var_stroke_taper(cr, &rng, a->points, a->point_count, width, a->stroke_width_end, rough);
            else
                miime_sketch_var_stroke(cr, &rng, a->points, a->point_count, width, rough);
            break;

        case MIIME_ANNOT_FREEHAND:
            if (a->point_count < 4) break;
            miime_freehand_build_path(cr, &rng, a->points, a->point_count, width,
                                      a->has_stroke_width_pair ? a->stroke_width_end : 0.0,
                                      rough);
            break;

        case MIIME_ANNOT_LINE:
        case MIIME_ANNOT_SPACING_RULER:
        case MIIME_ANNOT_ALIGNMENT_REDLINE:
        case MIIME_ANNOT_BRACKET:
        case MIIME_ANNOT_UNDERLINE:
        case MIIME_ANNOT_STRIKE:
            if (a->point_count < 4) break;
            if (a->type == MIIME_ANNOT_UNDERLINE || a->type == MIIME_ANNOT_STRIKE)
                cairo_set_line_cap(cr, CAIRO_LINE_CAP_SQUARE);
            if (rough && !appearance && a->type != MIIME_ANNOT_BRACKET) {
                /* halo */
                cairo_save(cr);
                cairo_set_source_rgba(cr, 1.0,1.0,1.0,0.85);
                cairo_set_line_width(cr, 5.5);
                if (a->type == MIIME_ANNOT_LINE && a->has_arrow && a->arrow != 0)
                    render_arrow_geometry(cr,&rng,a->points[0],a->points[1],a->points[2],a->points[3],
                                          1,a->arrow,a->has_dash?a->dash:0,0,5.5,5.5);
                else
                    miime_sketch_line(cr, &rng, a->points[0], a->points[1], a->points[2], a->points[3], 1);
                cairo_stroke(cr);
                cairo_restore(cr);
                cairo_set_source_rgba(cr, a->color_r, a->color_g, a->color_b, alpha);
                cairo_set_line_width(cr, width);
            }
            int dash = a->has_dash ? a->dash : (a->type == MIIME_ANNOT_ALIGNMENT_REDLINE ? 1 : 0);
            if (a->type == MIIME_ANNOT_BRACKET) {
                double x1=a->points[0], y1=a->points[1], x2=a->points[2], y2=a->points[3];
                double side=(x2 >= x1) ? 1.0 : -1.0;
                double arm=fmin(fabs(x2-x1)*0.35, 10.0);
                cairo_move_to(cr,x1+side*arm,y1); cairo_line_to(cr,x1,y1);
                cairo_line_to(cr,x1,y2); cairo_line_to(cr,x1+side*arm,y2); cairo_stroke(cr);
            } else if (a->type == MIIME_ANNOT_LINE && a->has_arrow && a->arrow != 0) {
                double x1=a->points[0], y1=a->points[1], x2=a->points[2], y2=a->points[3];
                double end_width=(a->has_stroke_width_pair && a->stroke_width_end>0)?a->stroke_width_end:width;
                render_arrow_geometry(cr,&rng,x1,y1,x2,y2,rough,a->arrow,dash,
                                      a->has_stroke_width_pair && a->stroke_width_end>0,
                                      width,end_width);
            } else if (a->type == MIIME_ANNOT_LINE && a->has_stroke_width_pair && a->stroke_width_end > 0) {
                render_styled_shaft(cr,&rng,a->points[0],a->points[1],a->points[2],a->points[3],
                                    rough,dash,1,width,a->stroke_width_end);
            } else {
                miime_sketch_line_dash(cr, &rng, a->points[0], a->points[1], a->points[2], a->points[3], rough, dash);
            }
            if (a->type == MIIME_ANNOT_SPACING_RULER) {
                double dx=a->points[2]-a->points[0], dy=a->points[3]-a->points[1], len=hypot(dx,dy);
                if (len > 0.001) {
                    double nx=-dy/len*5.0, ny=dx/len*5.0;
                    cairo_move_to(cr,a->points[0]-nx,a->points[1]-ny); cairo_line_to(cr,a->points[0]+nx,a->points[1]+ny);
                    cairo_move_to(cr,a->points[2]-nx,a->points[3]-ny); cairo_line_to(cr,a->points[2]+nx,a->points[3]+ny);
                    cairo_stroke(cr);
            if (a->label_text[0]) {
                        PangoLayout *layout=pango_cairo_create_layout(cr);
                        pango_layout_set_text(layout,a->label_text,-1);
                        cairo_move_to(cr,(a->points[0]+a->points[2])/2.0,(a->points[1]+a->points[3])/2.0-14.0);
                        pango_cairo_show_layout(cr,layout); g_object_unref(layout);
                    }
                }
            }
            if (a->type == MIIME_ANNOT_UNDERLINE || a->type == MIIME_ANNOT_STRIKE)
                cairo_set_line_cap(cr, marker ? CAIRO_LINE_CAP_SQUARE : CAIRO_LINE_CAP_ROUND);
            break;

        case MIIME_ANNOT_RECT:
        case MIIME_ANNOT_HIGHLIGHT_BOX: {
            if (a->point_count < 4) break;
            int opposite = a->point_count >= 8 ? 4 : 2;
            double x = (a->points[0] < a->points[opposite]) ? a->points[0] : a->points[opposite];
            double y = (a->points[1] < a->points[opposite + 1]) ? a->points[1] : a->points[opposite + 1];
            double w = fabs(a->points[opposite] - a->points[0]);
            double h = fabs(a->points[opposite + 1] - a->points[1]);
            double radius = (a->has_radius && a->radius > 0.0) ? a->radius : 0.0;
            render_rect_fill(cr, a, &rng, x, y, w, h, radius, rough);
            if (rough && !appearance) {
                cairo_save(cr);
                cairo_set_source_rgba(cr, 1.0,1.0,1.0,0.85);
                cairo_set_line_width(cr, 5.5);
                if (radius > 0.0) miime_sketch_rounded_rect(cr, &rng, x, y, w, h, radius, 1);
                else miime_sketch_rect(cr, &rng, x, y, w, h, 1);
                cairo_stroke(cr);
                cairo_restore(cr);
                cairo_set_source_rgba(cr, a->color_r, a->color_g, a->color_b, alpha);
                cairo_set_line_width(cr, width);
            }
            if (radius > 0.0) miime_sketch_rounded_rect(cr, &rng, x, y, w, h, radius, rough);
            else miime_sketch_rect(cr, &rng, x, y, w, h, rough);
            if (a->has_dash && a->dash == 1) { double d[]={8,5}; cairo_set_dash(cr,d,2,0); }
            else if (a->has_dash && a->dash == 2) { double d[]={2,4}; cairo_set_dash(cr,d,2,0); }
            cairo_stroke(cr);
            cairo_set_dash(cr,NULL,0,0);
            break;
        }

        case MIIME_ANNOT_CALLOUT: {
            if (a->point_count < 4) break;
            int opposite = a->point_count >= 8 ? 4 : 2;
            double x = fmin(a->points[0], a->points[opposite]);
            double y = fmin(a->points[1], a->points[opposite + 1]);
            double w = fabs(a->points[opposite] - a->points[0]);
            double h = fabs(a->points[opposite + 1] - a->points[1]);
            render_callout(cr, a, &rng, x, y, w, h, rough, width, alpha, appearance==NULL);
            break;
        }

        case MIIME_ANNOT_ELLIPSE:
        case MIIME_ANNOT_CIRCLE:
        case MIIME_ANNOT_PIN: {
            if (a->type == MIIME_ANNOT_PIN && a->point_count == 2) {
                double radius = a->radius > 0.0 ? a->radius : 12.0;
                int fill = a->has_fill ? a->fill : 2;
                if (fill != 0) {
                    cairo_save(cr);
                    miime_sketch_ellipse(cr, &rng, a->points[0], a->points[1], radius, radius, 0);
                    cairo_clip(cr);
                    set_annotation_fill_source(cr, a);
                    if (fill == 2) {
                        miime_sketch_ellipse(cr, &rng, a->points[0], a->points[1], radius, radius, rough);
                        cairo_fill(cr);
                    } else if (fill == 1) {
                        MiimeSketchRng fill_rng = rng;
                        miime_sketch_hachure(cr, &fill_rng, a->points[0]-radius,
                                            a->points[1]-radius, 2*radius, 2*radius);
                    }
                    cairo_restore(cr);
                }
                if (rough && !appearance) {
                    cairo_save(cr);
                    cairo_set_source_rgba(cr, 1.0,1.0,1.0,0.85);
                    cairo_set_line_width(cr, 5.5);
                    miime_sketch_ellipse(cr, &rng, a->points[0], a->points[1], radius, radius, 1);
                    cairo_stroke(cr);
                    cairo_restore(cr);
                    cairo_set_source_rgba(cr, a->color_r, a->color_g, a->color_b, alpha);
                    cairo_set_line_width(cr, width);
                }
                miime_sketch_ellipse(cr, &rng, a->points[0], a->points[1], radius, radius, rough);
                cairo_stroke(cr);
                PangoLayout *layout = pango_cairo_create_layout(cr);
                PangoFontDescription *font = pango_font_description_new();
                pango_font_description_set_family(font, options_aware ? MIIME_OPTIONS_LABEL_FONT_FAMILY : "Sans");
                pango_font_description_set_absolute_size(font,
                    (a->font_size > 0.0 ? a->font_size : radius) * PANGO_SCALE);
                pango_layout_set_font_description(layout, font);
                char number[24];
                g_snprintf(number, sizeof(number), "%d", pin_number > 0 ? pin_number : 1);
                pango_layout_set_text(layout, number, -1);
                int text_w = 0, text_h = 0;
                pango_layout_get_pixel_size(layout, &text_w, &text_h);
                cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, alpha);
                cairo_move_to(cr, a->points[0] - text_w / 2.0, a->points[1] - text_h / 2.0);
                pango_cairo_show_layout(cr, layout);
                pango_font_description_free(font);
                g_object_unref(layout);
                break;
            }
            if (a->point_count < 4) break;
            int opposite = a->point_count >= 8 ? 4 : 2;
            double cx = (a->points[0] + a->points[opposite]) / 2.0;
            double cy = (a->points[1] + a->points[opposite + 1]) / 2.0;
            double rx = fabs(a->points[opposite] - a->points[0]) / 2.0;
            double ry = fabs(a->points[opposite + 1] - a->points[1]) / 2.0;
            if (a->type == MIIME_ANNOT_CIRCLE) rx = ry = fmax(rx, ry);
            if (rx < 0.5 || ry < 0.5) break;
            render_ellipse_fill(cr, a, &rng, cx, cy, rx, ry, rough);
            if (rough && !appearance) {
                cairo_save(cr);
                cairo_set_source_rgba(cr, 1.0,1.0,1.0,0.85);
                cairo_set_line_width(cr, 5.5);
                miime_sketch_ellipse(cr, &rng, cx, cy, rx, ry, 1);
                cairo_stroke(cr);
                cairo_restore(cr);
                cairo_set_source_rgba(cr, a->color_r, a->color_g, a->color_b, alpha);
                cairo_set_line_width(cr, width);
            }
            miime_sketch_ellipse(cr, &rng, cx, cy, rx, ry, rough);
            if (a->has_dash && a->dash == 1) { double d[]={8,5}; cairo_set_dash(cr,d,2,0); }
            else if (a->has_dash && a->dash == 2) { double d[]={2,4}; cairo_set_dash(cr,d,2,0); }
            cairo_stroke(cr);
            cairo_set_dash(cr,NULL,0,0);
            break;
        }

        case MIIME_ANNOT_ARROW: {
            if (a->point_count < 4) break;
            double x1 = a->points[0], y1 = a->points[1];
            double x2 = a->points[2], y2 = a->points[3];
            gboolean legacy_arrow = !a->has_arrow && !a->has_dash &&
                                     !(a->has_stroke_width_pair && a->stroke_width_end > 0);
            if (rough && !appearance && legacy_arrow) {
                cairo_save(cr);
                cairo_set_source_rgba(cr, 1.0,1.0,1.0,0.85);
                cairo_set_line_width(cr, 5.5);
                miime_sketch_arrow(cr, &rng, x1, y1, x2, y2, 1);
                cairo_stroke(cr);
                cairo_restore(cr);
                cairo_set_source_rgba(cr, a->color_r, a->color_g, a->color_b, alpha);
                cairo_set_line_width(cr, width);
            } else if (rough && !appearance) {
                int heads=a->has_arrow?a->arrow:2;
                int dash=a->has_dash?a->dash:0;
                gboolean taper=a->has_stroke_width_pair && a->stroke_width_end>0;
                cairo_save(cr);
                cairo_set_source_rgba(cr,1.0,1.0,1.0,0.85);
                render_arrow_geometry(cr,&rng,x1,y1,x2,y2,1,heads,dash,taper,5.5,5.5);
                cairo_restore(cr);
                cairo_set_source_rgba(cr,a->color_r,a->color_g,a->color_b,alpha);
                cairo_set_line_width(cr,width);
            }
            if (legacy_arrow) {
                miime_sketch_arrow(cr, &rng, x1, y1, x2, y2, rough);
                cairo_stroke(cr);
            } else {
                int heads=a->has_arrow?a->arrow:2;
                int dash=a->has_dash?a->dash:0;
                gboolean taper=a->has_stroke_width_pair && a->stroke_width_end>0;
                double end_width=taper?a->stroke_width_end:width;
                render_arrow_geometry(cr,&rng,x1,y1,x2,y2,rough,heads,dash,taper,width,end_width);
            }
            break;
        }

        case MIIME_ANNOT_TEXT: {
            double size = (a->font_size > 0.0) ? a->font_size : 18.0;
            PangoLayout *layout = pango_cairo_create_layout(cr);
            PangoFontDescription *font;
            if (appearance || options_aware) {
                font=pango_font_description_new();
                pango_font_description_set_family(font,MIIME_OPTIONS_LABEL_FONT_FAMILY);
                pango_font_description_set_absolute_size(font,size*PANGO_SCALE);
            } else {
                char font_desc_str[64];
                snprintf(font_desc_str,sizeof(font_desc_str),"Sans %.0f",size);
                font=pango_font_description_from_string(font_desc_str);
            }
            pango_layout_set_font_description(layout, font);
            pango_layout_set_text(layout, a->text, -1);

            if (a->text[0] != '\0') {
                cairo_move_to(cr, a->points[0], a->points[1]);
                cairo_set_source_rgba(cr, a->color_r, a->color_g, a->color_b, a->color_a);
                pango_cairo_show_layout(cr, layout);
            }

            if (is_preview) {
                int text_w = 0, text_h = 0;
                pango_layout_get_pixel_size(layout, &text_w, &text_h);
                if (text_h <= 0) text_h = (int)(size * 1.3);
                if (text_h < 12) text_h = 12;

                /* Draw dashed text bounding box */
                cairo_save(cr);
                double dashes[] = {3.0, 3.0};
                cairo_set_dash(cr, dashes, 2, 0.0);
                cairo_set_source_rgba(cr, a->color_r, a->color_g, a->color_b, 0.7);
                cairo_set_line_width(cr, 1.0);
                int box_w = (text_w > 0) ? text_w + 4 : (int)(size * 0.8);
                cairo_rectangle(cr, a->points[0] - 2, a->points[1] - 2, box_w + 4, text_h + 4);
                cairo_stroke(cr);

                /* Draw insertion cursor caret */
                cairo_set_dash(cr, NULL, 0, 0.0);
                cairo_set_source_rgba(cr, a->color_r, a->color_g, a->color_b, 1.0);
                cairo_set_line_width(cr, 2.0);
                double caret_x = a->points[0] + text_w;
                cairo_move_to(cr, caret_x, a->points[1]);
                cairo_line_to(cr, caret_x, a->points[1] + text_h);
                cairo_stroke(cr);
                cairo_restore(cr);
            }

            pango_font_description_free(font);
            g_object_unref(layout);
            break;
        }

        case MIIME_ANNOT_BLUR: {
            if (a->blur_data && a->blur_width >= 1 && a->blur_height >= 1) {
                cairo_surface_t *patch = cairo_image_surface_create_for_data(
                    a->blur_data, CAIRO_FORMAT_ARGB32,
                    a->blur_width, a->blur_height, a->blur_stride);
                cairo_set_source_surface(cr, patch, a->points[0], a->points[1]);
                cairo_paint(cr);
                cairo_surface_destroy(patch);
            } else if (a->point_count >= 4) {
                double x = (a->points[0] < a->points[2]) ? a->points[0] : a->points[2];
                double y = (a->points[1] < a->points[3]) ? a->points[1] : a->points[3];
                double w = fabs(a->points[2] - a->points[0]);
                double h = fabs(a->points[3] - a->points[1]);
                cairo_set_source_rgba(cr, 0.3, 0.6, 1.0, 0.3);
                cairo_rectangle(cr, x, y, w, h);
                cairo_fill(cr);
                cairo_set_source_rgba(cr, 0.3, 0.6, 1.0, 0.8);
                cairo_set_line_width(cr, 1.5);
                cairo_rectangle(cr, x, y, w, h);
                cairo_stroke(cr);
            }
            break;
        }

        case MIIME_ANNOT_CROP: {
            if (a->point_count < 4) break;
            double x = (a->points[0] < a->points[2]) ? a->points[0] : a->points[2];
            double y = (a->points[1] < a->points[3]) ? a->points[1] : a->points[3];
            double w = fabs(a->points[2] - a->points[0]);
            double h = fabs(a->points[3] - a->points[1]);
            if (w < 1 || h < 1) break;

            cairo_save(cr);
            double dashes[] = {6.0, 4.0};
            cairo_set_dash(cr, dashes, 2, 0.0);
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.9);
            cairo_set_line_width(cr, 2.0);
            cairo_rectangle(cr, x, y, w, h);
            cairo_stroke(cr);

            cairo_set_dash(cr, dashes, 2, 5.0);
            cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.9);
            cairo_rectangle(cr, x, y, w, h);
            cairo_stroke(cr);
            cairo_restore(cr);
            break;
        }
    }
}

static int annotation_z(const MiimeAnnotation *a) {
    if (!a) return 1;
    if (a->has_ink && a->ink == MIIME_INK_MARKER) return 0;
    if (a->has_z && a->z >= 0 && a->z <= 2) return a->z;
    return 1;
}

static void render_annotation_pass(cairo_t *cr, const MiimeAnnotationStack *stack,
                                   const MiimeAnnotation *preview, int z) {
    if (stack) {
        int count = miime_annot_stack_count(stack);
        for (int i = 0; i < count; i++) {
            const MiimeAnnotation *a = miime_annot_stack_get(stack, i);
            if (annotation_z(a) == z) render_one_annotation(cr, a, FALSE, pin_number_before(stack, i), g_aesthetic, NULL, -1, FALSE);
        }
    }
    if (preview && annotation_z(preview) == z)
        render_one_annotation(cr, preview, TRUE,
                              pin_number_before(stack, stack ? miime_annot_stack_count(stack) : 0), g_aesthetic, NULL, -1, FALSE);
}

void miime_render_annotations(cairo_t *cr,
                              const MiimeAnnotationStack *stack,
                              const MiimeAnnotation *preview) {
    if (!cr) return;
    gboolean has_z_order = FALSE;
    if (stack) {
        int count = miime_annot_stack_count(stack);
        for (int i = 0; i < count; i++) {
            const MiimeAnnotation *a = miime_annot_stack_get(stack, i);
            if (a && (a->has_z || (a->has_ink && a->ink == MIIME_INK_MARKER))) { has_z_order = TRUE; break; }
        }
    }
    if (preview && (preview->has_z || (preview->has_ink && preview->ink == MIIME_INK_MARKER))) has_z_order = TRUE;
    if (!has_z_order) {
        if (stack) {
            int count = miime_annot_stack_count(stack);
            for (int i = 0; i < count; i++)
                render_one_annotation(cr, miime_annot_stack_get(stack, i), FALSE,
                                      pin_number_before(stack, i), g_aesthetic, NULL, -1, FALSE);
        }
        if (preview) render_one_annotation(cr, preview, TRUE,
                                           pin_number_before(stack, stack ? miime_annot_stack_count(stack) : 0), g_aesthetic, NULL, -1, FALSE);
        return;
    }
    for (int z = 0; z <= 2; z++) render_annotation_pass(cr, stack, preview, z);
}

void miime_render_on_screen(cairo_t *cr,
                            MiimeImage *img,
                            const MiimeAnnotationStack *stack,
                            const MiimeAnnotation *preview,
                            double zoom,
                            double pan_x, double pan_y,
                            int widget_w, int widget_h) {
    (void)widget_w;
    (void)widget_h;

    if (!cr || !img) return;

    cairo_surface_t *surface = miime_image_get_surface(img);
    if (!surface) return;

    /* Clear background to dark gray */
    cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
    cairo_paint(cr);

    /* Apply viewport transform */
    cairo_save(cr);
    cairo_translate(cr, pan_x, pan_y);
    cairo_scale(cr, zoom, zoom);

    cairo_set_source_surface(cr, surface, 0, 0);

    /* Use appropriate filtering */
    if (zoom > 2.0) {
        cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST);
    } else {
        cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BILINEAR);
    }

    cairo_paint(cr);

    /* Render annotations on top of the image */
    miime_render_annotations(cr, stack, preview);

    cairo_restore(cr);
}

static MiimeAestheticStyle drawing_aesthetic(MiimeDrawingStyle style) {
    return style == MIIME_DRAW_SKETCH ? MIIME_AESTHETIC_ROUGH_SKETCH : MIIME_AESTHETIC_CLEAN_TECHNICAL;
}

static double resolved_style_font(MiimeDrawingStyle style) {
    static const double sizes[]={18.0,18.0,20.0,16.0};
    return style>=MIIME_DRAW_TECHNICAL && style<=MIIME_DRAW_MINIMAL ? sizes[style] : 18.0;
}

static const MiimeAnnotationStyleResult *resolved_annotation(const MiimeStyleResolution *resolution, int index) {
    if (!resolution || index < 0 || (guint)index >= resolution->annotation_count) return NULL;
    return &resolution->annotations[index];
}

static void render_options_annotation(cairo_t *cr, const MiimeAnnotation *a, int index,
                                      int pin_number, MiimeAestheticStyle aesthetic,
                                      const MiimeStyleResolution *resolution) {
    const MiimeAnnotationStyleResult *appearance = resolved_annotation(resolution, index);
    if (appearance && appearance->halo_applied) {
        render_one_annotation(cr, a, FALSE, pin_number, aesthetic, appearance, 0, TRUE);
        render_one_annotation(cr, a, FALSE, pin_number, aesthetic, appearance, 1, TRUE);
    }
    render_one_annotation(cr, a, FALSE, pin_number, aesthetic, appearance, -1, TRUE);
}

static gboolean get_render_crop(const MiimeAnnotationStack *stack, int img_w, int img_h,
                                MiimeCrop *crop) {
    crop->x=0; crop->y=0; crop->width=img_w; crop->height=img_h; crop->enabled=FALSE;
    int count=stack?miime_annot_stack_count(stack):0;
    for (int i=count-1;i>=0;i--) {
        const MiimeAnnotation *a=miime_annot_stack_get(stack,i);
        if (!a || a->type!=MIIME_ANNOT_CROP || a->point_count<4) continue;
        double x=fmin(a->points[0],a->points[2]), y=fmin(a->points[1],a->points[3]);
        double w=fabs(a->points[2]-a->points[0]), h=fabs(a->points[3]-a->points[1]);
        if (w<1 || h<1) continue;
        if (x<0) { w+=x; x=0; } if (y<0) { h+=y; y=0; }
        if (x+w>img_w) w=img_w-x;
        if (y+h>img_h) h=img_h-y;
        if (w<1) w=1;
        if (h<1) h=1;
        crop->x=(int)x; crop->y=(int)y; crop->width=(int)w; crop->height=(int)h; crop->enabled=TRUE;
        return TRUE;
    }
    return FALSE;
}

cairo_surface_t *miime_render_export_with_options(MiimeImage *img,
                                     const MiimeAnnotationStack *stack,
                                     const MiimeRenderOptions *options,
                                     int *out_w, int *out_h) {
    if (!options) return miime_render_export(img, stack, out_w, out_h);
    if (!img) return NULL;
    int label_count=0, stack_count=stack?miime_annot_stack_count(stack):0;
    for(int i=0;i<stack_count;i++) {
        const MiimeAnnotation *a=miime_annot_stack_get(stack,i);
        if(a&&a->label_text[0]) label_count++;
    }
    if(label_count) {
        cairo_surface_t *laid_out=NULL;
        char *json=render_annotations_core(img,stack,0,MIIME_AESTHETIC_CLEAN_TECHNICAL,
                                           options,FALSE,&laid_out,out_w,out_h);
        g_free(json);
        return laid_out;
    }
    int img_w=miime_image_width(img), img_h=miime_image_height(img);
    if (img_w<=0 || img_h<=0) return NULL;
    MiimeCrop crop; get_render_crop(stack,img_w,img_h,&crop);
    const MiimeStyleResolution *resolution=options->resolution;
    MiimeStyleResolution *owned=NULL;
    if (!resolution) {
        owned=miime_style_resolve(miime_image_get_surface(img),&options->requested,stack,crop);
        resolution=owned;
    }
    int width=crop.width, height=crop.height;
    cairo_surface_t *surface=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,width,height);
    cairo_t *cr=cairo_create(surface);
    cairo_translate(cr,-crop.x,-crop.y);
    cairo_set_source_surface(cr,miime_image_get_surface(img),0,0); cairo_paint(cr);
    int count=stack?miime_annot_stack_count(stack):0;
    gboolean ordered=FALSE;
    for(int i=0;i<count;i++) { const MiimeAnnotation *a=miime_annot_stack_get(stack,i); if(a&&(a->has_z||(a->has_ink&&a->ink==MIIME_INK_MARKER))) { ordered=TRUE; break; } }
    MiimeAestheticStyle aesthetic=drawing_aesthetic(resolution->resolved_drawing_style);
    if (ordered) {
        for(int z=0;z<=2;z++) for(int i=0;i<count;i++) {
            const MiimeAnnotation *a=miime_annot_stack_get(stack,i);
            if(a&&a->type!=MIIME_ANNOT_CROP&&annotation_z(a)==z)
                render_options_annotation(cr,a,i,pin_number_before(stack,i),aesthetic,resolution);
        }
    } else for(int i=0;i<count;i++) {
        const MiimeAnnotation *a=miime_annot_stack_get(stack,i);
        if(a&&a->type!=MIIME_ANNOT_CROP)
            render_options_annotation(cr,a,i,pin_number_before(stack,i),aesthetic,resolution);
    }
    cairo_destroy(cr);
    if(out_w)*out_w=width;
    if(out_h)*out_h=height;
    miime_style_resolution_free(owned);
    return surface;
}

cairo_surface_t *miime_render_export_with_config(MiimeImage *img,
                                     const MiimeAnnotationStack *stack,
                                     const MiimeStyleConfig *config,
                                     int *out_w,int *out_h) {
    if(config&&(config->has_drawing_style||config->has_theme||config->has_analysis_background)) {
        MiimeRenderOptions options={.requested=*config,.resolution=NULL};
        return miime_render_export_with_options(img,stack,&options,out_w,out_h);
    }
    if(config&&config->has_aesthetic_style) {
        MiimeAestheticStyle old=miime_render_get_aesthetic();
        miime_render_set_aesthetic(config->aesthetic_style==MIIME_AESTHETIC_CLEAN_TECHNICAL
                                   ?MIIME_AESTHETIC_CLEAN_TECHNICAL:MIIME_AESTHETIC_ROUGH_SKETCH);
        cairo_surface_t *surface=miime_render_export(img,stack,out_w,out_h);
        miime_render_set_aesthetic(old);
        return surface;
    }
    return miime_render_export(img,stack,out_w,out_h);
}

cairo_surface_t *miime_render_export(MiimeImage *img,
                                     const MiimeAnnotationStack *stack,
                                     int *out_w, int *out_h) {
    if (!img) return NULL;

    int img_w = miime_image_width(img);
    int img_h = miime_image_height(img);
    if (img_w <= 0 || img_h <= 0) return NULL;

    /* Look for latest crop annotation on stack */
    gboolean has_crop = FALSE;
    double crop_x = 0, crop_y = 0, crop_w = img_w, crop_h = img_h;

    if (stack) {
        int count = miime_annot_stack_count(stack);
        for (int i = count - 1; i >= 0; i--) {
            const MiimeAnnotation *a = miime_annot_stack_get(stack, i);
            if (a && a->type == MIIME_ANNOT_CROP && a->point_count >= 4) {
                double x1 = (a->points[0] < a->points[2]) ? a->points[0] : a->points[2];
                double y1 = (a->points[1] < a->points[3]) ? a->points[1] : a->points[3];
                double w = fabs(a->points[2] - a->points[0]);
                double h = fabs(a->points[3] - a->points[1]);
                if (w >= 1 && h >= 1) {
                    crop_x = x1;
                    crop_y = y1;
                    crop_w = w;
                    crop_h = h;
                    has_crop = TRUE;
                    break;
                }
            }
        }
    }

    if (has_crop) {
        if (crop_x < 0) { crop_w += crop_x; crop_x = 0; }
        if (crop_y < 0) { crop_h += crop_y; crop_y = 0; }
        if (crop_x + crop_w > img_w) crop_w = img_w - crop_x;
        if (crop_y + crop_h > img_h) crop_h = img_h - crop_y;
        if (crop_w < 1) crop_w = 1;
        if (crop_h < 1) crop_h = 1;
    }

    int target_w = (int)crop_w;
    int target_h = (int)crop_h;

    cairo_surface_t *out_surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, target_w, target_h);
    cairo_t *cr = cairo_create(out_surf);

    if (has_crop) {
        cairo_translate(cr, -crop_x, -crop_y);
    }

    /* Paint base image */
    cairo_surface_t *base_surf = miime_image_get_surface(img);
    if (base_surf) {
        cairo_set_source_surface(cr, base_surf, 0, 0);
        cairo_paint(cr);
    }

    /* Paint annotations (skipping crop boxes from output render) */
    if (stack) {
        int count = miime_annot_stack_count(stack);
        for (int i = 0; i < count; i++) {
            const MiimeAnnotation *a = miime_annot_stack_get(stack, i);
            if (a && a->type != MIIME_ANNOT_CROP) {
                render_one_annotation(cr, a, FALSE, pin_number_before(stack, i), g_aesthetic, NULL, -1, FALSE);
            }
        }
    }

    cairo_destroy(cr);

    if (out_w) *out_w = target_w;
    if (out_h) *out_h = target_h;
    return out_surf;
}

typedef struct {
    cairo_t *cr;
    PangoFontDescription *font;
    double absolute_font_size;
    const MiimeLayoutItem *items;
    const int *stack_indices;
    int item_count;
    const MiimeStyleResolution *resolution;
} LayoutMeasureContext;

static double measure_layout_line(const char *text, void *userdata) {
    LayoutMeasureContext *ctx = userdata;
    PangoLayout *layout = pango_cairo_create_layout(ctx->cr);
    pango_layout_set_font_description(layout, ctx->font);
    pango_layout_set_text(layout, text ? text : "", -1);
    int width = 0, height = 0;
    pango_layout_get_pixel_size(layout, &width, &height);
    g_object_unref(layout);
    return width;
}

static void measure_layout_block(const char *text, void *userdata, double max_width,
                                 double *out_width, double *out_height) {
    LayoutMeasureContext *ctx=userdata;
    double font_size=ctx->absolute_font_size;
    if (ctx->resolution && ctx->items && ctx->stack_indices) {
        for (int i=0;i<ctx->item_count;i++) if (text==ctx->items[i].text) {
            const MiimeAnnotationStyleResult *appearance=resolved_annotation(ctx->resolution,ctx->stack_indices[i]);
            if (appearance) font_size=appearance->font_size;
            break;
        }
    }
    PangoLayout *layout=pango_cairo_create_layout(ctx->cr);
    PangoFontDescription *font=pango_font_description_new();
    pango_font_description_set_family(font,MIIME_OPTIONS_LABEL_FONT_FAMILY);
    pango_font_description_set_absolute_size(font,font_size*PANGO_SCALE);
    pango_layout_set_font_description(layout,font);
    pango_layout_set_width(layout,(int)(max_width*PANGO_SCALE));
    pango_layout_set_wrap(layout,PANGO_WRAP_WORD_CHAR);
    pango_layout_set_text(layout,text?text:"",-1);
    int width=0,height=0; pango_layout_get_pixel_size(layout,&width,&height);
    *out_width=MIN(width,(int)max_width)+16.0;
    *out_height=height+16.0;
    pango_font_description_free(font); g_object_unref(layout);
}

static void layout_port_normal(double x, double y, double w, double h,
                               double px, double py, int port,
                               double *nx, double *ny) {
    static const double normals[8][2]={{0,-1},{1,0},{0,1},{-1,0},
        {-0.70710678118,-0.70710678118},{0.70710678118,-0.70710678118},
        {0.70710678118,0.70710678118},{-0.70710678118,0.70710678118}};
    if (port>=0 && port<8) { *nx=normals[port][0]; *ny=normals[port][1]; return; }
    double d[4]={fabs(py-y),fabs(px-(x+w)),fabs(py-(y+h)),fabs(px-x)};
    int best=0;
    for (int i=1;i<4;i++) if (d[i]<d[best]) best=i;
    *nx=normals[best][0]; *ny=normals[best][1];
}

static void layout_label_normal(const MiimeLayoutResult *r, double *nx, double *ny) {
    double d[4]={fabs(r->port_label_y-r->label_y),
                 fabs(r->port_label_x-(r->label_x+r->label_w)),
                 fabs(r->port_label_y-(r->label_y+r->label_h)),
                 fabs(r->port_label_x-r->label_x)};
    int best=0;
    for (int i=1;i<4;i++) if (d[i]<d[best]) best=i;
    static const double normals[4][2]={{0,-1},{1,0},{0,1},{-1,0}};
    *nx=normals[best][0]; *ny=normals[best][1];
}

static void draw_layout_leader(cairo_t *cr, const MiimeAnnotation *a,
                               const MiimeLayoutItem *item,
                               const MiimeLayoutResult *r, int offset_x) {
    if (a->has_label_leader && a->label_leader==2) return;
    double tx=item->target_x+offset_x, ty=item->target_y;
    double tnx,tny,lnx,lny;
    layout_port_normal(tx,ty,item->target_w,item->target_h,
                       r->port_target_x,r->port_target_y,
                       a->has_label_anchor ? a->label_anchor : -1,&tnx,&tny);
    layout_label_normal(r,&lnx,&lny);
    double sx=r->port_target_x, sy=r->port_target_y;
    double ex=r->port_label_x, ey=r->port_label_y;
    double dist=hypot(ex-sx,ey-sy);
    int leader=a->has_label_leader ? a->label_leader : r->curve_type;
    double to_label_x=(r->label_x+r->label_w*0.5)-(tx+item->target_w*0.5);
    double to_label_y=(r->label_y+r->label_h*0.5)-(ty+item->target_h*0.5);
    gboolean opposite=to_label_x*tnx+to_label_y*tny<0.0;
    cairo_save(cr);
    cairo_set_source_rgba(cr,a->color_r,a->color_g,a->color_b,a->color_a);
    cairo_set_line_width(cr,1.5);
    cairo_move_to(cr,sx,sy);
    if (leader==0 && opposite) {
        double x1=sx+tnx*20.0,y1=sy+tny*20.0;
        double x2=ex-lnx*20.0,y2=ey-lny*20.0;
        cairo_line_to(cr,x1,y1);
        if (fabs(tnx)>0.5) {
            double corridor=(to_label_y<0.0)?ty-20.0:ty+item->target_h+20.0;
            cairo_curve_to(cr,x1,corridor,x2,corridor,x2,corridor);
        } else {
            double corridor=(to_label_x<0.0)?tx-20.0:tx+item->target_w+20.0;
            cairo_curve_to(cr,corridor,y1,corridor,y2,corridor,y2);
        }
        cairo_line_to(cr,ex,ey);
    } else if (leader==1) {
        double x1=sx+tnx*20.0,y1=sy+tny*20.0;
        double x2=ex-lnx*20.0,y2=ey-lny*20.0;
        cairo_line_to(cr,x1,y1);
        if (opposite) {
            /* If the selected target port faces away from the label, take a
             * clear corridor around the target before crossing its bounds. */
            if (fabs(tnx)>0.5) {
                double corridor=(to_label_y<0.0)?ty-20.0:ty+item->target_h+20.0;
                cairo_line_to(cr,x1,corridor);
                cairo_line_to(cr,x2,corridor);
            } else {
                double corridor=(to_label_x<0.0)?tx-20.0:tx+item->target_w+20.0;
                cairo_line_to(cr,corridor,y1);
                cairo_line_to(cr,corridor,y2);
            }
        } else {
            double cx=fabs(tnx)>0.5 ? x2 : x1;
            double cy=fabs(tnx)>0.5 ? y1 : y2;
            cairo_line_to(cr,cx,cy);
        }
        cairo_line_to(cr,x2,y2);
        cairo_line_to(cr,ex,ey);
    } else {
        cairo_curve_to(cr,sx+tnx*0.4*dist,sy+tny*0.4*dist,
                       ex-lnx*0.4*dist,ey-lny*0.4*dist,ex,ey);
    }
    cairo_stroke(cr);
    cairo_restore(cr);
}

static char *render_annotations_core(MiimeImage *img,
                                            const MiimeAnnotationStack *stack,
                                            int layout_mode,
                                            MiimeAestheticStyle aesthetic,
                                            const MiimeRenderOptions *options,
                                            gboolean dry_run,
                                            cairo_surface_t **out_surface,
                                            int *out_w, int *out_h) {
    if (out_surface) *out_surface = NULL;
    if (!img) return NULL;
    int img_w = miime_image_width(img), img_h = miime_image_height(img);
    if (img_w <= 0 || img_h <= 0) return NULL;
    int stack_count = stack ? miime_annot_stack_count(stack) : 0;
    int label_count = 0;
    gboolean has_z_order = FALSE;
    for (int i = 0; i < stack_count; i++) {
        const MiimeAnnotation *a = miime_annot_stack_get(stack, i);
        if (a && a->label_text[0]) label_count++;
        if (a && (a->has_z || (a->has_ink && a->ink == MIIME_INK_MARKER))) has_z_order = TRUE;
    }
    /* No labels must use the established export path, preserving crop and
     * pixel output exactly for existing callers. */
    if (!label_count && !dry_run && !has_z_order) {
        cairo_surface_t *legacy = options ? miime_render_export_with_options(img, stack, options, out_w, out_h)
                                          : miime_render_export(img, stack, out_w, out_h);
        if (out_surface) *out_surface = legacy; else if (legacy) cairo_surface_destroy(legacy);
        return NULL;
    }

    gboolean has_crop=FALSE;
    double crop_x=0,crop_y=0,crop_w=img_w,crop_h=img_h;
    for (int i=stack_count-1;i>=0;i--) {
        const MiimeAnnotation *a=miime_annot_stack_get(stack,i);
        if (a && a->type==MIIME_ANNOT_CROP && a->point_count>=4) {
            double x=fmin(a->points[0],a->points[2]);
            double y=fmin(a->points[1],a->points[3]);
            double w=fabs(a->points[2]-a->points[0]);
            double h=fabs(a->points[3]-a->points[1]);
            if (w>=1 && h>=1) { crop_x=x; crop_y=y; crop_w=w; crop_h=h; has_crop=TRUE; break; }
        }
    }
    if (has_crop) {
        if (crop_x<0) { crop_w+=crop_x; crop_x=0; }
        if (crop_y<0) { crop_h+=crop_y; crop_y=0; }
        if (crop_x+crop_w>img_w) crop_w=img_w-crop_x;
        if (crop_y+crop_h>img_h) crop_h=img_h-crop_y;
        if (crop_w<1) crop_w=1;
        if (crop_h<1) crop_h=1;
    }
    int layout_img_w=(int)crop_w, layout_img_h=(int)crop_h;
    MiimeCrop source_crop={(int)crop_x,(int)crop_y,layout_img_w,layout_img_h,has_crop};
    const MiimeStyleResolution *resolution=options?options->resolution:NULL;
    MiimeStyleResolution *owned_resolution=NULL;
    if (options && !resolution) {
        owned_resolution=miime_style_resolve(miime_image_get_surface(img),&options->requested,stack,source_crop);
        resolution=owned_resolution;
    }

    MiimeLayoutItem items[MIIME_LAYOUT_MAX_ITEMS];
    MiimeLayoutResult results[MIIME_LAYOUT_MAX_ITEMS];
    int stack_indices[MIIME_LAYOUT_MAX_ITEMS];
    int n = 0;
    for (int i = 0; i < stack_count; i++) {
        const MiimeAnnotation *a = miime_annot_stack_get(stack, i);
        if (!a || !a->label_text[0]) continue;
        if (n >= MIIME_LAYOUT_MAX_ITEMS) {
            g_warning("miime: layout limit %d reached; dropping label at annotation %d", MIIME_LAYOUT_MAX_ITEMS, i);
            continue;
        }
        double x0=0,y0=0,x1=0,y1=0;
        if (a->point_count >= 4) {
            x0=fmin(a->points[0],a->points[2]); y0=fmin(a->points[1],a->points[3]);
            x1=fmax(a->points[0],a->points[2]); y1=fmax(a->points[1],a->points[3]);
            for (int k=4;k+1<a->point_count;k+=2) {
                x0=fmin(x0,a->points[k]); y0=fmin(y0,a->points[k+1]);
                x1=fmax(x1,a->points[k]); y1=fmax(y1,a->points[k+1]);
            }
        } else if (a->point_count >= 2) { x0=a->points[0]; y0=a->points[1]; x1=x0; y1=y0; }
        items[n].target_x=x0-crop_x; items[n].target_y=y0-crop_y;
        items[n].target_w=fmax(1.0,x1-x0); items[n].target_h=fmax(1.0,y1-y0);
        g_strlcpy(items[n].text,a->label_text,sizeof(items[n].text));
        items[n].preferred_anchor=a->has_label_anchor ? a->label_anchor : -1;
        stack_indices[n]=i; n++;
    }
    int effective_mode=label_count == 0 ? 2 : layout_mode;
    if (effective_mode==0) effective_mode=miime_layout_mode_select(layout_img_w,layout_img_h,n,items);
    int offset_x=effective_mode==1 ? 300 : 0;
    int canvas_w=layout_img_w+offset_x*2;

    cairo_surface_t *measure_surface=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,1,1);
    cairo_t *measure_cr=cairo_create(measure_surface);
    double default_font=18.0;
    if (resolution) default_font=resolved_style_font(resolution->resolved_drawing_style);
    LayoutMeasureContext measure; memset(&measure,0,sizeof(measure));
    measure.cr=measure_cr;
    measure.font=pango_font_description_from_string("Sans 10");
    measure.absolute_font_size=default_font;
    measure.items=items; measure.stack_indices=stack_indices; measure.item_count=n;
    measure.resolution=resolution;
    int layout_status=0;
    if (n) {
        if (options) layout_status=miime_layout_run_with_metrics(layout_img_w,layout_img_h,items,n,effective_mode,(int)aesthetic,
                              measure_layout_block,&measure,results);
        else layout_status=miime_layout_run(layout_img_w,layout_img_h,items,n,effective_mode,(int)aesthetic,
                              measure_layout_line,&measure,results);
    }
    if (layout_status!=0) {
        pango_font_description_free(measure.font); cairo_destroy(measure_cr); cairo_surface_destroy(measure_surface);
        miime_style_resolution_free(owned_resolution);
        return NULL;
    }
    pango_font_description_free(measure.font); cairo_destroy(measure_cr); cairo_surface_destroy(measure_surface);

    if (dry_run) {
        GString *json=g_string_new("{\"placements\":[");
        gboolean comma=FALSE;
        int seen=0;
        for (int i=0;i<stack_count;i++) {
            const MiimeAnnotation *a=miime_annot_stack_get(stack,i);
            if (!a || !a->label_text[0]) continue;
            if (comma) g_string_append_c(json,',');
            comma=TRUE;
            if (seen < n) {
                MiimeLayoutResult *r=&results[seen];
                int leader=a->has_label_leader ? a->label_leader : (r->curve_type ? 1 : 0);
                g_string_append_printf(json,"{\"annotation_index\":%d,\"label_x\":%.2f,\"label_y\":%.2f,\"label_w\":%.2f,\"label_h\":%.2f,\"port_target_x\":%.2f,\"port_target_y\":%.2f,\"leader\":%d,\"obstacle_detour\":%d}",
                                       i,r->label_x,r->label_y,r->label_w,r->label_h,r->port_target_x,r->port_target_y,leader,r->obstacle_detour);
            } else {
                g_string_append_printf(json,"{\"annotation_index\":%d,\"label_x\":0,\"label_y\":0,\"label_w\":0,\"label_h\":0,\"obstacle_detour\":1}",i);
            }
            seen++;
        }
        g_string_append(json,"]");
        if (resolution) {
            char *diagnostics=miime_style_resolution_to_json(resolution,source_crop,offset_x);
            if(diagnostics) {
                g_string_append(json,",\"resolution\":");
                g_string_append(json,diagnostics);
                g_free(diagnostics);
            }
        }
        g_string_append_c(json,'}');
        miime_style_resolution_free(owned_resolution);
        return g_string_free(json,FALSE);
    }

    int target_w=canvas_w, target_h=layout_img_h;
    cairo_surface_t *surface=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,target_w,target_h);
    cairo_t *cr=cairo_create(surface);
    cairo_save(cr);
    cairo_translate(cr,offset_x-crop_x,-crop_y);
    cairo_surface_t *base=miime_image_get_surface(img);
    if (base) { cairo_set_source_surface(cr,base,0,0); cairo_paint(cr); }
    MiimeAestheticStyle render_aesthetic=resolution?drawing_aesthetic(resolution->resolved_drawing_style):aesthetic;
    if (!options) miime_render_set_aesthetic(aesthetic);
    if (!has_z_order) {
        for (int i=0;i<stack_count;i++) {
            const MiimeAnnotation *a=miime_annot_stack_get(stack,i);
            if (a && a->type!=MIIME_ANNOT_CROP) {
                if (a->label_text[0]) { MiimeAnnotation copy=*a; copy.label_text[0]='\0';
                    if(options) render_options_annotation(cr,&copy,i,pin_number_before(stack,i),render_aesthetic,resolution);
                    else render_one_annotation(cr,&copy,FALSE,pin_number_before(stack,i),g_aesthetic,NULL,-1,FALSE); }
                else if(options) render_options_annotation(cr,a,i,pin_number_before(stack,i),render_aesthetic,resolution);
                else render_one_annotation(cr,a,FALSE,pin_number_before(stack,i),g_aesthetic,NULL,-1,FALSE);
            }
        }
    } else {
        for (int z=0;z<=2;z++) for (int i=0;i<stack_count;i++) {
            const MiimeAnnotation *a=miime_annot_stack_get(stack,i);
            if (a && a->type!=MIIME_ANNOT_CROP && annotation_z(a)==z) {
                if (a->label_text[0]) { MiimeAnnotation copy=*a; copy.label_text[0]='\0';
                    if(options) render_options_annotation(cr,&copy,i,pin_number_before(stack,i),render_aesthetic,resolution);
                    else render_one_annotation(cr,&copy,FALSE,pin_number_before(stack,i),g_aesthetic,NULL,-1,FALSE); }
                else if(options) render_options_annotation(cr,a,i,pin_number_before(stack,i),render_aesthetic,resolution);
                else render_one_annotation(cr,a,FALSE,pin_number_before(stack,i),g_aesthetic,NULL,-1,FALSE);
            }
        }
    }
    cairo_restore(cr);
    for (int i=0;i<n;i++) {
        const MiimeAnnotation *a=miime_annot_stack_get(stack,stack_indices[i]);
        MiimeLayoutResult *r=&results[i];
        if (a) {
            if (resolution) {
                const MiimeAnnotationStyleResult *appearance=resolved_annotation(resolution,stack_indices[i]);
                if (appearance) { MiimeAnnotation copy=*a; copy.color_r=appearance->ink_rgba[0]; copy.color_g=appearance->ink_rgba[1]; copy.color_b=appearance->ink_rgba[2]; copy.color_a=appearance->ink_rgba[3]; draw_layout_leader(cr,&copy,&items[i],r,offset_x); }
                else draw_layout_leader(cr,a,&items[i],r,offset_x);
            } else draw_layout_leader(cr,a,&items[i],r,offset_x);
        }
    }
    for (int i=0;i<n;i++) {
        const MiimeAnnotation *a=miime_annot_stack_get(stack,stack_indices[i]);
        MiimeLayoutResult *r=&results[i];
        const MiimeAnnotationStyleResult *appearance=resolved_annotation(resolution,stack_indices[i]);
        if (appearance && appearance->has_label_surface) {
            cairo_set_source_rgba(cr,appearance->label_surface_rgba[0],appearance->label_surface_rgba[1],
                                  appearance->label_surface_rgba[2],appearance->label_surface_rgba[3]);
            cairo_rectangle(cr,r->label_x,r->label_y,r->label_w,r->label_h); cairo_fill(cr);
        }
        PangoLayout *layout=pango_cairo_create_layout(cr);
        PangoFontDescription *font;
        if (options) {
            font=pango_font_description_new();
            pango_font_description_set_family(font,MIIME_OPTIONS_LABEL_FONT_FAMILY);
            double label_font=appearance?appearance->font_size:default_font;
            pango_font_description_set_absolute_size(font,label_font*PANGO_SCALE);
        } else font=pango_font_description_from_string("Sans 10");
        pango_layout_set_font_description(layout,font);
        pango_layout_set_width(layout,240*PANGO_SCALE);
        pango_layout_set_wrap(layout,PANGO_WRAP_WORD_CHAR);
        pango_layout_set_text(layout,a->label_text,-1);
        double ink[4]={a->color_r,a->color_g,a->color_b,a->color_a};
        if (appearance) memcpy(ink,appearance->ink_rgba,sizeof(ink));
        cairo_set_source_rgba(cr,ink[0],ink[1],ink[2],ink[3]);
        cairo_move_to(cr,r->label_x+(options?8:0),r->label_y+(options?8:0));
        pango_cairo_show_layout(cr,layout);
        pango_font_description_free(font);
        g_object_unref(layout);
    }
    cairo_destroy(cr);
    if (out_w) *out_w=target_w;
    if (out_h) *out_h=target_h;
    if (out_surface) *out_surface=surface; else cairo_surface_destroy(surface);
    miime_style_resolution_free(owned_resolution);
    return NULL;
}

char *miime_render_annotations_with_dry_run(MiimeImage *img,
                                            const MiimeAnnotationStack *stack,
                                            int layout_mode, MiimeAestheticStyle aesthetic,
                                            gboolean dry_run, cairo_surface_t **out_surface,
                                            int *out_w, int *out_h) {
    return render_annotations_core(img,stack,layout_mode,aesthetic,NULL,dry_run,out_surface,out_w,out_h);
}

char *miime_render_annotations_with_options(MiimeImage *img,
                                            const MiimeAnnotationStack *stack,
                                            int layout_mode,
                                            MiimeAestheticStyle legacy_aesthetic,
                                            const MiimeRenderOptions *options,
                                            gboolean dry_run,
                                            cairo_surface_t **out_surface,
                                            int *out_w, int *out_h) {
    return render_annotations_core(img,stack,layout_mode,legacy_aesthetic,options,dry_run,out_surface,out_w,out_h);
}

gboolean miime_render_save_to_file(cairo_surface_t *surface, const char *filepath) {
    if (!surface || !filepath) return FALSE;

    const char *ext = strrchr(filepath, '.');
    if (ext && (g_ascii_strcasecmp(ext, ".jpg") == 0 || g_ascii_strcasecmp(ext, ".jpeg") == 0)) {
        cairo_surface_flush(surface);
        int w = cairo_image_surface_get_width(surface);
        int h = cairo_image_surface_get_height(surface);
        const unsigned char *data = cairo_image_surface_get_data(surface);
        int stride = cairo_image_surface_get_stride(surface);

        GdkPixbuf *pb = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, w, h);
        if (!pb) return FALSE;
        guchar *dst_pixels = gdk_pixbuf_get_pixels(pb);
        int dst_stride = gdk_pixbuf_get_rowstride(pb);

        for (int y = 0; y < h; y++) {
            const unsigned char *src_row = data + y * stride;
            guchar *dst_row = dst_pixels + y * dst_stride;
            for (int x = 0; x < w; x++) {
                dst_row[x * 3 + 0] = src_row[x * 4 + 2]; /* R */
                dst_row[x * 3 + 1] = src_row[x * 4 + 1]; /* G */
                dst_row[x * 3 + 2] = src_row[x * 4 + 0]; /* B */
            }
        }
        GError *err = NULL;
        gboolean ok = gdk_pixbuf_save(pb, filepath, "jpeg", &err, "quality", "95", NULL);
        g_object_unref(pb);
        if (err) {
            g_printerr("miime: error saving jpeg: %s\n", err->message);
            g_error_free(err);
        }
        return ok;
    } else {
        cairo_status_t st = cairo_surface_write_to_png(surface, filepath);
        return (st == CAIRO_STATUS_SUCCESS);
    }
}
