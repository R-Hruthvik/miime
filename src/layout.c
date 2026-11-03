#include "layout.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static double rect_intersect_area(double ax,double ay,double aw,double ah,
                                  double bx,double by,double bw,double bh) {
    double x0 = ax > bx ? ax : bx;
    double y0 = ay > by ? ay : by;
    double x1 = (ax+aw) < (bx+bw) ? (ax+aw) : (bx+bw);
    double y1 = (ay+ah) < (by+bh) ? (ay+ah) : (by+bh);
    if (x1 <= x0 || y1 <= y0) return 0.0;
    return (x1-x0)*(y1-y0);
}

static void measure_label(const char *text, MiimeMeasureLineFn fn, void *ud,
                          double maxw, double *out_w, double *out_h) {
    char current[4096]; current[0]='\0';
    double maxline = 0.0;
    int lines = 0;
    const char *p = text ? text : "";
    char word[256];
    while (*p) {
        while (*p==' ' || *p=='\t' || *p=='\n') {
            if (*p=='\n') {
                if (current[0]) {
                    double w = fn(current, ud);
                    if (w > maxline) maxline = w;
                    lines++;
                    current[0]='\0';
                }
            }
            p++;
        }
        if (!*p) break;
        int wi = 0;
        while (*p && *p!=' ' && *p!='\t' && *p!='\n' && wi < 255) word[wi++]=*p++;
        word[wi]='\0';
        char cand[8192];
        if (current[0]) snprintf(cand, sizeof(cand), "%s %s", current, word);
        else snprintf(cand, sizeof(cand), "%s", word);
        double cw = fn(cand, ud);
        if (cw <= maxw || !current[0]) {
            strcpy(current, cand);
        } else {
            double w = fn(current, ud);
            if (w > maxline) maxline = w;
            lines++;
            strcpy(current, word);
        }
    }
    if (current[0]) {
        double w = fn(current, ud);
        if (w > maxline) maxline = w;
        lines++;
    }
    if (lines == 0) lines = 1;
    if (maxline == 0.0) maxline = fn(text?text:"", ud);
    if (maxline > maxw) maxline = maxw;
    *out_w = maxline;
    *out_h = lines * 16.0 + 16.0;
}

int miime_layout_mode_select(int img_w, int img_h, int n, const MiimeLayoutItem *items) {
    (void)img_w;(void)img_h;(void)items;
    if (n >= 5) return 1; /* margin_rail */
    /* density heuristic */
    if (items) {
        double area = 0;
        for (int i=0;i<n;i++) area += items[i].target_w*items[i].target_h;
        double total = (double)img_w*img_h;
        if (total>0 && area/total > 0.35) return 1;
    }
    return 2; /* in_canvas */
}

static void liang_barsky(double x0,double y0,double x1,double y1,
                        double xmin,double ymin,double xmax,double ymax,
                        int *hit) {
    double dx=x1-x0, dy=y1-y0;
    double p[4]={-dx,dx,-dy,dy};
    double q[4]={x0-xmin, xmax-x0, y0-ymin, ymax-y0};
    double u1=0, u2=1;
    for (int i=0;i<4;i++) {
        if (p[i]==0) {
            if (q[i]<0) { *hit=0; return; }
        } else {
            double t=q[i]/p[i];
            if (p[i]<0) { if (t>u1) u1=t; }
            else { if (t<u2) u2=t; }
        }
    }
    if (u1>u2) { *hit=0; return; }
    *hit=1;
}

