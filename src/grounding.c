#include "grounding.h"
#include <cairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_ELEMS 512

static void ensure_cache_dir(char *buf, size_t sz) {
    const char *home = g_get_home_dir();
    snprintf(buf, sz, "%s/.cache/miime", home);
    g_mkdir_with_parents(buf, 0755);
}

static char *file_hash_bytes(const guchar *px, int w, int h, int stride) {
    GChecksum *cs = g_checksum_new(G_CHECKSUM_SHA256);
    g_checksum_update(cs, px, (gsize)(stride * h));
    char *hstr = g_strdup(g_checksum_get_string(cs));
    g_checksum_free(cs);
    (void)w;
    return hstr;
}

static GdkPixbuf *load_pixbuf(const char *path) {
    GError *err = NULL;
    GdkPixbuf *pb = gdk_pixbuf_new_from_file(path, &err);
    if (err) g_error_free(err);
    return pb;
}

/* ---- CV contour detection ---- */
typedef struct { int x,y,w,h; } Box;

static int iou_box(Box a, Box b) {
    int ix = (a.x > b.x ? a.x : b.x);
    int iy = (a.y > b.y ? a.y : b.y);
    int ix2 = (a.x+a.w < b.x+b.w ? a.x+a.w : b.x+b.w);
    int iy2 = (a.y+a.h < b.y+b.h ? a.y+a.h : b.y+b.h);
    if (ix2 <= ix || iy2 <= iy) return 0;
    int inter = (ix2-ix)*(iy2-iy);
    int ua = a.w*a.h + b.w*b.h - inter;
    return (int)(100.0 * inter / (ua > 0 ? ua : 1));
}

static int detect_contours(GdkPixbuf *pb, Box *out, int maxout) {
    int w = gdk_pixbuf_get_width(pb);
    int h = gdk_pixbuf_get_height(pb);
    int nch = gdk_pixbuf_get_n_channels(pb);
    int stride = gdk_pixbuf_get_rowstride(pb);
    const guchar *px = gdk_pixbuf_get_pixels(pb);

    /* grayscale */
    unsigned char *gray = g_malloc(w*h);
    for (int y=0;y<h;y++) for (int x=0;x<w;x++) {
        int i = y*stride + x*nch;
        gray[y*w+x] = (unsigned char)(0.299*px[i]+0.587*px[i+1]+0.114*px[i+2]);
    }

    /* sobel magnitude */
    int *mag = g_malloc0(sizeof(int)*w*h);
    for (int y=1;y<h-1;y++) for (int x=1;x<w-1;x++) {
        int gx = -gray[(y-1)*w+(x-1)] -2*gray[y*w+(x-1)] -gray[(y+1)*w+(x-1)]
               +gray[(y-1)*w+(x+1)] +2*gray[y*w+(x+1)] +gray[(y+1)*w+(x+1)];
        int gy = -gray[(y-1)*w+(x-1)] -2*gray[(y-1)*w+x] -gray[(y-1)*w+(x+1)]
               +gray[(y+1)*w+(x-1)] +2*gray[(y+1)*w+x] +gray[(y+1)*w+(x+1)];
        int m = (int)sqrt((double)(gx*gx+gy*gy));
        if (m>255) m=255;
        mag[y*w+x]=m;
    }

    /* threshold */
    unsigned char *edge = g_malloc0(w*h);
    int thr = 40;
    for (int i=0;i<w*h;i++) if (mag[i] > thr) edge[i]=1;

    /* dilate 2 */
    unsigned char *dil = g_malloc0(w*h);
    for (int y=0;y<h;y++) for (int x=0;x<w;x++) {
        if (!edge[y*w+x]) continue;
        for (int dy=-2;dy<=2;dy++) for (int dx=-2;dx<=2;dx++) {
            int nx=x+dx, ny=y+dy;
            if (nx>=0&&ny>=0&&nx<w&&ny<h) dil[ny*w+nx]=1;
        }
    }

    /* connected components (4-conn) flood fill -> bounding boxes */
    int *comp = g_malloc0(sizeof(int)*w*h);
    int ncomp=0;
    Box boxes[MAX_ELEMS*4];
    int nbox=0;
    /* use iterative BFS with simple queue */
    for (int sy=0;sy<h;sy++) for (int sx=0;sx<w;sx++) {
        if (!dil[sy*w+sx] || comp[sy*w+sx]) continue;
        /* BFS */
        int minx=sx,maxx=sx,miny=sy,maxy=sy;
        /* stack */
        int *qx = g_malloc(sizeof(int)*w*h);
        int *qy = g_malloc(sizeof(int)*w*h);
        int qh=0, qt=0;
        qx[qt]=sx; qy[qt]=sy; qt++;
        comp[sy*w+sx]=++ncomp;
        while (qh<qt) {
            int cx=qx[qh], cy=qy[qh]; qh++;
            const int dx[4]={1,-1,0,0};
            const int dy[4]={0,0,1,-1};
            for (int d=0;d<4;d++) {
                int nx=cx+dx[d], ny=cy+dy[d];
                if (nx<0||ny<0||nx>=w||ny>=h) continue;
                if (!dil[ny*w+nx] || comp[ny*w+nx]) continue;
                comp[ny*w+nx]=ncomp;
                qx[qt]=nx; qy[qt]=ny; qt++;
                if (nx<minx) minx=nx;
                if (nx>maxx) maxx=nx;
                if (ny<miny) miny=ny;
                if (ny>maxy) maxy=ny;
            }
        }
        g_free(qx); g_free(qy);
        int bw = maxx-minx+1, bh = maxy-miny+1;
        if (bw < 12 || bh < 12) continue;
        if (bw > w*0.85 || bh > h*0.85) continue;
        if (nbox < MAX_ELEMS*4) {
            boxes[nbox].x=minx; boxes[nbox].y=miny; boxes[nbox].w=bw; boxes[nbox].h=bh; nbox++;
        }
    }

    /* IoU dedup: keep smaller on high overlap */
    /* sort by area asc */
    for (int i=0;i<nbox;i++) for (int j=i+1;j<nbox;j++) {
        int ai=boxes[i].w*boxes[i].h, aj=boxes[j].w*boxes[j].h;
        if (ai>aj){ Box t=boxes[i]; boxes[i]=boxes[j]; boxes[j]=t; }
    }
    int kept=0;
    for (int i=0;i<nbox && kept<maxout;i++) {
        int ok=1;
        for (int k=0;k<kept;k++) {
            if (iou_box(boxes[i], out[k]) > 85) { ok=0; break; }
        }
        if (ok) { out[kept++]=boxes[i]; }
    }

    g_free(gray); g_free(mag); g_free(edge); g_free(dil); g_free(comp);
    return kept;
}

