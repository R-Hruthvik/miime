#include "style.h"
#include <math.h>
#include <pango/pangocairo.h>
#include <stdio.h>
#include <string.h>

#define MIIME_OPTIONS_LABEL_FONT_FAMILY "DejaVu Sans"

#define C(r,g,b,a) {(r)/255.0,(g)/255.0,(b)/255.0,(a)/255.0}
static const MiimeStylePalette palettes[] = {
    { C(32,33,36,255), C(255,255,255,255), C(241,243,244,255), C(179,38,30,255), C(138,75,0,255), C(21,88,166,255) },
    { C(241,243,244,255), C(32,33,36,255), C(48,49,52,255), C(255,107,107,255), C(255,200,87,255), C(114,183,255,255) },
    { C(53,42,36,255), C(255,248,237,255), C(241,228,209,255), C(165,42,42,255), C(130,80,0,255), C(36,91,143,255) },
    { C(34,34,34,255), C(255,255,255,255), C(232,232,232,255), C(166,27,27,255), C(120,85,0,255), C(24,90,141,255) }
};

const MiimeStylePalette *miime_style_palette(MiimeTheme t) {
    return t >= MIIME_THEME_LIGHT && t <= MIIME_THEME_NEUTRAL ? &palettes[t - 1] : NULL;
}
const char *miime_style_token(MiimeDrawingStyle s) {
    static const char *tokens[] = {"technical", "sketch", "editorial", "minimal"};
    return s >= 0 && s < 4 ? tokens[s] : "sketch";
}
static double linear(double c) { return c <= .04045 ? c / 12.92 : pow((c + .055) / 1.055, 2.4); }
static double lum(double r, double g, double b) { return .2126 * linear(r) + .7152 * linear(g) + .0722 * linear(b); }
static double contrast(double a, double b) { return (fmax(a,b)+.05)/(fmin(a,b)+.05); }
static void composite_rgba(const double *fg,const double *bg,double *out) {
    double alpha=fmin(1.0,fmax(0.0,fg[3]));
    for(int i=0;i<3;i++) out[i]=fg[i]*alpha+bg[i]*(1-alpha);
}
static void append_warning(char *buf,const char *s) {
    if (strstr(buf,s)) return;
    size_t n = strlen(buf); if (n < 255) snprintf(buf+n,256-n,"%s%s",n ? "," : "",s);
}
static void sample_rgb(const unsigned char *data,int stride,int x,int y,guint32 bg,double *r,double *g,double *b) {
    guint32 p; memcpy(&p,data+y*stride+x*4,4);
    double a=((p>>24)&255)/255.0, rr=((p>>16)&255)/255.0, gg=((p>>8)&255)/255.0, bb=(p&255)/255.0;
    if (a > 0) { rr=fmin(1,rr/a); gg=fmin(1,gg/a); bb=fmin(1,bb/a); } else rr=gg=bb=0;
    *r=rr*a+((bg>>24)&255)/255.0*(1-a); *g=gg*a+((bg>>16)&255)/255.0*(1-a); *b=bb*a+((bg>>8)&255)/255.0*(1-a);
}
static guint coord(guint i,guint extent,guint count) { return (guint)floor(((double)i+.5)*extent/count); }
static double style_width(MiimeDrawingStyle s) { const double v[]={2,2.5,3,1.5}; return v[s]; }
static double style_font(MiimeDrawingStyle s) { const double v[]={18,18,20,16}; return v[s]; }
static double style_radius(MiimeDrawingStyle s) { const double v[]={4,8,10,2}; return v[s]; }
static double style_halo(MiimeDrawingStyle s) { const double v[]={1,1.5,2,1}; return v[s]; }
static double fill_alpha(MiimeDrawingStyle s) { const double v[]={.12,.18,.16,.08}; return v[s]; }
static void copy_rgba(double *dst,const double *src) { memcpy(dst,src,4*sizeof(double)); }
static void add_sample(int *xs,int *ys,guint *count,int x,int y,int x0,int y0,int w,int h) {
    if(x<x0||y<y0||x>=x0+w||y>=y0+h||*count>=64)return;
    for(guint i=0;i<*count;i++)if(xs[i]==x&&ys[i]==y)return;
    xs[*count]=x;ys[*count]=y;(*count)++;
}
static void sample_polyline(const double *points,int point_count,int close_path,int x0,int y0,int w,int h,int *xs,int *ys,guint *count) {
    int segments=point_count/2-1+!!close_path;double lengths[MIIME_MAX_POINTS/2+1],total=0;
    for(int i=0;i<segments;i++){int j=(i+1)%(point_count/2);lengths[i]=hypot(points[j*2]-points[i*2],points[j*2+1]-points[i*2+1]);total+=lengths[i];}
    guint n=MIN(64u,MAX(1u,(guint)ceil(total)));
    for(guint i=0;i<n&&*count<64;i++){double d=((double)i+.5)*total/n;int seg=0;while(seg<segments-1&&d>lengths[seg]){d-=lengths[seg];seg++;}int j=(seg+1)%(point_count/2);double t=lengths[seg]>0?d/lengths[seg]:0;add_sample(xs,ys,count,(int)floor(points[seg*2]+t*(points[j*2]-points[seg*2])),(int)floor(points[seg*2+1]+t*(points[j*2+1]-points[seg*2+1])),x0,y0,w,h);}
}
static gboolean is_box_shape(MiimeAnnotationType t) {
    return t==MIIME_ANNOT_RECT||t==MIIME_ANNOT_ELLIPSE||t==MIIME_ANNOT_CIRCLE||t==MIIME_ANNOT_HIGHLIGHT_BOX||t==MIIME_ANNOT_CALLOUT;
}
static void sample_annotation(const MiimeAnnotation *a,int x0,int y0,int w,int h,int *xs,int *ys,guint *count,double font_size) {
    if(a->type==MIIME_ANNOT_TEXT&&a->point_count>=2) {
        cairo_surface_t *measure=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,1,1);cairo_t *cr=cairo_create(measure);PangoLayout *layout=pango_cairo_create_layout(cr);
        PangoFontDescription *font=pango_font_description_new();pango_font_description_set_family(font,MIIME_OPTIONS_LABEL_FONT_FAMILY);pango_font_description_set_absolute_size(font,font_size*PANGO_SCALE);pango_layout_set_font_description(layout,font);pango_layout_set_text(layout,a->text,-1);
        int tw=0,th=0;pango_layout_get_pixel_size(layout,&tw,&th);if(tw<1)tw=1;if(th<1)th=(int)ceil(font_size*1.3);
        guint nx=MIN((guint)tw,8u),ny=MIN((guint)th,8u);
        for(guint iy=0;iy<ny;iy++)for(guint ix=0;ix<nx;ix++)add_sample(xs,ys,count,(int)floor(a->points[0]+((double)ix+.5)*tw/nx),(int)floor(a->points[1]+((double)iy+.5)*th/ny),x0,y0,w,h);
        g_object_unref(layout);pango_font_description_free(font);cairo_destroy(cr);cairo_surface_destroy(measure);return;
    }
    if(a->type==MIIME_ANNOT_PIN&&a->point_count==2) {
        double radius=a->has_radius&&a->radius>0?a->radius:12.0;
        guint n=MIN(64u,MAX(8u,(guint)ceil(2*G_PI*radius)));
        for(guint i=0;i<n;i++){double t=2*G_PI*((double)i+.5)/n;add_sample(xs,ys,count,(int)floor(a->points[0]+radius*cos(t)),(int)floor(a->points[1]+radius*sin(t)),x0,y0,w,h);}
        return;
    }
    if(a->type==MIIME_ANNOT_CALLOUT&&a->point_count>=4) {
        int opposite=a->point_count>=8?4:2;double x=fmin(a->points[0],a->points[opposite]),y=fmin(a->points[1],a->points[opposite+1]);
        double box_w=fabs(a->points[opposite]-a->points[0]),box_h=fabs(a->points[opposite+1]-a->points[1]);
        double bx=x+box_w*.15,by=y+box_h*.15,bw=box_w*.7,bh=box_h*.7,cx=x+box_w*.5,base=fmin(8.0,bw*.3);
        double path[]={bx,by,cx-base,by,cx,y,cx+base,by,bx+bw,by,bx+bw,by+bh,bx,by+bh};
        sample_polyline(path,14,TRUE,x0,y0,w,h,xs,ys,count);
        return;
    }
    if(is_box_shape(a->type)&&a->point_count>=4) {
        int opposite=a->point_count>=8?4:2;double left=fmin(a->points[0],a->points[opposite]),right=fmax(a->points[0],a->points[opposite]),top=fmin(a->points[1],a->points[opposite+1]),bottom=fmax(a->points[1],a->points[opposite+1]);
        guint n=64;double rx=(right-left)/2,ry=(bottom-top)/2,cx=(left+right)/2,cy=(top+bottom)/2;
        if(a->type==MIIME_ANNOT_RECT||a->type==MIIME_ANNOT_HIGHLIGHT_BOX||a->type==MIIME_ANNOT_CALLOUT) {
            double perimeter=2*((right-left)+(bottom-top));n=MIN(64u,MAX(4u,(guint)ceil(perimeter)));
            for(guint i=0;i<n;i++){double d=((double)i+.5)*perimeter/n;double px,py;if(d<right-left){px=left+d;py=top;}else if((d-=right-left)<bottom-top){px=right;py=top+d;}else if((d-=bottom-top)<right-left){px=right-d;py=bottom;}else{d-=right-left;px=left;py=bottom-d;}add_sample(xs,ys,count,(int)floor(px),(int)floor(py),x0,y0,w,h);}
        } else {
            if(a->type==MIIME_ANNOT_CIRCLE)rx=ry=fmax(rx,ry);
            double perimeter=2*G_PI*sqrt((rx*rx+ry*ry)/2);n=MIN(64u,MAX(8u,(guint)ceil(perimeter)));
            for(guint i=0;i<n;i++){double t=2*G_PI*((double)i+.5)/n;add_sample(xs,ys,count,(int)floor(cx+rx*cos(t)),(int)floor(cy+ry*sin(t)),x0,y0,w,h);}
        }
        return;
    }
    if(a->point_count>=4&&a->point_count%2==0) {
        sample_polyline(a->points,a->point_count,FALSE,x0,y0,w,h,xs,ys,count);
    } else if(a->point_count>=2) add_sample(xs,ys,count,(int)floor(a->points[0]),(int)floor(a->points[1]),x0,y0,w,h);
}
static void annotation_ink(const MiimeAnnotation *a,const MiimeStylePalette *p,gboolean active,double *ink) {
    copy_rgba(ink,p->ink);
    if (active && a->has_role) {
        if (a->role==MIIME_ROLE_ERROR) copy_rgba(ink,p->error);
        else if (a->role==MIIME_ROLE_WARNING) copy_rgba(ink,p->warning);
        else if (a->role==MIIME_ROLE_INFO) copy_rgba(ink,p->info);
    }
    if (a->has_color) { ink[0]=a->color_r; ink[1]=a->color_g; ink[2]=a->color_b; ink[3]=a->color_a; }
}
static gboolean active_config(const MiimeStyleConfig *r) { return r && (r->has_drawing_style || r->has_theme); }

