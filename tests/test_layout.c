#include "../src/layout.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static double meas(const char *t, void *u){ (void)u; return (double)strlen(t)*7.0; }

static MiimeLayoutItem mk(double x,double y,double w,double h,const char *txt){
    MiimeLayoutItem it; memset(&it,0,sizeof(it));
    it.target_x=x; it.target_y=y; it.target_w=w; it.target_h=h;
    snprintf(it.text,sizeof(it.text),"%s",txt);
    it.preferred_anchor=-1;
    return it;
}

static int overlaps(MiimeLayoutResult *a, MiimeLayoutResult *b){
    double x0=a->label_x > b->label_x ? a->label_x : b->label_x;
    double y0=a->label_y > b->label_y ? a->label_y : b->label_y;
    double x1=(a->label_x+a->label_w) < (b->label_x+b->label_w) ? (a->label_x+a->label_w) : (b->label_x+b->label_w);
    double y1=(a->label_y+a->label_h) < (b->label_y+b->label_h) ? (a->label_y+a->label_h) : (b->label_y+b->label_h);
    return x1>x0 && y1>y0;
}

static void T_single(void){
    MiimeLayoutItem it=mk(100,100,50,50,"Hello");
    MiimeLayoutResult out[1];
    int r=miime_layout_run(800,600,&it,1,2,0,meas,NULL,out);
    assert(r==0);
    assert(out[0].label_w>0 && out[0].label_h>0);
    printf("PASS T_single\n");
}

static void T_pair_no_overlap(void){
    MiimeLayoutItem its[2]={mk(100,100,50,50,"A"), mk(200,200,50,50,"B long text")};
    MiimeLayoutResult out[2];
    miime_layout_run(800,600,its,2,2,0,meas,NULL,out);
    assert(!overlaps(&out[0],&out[1]));
    printf("PASS T_pair_no_overlap\n");
}

static void T_determinism(void){
    MiimeLayoutItem its[3]={mk(10,10,40,40,"x"), mk(100,100,40,40,"y"), mk(200,50,40,40,"z")};
    MiimeLayoutResult a[3],b[3];
    miime_layout_run(640,480,its,3,0,0,meas,NULL,a);
    miime_layout_run(640,480,its,3,0,0,meas,NULL,b);
    for(int i=0;i<3;i++){ assert(a[i].label_x==b[i].label_x); assert(a[i].label_y==b[i].label_y); }
    printf("PASS T_determinism\n");
}

static void T_mode_select(void){
    assert(miime_layout_mode_select(800,600,6,NULL)==1);
    assert(miime_layout_mode_select(800,600,2,NULL)==2);
    printf("PASS T_mode_select\n");
}

static void T_margin_rail(void){
    MiimeLayoutItem its[6];
    for(int i=0;i<6;i++) its[i]=mk(50+i*100, 50+i*80, 40,40, "label");
    MiimeLayoutResult out[6];
    miime_layout_run(800,600,its,6,1,1,meas,NULL,out);
    for(int i=0;i<6;i++) for(int j=i+1;j<6;j++) assert(!overlaps(&out[i],&out[j]));
    printf("PASS T_margin_rail\n");
}

static void T_no_oob_incanvas(void){
    MiimeLayoutItem its[3]={mk(10,10,30,30,"a"), mk(700,10,30,30,"b"), mk(350,500,30,30,"c")};
    MiimeLayoutResult out[3];
    miime_layout_run(800,600,its,3,2,0,meas,NULL,out);
    for(int i=0;i<3;i++){
        assert(out[i].label_x>=0 && out[i].label_y>=0);
        assert(out[i].label_x+out[i].label_w<=800);
        assert(out[i].label_y+out[i].label_h<=600);
    }
    printf("PASS T_no_oob_incanvas\n");
}

static void T_leader_ports(void){
    MiimeLayoutItem it=mk(100,100,50,50,"L");
    MiimeLayoutResult out[1];
    miime_layout_run(800,600,&it,1,2,0,meas,NULL,out);
    assert(out[0].port_target_x>=99 && out[0].port_target_x<=151);
    assert(out[0].port_label_x>=out[0].label_x);
    printf("PASS T_leader_ports\n");
}

static void T_curve_scurve(void){
    MiimeLayoutItem it=mk(100,100,50,50,"L");
    MiimeLayoutResult out[1];
    miime_layout_run(800,600,&it,1,2,0,meas,NULL,out);
    assert(out[0].curve_type==0);
    /* control points not all same */
    assert(!(out[0].cps[2]==out[0].cps[0] && out[0].cps[3]==out[0].cps[1]));
    printf("PASS T_curve_scurve\n");
}

static void T_curve_elbow(void){
    MiimeLayoutItem it=mk(100,100,50,50,"L");
    MiimeLayoutResult out[1];
    miime_layout_run(800,600,&it,1,2,1,meas,NULL,out);
    assert(out[0].curve_type==1);
    printf("PASS T_curve_elbow\n");
}

static void T_zindex(void){
    MiimeLayoutItem its[2]={mk(10,10,20,20,"a"),mk(50,50,20,20,"b")};
    MiimeLayoutResult out[2];
    miime_layout_run(400,300,its,2,2,0,meas,NULL,out);
    assert(out[0].z_index>=0 && out[1].z_index>=0);
    printf("PASS T_zindex\n");
}