static double region_contrast(GdkPixbuf *pb, Box b) {
    int nch=gdk_pixbuf_get_n_channels(pb);
    int stride=gdk_pixbuf_get_rowstride(pb);
    const guchar *px=gdk_pixbuf_get_pixels(pb);
    long r=0,g=0,bl=0,n=0;
    for (int y=b.y;y<b.y+b.h && y<gdk_pixbuf_get_height(pb);y++)
      for (int x=b.x;x<b.x+b.w && x<gdk_pixbuf_get_width(pb);x++){
        int i=y*stride+x*nch;
        r+=px[i];g+=px[i+1];bl+=px[i+2]; n++;
      }
    if (n==0) return 1.0;
    double lum=(0.2126*(r/n)+0.7152*(g/n)+0.0722*(bl/n))/255.0;
    /* contrast vs white text */
    double l1=lum, l2=1.0;
    if (l1>l2){double t=l1;l1=l2;l2=t;}
    return (l2+0.05)/(l1+0.05);
}

static void draw_grid(cairo_t *cr, int w, int h) {
    /* major every 100 units, minor every 50 in 0..1000 normalized */
    /* map normalized 0..1000 -> pixels */
    cairo_set_line_width(cr, 1.0);
    for (int u=0; u<=1000; u+=50) {
        int major = (u % 100 == 0);
        double x = (double)u/1000.0 * w;
        double y = (double)u/1000.0 * h;
        /* vertical */
        cairo_set_source_rgba(cr, 1,1,1,0.25);
        cairo_move_to(cr, x,0); cairo_line_to(cr, x,h); cairo_stroke(cr);
        cairo_set_source_rgba(cr, 0,0,0,0.5);
        cairo_move_to(cr, x,0); cairo_line_to(cr, x,h); cairo_stroke(cr);
        /* horizontal */
        cairo_set_source_rgba(cr, 1,1,1,0.25);
        cairo_move_to(cr, 0,y); cairo_line_to(cr, w,y); cairo_stroke(cr);
        cairo_set_source_rgba(cr, 0,0,0,0.5);
        cairo_move_to(cr, 0,y); cairo_line_to(cr, w,y); cairo_stroke(cr);
        if (major) {
            char buf[32];
            /* watermark at crossing with other majors handled below */
            (void)buf;
        }
    }
    /* watermarks at major crossings */
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 9);
    for (int ux=0; ux<=1000; ux+=100) for (int uy=0; uy<=1000; uy+=100) {
        double x=(double)ux/1000.0*w, y=(double)uy/1000.0*h;
        char buf[32]; snprintf(buf,sizeof(buf),"%d,%d",ux,uy);
        cairo_set_source_rgba(cr, 1,1,1,0.7);
        cairo_move_to(cr, x+2, y+10); cairo_show_text(cr, buf);
        cairo_set_source_rgba(cr, 0,0,0,0.8);
        cairo_move_to(cr, x+1, y+9); cairo_show_text(cr, buf);
    }
}