/* port normals */
static void port_point(double rx,double ry,double rw,double rh, int port, double *px, double *py, double *nx, double *ny) {
    /* 0 top mid,1 right mid,2 bottom mid,3 left mid, 4-7 corners */
    switch(port) {
        case 0: *px=rx+rw/2; *py=ry; *nx=0; *ny=-1; break;
        case 1: *px=rx+rw; *py=ry+rh/2; *nx=1; *ny=0; break;
        case 2: *px=rx+rw/2; *py=ry+rh; *nx=0; *ny=1; break;
        case 3: *px=rx; *py=ry+rh/2; *nx=-1; *ny=0; break;
        case 4: *px=rx; *py=ry; *nx=-1; *ny=-1; break;
        case 5: *px=rx+rw; *py=ry; *nx=1; *ny=-1; break;
        case 6: *px=rx+rw; *py=ry+rh; *nx=1; *ny=1; break;
        case 7: *px=rx; *py=ry+rh; *nx=-1; *ny=1; break;
    }
    /* normalize corner normals */
    double len = hypot(*nx,*ny);
    if (len>0) { *nx/=len; *ny/=len; }
}

int miime_layout_run(int img_w, int img_h, MiimeLayoutItem *items, int n,
                      int layout_mode, int aesthetic,
                      MiimeMeasureLineFn measure_line, void *userdata,
                      MiimeLayoutResult *out) {
    if (!items || !out || n<=0 || n>MIIME_LAYOUT_MAX_ITEMS) return -1;
    if (!measure_line) return -1;

    double *lw = calloc(n, sizeof(double));
    double *lh = calloc(n, sizeof(double));
    double *ix = calloc(n, sizeof(double)); /* initial x */
    double *iy = calloc(n, sizeof(double));
    int mode = layout_mode;
    if (mode == 0) mode = miime_layout_mode_select(img_w, img_h, n, items);

    for (int i=0;i<n;i++) {
        measure_label(items[i].text, measure_line, userdata, 240.0, &lw[i], &lh[i]);
    }

    int canvas_w = (mode==1) ? img_w + 600 : img_w;
    int canvas_h = img_h;
    double target_offset = (mode==1) ? 300.0 : 0.0;

    /* placement */
    double *cx = calloc(n, sizeof(double));
    double *cy = calloc(n, sizeof(double));

    if (mode == 1) {
        /* margin rail */
        /* partition */
        int left_idx[MIIME_LAYOUT_MAX_ITEMS], right_idx[MIIME_LAYOUT_MAX_ITEMS];
        int nl=0, nr=0;
        for (int i=0;i<n;i++) {
            double tcx = items[i].target_x + items[i].target_w/2.0;
            if (tcx < img_w/2.0) left_idx[nl++]=i; else right_idx[nr++]=i;
        }
        /* sort each by target center y ascending */
        for (int a=0;a<nl-1;a++) for (int b=a+1;b<nl;b++) {
            double ya = items[left_idx[a]].target_y + items[left_idx[a]].target_h/2.0;
            double yb = items[left_idx[b]].target_y + items[left_idx[b]].target_h/2.0;
            if (ya > yb) { int t=left_idx[a]; left_idx[a]=left_idx[b]; left_idx[b]=t; }
        }
        for (int a=0;a<nr-1;a++) for (int b=a+1;b<nr;b++) {
            double ya = items[right_idx[a]].target_y + items[right_idx[a]].target_h/2.0;
            double yb = items[right_idx[b]].target_y + items[right_idx[b]].target_h/2.0;
            if (ya > yb) { int t=right_idx[a]; right_idx[a]=right_idx[b]; right_idx[b]=t; }
        }
        /* left rail x=8, right rail x=canvas_w-8-lw */
        /* forward sweep left */
        double y = 8.0;
        for (int k=0;k<nl;k++) {
            int i=left_idx[k];
            cx[i]=8.0;
            cy[i]=y;
            y += lh[i] + 12.0;
        }
        /* overflow reverse sweep */
        if (y > canvas_h - 8.0) {
            double yy = canvas_h - 8.0;
            for (int k=nl-1;k>=0;k--) {
                int i=left_idx[k];
                yy -= lh[i];
                cy[i]=yy;
                yy -= 12.0;
                if (cy[i] < 8.0) cy[i]=8.0;
            }
        }
        y = 8.0;
        for (int k=0;k<nr;k++) {
            int i=right_idx[k];
            cx[i]=canvas_w - 8.0 - lw[i];
            cy[i]=y;
            y += lh[i] + 12.0;
        }
        if (y > canvas_h - 8.0) {
            double yy = canvas_h - 8.0;
            for (int k=nr-1;k>=0;k--) {
                int i=right_idx[k];
                yy -= lh[i];
                cy[i]=yy;
                yy -= 12.0;
                if (cy[i] < 8.0) cy[i]=8.0;
            }
        }
    } else {
        /* in-canvas floating */
        /* sequential greedy */
        int placed[MIIME_LAYOUT_MAX_ITEMS]; for (int i=0;i<n;i++) placed[i]=0;
        for (int idx=0; idx<n; idx++) {
            /* choose item order by area descending? keep input order */
            int i = idx;
            /* generate 8 candidates */
            double tx=items[i].target_x, ty=items[i].target_y, tw=items[i].target_w, th=items[i].target_h;
            double cands[8][2];
            /* positions: top, top-right, right, bottom-right, bottom, bottom-left, left, top-left */
            cands[0][0]=tx+(tw-lw[i])/2.0; cands[0][1]=ty-lh[i]-24.0;
            cands[1][0]=tx+tw+24.0; cands[1][1]=ty-lh[i]-24.0+th/2.0?0:0; // adjust
            /* simplify: 8 around */
            cands[1][0]=tx+tw+24.0; cands[1][1]=ty+(th-lh[i])/2.0;
            cands[2][0]=tx+tw+24.0; cands[2][1]=ty+th+24.0-lh[i];
            cands[3][0]=tx+(tw-lw[i])/2.0; cands[3][1]=ty+th+24.0;
            cands[4][0]=tx-lw[i]-24.0; cands[4][1]=ty+th+24.0-lh[i];
            cands[5][0]=tx-lw[i]-24.0; cands[5][1]=ty+(th-lh[i])/2.0;
            cands[6][0]=tx-lw[i]-24.0; cands[6][1]=ty-lh[i]-24.0;
            cands[7][0]=tx+(tw-lw[i])/2.0; cands[7][1]=ty-lh[i]-24.0-0; // duplicate top; vary
            /* Actually make 8 distinct: adjust last */
            cands[7][0]=tx+tw+24.0; cands[7][1]=ty-lh[i]-24.0;

            double bestscore=1e18; int bestk=0;
            for (int k=0;k<8;k++) {
                double x=cands[k][0], y=cands[k][1];
                double score=0;
                /* OOB */
                double ox=0, oy=0;
                if (x<0) ox += -x;
                if (y<0) oy += -y;
                if (x+lw[i]>img_w) ox += (x+lw[i]-img_w);
                if (y+lh[i]>img_h) oy += (y+lh[i]-img_h);
                score += 1000.0*(ox+oy)*10.0;
                if (ox>0||oy>0) score += 1000.0;
                /* target overlap with all targets */
                for (int t=0;t<n;t++) {
                    double a = rect_intersect_area(x,y,lw[i],lh[i],
                        items[t].target_x+target_offset,items[t].target_y,items[t].target_w,items[t].target_h);
                    score += 1000.0*a;
                }
                /* label overlap with placed */
                for (int t=0;t<n;t++) {
                    if (!placed[t] || t==i) continue;
                    double a = rect_intersect_area(x,y,lw[i],lh[i], cx[t],cy[t],lw[t],lh[t]);
                    score += 500.0*a;
                }
                if (score < bestscore) { bestscore=score; bestk=k; }
            }
            cx[i]=cands[bestk][0];
            cy[i]=cands[bestk][1];
            /* clamp */
            if (cx[i]<8) cx[i]=8;
            if (cy[i]<8) cy[i]=8;
            if (cx[i]+lw[i]>img_w-8) cx[i]=img_w-8-lw[i];
            if (cy[i]+lh[i]>img_h-8) cy[i]=img_h-8-lh[i];
            if (cx[i]<8) cx[i]=8;
            if (cy[i]<8) cy[i]=8;
            placed[i]=1;
        }
    }

    /* store initial */
    for (int i=0;i<n;i++) { ix[i]=cx[i]; iy[i]=cy[i]; }

    /* force relaxation */
    for (int it=0; it<15; it++) {
        for (int i=0;i<n;i++) {
            double fx=0, fy=0;
            /* spring anchor */
            fx += 0.1*(ix[i]-cx[i]);
            fy += 0.1*(iy[i]-cy[i]);
            /* repulsion label-label */
            for (int j=0;j<n;j++) {
                if (i==j) continue;
                double a = rect_intersect_area(cx[i],cy[i],lw[i],lh[i], cx[j],cy[j],lw[j],lh[j]);
                if (a>0) {
                    double dx = (cx[i]+lw[i]/2)-(cx[j]+lw[j]/2);
                    double dy = (cy[i]+lh[i]/2)-(cy[j]+lh[j]/2);
                    double d = hypot(dx,dy); if (d<1) d=1;
                    double push = (a/ (lw[i]*lh[i]+1)) * 12.0 + 12.0;
                    fx += (dx/d)*push;
                    fy += (dy/d)*push;
                }
            }
            /* repulsion label-target */
            for (int t=0;t<n;t++) {
                if (t==i) continue;
                double a = rect_intersect_area(cx[i],cy[i],lw[i],lh[i],
                    items[t].target_x+target_offset,items[t].target_y,items[t].target_w,items[t].target_h);
                if (a>0) {
                    double dx = (cx[i]+lw[i]/2)-(items[t].target_x+target_offset+items[t].target_w/2);
                    double dy = (cy[i]+lh[i]/2)-(items[t].target_y+items[t].target_h/2);
                    double d = hypot(dx,dy); if (d<1) d=1;
                    fx += (dx/d)*12.0;
                    fy += (dy/d)*12.0;
                }
            }
            cx[i]+=fx*0.5;
            cy[i]+=fy*0.5;
            /* clamp */
            if (cx[i]<8) cx[i]=8;
            if (cy[i]<8) cy[i]=8;
            if (cx[i]+lw[i]>canvas_w-8) cx[i]=canvas_w-8-lw[i];
            if (cy[i]+lh[i]>canvas_h-8) cy[i]=canvas_h-8-lh[i];
            if (cx[i]<8) cx[i]=8;
            if (cy[i]<8) cy[i]=8;
        }
    }

    /* leader routing */
    for (int i=0;i<n;i++) {
        double tx=items[i].target_x+target_offset, ty=items[i].target_y, tw=items[i].target_w, th=items[i].target_h;
        double lx=cx[i], ly=cy[i], lwv=lw[i], lhv=lh[i];
        /* try all port pairs */
        int bestTi=0, bestLi=0; double bestD=1e18; int found=0;
        for (int tp=0; tp<8; tp++) {
            if (items[i].preferred_anchor >= 0 && tp != items[i].preferred_anchor) continue;
            for (int lp=0; lp<4; lp++) {
                double tpx,tpy,tnx,tny, lpx,lpy,lnx,lny;
                port_point(tx,ty,tw,th,tp,&tpx,&tpy,&tnx,&tny);
                port_point(lx,ly,lwv,lhv,lp,&lpx,&lpy,&lnx,&lny);
                double vx=lpx-tpx, vy=lpy-tpy;
                double d=hypot(vx,vy);
                /* outward normal constraint */
                double dot1 = vx*tnx + vy*tny;
                double dot2 = (-vx)*lnx + (-vy)*lny;
                if (dot1 >= -0.01 && dot2 >= -0.01) {
                    if (d < bestD) { bestD=d; bestTi=tp; bestLi=lp; found=1; }
                }
            }
        }
        if (!found) {
            /* fallback min dist */
            for (int tp=0; tp<8; tp++) for (int lp=0; lp<4; lp++) {
                if (items[i].preferred_anchor >= 0 && tp != items[i].preferred_anchor) continue;
                double tpx,tpy,tnx,tny, lpx,lpy,lnx,lny;
                port_point(tx,ty,tw,th,tp,&tpx,&tpy,&tnx,&tny);
                port_point(lx,ly,lwv,lhv,lp,&lpx,&lpy,&lnx,&lny);
                double d=hypot(lpx-tpx,lpy-tpy);
                if (d<bestD) { bestD=d; bestTi=tp; bestLi=lp; found=1; }
            }
        }
        double tpx,tpy,tnx,tny, lpx,lpy,lnx,lny;
        port_point(tx,ty,tw,th,bestTi,&tpx,&tpy,&tnx,&tny);
        port_point(lx,ly,lwv,lhv,bestLi,&lpx,&lpy,&lnx,&lny);

        /* obstacle avoidance check: segment vs all target rects (padded) */
        int hit=0;
        for (int t=0;t<n;t++) {
            if (t == i) continue;
            double rx=items[t].target_x+target_offset-6, ry=items[t].target_y-6;
            double rw=items[t].target_w+12, rh=items[t].target_h+12;
            int h; liang_barsky(tpx,tpy,lpx,lpy, rx,ry,rx+rw,ry+rh,&h);
            if (h) { hit=1; break; }
        }
        /* also label rects */
        for (int t=0;t<n;t++) {
            if (t==i) continue;
            /* label rect of t approximated by cx,cy */
        }

        out[i].label_x=cx[i]; out[i].label_y=cy[i];
        out[i].label_w=lw[i]; out[i].label_h=lh[i];
        out[i].port_target_x=tpx; out[i].port_target_y=tpy;
        out[i].port_label_x=lpx; out[i].port_label_y=lpy;
        out[i].curve_type = (aesthetic==1) ? 1 : 0;
        out[i].obstacle_detour = hit;
        out[i].z_index = 5;
        out[i].mode = (mode==1)?0:1;

        double dist = hypot(lpx-tpx, lpy-tpy);
        if (out[i].curve_type == 0) {
            /* S-curve cubic bezier */
            out[i].cps[0]=tpx; out[i].cps[1]=tpy;
            out[i].cps[2]=tpx + tnx*0.4*dist;
            out[i].cps[3]=tpy + tny*0.4*dist;
            out[i].cps[4]=lpx - lnx*0.4*dist;
            out[i].cps[5]=lpy - lny*0.4*dist;
            out[i].cps[6]=lpx; out[i].cps[7]=lpy;
        } else {
            /* elbow: stub 20 then corner */
            double sx=tpx+tnx*20.0, sy=tpy+tny*20.0;
            out[i].cps[0]=tpx; out[i].cps[1]=tpy;
            out[i].cps[2]=sx; out[i].cps[3]=sy;
            /* corner aligned to label */
            double ex=lpx-lnx*20.0, ey=lpy-lny*20.0;
            out[i].cps[4]= (fabs(tnx)>0.5)? ex : sx; // horizontal first
            out[i].cps[5]= (fabs(tnx)>0.5)? sy : ey;
            out[i].cps[6]=lpx; out[i].cps[7]=lpy;
        }
        if (hit) {
            /* detour: shift cp1/cp2 toward padded corner midpoint */
            double mx = (tpx+lpx)/2.0;
            double my = (tpy+lpy)/2.0;
            out[i].cps[2]=mx; out[i].cps[3]=my-20;
            out[i].cps[4]=mx; out[i].cps[5]=my+20;
        }
    }

    free(lw); free(lh); free(ix); free(iy); free(cx); free(cy);
    return 0;
}