MiimeStyleResolution *miime_style_resolve(const cairo_surface_t *src,const MiimeStyleConfig *req,const MiimeAnnotationStack *stack,MiimeCrop crop) {
    MiimeStyleResolution *o=g_new0(MiimeStyleResolution,1);
    o->requested=req ? *req : (MiimeStyleConfig){0}; o->text_target=4.5; o->shape_target=3.0;
    o->resolved_theme=req&&req->has_theme?req->theme:MIIME_THEME_NEUTRAL;
    o->resolved_drawing_style=req&&req->has_drawing_style?req->drawing_style:MIIME_DRAW_SKETCH;
    if (!src || cairo_surface_get_type((cairo_surface_t*)src)!=CAIRO_SURFACE_TYPE_IMAGE || cairo_surface_status((cairo_surface_t*)src)!=CAIRO_STATUS_SUCCESS) { strcpy(o->policy,"invalid_source"); return o; }
    cairo_surface_t *surface=(cairo_surface_t*)src; cairo_surface_flush(surface);
    int iw=cairo_image_surface_get_width(surface), ih=cairo_image_surface_get_height(surface), stride=cairo_image_surface_get_stride(surface);
    const unsigned char *data=cairo_image_surface_get_data(surface);
    int x0=0,y0=0,w=iw,h=ih;
    if (crop.enabled) {
        x0=crop.x; y0=crop.y; w=crop.width; h=crop.height;
        if (w<=0||h<=0||x0<0||y0<0||x0>=iw||y0>=ih) w=h=0;
        else { w=MIN(w,iw-x0); h=MIN(h,ih-y0); }
    }
    if (!w||!h) { strcpy(o->policy,"empty_crop"); return o; }
    guint32 bg=req&&req->has_analysis_background?req->analysis_background_rgba:0xFFFFFFFFu;
    guint nx=MIN((guint)w,64), ny=MIN((guint)h,64);
    double *values=g_new(double,nx*ny), sum=0, warm=0; guint k=0;
    for (guint iy=0;iy<ny;iy++) for (guint ix=0;ix<nx;ix++) {
        int x=x0+coord(ix,w,nx), y=y0+coord(iy,h,ny); double r,g,b;
        sample_rgb(data,stride,x,y,bg,&r,&g,&b); double l=lum(r,g,b); values[k++]=l; sum+=l;
        double mx=fmax(r,fmax(g,b)), mn=fmin(r,fmin(g,b)), d=mx-mn;
        if (d>.04) {
            double hue; if(mx==r)hue=60*fmod((g-b)/d,6); else if(mx==g)hue=60*((b-r)/d+2); else hue=60*((r-g)/d+4);
            if(hue<0)hue+=360;
            /* 8 coarse hue bins; bins 0, 1 and 7 cover red through yellow. */
            int bin=(int)floor(hue/45.0); if(bin==0||bin==1||bin==7) warm++;
        }
    }
    o->image_sample_count=k; double mean=sum/k;
    if (req&&req->has_theme) {
        if (req->theme!=MIIME_THEME_AUTO) o->resolved_theme=req->theme;
        else if(mean<.35) o->resolved_theme=MIIME_THEME_DARK;
        else if(mean>.65) o->resolved_theme=MIIME_THEME_LIGHT;
        else o->resolved_theme=warm/k>.5?MIIME_THEME_WARM:MIIME_THEME_NEUTRAL;
    } else o->resolved_theme=MIIME_THEME_NEUTRAL;
    guint edges=0,checks=0;
    for(guint iy=0;iy<ny;iy++)for(guint ix=0;ix<nx;ix++) {
        guint i=iy*nx+ix;
        if(ix+1<nx){checks++;if(fabs(values[i]-values[i+1])>=.15)edges++;}
        if(iy+1<ny){checks++;if(fabs(values[i]-values[i+nx])>=.15)edges++;}
    }
    double density=checks?(double)edges/checks:0;
    if(req&&req->has_drawing_style)o->resolved_drawing_style=req->drawing_style;
    else if(req&&req->has_theme&&req->theme==MIIME_THEME_AUTO)o->resolved_drawing_style=density>=.45?MIIME_DRAW_TECHNICAL:MIIME_DRAW_SKETCH;
    if(req&&req->has_aesthetic_style&&active_config(req)) {
        MiimeDrawingStyle alias=req->aesthetic_style==MIIME_AESTHETIC_CLEAN_TECHNICAL?MIIME_DRAW_TECHNICAL:MIIME_DRAW_SKETCH;
        if(req->has_drawing_style&&req->drawing_style!=alias)append_warning(o->warning_codes,"style_alias_conflict");
        else if(!req->has_drawing_style)o->resolved_drawing_style=alias;
    }
    const MiimeStylePalette *pal=miime_style_palette(o->resolved_theme); strcpy(o->policy,"sRGB_linear_luminance_v1");
    int n=stack?miime_annot_stack_count(stack):0; o->annotation_count=n; o->annotations=n?g_new0(MiimeAnnotationStyleResult,n):NULL;
    gboolean active=active_config(req);
    for(int i=0;i<n;i++) {
        const MiimeAnnotation *a=miime_annot_stack_get(stack,i); MiimeAnnotationStyleResult *z=&o->annotations[i];
        snprintf(z->annotation_id,sizeof z->annotation_id,"%s",a->json_id);
        annotation_ink(a,pal,active,z->ink_rgba); copy_rgba(z->fill_rgba,z->ink_rgba); z->fill_rgba[3]=fill_alpha(o->resolved_drawing_style);
        copy_rgba(z->label_surface_rgba,pal->label_surface);
        z->stroke_width=(a->has_stroke_width||a->has_stroke_width_pair)?a->stroke_width:(a->has_ink?(double[]){1.5,2.5,4,14}[a->ink]:style_width(o->resolved_drawing_style));
        z->stroke_width_end=a->has_stroke_width_pair?a->stroke_width_end:z->stroke_width;
        /* Explicit zero retains the legacy 24px fallback while remaining an intentional override. */
        z->font_size=a->has_font_size?(a->font_size>0?a->font_size:24):style_font(o->resolved_drawing_style);
        z->radius=a->has_radius?a->radius:style_radius(o->resolved_drawing_style); z->halo_width=style_halo(o->resolved_drawing_style);
        z->has_fill=a->fill!=0; z->has_label_surface=a->label_text[0]!='\0';
        if(a->has_fill_color){z->fill_rgba[0]=a->fill_r;z->fill_rgba[1]=a->fill_g;z->fill_rgba[2]=a->fill_b;z->fill_rgba[3]=a->fill_a;}
        if(a->has_label_surface)copy_rgba(z->label_surface_rgba,a->label_surface_rgba);
        if(a->type==MIIME_ANNOT_CROP||a->type==MIIME_ANNOT_BLUR)continue;
        gboolean has_override=a->has_color||a->has_fill_color||a->has_label_surface||a->has_font_size;
        /* Sample the visible text/shape geometry, bounded to 64 unique source pixels. */
        int sample_x[64],sample_y[64];
        if(a->type==MIIME_ANNOT_TEXT||a->type==MIIME_ANNOT_PIN||is_box_shape(a->type)||a->point_count>=4) {
            sample_annotation(a,x0,y0,w,h,sample_x,sample_y,&z->local_sample_count,z->font_size);
        }
        gboolean is_text=(a->type==MIIME_ANNOT_TEXT||a->label_text[0]!='\0');
        gboolean has_text_surface=a->label_text[0]!='\0';
        double target=is_text?o->text_target:o->shape_target;
        o->contrast_total+=z->local_sample_count;
        for(guint q=0;q<z->local_sample_count;q++) {
            double sr,sg,sb,source[3],surface[3],foreground[3];
            sample_rgb(data,stride,sample_x[q],sample_y[q],bg,&sr,&sg,&sb);source[0]=sr;source[1]=sg;source[2]=sb;
            double ratio;
            if(has_text_surface) {
                composite_rgba(z->label_surface_rgba,source,surface);
                double ink[4]={z->ink_rgba[0],z->ink_rgba[1],z->ink_rgba[2],z->ink_rgba[3]};
                composite_rgba(ink,surface,foreground);
                ratio=contrast(lum(foreground[0],foreground[1],foreground[2]),lum(surface[0],surface[1],surface[2]));
            } else {
                double marker_alpha=(a->has_ink&&a->ink==MIIME_INK_MARKER)?.35:1.0;
                double ink[4]={z->ink_rgba[0],z->ink_rgba[1],z->ink_rgba[2],z->ink_rgba[3]*marker_alpha};
                composite_rgba(ink,source,foreground);
                ratio=contrast(lum(foreground[0],foreground[1],foreground[2]),lum(source[0],source[1],source[2]));
            }
            if(ratio>=target){z->contrast_hits++;o->contrast_hits++;}
            else if(!is_text) {
                /* A missed shape sample receives the specified opaque dual halo.
                 * Evaluate the ink against each adjacent neutral edge; either edge
                 * can provide enough separation for the rendered stroke. */
                z->halo_applied=TRUE;
                const double halo_edges[2][3]={{1,1,1},{0,0,0}};
                for(int edge=0;edge<2;edge++) {
                    double marker_alpha=(a->has_ink&&a->ink==MIIME_INK_MARKER)?.35:1.0;
                    double ink[4]={z->ink_rgba[0],z->ink_rgba[1],z->ink_rgba[2],z->ink_rgba[3]*marker_alpha}, halo_fg[3];
                    composite_rgba(ink,halo_edges[edge],halo_fg);
                    double halo_ratio=contrast(lum(halo_fg[0],halo_fg[1],halo_fg[2]),lum(halo_edges[edge][0],halo_edges[edge][1],halo_edges[edge][2]));
                    if(halo_ratio>=target){z->contrast_hits++;o->contrast_hits++;break;}
                }
            }
        }
        if(z->contrast_hits<z->local_sample_count){z->contrast_unresolved=TRUE;append_warning(o->warning_codes,"contrast_unresolved");if(has_override){z->override_warning=TRUE;append_warning(o->warning_codes,"override_warning");}}
    }
    g_free(values); return o;
}
void miime_style_resolution_free(MiimeStyleResolution *r) { if(r){g_free(r->annotations);g_free(r);} }