static int run_cv(GdkPixbuf *pb, MiimeGroundingRegistry *reg, cairo_surface_t *surf, int visual_aids) {
    int w = gdk_pixbuf_get_width(pb);
    int h = gdk_pixbuf_get_height(pb);
    Box boxes[MAX_ELEMS];
    int n = detect_contours(pb, boxes, MAX_ELEMS);

    cairo_t *cr = cairo_create(surf);
    if (visual_aids) draw_grid(cr, w, h);

    /* badges */
    /* keep badge rects for collision */
    typedef struct { double x,y,w,h; } R;
    R badges[MAX_ELEMS]; int nb=0;

    for (int i=0;i<n;i++) {
        MiimeGroundingElement el;
        memset(&el,0,sizeof(el));
        el.id = i+1;
        snprintf(el.tag, sizeof(el.tag), "region");
        el.x=boxes[i].x; el.y=boxes[i].y; el.w=boxes[i].w; el.h=boxes[i].h;
        el.category = 2;
        el.contrast = region_contrast(pb, boxes[i]);
        snprintf(el.telemetry, sizeof(el.telemetry), "cv contour");

        /* capacity */
        if (reg->count >= reg->capacity) {
            reg->capacity = reg->capacity ? reg->capacity*2 : 256;
            reg->items = g_realloc(reg->items, reg->capacity*sizeof(MiimeGroundingElement));
        }
        reg->items[reg->count++] = el;

        if (!visual_aids) continue;
        /* draw outline */
        cairo_set_source_rgba(cr, 1,0,1,0.6);
        cairo_set_line_width(cr, 1.5);
        cairo_rectangle(cr, boxes[i].x, boxes[i].y, boxes[i].w, boxes[i].h);
        cairo_stroke(cr);

        /* badge */
        char label[16]; snprintf(label,sizeof(label),"#%d",el.id);
        double bx = boxes[i].x;
        double by = boxes[i].y - 16;
        if (by < 0) by = boxes[i].y + 2;
        /* collision slide */
        int collide=1, tries=0;
        while (collide && tries < 20) {
            collide=0;
            for (int k=0;k<nb;k++) {
                if (bx < badges[k].x+badges[k].w+4 && bx+40 > badges[k].x &&
                    by < badges[k].y+badges[k].h+4 && by+16 > badges[k].y) { collide=1; break; }
            }
            if (collide) { bx += 44; tries++; if (bx + 40 > w) { bx = boxes[i].x; by += 18; } }
        }
        /* draw pill */
        cairo_set_source_rgba(cr, 1,0,1,0.9);
        cairo_rectangle(cr, bx, by, 36, 14);
        cairo_fill(cr);
        cairo_set_source_rgb(cr, 1,1,1);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 10);
        cairo_move_to(cr, bx+4, by+11);
        cairo_show_text(cr, label);
        badges[nb].x=bx; badges[nb].y=by; badges[nb].w=36; badges[nb].h=14; nb++;
    }
    cairo_destroy(cr);
    return n;
}