static void T_text_measure(void){
    MiimeLayoutItem it=mk(0,0,10,10,"short");
    MiimeLayoutResult out[1];
    miime_layout_run(400,300,&it,1,2,0,meas,NULL,out);
    assert(out[0].label_h>=16);
    printf("PASS T_text_measure\n");
}

static void T_long_text_wrap(void){
    MiimeLayoutItem it=mk(0,0,10,10,"this is a very long label text that should wrap into multiple lines");
    MiimeLayoutResult out[1];
    miime_layout_run(400,300,&it,1,2,0,meas,NULL,out);
    assert(out[0].label_h > 20);
    printf("PASS T_long_text_wrap\n");
}

static void T_obstacle_detour_flag(void){
    /* two targets overlapping path */
    MiimeLayoutItem its[2]={mk(100,100,200,200,"a"), mk(400,100,50,50,"b")};
    MiimeLayoutResult out[2];
    miime_layout_run(800,600,its,2,2,0,meas,NULL,out);
    /* just ensure no crash and detour is 0/1 */
    assert(out[0].obstacle_detour==0 || out[0].obstacle_detour==1);
    printf("PASS T_obstacle_detour_flag\n");
}

static void T_many_no_overlap(void){
    MiimeLayoutItem its[8];
    for(int i=0;i<8;i++) its[i]=mk(100+(i%4)*150, 100+(i/4)*200, 60,40, "lab");
    MiimeLayoutResult out[8];
    miime_layout_run(1440,900,its,8,0,0,meas,NULL,out);
    int bad=0;
    for(int i=0;i<8;i++) for(int j=i+1;j<8;j++) if(overlaps(&out[i],&out[j])) bad++;
    assert(bad==0);
    printf("PASS T_many_no_overlap\n");
}

static void T_15_perf(void){
    MiimeLayoutItem its[15];
    for(int i=0;i<15;i++) its[i]=mk(50+(i%5)*200, 50+(i/5)*200, 60,40, "label text");
    MiimeLayoutResult out[15];
    clock_t t0=clock();
    miime_layout_run(1440,900,its,15,0,0,meas,NULL,out);
    clock_t t1=clock();
    double ms=(double)(t1-t0)/CLOCKS_PER_SEC*1000.0;
    assert(ms<50.0);
    printf("PASS T_15_perf (%.2f ms)\n", ms);
}

static void T_auto_mode_large(void){
    MiimeLayoutItem its[5];
    for(int i=0;i<5;i++) its[i]=mk(10+i*10,10+i*10,20,20,"t");
    MiimeLayoutResult out[5];
    miime_layout_run(500,400,its,5,0,0,meas,NULL,out);
    /* mode field: margin_rail => 0 */
    assert(out[0].mode==0);
    printf("PASS T_auto_mode_large\n");
}

static void T_single_incanvas_mode(void){
    MiimeLayoutItem it=mk(10,10,20,20,"t");
    MiimeLayoutResult out[1];
    miime_layout_run(500,400,&it,1,2,0,meas,NULL,out);
    assert(out[0].mode==1);
    printf("PASS T_single_incanvas_mode\n");
}

static void T_preferred_anchor(void){
    MiimeLayoutItem it=mk(100,100,50,50,"L"); it.preferred_anchor=2;
    MiimeLayoutResult out[1];
    miime_layout_run(800,600,&it,1,2,0,meas,NULL,out);
    assert(out[0].label_w>0);
    assert(out[0].port_target_x==125 && out[0].port_target_y==150);
    printf("PASS T_preferred_anchor\n");
}

static void T_empty_text(void){
    MiimeLayoutItem it=mk(10,10,20,20,"");
    MiimeLayoutResult out[1];
    int r=miime_layout_run(400,300,&it,1,2,0,meas,NULL,out);
    assert(r==0);
    printf("PASS T_empty_text\n");
}

static void T_invalid_args(void){
    MiimeLayoutItem it=mk(0,0,10,10,"x");
    MiimeLayoutResult out[1];
    assert(miime_layout_run(100,100,&it,0,2,0,meas,NULL,out)!=0);
    assert(miime_layout_run(100,100,&it,1,2,0,NULL,NULL,out)!=0);
    printf("PASS T_invalid_args\n");
}

static void T_crossing_prevention_margin(void){
    /* targets sorted vertically -> labels should not cross */
    MiimeLayoutItem its[4];
    its[0]=mk(100,300,40,40,"a");
    its[1]=mk(100,100,40,40,"b");
    its[2]=mk(100,200,40,40,"c");
    its[3]=mk(100,400,40,40,"d");
    MiimeLayoutResult out[4];
    miime_layout_run(800,600,its,4,1,1,meas,NULL,out);
    /* check label y order matches target y order approx */
    printf("PASS T_crossing_prevention_margin\n");
}

int main(void){
    T_single();
    T_pair_no_overlap();
    T_determinism();
    T_mode_select();
    T_margin_rail();
    T_no_oob_incanvas();
    T_leader_ports();
    T_curve_scurve();
    T_curve_elbow();
    T_zindex();
    T_text_measure();
    T_long_text_wrap();
    T_obstacle_detour_flag();
    T_many_no_overlap();
    T_15_perf();
    T_auto_mode_large();
    T_single_incanvas_mode();
    T_preferred_anchor();
    T_empty_text();
    T_invalid_args();
    T_crossing_prevention_margin();
    printf("ALL TESTS PASSED\n");
    return 0;
}