static const char *theme_token(MiimeTheme theme) {
    static const char *tokens[]={"auto","light","dark","warm","neutral"};
    return theme>=MIIME_THEME_AUTO&&theme<=MIIME_THEME_NEUTRAL?tokens[theme]:"neutral";
}

static void json_append_string(GString *json,const char *value) {
    g_string_append_c(json,'"');
    for(const unsigned char *p=(const unsigned char *)(value?value:"");*p;p++) {
        if(*p=='"'||*p=='\\') { g_string_append_c(json,'\\'); g_string_append_c(json,(char)*p); }
        else if(*p<0x20) g_string_append_printf(json,"\\u%04x",(unsigned)*p);
        else g_string_append_c(json,(char)*p);
    }
    g_string_append_c(json,'"');
}

static void json_append_rgba(GString *json,const double rgba[4]) {
    g_string_append_printf(json,"[%.6f,%.6f,%.6f,%.6f]",rgba[0],rgba[1],rgba[2],rgba[3]);
}

char *miime_style_resolution_to_json(const MiimeStyleResolution *r,MiimeCrop crop,int rail_offset_x) {
    if(!r)return NULL;
    GString *json=g_string_new("{\"requested\":{\"drawing_style\":");
    if(r->requested.has_drawing_style) json_append_string(json,miime_style_token(r->requested.drawing_style));
    else g_string_append(json,"null");
    g_string_append(json,",\"theme\":");
    if(r->requested.has_theme) json_append_string(json,theme_token(r->requested.theme));
    else g_string_append(json,"null");
    g_string_append(json,",\"analysis_background\":");
    if(r->requested.has_analysis_background) {
        guint32 c=r->requested.analysis_background_rgba;
        g_string_append_printf(json,"\"#%02X%02X%02X\"",(c>>24)&255,(c>>16)&255,(c>>8)&255);
    } else g_string_append(json,"null");
    g_string_append(json,",\"aesthetic_style\":");
    if(r->requested.has_aesthetic_style)
        json_append_string(json,r->requested.aesthetic_style==MIIME_AESTHETIC_CLEAN_TECHNICAL?"clean_technical":"rough_sketch");
    else g_string_append(json,"null");
    g_string_append_printf(json,"},\"resolved\":{\"drawing_style\":\"%s\",\"theme\":\"%s\"},\"analysis_background\":\"#%02X%02X%02X\",\"image_sampling\":{\"policy\":\"%s\",\"count\":%u,\"max_count\":4096,\"coordinate_space\":\"source_image\",\"crop_x\":%d,\"crop_y\":%d,\"crop_width\":%d,\"crop_height\":%d,\"rail_offset_x\":%d},\"contrast\":{\"policy\":\"alpha_composited_relative_luminance\",\"text_target\":%.1f,\"shape_target\":%.1f,\"hits\":%u,\"total\":%u,\"coverage\":",
        miime_style_token(r->resolved_drawing_style),theme_token(r->resolved_theme),
        (r->requested.has_analysis_background?(r->requested.analysis_background_rgba>>24)&255:255),
        (r->requested.has_analysis_background?(r->requested.analysis_background_rgba>>16)&255:255),
        (r->requested.has_analysis_background?(r->requested.analysis_background_rgba>>8)&255:255),
        r->policy,r->image_sample_count,crop.x,crop.y,crop.width,crop.height,rail_offset_x,
        r->text_target,r->shape_target,r->contrast_hits,r->contrast_total);
    if(r->contrast_total)g_string_append_printf(json,"%.6f",(double)r->contrast_hits/r->contrast_total);
    else g_string_append(json,"null");
    g_string_append(json,"},\"warnings\":[");
    gboolean comma=FALSE;
    if(r->warning_codes[0]) {
        char **warnings=g_strsplit(r->warning_codes,",",-1);
        for(guint i=0;warnings[i];i++) if(warnings[i][0]) {
            if(comma) g_string_append_c(json,',');
            comma=TRUE;
            json_append_string(json,warnings[i]);
        }
        g_strfreev(warnings);
    }
    g_string_append(json,"],\"annotations\":[");
    for(guint i=0;i<r->annotation_count;i++) {
        const MiimeAnnotationStyleResult *a=&r->annotations[i];
        if(i)g_string_append_c(json,',');
        g_string_append(json,"{\"id\":"); json_append_string(json,a->annotation_id);
        g_string_append_printf(json,",\"local_sample_count\":%u,\"contrast_hits\":%u,\"coverage\":",
            a->local_sample_count,a->contrast_hits);
        if(a->local_sample_count)g_string_append_printf(json,"%.6f",(double)a->contrast_hits/a->local_sample_count);
        else g_string_append(json,"null");
        g_string_append_printf(json,",\"halo_applied\":%s,\"contrast_unresolved\":%s,\"override_warning\":%s,\"warnings\":[",
            a->halo_applied?"true":"false",a->contrast_unresolved?"true":"false",a->override_warning?"true":"false");
        if(a->contrast_unresolved)json_append_string(json,"contrast_unresolved");
        if(a->contrast_unresolved&&a->override_warning)g_string_append_c(json,',');
        if(a->override_warning)json_append_string(json,"override_warning");
        g_string_append(json,"],\"ink\":"); json_append_rgba(json,a->ink_rgba);
        g_string_append(json,"}");
    }
    g_string_append(json,"]}");
    return g_string_free(json,FALSE);
}