int miime_grounding_run(int source_type, const char *source, MiimeViewport viewport,
                        int visual_aids, char **out_buffer_b_path, char **out_registry_json) {
    if (out_buffer_b_path) *out_buffer_b_path = NULL;
    if (out_registry_json) *out_registry_json = NULL;

    char srcpath[4096];
    if (source_type == 0) {
        /* url: render a screenshot with headless Chrome (no Playwright needed) */
        char tmp[4096]; snprintf(tmp,sizeof(tmp),"/tmp/miime_grounding_%d.png", (int)getpid());
        char cmd[8192]; snprintf(cmd,sizeof(cmd),
            "google-chrome-stable --headless --disable-gpu --hide-scrollbars "
            "--window-size=%d,%d --screenshot='%s' '%s' >/dev/null 2>&1",
            viewport.width>0?viewport.width:1440, viewport.height>0?viewport.height:900, tmp, source);
        int rc = system(cmd);
        (void)rc;
        snprintf(srcpath,sizeof(srcpath),"%s",tmp);
    } else {
        snprintf(srcpath,sizeof(srcpath),"%s", source ? source : "");
    }

    GdkPixbuf *pb = load_pixbuf(srcpath);
    if (!pb) return -1;

    int w = gdk_pixbuf_get_width(pb);
    int h = gdk_pixbuf_get_height(pb);
    if (viewport.width > 0 && viewport.height > 0) { w = viewport.width; h = viewport.height; }
    /* resize pixbuf if needed */
    if (w != gdk_pixbuf_get_width(pb) || h != gdk_pixbuf_get_height(pb)) {
        GdkPixbuf *scaled = gdk_pixbuf_scale_simple(pb, w, h, GDK_INTERP_BILINEAR);
        g_object_unref(pb);
        pb = scaled;
    }

    char hashstr[128];
    const guchar *px = gdk_pixbuf_get_pixels(pb);
    char *hsh = file_hash_bytes(px, w, h, gdk_pixbuf_get_rowstride(pb));
    snprintf(hashstr, sizeof(hashstr), "%s", hsh);

    char cachedir[256]; ensure_cache_dir(cachedir, sizeof(cachedir));
    char pngpath[4096]; snprintf(pngpath, sizeof(pngpath), "%s/%.64s.png", cachedir, hashstr);
    char jsonpath[4096]; snprintf(jsonpath, sizeof(jsonpath), "%s/%.64s.json", cachedir, hashstr);

    /* if cached, return cached */
    if (g_file_test(pngpath, G_FILE_TEST_EXISTS) && g_file_test(jsonpath, G_FILE_TEST_EXISTS)) {
        char *json = NULL;
        g_file_get_contents(jsonpath, &json, NULL, NULL);
        if (out_buffer_b_path) *out_buffer_b_path = g_strdup(pngpath);
        if (out_registry_json) *out_registry_json = json ? json : g_strdup("[]");
        g_free(hsh);
        g_object_unref(pb);
        return 0;
    }

    cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    /* copy pixbuf to surface */
    {
        unsigned char *sd = cairo_image_surface_get_data(surf);
        int ss = cairo_image_surface_get_stride(surf);
        int nch = gdk_pixbuf_get_n_channels(pb);
        int rs = gdk_pixbuf_get_rowstride(pb);
        gboolean alpha = gdk_pixbuf_get_has_alpha(pb);
        cairo_surface_flush(surf);
        for (int y=0;y<h;y++){
            const guchar *sr = px + y*rs;
            unsigned char *dr = sd + y*ss;
            for (int x=0;x<w;x++){
                guchar r=sr[x*nch+0], g=sr[x*nch+1], b=sr[x*nch+2];
                guchar a = alpha ? sr[x*nch+3] : 255;
                dr[x*4+0]=(guchar)((b*a)/255);
                dr[x*4+1]=(guchar)((g*a)/255);
                dr[x*4+2]=(guchar)((r*a)/255);
                dr[x*4+3]=a;
            }
        }
        cairo_surface_mark_dirty(surf);
    }

    MiimeGroundingRegistry reg; memset(&reg,0,sizeof(reg));
    reg.capacity=256; reg.items=g_malloc(reg.capacity*sizeof(MiimeGroundingElement));

    run_cv(pb, &reg, surf, visual_aids);

    cairo_surface_write_to_png(surf, pngpath);

    /* build json */
    GString *gs = g_string_new("[");
    for (int i=0;i<reg.count;i++) {
        if (i) g_string_append(gs, ",");
        g_string_append_printf(gs, "{\"id\":%d,\"tag\":\"%s\",\"bounds\":[%d,%d,%d,%d],\"category\":%d,\"contrast\":%.2f,\"telemetry\":\"%s\"}",
            reg.items[i].id, reg.items[i].tag, reg.items[i].x, reg.items[i].y, reg.items[i].w, reg.items[i].h,
            reg.items[i].category, reg.items[i].contrast, reg.items[i].telemetry);
    }
    g_string_append(gs, "]");
    g_file_set_contents(jsonpath, gs->str, -1, NULL);

    if (out_buffer_b_path) *out_buffer_b_path = g_strdup(pngpath);
    if (out_registry_json) *out_registry_json = g_strdup(gs->str);

    g_string_free(gs, TRUE);
    cairo_surface_destroy(surf);
    g_free(reg.items);
    g_object_unref(pb);
    g_free(hsh);
    return 0;
}

void miime_grounding_registry_free(MiimeGroundingRegistry *reg) {
    if (reg && reg->items) g_free(reg->items);
    if (reg) memset(reg,0,sizeof(*reg));
}

char *miime_grounding_image_hash(const unsigned char *data, int len) {
    GChecksum *cs = g_checksum_new(G_CHECKSUM_SHA256);
    g_checksum_update(cs, data, len);
    char *s = g_strdup(g_checksum_get_string(cs));
    g_checksum_free(cs);
    return s;
}
