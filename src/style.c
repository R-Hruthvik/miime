#include "style.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

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
        z->has_fill=a->fill!=0; z->has_label_surface=TRUE;
        if(a->has_fill_color){z->fill_rgba[0]=a->fill_r;z->fill_rgba[1]=a->fill_g;z->fill_rgba[2]=a->fill_b;z->fill_rgba[3]=a->fill_a;}
        if(a->has_label_surface)copy_rgba(z->label_surface_rgba,a->label_surface_rgba);
        gboolean has_override=a->has_color||a->has_fill_color||a->has_label_surface||a->has_font_size;
        /* Sample a flat, deterministic grid over annotation bounds. For path annotations,
         * replace it with samples distributed over stored segments so empty interiors
         * cannot hide contrast. */
        int ax0=x0, ay0=y0, aw=w, ah=h;
        if(a->point_count>=2&&a->point_count%2==0){
            double minx=a->points[0],maxx=minx,miny=a->points[1],maxy=miny;
            for(int q=2;q<a->point_count;q+=2){minx=fmin(minx,a->points[q]);maxx=fmax(maxx,a->points[q]);miny=fmin(miny,a->points[q+1]);maxy=fmax(maxy,a->points[q+1]);}
            ax0=MAX(x0,(int)floor(minx));ay0=MAX(y0,(int)floor(miny));
            int ax1=MIN(x0+w,(int)ceil(maxx)+1),ay1=MIN(y0+h,(int)ceil(maxy)+1);
            aw=MAX(0,ax1-ax0);ah=MAX(0,ay1-ay0);
        }
        guint lx=MIN((guint)MAX(aw,1),8u),ly=MIN((guint)MAX(ah,1),8u);
        int sample_x[64],sample_y[64];
        if(a->point_count>=4 && a->point_count%2==0) {
            int segs=a->point_count/2-1; double lengths[MIIME_MAX_POINTS/2],total=0;
            for(int j=0;j<segs;j++){double dx=a->points[(j+1)*2]-a->points[j*2],dy=a->points[(j+1)*2+1]-a->points[j*2+1];lengths[j]=hypot(dx,dy);total+=lengths[j];}
            guint count=MIN(64u,MAX(1u,(guint)ceil(total)));
            for(guint q=0;q<count;q++) {
                double dist=((double)q+.5)*total/count; int seg=0; while(seg<segs-1&&dist>lengths[seg]){dist-=lengths[seg];seg++;}
                double t=lengths[seg]>0?dist/lengths[seg]:0; int px=(int)floor(a->points[seg*2]+t*(a->points[(seg+1)*2]-a->points[seg*2])); int py=(int)floor(a->points[seg*2+1]+t*(a->points[(seg+1)*2+1]-a->points[seg*2+1]));
                if(px<x0||py<y0||px>=x0+w||py>=y0+h)continue;
                gboolean duplicate=FALSE;
                for(guint q=0;q<z->local_sample_count;q++)if(sample_x[q]==px&&sample_y[q]==py){duplicate=TRUE;break;}
                if(duplicate)continue;
                sample_x[z->local_sample_count]=px;sample_y[z->local_sample_count]=py;z->local_sample_count++;
            }
        } else {
            for(guint iy=0;iy<ly;iy++)for(guint ix=0;ix<lx;ix++) {
                int px=ax0+coord(ix,(guint)MAX(aw,1),lx),py=ay0+coord(iy,(guint)MAX(ah,1),ly);gboolean duplicate=FALSE;
                for(guint q=0;q<z->local_sample_count;q++)if(sample_x[q]==px&&sample_y[q]==py){duplicate=TRUE;break;}
                if(duplicate||px<x0||py<y0||px>=x0+w||py>=y0+h)continue;
                sample_x[z->local_sample_count]=px;sample_y[z->local_sample_count]=py;z->local_sample_count++;
            }
        }
        gboolean is_text=(a->type==MIIME_ANNOT_TEXT||a->label_text[0]);
        double target=is_text?o->text_target:o->shape_target;
        o->contrast_total+=z->local_sample_count;
        for(guint q=0;q<z->local_sample_count;q++) {
            double sr,sg,sb,source[3],surface[3],foreground[3];
            sample_rgb(data,stride,sample_x[q],sample_y[q],bg,&sr,&sg,&sb);source[0]=sr;source[1]=sg;source[2]=sb;
            double ratio;
            if(is_text) {
                composite_rgba(z->label_surface_rgba,source,surface);
                double ink[4]={z->ink_rgba[0],z->ink_rgba[1],z->ink_rgba[2],z->ink_rgba[3]};
                composite_rgba(ink,surface,foreground);
                ratio=contrast(lum(foreground[0],foreground[1],foreground[2]),lum(surface[0],surface[1],surface[2]));
            } else {
                double ink[4]={z->ink_rgba[0],z->ink_rgba[1],z->ink_rgba[2],z->ink_rgba[3]};
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
                    double ink[4]={z->ink_rgba[0],z->ink_rgba[1],z->ink_rgba[2],z->ink_rgba[3]}, halo_fg[3];
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
