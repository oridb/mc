/* CFLAGS: -I/usr/include/ */
/* CFLAGS: -I/usr/include */
/* LIBS: c */

#include <stdint.h>
#include "005_types.h"

extern t9 fn_1_myr(t2 a1, uint32_t a2, t4 a3, t2 a4, t7 a5, float a6, double a7, t8 a8);
extern t17 fn_2_myr(t12 a1, t14 a2, t7 a3, float a4, t15 a5, t14 a6, uint8_t a7);
extern uint32_t fn_3_myr(t15 a1, t23 a2, t21 a3, uint8_t a4, uint8_t a5, t27 a6);
extern uint64_t fn_4_myr(t30 a1);
extern double fn_5_myr(t33 a1, t34 a2, t36 a3, t37 a4, uint64_t a5);
extern t43 fn_6_myr(t40 a1, t41 a2, t34 a3, t42 a4, double a5, double a6, uint64_t a7, uint64_t a8, t26 a9);
extern uint32_t fn_7_myr(t48 a1);
extern float fn_8_myr(t1 a1, t50 a2, t51 a3, float a4, t52 a5, uint32_t a6, t53 a7, uint8_t a8);
extern uint64_t fn_9_myr(t51 a1, float a2, float a3, t56 a4, t58 a5, t61 a6, t65 a7, t66 a8, t67 a9, double a10);
extern float fn_10_myr(t74 a1, float a2, t75 a3, uint8_t a4, t78 a5, t79 a6);

t9
fn_1_c(t2 a1, uint32_t a2, t4 a3, t2 a4, t7 a5, float a6, double a7, t8 a8)
{
    if (!((a1.field_1==0.0625) && ((a1.field_2.field_1==112) && (a1.field_2.field_2==-0.625) && (a1.field_2.field_3==110) && (a1.field_2.field_4==44)))) {
        goto bad;
    }

    if (!(a2==2189228706)) {
        goto bad;
    }

    if (!(((a3.field_1.field_1==-0.84375) && ((a3.field_1.field_2.field_1==150) && (a3.field_1.field_2.field_2==0.359375) && (a3.field_1.field_2.field_3==172) && (a3.field_1.field_2.field_4==26))) && ((a3.field_2.field_1==241023379308544) && (a3.field_2.field_2==115) && ((a3.field_2.field_3.field_1==228) && (a3.field_2.field_3.field_2==0.65625) && (a3.field_2.field_3.field_3==247) && (a3.field_2.field_3.field_4==84))) && ((a3.field_3.field_1==3134223810560) && (a3.field_3.field_2==102) && ((a3.field_3.field_3.field_1==128) && (a3.field_3.field_3.field_2==-1.21875) && (a3.field_3.field_3.field_3==232) && (a3.field_3.field_3.field_4==105))))) {
        goto bad;
    }

    if (!((a4.field_1==-1.203125) && ((a4.field_2.field_1==110) && (a4.field_2.field_2==0.6875) && (a4.field_2.field_3==119) && (a4.field_2.field_4==200)))) {
        goto bad;
    }

    if (!(((a5.field_1.field_1==94503966736384) && (a5.field_1.field_2==2770153426) && (a5.field_1.field_3==187) && (a5.field_1.field_4==214) && (a5.field_1.field_5==213410253897728)) && (a5.field_2==164) && (a5.field_3==E_6_2))) {
        goto bad;
    }

    if (!(a6==1.53125)) {
        goto bad;
    }

    if (!(a7==1.3984375)) {
        goto bad;
    }

    if (!((((a8.field_1.field_1.field_1==0.578125) && ((a8.field_1.field_1.field_2.field_1==170) && (a8.field_1.field_1.field_2.field_2==1.09375) && (a8.field_1.field_1.field_2.field_3==193) && (a8.field_1.field_1.field_2.field_4==148))) && ((a8.field_1.field_2.field_1==271021012025344) && (a8.field_1.field_2.field_2==173) && ((a8.field_1.field_2.field_3.field_1==241) && (a8.field_1.field_2.field_3.field_2==1.46875) && (a8.field_1.field_2.field_3.field_3==80) && (a8.field_1.field_2.field_3.field_4==71))) && ((a8.field_1.field_3.field_1==86092233441280) && (a8.field_1.field_3.field_2==245) && ((a8.field_1.field_3.field_3.field_1==3) && (a8.field_1.field_3.field_3.field_2==-1.125) && (a8.field_1.field_3.field_3.field_3==183) && (a8.field_1.field_3.field_3.field_4==168)))) && ((a8.field_2.field_1==110146554691584) && (a8.field_2.field_2==246) && ((a8.field_2.field_3.field_1==164) && (a8.field_2.field_3.field_2==0.8125) && (a8.field_2.field_3.field_3==164) && (a8.field_2.field_3.field_4==89))) && (a8.field_3==0.28125) && (a8.field_4==E_6_1))) {
        goto bad;
    }

    return (t9) {.field_1=(t1) {.field_1=192,.field_2=-1.296875,.field_3=50,.field_4=129},.field_2=(t3) {.field_1=188458318561280,.field_2=40,.field_3=(t1) {.field_1=47,.field_2=0.359375,.field_3=32,.field_4=127}}};

bad:
    return (t9) {.field_1=(t1) {.field_1=228,.field_2=-0.203125,.field_3=117,.field_4=231},.field_2=(t3) {.field_1=162396161703936,.field_2=44,.field_3=(t1) {.field_1=143,.field_2=1.46875,.field_3=35,.field_4=52}}};
}

t17
fn_2_c(t12 a1, t14 a2, t7 a3, float a4, t15 a5, t14 a6, uint8_t a7)
{
    if (!((a1.field_1==2.7734375) && ((a1.field_2.field_1==258260610842624) && (((a1.field_2.field_2.field_1.field_1==67688764407808) && (a1.field_2.field_2.field_1.field_2==122501974) && (a1.field_2.field_2.field_1.field_3==122) && (a1.field_2.field_2.field_1.field_4==53) && (a1.field_2.field_2.field_1.field_5==46816349519872)) && (a1.field_2.field_2.field_2==175) && (a1.field_2.field_2.field_3==E_6_7)) && (((a1.field_2.field_3.field_1.field_1==242) && (a1.field_2.field_3.field_1.field_2==-0.828125) && (a1.field_2.field_3.field_1.field_3==64) && (a1.field_2.field_3.field_1.field_4==212)) && ((a1.field_2.field_3.field_2.field_1==175981577109504) && (a1.field_2.field_3.field_2.field_2==146) && ((a1.field_2.field_3.field_2.field_3.field_1==58) && (a1.field_2.field_3.field_2.field_3.field_2==-1.3125) && (a1.field_2.field_3.field_2.field_3.field_3==81) && (a1.field_2.field_3.field_2.field_3.field_4==249))))) && (((a1.field_3.field_1.field_1==-0.671875) && ((a1.field_3.field_1.field_2.field_1==39) && (a1.field_3.field_1.field_2.field_2==1.3125) && (a1.field_3.field_1.field_2.field_3==255) && (a1.field_3.field_1.field_2.field_4==168))) && (a1.field_3.field_2==5680861020160) && (a1.field_3.field_3==3.1015625)))) {
        goto bad;
    }

    if (!((a2.field_1==-0.65625) && ((a2.field_2.field_1==-3.07421875) && (a2.field_2.field_2==2364459620) && (a2.field_2.field_3==34004063092736) && (((a2.field_2.field_4.field_1.field_1==0.734375) && ((a2.field_2.field_4.field_1.field_2.field_1==31) && (a2.field_2.field_4.field_1.field_2.field_2==-0.46875) && (a2.field_2.field_4.field_1.field_2.field_3==207) && (a2.field_2.field_4.field_1.field_2.field_4==112))) && ((a2.field_2.field_4.field_2.field_1==91868031090688) && (a2.field_2.field_4.field_2.field_2==24) && ((a2.field_2.field_4.field_2.field_3.field_1==152) && (a2.field_2.field_4.field_2.field_3.field_2==-0.796875) && (a2.field_2.field_4.field_2.field_3.field_3==24) && (a2.field_2.field_4.field_2.field_3.field_4==64))) && ((a2.field_2.field_4.field_3.field_1==74808418369536) && (a2.field_2.field_4.field_3.field_2==254) && ((a2.field_2.field_4.field_3.field_3.field_1==6) && (a2.field_2.field_4.field_3.field_3.field_2==0.046875) && (a2.field_2.field_4.field_3.field_3.field_3==73) && (a2.field_2.field_4.field_3.field_3.field_4==208))))))) {
        goto bad;
    }

    if (!(((a3.field_1.field_1==145965008551936) && (a3.field_1.field_2==2498931774) && (a3.field_1.field_3==156) && (a3.field_1.field_4==179) && (a3.field_1.field_5==265737065725952)) && (a3.field_2==244) && (a3.field_3==E_6_5))) {
        goto bad;
    }

    if (!(a4==-0.796875)) {
        goto bad;
    }

    if (!(a5==E_15_4)) {
        goto bad;
    }

    if (!((a6.field_1==0.6875) && ((a6.field_2.field_1==-2.171875) && (a6.field_2.field_2==1533036440) && (a6.field_2.field_3==12142327889920) && (((a6.field_2.field_4.field_1.field_1==-0.5625) && ((a6.field_2.field_4.field_1.field_2.field_1==243) && (a6.field_2.field_4.field_1.field_2.field_2==-0.65625) && (a6.field_2.field_4.field_1.field_2.field_3==106) && (a6.field_2.field_4.field_1.field_2.field_4==51))) && ((a6.field_2.field_4.field_2.field_1==156078303608832) && (a6.field_2.field_4.field_2.field_2==104) && ((a6.field_2.field_4.field_2.field_3.field_1==58) && (a6.field_2.field_4.field_2.field_3.field_2==-1.34375) && (a6.field_2.field_4.field_2.field_3.field_3==178) && (a6.field_2.field_4.field_2.field_3.field_4==10))) && ((a6.field_2.field_4.field_3.field_1==140200370896896) && (a6.field_2.field_4.field_3.field_2==33) && ((a6.field_2.field_4.field_3.field_3.field_1==143) && (a6.field_2.field_4.field_3.field_3.field_2==-1.3125) && (a6.field_2.field_4.field_3.field_3.field_3==190) && (a6.field_2.field_4.field_3.field_3.field_4==67))))))) {
        goto bad;
    }

    if (!(a7==179)) {
        goto bad;
    }

    return (t17) {.field_1=(t16) {.field_1=101589888270336,.field_2=(t1) {.field_1=225,.field_2=-0.046875,.field_3=234,.field_4=74},.field_3=200069064425472,.field_4=1.53125},.field_2=1.53125};

bad:
    return (t17) {.field_1=(t16) {.field_1=211711070699520,.field_2=(t1) {.field_1=123,.field_2=-1.203125,.field_3=79,.field_4=144},.field_3=242499850862592,.field_4=0.234375},.field_2=0.28125};
}

uint32_t
fn_3_c(t15 a1, t23 a2, t21 a3, uint8_t a4, uint8_t a5, t27 a6)
{
    if (!(a1==E_15_5)) {
        goto bad;
    }

    if (!(((((a2.field_1.field_1.field_1.field_1==208514472345600) && ((a2.field_1.field_1.field_1.field_2.field_1==171) && (a2.field_1.field_1.field_1.field_2.field_2==0.53125) && (a2.field_1.field_1.field_1.field_2.field_3==170) && (a2.field_1.field_1.field_1.field_2.field_4==148)) && (a2.field_1.field_1.field_1.field_3==137775610724352) && (a2.field_1.field_1.field_1.field_4==0.046875)) && (a2.field_1.field_1.field_2==-0.109375)) && ((((a2.field_1.field_2.field_1.field_1.field_1==-0.515625) && ((a2.field_1.field_2.field_1.field_1.field_2.field_1==130) && (a2.field_1.field_2.field_1.field_1.field_2.field_2==0.5625) && (a2.field_1.field_2.field_1.field_1.field_2.field_3==65) && (a2.field_1.field_2.field_1.field_1.field_2.field_4==249))) && ((a2.field_1.field_2.field_1.field_2.field_1==49920772014080) && (a2.field_1.field_2.field_1.field_2.field_2==137) && ((a2.field_1.field_2.field_1.field_2.field_3.field_1==81) && (a2.field_1.field_2.field_1.field_2.field_3.field_2==-1.078125) && (a2.field_1.field_2.field_1.field_2.field_3.field_3==187) && (a2.field_1.field_2.field_1.field_2.field_3.field_4==244))) && ((a2.field_1.field_2.field_1.field_3.field_1==238917205884928) && (a2.field_1.field_2.field_1.field_3.field_2==234) && ((a2.field_1.field_2.field_1.field_3.field_3.field_1==148) && (a2.field_1.field_2.field_1.field_3.field_3.field_2==-0.65625) && (a2.field_1.field_2.field_1.field_3.field_3.field_3==148) && (a2.field_1.field_2.field_1.field_3.field_3.field_4==82)))) && ((a2.field_1.field_2.field_2.field_1==13348047552512) && (a2.field_1.field_2.field_2.field_2==206) && ((a2.field_1.field_2.field_2.field_3.field_1==236) && (a2.field_1.field_2.field_2.field_3.field_2==-0.390625) && (a2.field_1.field_2.field_2.field_3.field_3==159) && (a2.field_1.field_2.field_2.field_3.field_4==22))) && (a2.field_1.field_2.field_3==-1.203125) && (a2.field_1.field_2.field_4==E_6_3)) && (a2.field_1.field_3==3263768112) && (((a2.field_1.field_4.field_1.field_1==0.890625) && ((a2.field_1.field_4.field_1.field_2.field_1==223) && (a2.field_1.field_4.field_1.field_2.field_2==-0.78125) && (a2.field_1.field_4.field_1.field_2.field_3==76) && (a2.field_1.field_4.field_1.field_2.field_4==85))) && ((a2.field_1.field_4.field_2.field_1==165112780685312) && (a2.field_1.field_4.field_2.field_2==206) && ((a2.field_1.field_4.field_2.field_3.field_1==4) && (a2.field_1.field_4.field_2.field_3.field_2==1.078125) && (a2.field_1.field_4.field_2.field_3.field_3==200) && (a2.field_1.field_4.field_2.field_3.field_4==49))) && ((a2.field_1.field_4.field_3.field_1==107450067189760) && (a2.field_1.field_4.field_3.field_2==25) && ((a2.field_1.field_4.field_3.field_3.field_1==89) && (a2.field_1.field_4.field_3.field_3.field_2==-1.203125) && (a2.field_1.field_4.field_3.field_3.field_3==14) && (a2.field_1.field_4.field_3.field_3.field_4==51))))) && (a2.field_2==E_19_1) && (a2.field_3==178689565917184) && (a2.field_4==E_20_4) && (a2.field_5==E_21_6) && ((a2.field_6.field_1==269113953746944) && (a2.field_6.field_2==2315358406)))) {
        goto bad;
    }

    if (!(a3==E_21_9)) {
        goto bad;
    }

    if (!(a4==248)) {
        goto bad;
    }

    if (!(a5==86)) {
        goto bad;
    }

    if (!((a6.field_1==85770339090432) && ((a6.field_2.field_1==-0.671875)) && ((a6.field_3.field_1==-1.15234375) && (a6.field_3.field_2==E_20_1) && (a6.field_3.field_3==E_21_1) && ((a6.field_3.field_4.field_1==0.46875) && ((a6.field_3.field_4.field_2.field_1==3865086263296) && (((a6.field_3.field_4.field_2.field_2.field_1.field_1==62745143410688) && (a6.field_3.field_4.field_2.field_2.field_1.field_2==444241798) && (a6.field_3.field_4.field_2.field_2.field_1.field_3==148) && (a6.field_3.field_4.field_2.field_2.field_1.field_4==38) && (a6.field_3.field_4.field_2.field_2.field_1.field_5==186451756843008)) && (a6.field_3.field_4.field_2.field_2.field_2==253) && (a6.field_3.field_4.field_2.field_2.field_3==E_6_6)) && (((a6.field_3.field_4.field_2.field_3.field_1.field_1==106) && (a6.field_3.field_4.field_2.field_3.field_1.field_2==0.875) && (a6.field_3.field_4.field_2.field_3.field_1.field_3==169) && (a6.field_3.field_4.field_2.field_3.field_1.field_4==132)) && ((a6.field_3.field_4.field_2.field_3.field_2.field_1==193552010969088) && (a6.field_3.field_4.field_2.field_3.field_2.field_2==134) && ((a6.field_3.field_4.field_2.field_3.field_2.field_3.field_1==209) && (a6.field_3.field_4.field_2.field_3.field_2.field_3.field_2==0.390625) && (a6.field_3.field_4.field_2.field_3.field_2.field_3.field_3==6) && (a6.field_3.field_4.field_2.field_3.field_2.field_3.field_4==157))))) && (((a6.field_3.field_4.field_3.field_1.field_1==-1.015625) && ((a6.field_3.field_4.field_3.field_1.field_2.field_1==122) && (a6.field_3.field_4.field_3.field_1.field_2.field_2==1.40625) && (a6.field_3.field_4.field_3.field_1.field_2.field_3==38) && (a6.field_3.field_4.field_3.field_1.field_2.field_4==208))) && (a6.field_3.field_4.field_3.field_2==55544454578176) && (a6.field_3.field_4.field_3.field_3==-2.609375)))) && ((a6.field_4.field_1==33366908207104) && (a6.field_4.field_2==136001504280576) && (a6.field_4.field_3==132)))) {
        goto bad;
    }

    return 2527618668;

bad:
    return 339855078;
}

uint64_t
fn_4_c(t30 a1)
{
    if (!(((((((a1.field_1.field_1.field_1.field_1.field_1.field_1==44445459546112) && ((a1.field_1.field_1.field_1.field_1.field_1.field_2.field_1==83) && (a1.field_1.field_1.field_1.field_1.field_1.field_2.field_2==-0.453125) && (a1.field_1.field_1.field_1.field_1.field_1.field_2.field_3==82) && (a1.field_1.field_1.field_1.field_1.field_1.field_2.field_4==113)) && (a1.field_1.field_1.field_1.field_1.field_1.field_3==235939541549056) && (a1.field_1.field_1.field_1.field_1.field_1.field_4==1.078125)) && (a1.field_1.field_1.field_1.field_1.field_2==0.3125)) && ((((a1.field_1.field_1.field_1.field_2.field_1.field_1.field_1==-0.234375) && ((a1.field_1.field_1.field_1.field_2.field_1.field_1.field_2.field_1==236) && (a1.field_1.field_1.field_1.field_2.field_1.field_1.field_2.field_2==0.828125) && (a1.field_1.field_1.field_1.field_2.field_1.field_1.field_2.field_3==60) && (a1.field_1.field_1.field_1.field_2.field_1.field_1.field_2.field_4==26))) && ((a1.field_1.field_1.field_1.field_2.field_1.field_2.field_1==143671663132672) && (a1.field_1.field_1.field_1.field_2.field_1.field_2.field_2==50) && ((a1.field_1.field_1.field_1.field_2.field_1.field_2.field_3.field_1==243) && (a1.field_1.field_1.field_1.field_2.field_1.field_2.field_3.field_2==-0.796875) && (a1.field_1.field_1.field_1.field_2.field_1.field_2.field_3.field_3==3) && (a1.field_1.field_1.field_1.field_2.field_1.field_2.field_3.field_4==53))) && ((a1.field_1.field_1.field_1.field_2.field_1.field_3.field_1==214366877515776) && (a1.field_1.field_1.field_1.field_2.field_1.field_3.field_2==126) && ((a1.field_1.field_1.field_1.field_2.field_1.field_3.field_3.field_1==252) && (a1.field_1.field_1.field_1.field_2.field_1.field_3.field_3.field_2==1.453125) && (a1.field_1.field_1.field_1.field_2.field_1.field_3.field_3.field_3==128) && (a1.field_1.field_1.field_1.field_2.field_1.field_3.field_3.field_4==149)))) && ((a1.field_1.field_1.field_1.field_2.field_2.field_1==223779870998528) && (a1.field_1.field_1.field_1.field_2.field_2.field_2==137) && ((a1.field_1.field_1.field_1.field_2.field_2.field_3.field_1==203) && (a1.field_1.field_1.field_1.field_2.field_2.field_3.field_2==0.953125) && (a1.field_1.field_1.field_1.field_2.field_2.field_3.field_3==12) && (a1.field_1.field_1.field_1.field_2.field_2.field_3.field_4==243))) && (a1.field_1.field_1.field_1.field_2.field_3==1.453125) && (a1.field_1.field_1.field_1.field_2.field_4==E_6_3)) && (a1.field_1.field_1.field_1.field_3==1314332054) && (((a1.field_1.field_1.field_1.field_4.field_1.field_1==-0.671875) && ((a1.field_1.field_1.field_1.field_4.field_1.field_2.field_1==209) && (a1.field_1.field_1.field_1.field_4.field_1.field_2.field_2==-1.125) && (a1.field_1.field_1.field_1.field_4.field_1.field_2.field_3==32) && (a1.field_1.field_1.field_1.field_4.field_1.field_2.field_4==106))) && ((a1.field_1.field_1.field_1.field_4.field_2.field_1==160715212455936) && (a1.field_1.field_1.field_1.field_4.field_2.field_2==12) && ((a1.field_1.field_1.field_1.field_4.field_2.field_3.field_1==45) && (a1.field_1.field_1.field_1.field_4.field_2.field_3.field_2==0.96875) && (a1.field_1.field_1.field_1.field_4.field_2.field_3.field_3==39) && (a1.field_1.field_1.field_1.field_4.field_2.field_3.field_4==176))) && ((a1.field_1.field_1.field_1.field_4.field_3.field_1==2142336450560) && (a1.field_1.field_1.field_1.field_4.field_3.field_2==26) && ((a1.field_1.field_1.field_1.field_4.field_3.field_3.field_1==239) && (a1.field_1.field_1.field_1.field_4.field_3.field_3.field_2==-1.5) && (a1.field_1.field_1.field_1.field_4.field_3.field_3.field_3==80) && (a1.field_1.field_1.field_1.field_4.field_3.field_3.field_4==178))))) && (a1.field_1.field_1.field_2==E_19_5) && (a1.field_1.field_1.field_3==84526311014400) && (a1.field_1.field_1.field_4==E_20_6) && (a1.field_1.field_1.field_5==E_21_1) && ((a1.field_1.field_1.field_6.field_1==248535774265344) && (a1.field_1.field_1.field_6.field_2==1964811016))) && (a1.field_1.field_2==0.1875) && (a1.field_1.field_3==E_6_5) && (a1.field_1.field_4==-1.87109375)) && ((a1.field_2.field_1==155) && (a1.field_2.field_2==2693759888)))) {
        goto bad;
    }

    return 276275068796928;

bad:
    return 262674313510912;
}

double
fn_5_c(t33 a1, t34 a2, t36 a3, t37 a4, uint64_t a5)
{
    if (!((a1.field_1==E_31_3) && (a1.field_2==E_32_5))) {
        goto bad;
    }

    if (!((a2.field_1==E_32_5) && (a2.field_2==66))) {
        goto bad;
    }

    if (!((a3.field_1==E_35_4) && (a3.field_2==214))) {
        goto bad;
    }

    if (!(a4==E_37_1)) {
        goto bad;
    }

    if (!(a5==210077727850496)) {
        goto bad;
    }

    return -2.5234375;

bad:
    return 1.40234375;
}

t43
fn_6_c(t40 a1, t41 a2, t34 a3, t42 a4, double a5, double a6, uint64_t a7, uint64_t a8, t26 a9)
{
    if (!(((((a1.field_1.field_1.field_1.field_1==0.71875) && ((a1.field_1.field_1.field_1.field_2.field_1==162) && (a1.field_1.field_1.field_1.field_2.field_2==-1.375) && (a1.field_1.field_1.field_1.field_2.field_3==12) && (a1.field_1.field_1.field_1.field_2.field_4==208))) && ((a1.field_1.field_1.field_2.field_1==208257259929600) && (a1.field_1.field_1.field_2.field_2==224) && ((a1.field_1.field_1.field_2.field_3.field_1==202) && (a1.field_1.field_1.field_2.field_3.field_2==-0.015625) && (a1.field_1.field_1.field_2.field_3.field_3==86) && (a1.field_1.field_1.field_2.field_3.field_4==251))) && ((a1.field_1.field_1.field_3.field_1==253976550440960) && (a1.field_1.field_1.field_3.field_2==123) && ((a1.field_1.field_1.field_3.field_3.field_1==146) && (a1.field_1.field_1.field_3.field_3.field_2==0.828125) && (a1.field_1.field_1.field_3.field_3.field_3==120) && (a1.field_1.field_1.field_3.field_3.field_4==62)))) && ((a1.field_1.field_2.field_1==26298971062272) && (a1.field_1.field_2.field_2==103) && ((a1.field_1.field_2.field_3.field_1==239) && (a1.field_1.field_2.field_3.field_2==1.28125) && (a1.field_1.field_2.field_3.field_3==152) && (a1.field_1.field_2.field_3.field_4==89))) && (a1.field_1.field_3==0.734375) && (a1.field_1.field_4==E_6_3)) && (a1.field_2==E_6_1) && (a1.field_3==E_38_6) && (((a1.field_4.field_1.field_1==152549047009280) && ((a1.field_4.field_1.field_2.field_1==0.859375)) && ((a1.field_4.field_1.field_3.field_1==-3.7109375) && (a1.field_4.field_1.field_3.field_2==E_20_7) && (a1.field_4.field_1.field_3.field_3==E_21_5) && ((a1.field_4.field_1.field_3.field_4.field_1==1.26171875) && ((a1.field_4.field_1.field_3.field_4.field_2.field_1==257906686427136) && (((a1.field_4.field_1.field_3.field_4.field_2.field_2.field_1.field_1==154973503488000) && (a1.field_4.field_1.field_3.field_4.field_2.field_2.field_1.field_2==2040012194) && (a1.field_4.field_1.field_3.field_4.field_2.field_2.field_1.field_3==167) && (a1.field_4.field_1.field_3.field_4.field_2.field_2.field_1.field_4==109) && (a1.field_4.field_1.field_3.field_4.field_2.field_2.field_1.field_5==74787062284288)) && (a1.field_4.field_1.field_3.field_4.field_2.field_2.field_2==39) && (a1.field_4.field_1.field_3.field_4.field_2.field_2.field_3==E_6_6)) && (((a1.field_4.field_1.field_3.field_4.field_2.field_3.field_1.field_1==63) && (a1.field_4.field_1.field_3.field_4.field_2.field_3.field_1.field_2==-1.390625) && (a1.field_4.field_1.field_3.field_4.field_2.field_3.field_1.field_3==63) && (a1.field_4.field_1.field_3.field_4.field_2.field_3.field_1.field_4==209)) && ((a1.field_4.field_1.field_3.field_4.field_2.field_3.field_2.field_1==230983872282624) && (a1.field_4.field_1.field_3.field_4.field_2.field_3.field_2.field_2==184) && ((a1.field_4.field_1.field_3.field_4.field_2.field_3.field_2.field_3.field_1==15) && (a1.field_4.field_1.field_3.field_4.field_2.field_3.field_2.field_3.field_2==1.296875) && (a1.field_4.field_1.field_3.field_4.field_2.field_3.field_2.field_3.field_3==31) && (a1.field_4.field_1.field_3.field_4.field_2.field_3.field_2.field_3.field_4==255))))) && (((a1.field_4.field_1.field_3.field_4.field_3.field_1.field_1==1.015625) && ((a1.field_4.field_1.field_3.field_4.field_3.field_1.field_2.field_1==183) && (a1.field_4.field_1.field_3.field_4.field_3.field_1.field_2.field_2==-0.484375) && (a1.field_4.field_1.field_3.field_4.field_3.field_1.field_2.field_3==142) && (a1.field_4.field_1.field_3.field_4.field_3.field_1.field_2.field_4==16))) && (a1.field_4.field_1.field_3.field_4.field_3.field_2==137163913822208) && (a1.field_4.field_1.field_3.field_4.field_3.field_3==-2.80859375)))) && ((a1.field_4.field_1.field_4.field_1==170983975682048) && (a1.field_4.field_1.field_4.field_2==72796805398528) && (a1.field_4.field_1.field_4.field_3==42))) && (a1.field_4.field_2==1990287632) && ((a1.field_4.field_3.field_1==194) && (a1.field_4.field_3.field_2==3552591666)) && ((((a1.field_4.field_4.field_1.field_1.field_1==-0.375) && ((a1.field_4.field_4.field_1.field_1.field_2.field_1==79) && (a1.field_4.field_4.field_1.field_1.field_2.field_2==-0.625) && (a1.field_4.field_4.field_1.field_1.field_2.field_3==9) && (a1.field_4.field_4.field_1.field_1.field_2.field_4==188))) && ((a1.field_4.field_4.field_1.field_2.field_1==159828972142592) && (a1.field_4.field_4.field_1.field_2.field_2==48) && ((a1.field_4.field_4.field_1.field_2.field_3.field_1==128) && (a1.field_4.field_4.field_1.field_2.field_3.field_2==1.0) && (a1.field_4.field_4.field_1.field_2.field_3.field_3==62) && (a1.field_4.field_4.field_1.field_2.field_3.field_4==192))) && ((a1.field_4.field_4.field_1.field_3.field_1==179052505464832) && (a1.field_4.field_4.field_1.field_3.field_2==16) && ((a1.field_4.field_4.field_1.field_3.field_3.field_1==120) && (a1.field_4.field_4.field_1.field_3.field_3.field_2==0.625) && (a1.field_4.field_4.field_1.field_3.field_3.field_3==250) && (a1.field_4.field_4.field_1.field_3.field_3.field_4==151)))) && ((a1.field_4.field_4.field_2.field_1==196274257788928) && (a1.field_4.field_4.field_2.field_2==205) && ((a1.field_4.field_4.field_2.field_3.field_1==79) && (a1.field_4.field_4.field_2.field_3.field_2==-1.4375) && (a1.field_4.field_4.field_2.field_3.field_3==91) && (a1.field_4.field_4.field_2.field_3.field_4==96))) && (a1.field_4.field_4.field_3==0.09375) && (a1.field_4.field_4.field_4==E_6_4)) && (a1.field_4.field_5==1.515625)) && (a1.field_5==2.265625))) {
        goto bad;
    }

    if (!(a2==E_41_5)) {
        goto bad;
    }

    if (!((a3.field_1==E_32_8) && (a3.field_2==203))) {
        goto bad;
    }

    if (!((a4.field_1==-1.58984375) && (a4.field_2==217) && (a4.field_3==3500137676) && (a4.field_4==101686982737920))) {
        goto bad;
    }

    if (!(a5==0.10546875)) {
        goto bad;
    }

    if (!(a6==3.21875)) {
        goto bad;
    }

    if (!(a7==40133181046784)) {
        goto bad;
    }

    if (!(a8==71741835313152)) {
        goto bad;
    }

    if (!((a9.field_1==221313136459776) && (a9.field_2==104460582912) && (a9.field_3==149))) {
        goto bad;
    }

    return E_43_1;

bad:
    return E_43_2;
}

uint32_t
fn_7_c(t48 a1)
{
    if (!(((a1.field_1.field_1==6386332991488) && (a1.field_1.field_2==-3.90625) && (a1.field_1.field_3==163) && (a1.field_1.field_4==155193554763776) && ((a1.field_1.field_5.field_1==E_35_3) && (a1.field_1.field_5.field_2==18))) && (((a1.field_2.field_1.field_1==112229799297024) && (a1.field_2.field_1.field_2==-2.8984375) && (a1.field_2.field_1.field_3==216) && (a1.field_2.field_1.field_4==44483204612096) && ((a1.field_2.field_1.field_5.field_1==E_35_2) && (a1.field_2.field_1.field_5.field_2==195))) && (a1.field_2.field_2==39665754832896) && (a1.field_2.field_3==114) && (a1.field_2.field_4==776337414)) && (a1.field_3==E_46_5) && (a1.field_4==146729052536832) && ((((a1.field_5.field_1.field_1.field_1==153) && (a1.field_5.field_1.field_1.field_2==1.234375) && (a1.field_5.field_1.field_1.field_3==43) && (a1.field_5.field_1.field_1.field_4==174)) && ((a1.field_5.field_1.field_2.field_1==26828980748288) && (a1.field_5.field_1.field_2.field_2==242) && ((a1.field_5.field_1.field_2.field_3.field_1==236) && (a1.field_5.field_1.field_2.field_3.field_2==-1.078125) && (a1.field_5.field_1.field_2.field_3.field_3==180) && (a1.field_5.field_1.field_2.field_3.field_4==30)))) && (a1.field_5.field_2==266092824363008)) && ((a1.field_6.field_1==13831859994624) && ((a1.field_6.field_2.field_1==0.390625)) && ((a1.field_6.field_3.field_1==0.0703125) && (a1.field_6.field_3.field_2==E_20_1) && (a1.field_6.field_3.field_3==E_21_6) && ((a1.field_6.field_3.field_4.field_1==-2.765625) && ((a1.field_6.field_3.field_4.field_2.field_1==175411747749888) && (((a1.field_6.field_3.field_4.field_2.field_2.field_1.field_1==262729443049472) && (a1.field_6.field_3.field_4.field_2.field_2.field_1.field_2==932333724) && (a1.field_6.field_3.field_4.field_2.field_2.field_1.field_3==5) && (a1.field_6.field_3.field_4.field_2.field_2.field_1.field_4==16) && (a1.field_6.field_3.field_4.field_2.field_2.field_1.field_5==17637135220736)) && (a1.field_6.field_3.field_4.field_2.field_2.field_2==46) && (a1.field_6.field_3.field_4.field_2.field_2.field_3==E_6_2)) && (((a1.field_6.field_3.field_4.field_2.field_3.field_1.field_1==211) && (a1.field_6.field_3.field_4.field_2.field_3.field_1.field_2==-0.5625) && (a1.field_6.field_3.field_4.field_2.field_3.field_1.field_3==171) && (a1.field_6.field_3.field_4.field_2.field_3.field_1.field_4==1)) && ((a1.field_6.field_3.field_4.field_2.field_3.field_2.field_1==12764915826688) && (a1.field_6.field_3.field_4.field_2.field_3.field_2.field_2==49) && ((a1.field_6.field_3.field_4.field_2.field_3.field_2.field_3.field_1==154) && (a1.field_6.field_3.field_4.field_2.field_3.field_2.field_3.field_2==1.375) && (a1.field_6.field_3.field_4.field_2.field_3.field_2.field_3.field_3==93) && (a1.field_6.field_3.field_4.field_2.field_3.field_2.field_3.field_4==73))))) && (((a1.field_6.field_3.field_4.field_3.field_1.field_1==-1.46875) && ((a1.field_6.field_3.field_4.field_3.field_1.field_2.field_1==79) && (a1.field_6.field_3.field_4.field_3.field_1.field_2.field_2==-0.90625) && (a1.field_6.field_3.field_4.field_3.field_1.field_2.field_3==49) && (a1.field_6.field_3.field_4.field_3.field_1.field_2.field_4==4))) && (a1.field_6.field_3.field_4.field_3.field_2==93216219463680) && (a1.field_6.field_3.field_4.field_3.field_3==-2.83203125)))) && ((a1.field_6.field_4.field_1==18454498639872) && (a1.field_6.field_4.field_2==269237052506112) && (a1.field_6.field_4.field_3==165))))) {
        goto bad;
    }

    return 2499773006;

bad:
    return 3403919486;
}

float
fn_8_c(t1 a1, t50 a2, t51 a3, float a4, t52 a5, uint32_t a6, t53 a7, uint8_t a8)
{
    if (!((a1.field_1==200) && (a1.field_2==0.25) && (a1.field_3==18) && (a1.field_4==246))) {
        goto bad;
    }

    if (!((a2.field_1==-0.734375) && ((a2.field_2.field_1==8592161505280) && (a2.field_2.field_2==2.32421875) && (a2.field_2.field_3==77) && (a2.field_2.field_4==95804356820992) && ((a2.field_2.field_5.field_1==E_35_1) && (a2.field_2.field_5.field_2==126))) && (a2.field_3==-1.12109375) && (a2.field_4==E_49_3))) {
        goto bad;
    }

    if (!(a3==E_51_3)) {
        goto bad;
    }

    if (!(a4==0.1875)) {
        goto bad;
    }

    if (!((a5.field_1==-0.81640625))) {
        goto bad;
    }

    if (!(a6==441584498)) {
        goto bad;
    }

    if (!((a7.field_1==198))) {
        goto bad;
    }

    if (!(a8==89)) {
        goto bad;
    }

    return 1.421875;

bad:
    return 0.5625;
}

uint64_t
fn_9_c(t51 a1, float a2, float a3, t56 a4, t58 a5, t61 a6, t65 a7, t66 a8, t67 a9, double a10)
{
    if (!(a1==E_51_8)) {
        goto bad;
    }

    if (!(a2==-0.796875)) {
        goto bad;
    }

    if (!(a3==-0.71875)) {
        goto bad;
    }

    if (!((a4.field_1==E_54_7) && (a4.field_2==255) && ((((a4.field_3.field_1.field_1.field_1==130105287114752) && ((a4.field_3.field_1.field_1.field_2.field_1==-1.0)) && ((a4.field_3.field_1.field_1.field_3.field_1==3.53125) && (a4.field_3.field_1.field_1.field_3.field_2==E_20_2) && (a4.field_3.field_1.field_1.field_3.field_3==E_21_1) && ((a4.field_3.field_1.field_1.field_3.field_4.field_1==-0.67578125) && ((a4.field_3.field_1.field_1.field_3.field_4.field_2.field_1==23617654620160) && (((a4.field_3.field_1.field_1.field_3.field_4.field_2.field_2.field_1.field_1==12594182750208) && (a4.field_3.field_1.field_1.field_3.field_4.field_2.field_2.field_1.field_2==2889081194) && (a4.field_3.field_1.field_1.field_3.field_4.field_2.field_2.field_1.field_3==84) && (a4.field_3.field_1.field_1.field_3.field_4.field_2.field_2.field_1.field_4==84) && (a4.field_3.field_1.field_1.field_3.field_4.field_2.field_2.field_1.field_5==265251503210496)) && (a4.field_3.field_1.field_1.field_3.field_4.field_2.field_2.field_2==17) && (a4.field_3.field_1.field_1.field_3.field_4.field_2.field_2.field_3==E_6_7)) && (((a4.field_3.field_1.field_1.field_3.field_4.field_2.field_3.field_1.field_1==141) && (a4.field_3.field_1.field_1.field_3.field_4.field_2.field_3.field_1.field_2==-0.1875) && (a4.field_3.field_1.field_1.field_3.field_4.field_2.field_3.field_1.field_3==10) && (a4.field_3.field_1.field_1.field_3.field_4.field_2.field_3.field_1.field_4==118)) && ((a4.field_3.field_1.field_1.field_3.field_4.field_2.field_3.field_2.field_1==24302215233536) && (a4.field_3.field_1.field_1.field_3.field_4.field_2.field_3.field_2.field_2==170) && ((a4.field_3.field_1.field_1.field_3.field_4.field_2.field_3.field_2.field_3.field_1==64) && (a4.field_3.field_1.field_1.field_3.field_4.field_2.field_3.field_2.field_3.field_2==0.953125) && (a4.field_3.field_1.field_1.field_3.field_4.field_2.field_3.field_2.field_3.field_3==156) && (a4.field_3.field_1.field_1.field_3.field_4.field_2.field_3.field_2.field_3.field_4==112))))) && (((a4.field_3.field_1.field_1.field_3.field_4.field_3.field_1.field_1==0.3125) && ((a4.field_3.field_1.field_1.field_3.field_4.field_3.field_1.field_2.field_1==168) && (a4.field_3.field_1.field_1.field_3.field_4.field_3.field_1.field_2.field_2==0.3125) && (a4.field_3.field_1.field_1.field_3.field_4.field_3.field_1.field_2.field_3==140) && (a4.field_3.field_1.field_1.field_3.field_4.field_3.field_1.field_2.field_4==197))) && (a4.field_3.field_1.field_1.field_3.field_4.field_3.field_2==149682231705600) && (a4.field_3.field_1.field_1.field_3.field_4.field_3.field_3==0.3828125)))) && ((a4.field_3.field_1.field_1.field_4.field_1==66190105247744) && (a4.field_3.field_1.field_1.field_4.field_2==201243456045056) && (a4.field_3.field_1.field_1.field_4.field_3==128))) && (a4.field_3.field_1.field_2==1608787508) && ((a4.field_3.field_1.field_3.field_1==208) && (a4.field_3.field_1.field_3.field_2==3925244758)) && ((((a4.field_3.field_1.field_4.field_1.field_1.field_1==-0.125) && ((a4.field_3.field_1.field_4.field_1.field_1.field_2.field_1==219) && (a4.field_3.field_1.field_4.field_1.field_1.field_2.field_2==0.265625) && (a4.field_3.field_1.field_4.field_1.field_1.field_2.field_3==201) && (a4.field_3.field_1.field_4.field_1.field_1.field_2.field_4==48))) && ((a4.field_3.field_1.field_4.field_1.field_2.field_1==148885215510528) && (a4.field_3.field_1.field_4.field_1.field_2.field_2==219) && ((a4.field_3.field_1.field_4.field_1.field_2.field_3.field_1==41) && (a4.field_3.field_1.field_4.field_1.field_2.field_3.field_2==-1.3125) && (a4.field_3.field_1.field_4.field_1.field_2.field_3.field_3==75) && (a4.field_3.field_1.field_4.field_1.field_2.field_3.field_4==52))) && ((a4.field_3.field_1.field_4.field_1.field_3.field_1==153670362464256) && (a4.field_3.field_1.field_4.field_1.field_3.field_2==97) && ((a4.field_3.field_1.field_4.field_1.field_3.field_3.field_1==222) && (a4.field_3.field_1.field_4.field_1.field_3.field_3.field_2==0.9375) && (a4.field_3.field_1.field_4.field_1.field_3.field_3.field_3==47) && (a4.field_3.field_1.field_4.field_1.field_3.field_3.field_4==122)))) && ((a4.field_3.field_1.field_4.field_2.field_1==66828912033792) && (a4.field_3.field_1.field_4.field_2.field_2==201) && ((a4.field_3.field_1.field_4.field_2.field_3.field_1==35) && (a4.field_3.field_1.field_4.field_2.field_3.field_2==1.046875) && (a4.field_3.field_1.field_4.field_2.field_3.field_3==86) && (a4.field_3.field_1.field_4.field_2.field_3.field_4==233))) && (a4.field_3.field_1.field_4.field_3==-0.421875) && (a4.field_3.field_1.field_4.field_4==E_6_7)) && (a4.field_3.field_1.field_5==-1.109375)) && (a4.field_3.field_2==350527626) && (a4.field_3.field_3==98) && (((a4.field_3.field_4.field_1.field_1==146849136115712) && (a4.field_3.field_4.field_1.field_2==3845172994) && (a4.field_3.field_4.field_1.field_3==76) && (a4.field_3.field_4.field_1.field_4==250) && (a4.field_3.field_4.field_1.field_5==212141542211584)) && (a4.field_3.field_4.field_2==227) && (a4.field_3.field_4.field_3==E_6_6))))) {
        goto bad;
    }

    if (!((a5.field_1==E_57_1) && ((a5.field_2.field_1==77)) && (a5.field_3==-2.5))) {
        goto bad;
    }

    if (!((a6.field_1==E_59_2) && (a6.field_2==-0.984375) && ((a6.field_3.field_1==-1.09375) && (a6.field_3.field_2==-1.53125) && (a6.field_3.field_3==20793480314880) && (a6.field_3.field_4==36073398206464)))) {
        goto bad;
    }

    if (!(((a7.field_1.field_1==-2.32421875) && (a7.field_1.field_2==208)) && ((a7.field_2.field_1==-0.0625) && ((((a7.field_2.field_2.field_1.field_1.field_1==224) && (a7.field_2.field_2.field_1.field_1.field_2==-0.984375) && (a7.field_2.field_2.field_1.field_1.field_3==131) && (a7.field_2.field_2.field_1.field_1.field_4==218)) && ((a7.field_2.field_2.field_1.field_2.field_1==209069555974144) && (a7.field_2.field_2.field_1.field_2.field_2==100) && ((a7.field_2.field_2.field_1.field_2.field_3.field_1==82) && (a7.field_2.field_2.field_1.field_2.field_3.field_2==1.4375) && (a7.field_2.field_2.field_1.field_2.field_3.field_3==89) && (a7.field_2.field_2.field_1.field_2.field_3.field_4==16)))) && (a7.field_2.field_2.field_2==273030717767680)) && (((((a7.field_2.field_3.field_1.field_1.field_1.field_1==62938097254400) && ((a7.field_2.field_3.field_1.field_1.field_1.field_2.field_1==107) && (a7.field_2.field_3.field_1.field_1.field_1.field_2.field_2==0.609375) && (a7.field_2.field_3.field_1.field_1.field_1.field_2.field_3==97) && (a7.field_2.field_3.field_1.field_1.field_1.field_2.field_4==70)) && (a7.field_2.field_3.field_1.field_1.field_1.field_3==77561457147904) && (a7.field_2.field_3.field_1.field_1.field_1.field_4==1.265625)) && (a7.field_2.field_3.field_1.field_1.field_2==-1.515625)) && ((((a7.field_2.field_3.field_1.field_2.field_1.field_1.field_1==-0.828125) && ((a7.field_2.field_3.field_1.field_2.field_1.field_1.field_2.field_1==174) && (a7.field_2.field_3.field_1.field_2.field_1.field_1.field_2.field_2==1.390625) && (a7.field_2.field_3.field_1.field_2.field_1.field_1.field_2.field_3==142) && (a7.field_2.field_3.field_1.field_2.field_1.field_1.field_2.field_4==121))) && ((a7.field_2.field_3.field_1.field_2.field_1.field_2.field_1==266278944636928) && (a7.field_2.field_3.field_1.field_2.field_1.field_2.field_2==1) && ((a7.field_2.field_3.field_1.field_2.field_1.field_2.field_3.field_1==57) && (a7.field_2.field_3.field_1.field_2.field_1.field_2.field_3.field_2==-0.1875) && (a7.field_2.field_3.field_1.field_2.field_1.field_2.field_3.field_3==53) && (a7.field_2.field_3.field_1.field_2.field_1.field_2.field_3.field_4==10))) && ((a7.field_2.field_3.field_1.field_2.field_1.field_3.field_1==258630503497728) && (a7.field_2.field_3.field_1.field_2.field_1.field_3.field_2==22) && ((a7.field_2.field_3.field_1.field_2.field_1.field_3.field_3.field_1==58) && (a7.field_2.field_3.field_1.field_2.field_1.field_3.field_3.field_2==-0.203125) && (a7.field_2.field_3.field_1.field_2.field_1.field_3.field_3.field_3==241) && (a7.field_2.field_3.field_1.field_2.field_1.field_3.field_3.field_4==248)))) && ((a7.field_2.field_3.field_1.field_2.field_2.field_1==232005730828288) && (a7.field_2.field_3.field_1.field_2.field_2.field_2==67) && ((a7.field_2.field_3.field_1.field_2.field_2.field_3.field_1==237) && (a7.field_2.field_3.field_1.field_2.field_2.field_3.field_2==-1.03125) && (a7.field_2.field_3.field_1.field_2.field_2.field_3.field_3==83) && (a7.field_2.field_3.field_1.field_2.field_2.field_3.field_4==229))) && (a7.field_2.field_3.field_1.field_2.field_3==-0.328125) && (a7.field_2.field_3.field_1.field_2.field_4==E_6_6)) && (a7.field_2.field_3.field_1.field_3==2541087240) && (((a7.field_2.field_3.field_1.field_4.field_1.field_1==0.875) && ((a7.field_2.field_3.field_1.field_4.field_1.field_2.field_1==4) && (a7.field_2.field_3.field_1.field_4.field_1.field_2.field_2==1.15625) && (a7.field_2.field_3.field_1.field_4.field_1.field_2.field_3==173) && (a7.field_2.field_3.field_1.field_4.field_1.field_2.field_4==8))) && ((a7.field_2.field_3.field_1.field_4.field_2.field_1==29310895783936) && (a7.field_2.field_3.field_1.field_4.field_2.field_2==92) && ((a7.field_2.field_3.field_1.field_4.field_2.field_3.field_1==250) && (a7.field_2.field_3.field_1.field_4.field_2.field_3.field_2==0.515625) && (a7.field_2.field_3.field_1.field_4.field_2.field_3.field_3==214) && (a7.field_2.field_3.field_1.field_4.field_2.field_3.field_4==236))) && ((a7.field_2.field_3.field_1.field_4.field_3.field_1==188538419019776) && (a7.field_2.field_3.field_1.field_4.field_3.field_2==16) && ((a7.field_2.field_3.field_1.field_4.field_3.field_3.field_1==92) && (a7.field_2.field_3.field_1.field_4.field_3.field_3.field_2==1.1875) && (a7.field_2.field_3.field_1.field_4.field_3.field_3.field_3==26) && (a7.field_2.field_3.field_1.field_4.field_3.field_3.field_4==72))))) && (a7.field_2.field_3.field_2==E_19_9) && (a7.field_2.field_3.field_3==93123907813376) && (a7.field_2.field_3.field_4==E_20_6) && (a7.field_2.field_3.field_5==E_21_9) && ((a7.field_2.field_3.field_6.field_1==84919533043712) && (a7.field_2.field_3.field_6.field_2==2312961846))) && (a7.field_2.field_4==738749758)) && (((a7.field_3.field_1.field_1==E_59_2) && (a7.field_3.field_1.field_2==0.671875) && ((a7.field_3.field_1.field_3.field_1==-0.01953125) && (a7.field_3.field_1.field_3.field_2==-1.171875) && (a7.field_3.field_1.field_3.field_3==30820079632384) && (a7.field_3.field_1.field_3.field_4==68216701976576))) && (a7.field_3.field_2==E_21_7) && (a7.field_3.field_3==227) && (a7.field_3.field_4==E_31_2)))) {
        goto bad;
    }

    if (!(a8==E_66_3)) {
        goto bad;
    }

    if (!(a9==E_67_3)) {
        goto bad;
    }

    if (!(a10==2.06640625)) {
        goto bad;
    }

    return 6316575686656;

bad:
    return 232898686615552;
}

float
fn_10_c(t74 a1, float a2, t75 a3, uint8_t a4, t78 a5, t79 a6)
{
    if (!(((a1.field_1.field_1==1.21875) && (a1.field_1.field_2==-0.890625) && (a1.field_1.field_3==E_54_5) && (a1.field_1.field_4==-1.140625)) && ((a1.field_2.field_1==-2.86328125) && ((a1.field_2.field_2.field_1==272734424006656) && ((a1.field_2.field_2.field_2.field_1==168) && (a1.field_2.field_2.field_2.field_2==-0.3125) && (a1.field_2.field_2.field_2.field_3==7) && (a1.field_2.field_2.field_2.field_4==96)) && (a1.field_2.field_2.field_3==194650230882304) && (a1.field_2.field_2.field_4==1.3125)) && (a1.field_2.field_3==2832640246) && (a1.field_2.field_4==248902425116672) && (a1.field_2.field_5==3177491204) && (a1.field_2.field_6==E_20_7)) && ((((a1.field_3.field_1.field_1.field_1==242) && (a1.field_3.field_1.field_1.field_2==1.0) && (a1.field_3.field_1.field_1.field_3==118) && (a1.field_3.field_1.field_1.field_4==202)) && ((a1.field_3.field_1.field_2.field_1==130511054045184) && (a1.field_3.field_1.field_2.field_2==60) && ((a1.field_3.field_1.field_2.field_3.field_1==149) && (a1.field_3.field_1.field_2.field_3.field_2==1.25) && (a1.field_3.field_1.field_2.field_3.field_3==209) && (a1.field_3.field_1.field_2.field_3.field_4==211)))) && (a1.field_3.field_2==65)) && (a1.field_4==E_71_4) && (a1.field_5==E_72_7) && ((a1.field_6.field_1==1065245434) && (a1.field_6.field_2==0.359375) && ((a1.field_6.field_3.field_1==182449277763584) && (a1.field_6.field_3.field_2==2003708228))))) {
        goto bad;
    }

    if (!(a2==0.890625)) {
        goto bad;
    }

    if (!(a3==E_75_4)) {
        goto bad;
    }

    if (!(a4==73)) {
        goto bad;
    }

    if (!((a5.field_1==100724679639040) && ((a5.field_2.field_1==197606249070592) && (a5.field_2.field_2==0.3515625) && (a5.field_2.field_3==45) && (a5.field_2.field_4==111205168709632) && ((a5.field_2.field_5.field_1==E_35_6) && (a5.field_2.field_5.field_2==168))) && (((((a5.field_3.field_1.field_1.field_1.field_1==273706513334272) && ((a5.field_3.field_1.field_1.field_1.field_2.field_1==0.625)) && ((a5.field_3.field_1.field_1.field_1.field_3.field_1==3.27734375) && (a5.field_3.field_1.field_1.field_1.field_3.field_2==E_20_4) && (a5.field_3.field_1.field_1.field_1.field_3.field_3==E_21_5) && ((a5.field_3.field_1.field_1.field_1.field_3.field_4.field_1==1.125) && ((a5.field_3.field_1.field_1.field_1.field_3.field_4.field_2.field_1==228193040859136) && (((a5.field_3.field_1.field_1.field_1.field_3.field_4.field_2.field_2.field_1.field_1==197736696643584) && (a5.field_3.field_1.field_1.field_1.field_3.field_4.field_2.field_2.field_1.field_2==2597012506) && (a5.field_3.field_1.field_1.field_1.field_3.field_4.field_2.field_2.field_1.field_3==181) && (a5.field_3.field_1.field_1.field_1.field_3.field_4.field_2.field_2.field_1.field_4==213) && (a5.field_3.field_1.field_1.field_1.field_3.field_4.field_2.field_2.field_1.field_5==107853796474880)) && (a5.field_3.field_1.field_1.field_1.field_3.field_4.field_2.field_2.field_2==110) && (a5.field_3.field_1.field_1.field_1.field_3.field_4.field_2.field_2.field_3==E_6_2)) && (((a5.field_3.field_1.field_1.field_1.field_3.field_4.field_2.field_3.field_1.field_1==122) && (a5.field_3.field_1.field_1.field_1.field_3.field_4.field_2.field_3.field_1.field_2==1.296875) && (a5.field_3.field_1.field_1.field_1.field_3.field_4.field_2.field_3.field_1.field_3==218) && (a5.field_3.field_1.field_1.field_1.field_3.field_4.field_2.field_3.field_1.field_4==48)) && ((a5.field_3.field_1.field_1.field_1.field_3.field_4.field_2.field_3.field_2.field_1==13448229421056) && (a5.field_3.field_1.field_1.field_1.field_3.field_4.field_2.field_3.field_2.field_2==244) && ((a5.field_3.field_1.field_1.field_1.field_3.field_4.field_2.field_3.field_2.field_3.field_1==47) && (a5.field_3.field_1.field_1.field_1.field_3.field_4.field_2.field_3.field_2.field_3.field_2==1.453125) && (a5.field_3.field_1.field_1.field_1.field_3.field_4.field_2.field_3.field_2.field_3.field_3==167) && (a5.field_3.field_1.field_1.field_1.field_3.field_4.field_2.field_3.field_2.field_3.field_4==113))))) && (((a5.field_3.field_1.field_1.field_1.field_3.field_4.field_3.field_1.field_1==-0.5625) && ((a5.field_3.field_1.field_1.field_1.field_3.field_4.field_3.field_1.field_2.field_1==91) && (a5.field_3.field_1.field_1.field_1.field_3.field_4.field_3.field_1.field_2.field_2==1.53125) && (a5.field_3.field_1.field_1.field_1.field_3.field_4.field_3.field_1.field_2.field_3==127) && (a5.field_3.field_1.field_1.field_1.field_3.field_4.field_3.field_1.field_2.field_4==192))) && (a5.field_3.field_1.field_1.field_1.field_3.field_4.field_3.field_2==208774694567936) && (a5.field_3.field_1.field_1.field_1.field_3.field_4.field_3.field_3==-2.68359375)))) && ((a5.field_3.field_1.field_1.field_1.field_4.field_1==203837156753408) && (a5.field_3.field_1.field_1.field_1.field_4.field_2==124751440248832) && (a5.field_3.field_1.field_1.field_1.field_4.field_3==19))) && (a5.field_3.field_1.field_1.field_2==949240352) && ((a5.field_3.field_1.field_1.field_3.field_1==233) && (a5.field_3.field_1.field_1.field_3.field_2==3093393614)) && ((((a5.field_3.field_1.field_1.field_4.field_1.field_1.field_1==-1.46875) && ((a5.field_3.field_1.field_1.field_4.field_1.field_1.field_2.field_1==157) && (a5.field_3.field_1.field_1.field_4.field_1.field_1.field_2.field_2==-0.546875) && (a5.field_3.field_1.field_1.field_4.field_1.field_1.field_2.field_3==189) && (a5.field_3.field_1.field_1.field_4.field_1.field_1.field_2.field_4==115))) && ((a5.field_3.field_1.field_1.field_4.field_1.field_2.field_1==199305275375616) && (a5.field_3.field_1.field_1.field_4.field_1.field_2.field_2==43) && ((a5.field_3.field_1.field_1.field_4.field_1.field_2.field_3.field_1==171) && (a5.field_3.field_1.field_1.field_4.field_1.field_2.field_3.field_2==-0.984375) && (a5.field_3.field_1.field_1.field_4.field_1.field_2.field_3.field_3==22) && (a5.field_3.field_1.field_1.field_4.field_1.field_2.field_3.field_4==134))) && ((a5.field_3.field_1.field_1.field_4.field_1.field_3.field_1==105055744950272) && (a5.field_3.field_1.field_1.field_4.field_1.field_3.field_2==34) && ((a5.field_3.field_1.field_1.field_4.field_1.field_3.field_3.field_1==122) && (a5.field_3.field_1.field_1.field_4.field_1.field_3.field_3.field_2==0.1875) && (a5.field_3.field_1.field_1.field_4.field_1.field_3.field_3.field_3==25) && (a5.field_3.field_1.field_1.field_4.field_1.field_3.field_3.field_4==33)))) && ((a5.field_3.field_1.field_1.field_4.field_2.field_1==1198625914880) && (a5.field_3.field_1.field_1.field_4.field_2.field_2==107) && ((a5.field_3.field_1.field_1.field_4.field_2.field_3.field_1==125) && (a5.field_3.field_1.field_1.field_4.field_2.field_3.field_2==1.546875) && (a5.field_3.field_1.field_1.field_4.field_2.field_3.field_3==235) && (a5.field_3.field_1.field_1.field_4.field_2.field_3.field_4==61))) && (a5.field_3.field_1.field_1.field_4.field_3==0.734375) && (a5.field_3.field_1.field_1.field_4.field_4==E_6_1)) && (a5.field_3.field_1.field_1.field_5==1.453125)) && (a5.field_3.field_1.field_2==763193548) && (a5.field_3.field_1.field_3==39) && (((a5.field_3.field_1.field_4.field_1.field_1==52338931924992) && (a5.field_3.field_1.field_4.field_1.field_2==391110206) && (a5.field_3.field_1.field_4.field_1.field_3==223) && (a5.field_3.field_1.field_4.field_1.field_4==55) && (a5.field_3.field_1.field_4.field_1.field_5==198983641202688)) && (a5.field_3.field_1.field_4.field_2==50) && (a5.field_3.field_1.field_4.field_3==E_6_7))) && (a5.field_3.field_2==673599174)) && (((((a5.field_4.field_1.field_1.field_1.field_1==254957327745024) && ((a5.field_4.field_1.field_1.field_1.field_2.field_1==-1.171875)) && ((a5.field_4.field_1.field_1.field_1.field_3.field_1==2.5546875) && (a5.field_4.field_1.field_1.field_1.field_3.field_2==E_20_1) && (a5.field_4.field_1.field_1.field_1.field_3.field_3==E_21_2) && ((a5.field_4.field_1.field_1.field_1.field_3.field_4.field_1==-1.171875) && ((a5.field_4.field_1.field_1.field_1.field_3.field_4.field_2.field_1==130672052011008) && (((a5.field_4.field_1.field_1.field_1.field_3.field_4.field_2.field_2.field_1.field_1==97871770550272) && (a5.field_4.field_1.field_1.field_1.field_3.field_4.field_2.field_2.field_1.field_2==3555186718) && (a5.field_4.field_1.field_1.field_1.field_3.field_4.field_2.field_2.field_1.field_3==6) && (a5.field_4.field_1.field_1.field_1.field_3.field_4.field_2.field_2.field_1.field_4==114) && (a5.field_4.field_1.field_1.field_1.field_3.field_4.field_2.field_2.field_1.field_5==270228958085120)) && (a5.field_4.field_1.field_1.field_1.field_3.field_4.field_2.field_2.field_2==7) && (a5.field_4.field_1.field_1.field_1.field_3.field_4.field_2.field_2.field_3==E_6_7)) && (((a5.field_4.field_1.field_1.field_1.field_3.field_4.field_2.field_3.field_1.field_1==114) && (a5.field_4.field_1.field_1.field_1.field_3.field_4.field_2.field_3.field_1.field_2==-1.5) && (a5.field_4.field_1.field_1.field_1.field_3.field_4.field_2.field_3.field_1.field_3==200) && (a5.field_4.field_1.field_1.field_1.field_3.field_4.field_2.field_3.field_1.field_4==176)) && ((a5.field_4.field_1.field_1.field_1.field_3.field_4.field_2.field_3.field_2.field_1==212862159159296) && (a5.field_4.field_1.field_1.field_1.field_3.field_4.field_2.field_3.field_2.field_2==220) && ((a5.field_4.field_1.field_1.field_1.field_3.field_4.field_2.field_3.field_2.field_3.field_1==167) && (a5.field_4.field_1.field_1.field_1.field_3.field_4.field_2.field_3.field_2.field_3.field_2==1.359375) && (a5.field_4.field_1.field_1.field_1.field_3.field_4.field_2.field_3.field_2.field_3.field_3==3) && (a5.field_4.field_1.field_1.field_1.field_3.field_4.field_2.field_3.field_2.field_3.field_4==215))))) && (((a5.field_4.field_1.field_1.field_1.field_3.field_4.field_3.field_1.field_1==-1.484375) && ((a5.field_4.field_1.field_1.field_1.field_3.field_4.field_3.field_1.field_2.field_1==226) && (a5.field_4.field_1.field_1.field_1.field_3.field_4.field_3.field_1.field_2.field_2==-1.390625) && (a5.field_4.field_1.field_1.field_1.field_3.field_4.field_3.field_1.field_2.field_3==187) && (a5.field_4.field_1.field_1.field_1.field_3.field_4.field_3.field_1.field_2.field_4==21))) && (a5.field_4.field_1.field_1.field_1.field_3.field_4.field_3.field_2==4365063815168) && (a5.field_4.field_1.field_1.field_1.field_3.field_4.field_3.field_3==3.0390625)))) && ((a5.field_4.field_1.field_1.field_1.field_4.field_1==278530325086209) && (a5.field_4.field_1.field_1.field_1.field_4.field_2==40351778734080) && (a5.field_4.field_1.field_1.field_1.field_4.field_3==183))) && (a5.field_4.field_1.field_1.field_2==345942008) && ((a5.field_4.field_1.field_1.field_3.field_1==91) && (a5.field_4.field_1.field_1.field_3.field_2==281298906)) && ((((a5.field_4.field_1.field_1.field_4.field_1.field_1.field_1==0.140625) && ((a5.field_4.field_1.field_1.field_4.field_1.field_1.field_2.field_1==180) && (a5.field_4.field_1.field_1.field_4.field_1.field_1.field_2.field_2==1.234375) && (a5.field_4.field_1.field_1.field_4.field_1.field_1.field_2.field_3==145) && (a5.field_4.field_1.field_1.field_4.field_1.field_1.field_2.field_4==38))) && ((a5.field_4.field_1.field_1.field_4.field_1.field_2.field_1==240181903360000) && (a5.field_4.field_1.field_1.field_4.field_1.field_2.field_2==153) && ((a5.field_4.field_1.field_1.field_4.field_1.field_2.field_3.field_1==4) && (a5.field_4.field_1.field_1.field_4.field_1.field_2.field_3.field_2==-0.625) && (a5.field_4.field_1.field_1.field_4.field_1.field_2.field_3.field_3==158) && (a5.field_4.field_1.field_1.field_4.field_1.field_2.field_3.field_4==204))) && ((a5.field_4.field_1.field_1.field_4.field_1.field_3.field_1==279255610818561) && (a5.field_4.field_1.field_1.field_4.field_1.field_3.field_2==96) && ((a5.field_4.field_1.field_1.field_4.field_1.field_3.field_3.field_1==169) && (a5.field_4.field_1.field_1.field_4.field_1.field_3.field_3.field_2==0.46875) && (a5.field_4.field_1.field_1.field_4.field_1.field_3.field_3.field_3==79) && (a5.field_4.field_1.field_1.field_4.field_1.field_3.field_3.field_4==172)))) && ((a5.field_4.field_1.field_1.field_4.field_2.field_1==137219642097664) && (a5.field_4.field_1.field_1.field_4.field_2.field_2==85) && ((a5.field_4.field_1.field_1.field_4.field_2.field_3.field_1==143) && (a5.field_4.field_1.field_1.field_4.field_2.field_3.field_2==0.140625) && (a5.field_4.field_1.field_1.field_4.field_2.field_3.field_3==17) && (a5.field_4.field_1.field_1.field_4.field_2.field_3.field_4==164))) && (a5.field_4.field_1.field_1.field_4.field_3==0.1875) && (a5.field_4.field_1.field_1.field_4.field_4==E_6_7)) && (a5.field_4.field_1.field_1.field_5==0.421875)) && (a5.field_4.field_1.field_2==3026647616) && (a5.field_4.field_1.field_3==171) && (((a5.field_4.field_1.field_4.field_1.field_1==200920828215296) && (a5.field_4.field_1.field_4.field_1.field_2==266056950) && (a5.field_4.field_1.field_4.field_1.field_3==188) && (a5.field_4.field_1.field_4.field_1.field_4==66) && (a5.field_4.field_1.field_4.field_1.field_5==215864985649152)) && (a5.field_4.field_1.field_4.field_2==161) && (a5.field_4.field_1.field_4.field_3==E_6_6))) && (a5.field_4.field_2==234) && (a5.field_4.field_3==-0.12890625) && (a5.field_4.field_4==109) && ((((a5.field_4.field_5.field_1.field_1.field_1==238) && (a5.field_4.field_5.field_1.field_1.field_2==0.890625) && (a5.field_4.field_5.field_1.field_1.field_3==11) && (a5.field_4.field_5.field_1.field_1.field_4==187)) && ((a5.field_4.field_5.field_1.field_2.field_1==218973062561792) && (a5.field_4.field_5.field_1.field_2.field_2==108) && ((a5.field_4.field_5.field_1.field_2.field_3.field_1==100) && (a5.field_4.field_5.field_1.field_2.field_3.field_2==-0.234375) && (a5.field_4.field_5.field_1.field_2.field_3.field_3==187) && (a5.field_4.field_5.field_1.field_2.field_3.field_4==17)))) && (a5.field_4.field_5.field_2==233)) && (a5.field_4.field_6==296769572)))) {
        goto bad;
    }

    if (!(a6==E_79_2)) {
        goto bad;
    }

    return -1.515625;

bad:
    return -1.28125;
}

int
const check_c_to_myr_fns(void)
{
    t2 a_1_1 = (t2) {.field_1=0.0625,.field_2=(t1) {.field_1=112,.field_2=-0.625,.field_3=110,.field_4=44}};
    uint32_t a_1_2 = 2189228706;
    t4 a_1_3 = (t4) {.field_1=(t2) {.field_1=-0.84375,.field_2=(t1) {.field_1=150,.field_2=0.359375,.field_3=172,.field_4=26}},.field_2=(t3) {.field_1=241023379308544,.field_2=115,.field_3=(t1) {.field_1=228,.field_2=0.65625,.field_3=247,.field_4=84}},.field_3=(t3) {.field_1=3134223810560,.field_2=102,.field_3=(t1) {.field_1=128,.field_2=-1.21875,.field_3=232,.field_4=105}}};
    t2 a_1_4 = (t2) {.field_1=-1.203125,.field_2=(t1) {.field_1=110,.field_2=0.6875,.field_3=119,.field_4=200}};
    t7 a_1_5 = (t7) {.field_1=(t5) {.field_1=94503966736384,.field_2=2770153426,.field_3=187,.field_4=214,.field_5=213410253897728},.field_2=164,.field_3=E_6_2};
    float a_1_6 = 1.53125;
    double a_1_7 = 1.3984375;
    t8 a_1_8 = (t8) {.field_1=(t4) {.field_1=(t2) {.field_1=0.578125,.field_2=(t1) {.field_1=170,.field_2=1.09375,.field_3=193,.field_4=148}},.field_2=(t3) {.field_1=271021012025344,.field_2=173,.field_3=(t1) {.field_1=241,.field_2=1.46875,.field_3=80,.field_4=71}},.field_3=(t3) {.field_1=86092233441280,.field_2=245,.field_3=(t1) {.field_1=3,.field_2=-1.125,.field_3=183,.field_4=168}}},.field_2=(t3) {.field_1=110146554691584,.field_2=246,.field_3=(t1) {.field_1=164,.field_2=0.8125,.field_3=164,.field_4=89}},.field_3=0.28125,.field_4=E_6_1};
    t9 ret_1 = fn_1_myr(a_1_1, a_1_2, a_1_3, a_1_4, a_1_5, a_1_6, a_1_7, a_1_8);
    if (!(((ret_1.field_1.field_1==192) && (ret_1.field_1.field_2==-1.296875) && (ret_1.field_1.field_3==50) && (ret_1.field_1.field_4==129)) && ((ret_1.field_2.field_1==188458318561280) && (ret_1.field_2.field_2==40) && ((ret_1.field_2.field_3.field_1==47) && (ret_1.field_2.field_3.field_2==0.359375) && (ret_1.field_2.field_3.field_3==32) && (ret_1.field_2.field_3.field_4==127))))) {
        return -1;
    }

    t12 a_2_1 = (t12) {.field_1=2.7734375,.field_2=(t10) {.field_1=258260610842624,.field_2=(t7) {.field_1=(t5) {.field_1=67688764407808,.field_2=122501974,.field_3=122,.field_4=53,.field_5=46816349519872},.field_2=175,.field_3=E_6_7},.field_3=(t9) {.field_1=(t1) {.field_1=242,.field_2=-0.828125,.field_3=64,.field_4=212},.field_2=(t3) {.field_1=175981577109504,.field_2=146,.field_3=(t1) {.field_1=58,.field_2=-1.3125,.field_3=81,.field_4=249}}}},.field_3=(t11) {.field_1=(t2) {.field_1=-0.671875,.field_2=(t1) {.field_1=39,.field_2=1.3125,.field_3=255,.field_4=168}},.field_2=5680861020160,.field_3=3.1015625}};
    t14 a_2_2 = (t14) {.field_1=-0.65625,.field_2=(t13) {.field_1=-3.07421875,.field_2=2364459620,.field_3=34004063092736,.field_4=(t4) {.field_1=(t2) {.field_1=0.734375,.field_2=(t1) {.field_1=31,.field_2=-0.46875,.field_3=207,.field_4=112}},.field_2=(t3) {.field_1=91868031090688,.field_2=24,.field_3=(t1) {.field_1=152,.field_2=-0.796875,.field_3=24,.field_4=64}},.field_3=(t3) {.field_1=74808418369536,.field_2=254,.field_3=(t1) {.field_1=6,.field_2=0.046875,.field_3=73,.field_4=208}}}}};
    t7 a_2_3 = (t7) {.field_1=(t5) {.field_1=145965008551936,.field_2=2498931774,.field_3=156,.field_4=179,.field_5=265737065725952},.field_2=244,.field_3=E_6_5};
    float a_2_4 = -0.796875;
    t15 a_2_5 = E_15_4;
    t14 a_2_6 = (t14) {.field_1=0.6875,.field_2=(t13) {.field_1=-2.171875,.field_2=1533036440,.field_3=12142327889920,.field_4=(t4) {.field_1=(t2) {.field_1=-0.5625,.field_2=(t1) {.field_1=243,.field_2=-0.65625,.field_3=106,.field_4=51}},.field_2=(t3) {.field_1=156078303608832,.field_2=104,.field_3=(t1) {.field_1=58,.field_2=-1.34375,.field_3=178,.field_4=10}},.field_3=(t3) {.field_1=140200370896896,.field_2=33,.field_3=(t1) {.field_1=143,.field_2=-1.3125,.field_3=190,.field_4=67}}}}};
    uint8_t a_2_7 = 179;
    t17 ret_2 = fn_2_myr(a_2_1, a_2_2, a_2_3, a_2_4, a_2_5, a_2_6, a_2_7);
    if (!(((ret_2.field_1.field_1==101589888270336) && ((ret_2.field_1.field_2.field_1==225) && (ret_2.field_1.field_2.field_2==-0.046875) && (ret_2.field_1.field_2.field_3==234) && (ret_2.field_1.field_2.field_4==74)) && (ret_2.field_1.field_3==200069064425472) && (ret_2.field_1.field_4==1.53125)) && (ret_2.field_2==1.53125))) {
        return -1;
    }

    t15 a_3_1 = E_15_5;
    t23 a_3_2 = (t23) {.field_1=(t18) {.field_1=(t17) {.field_1=(t16) {.field_1=208514472345600,.field_2=(t1) {.field_1=171,.field_2=0.53125,.field_3=170,.field_4=148},.field_3=137775610724352,.field_4=0.046875},.field_2=-0.109375},.field_2=(t8) {.field_1=(t4) {.field_1=(t2) {.field_1=-0.515625,.field_2=(t1) {.field_1=130,.field_2=0.5625,.field_3=65,.field_4=249}},.field_2=(t3) {.field_1=49920772014080,.field_2=137,.field_3=(t1) {.field_1=81,.field_2=-1.078125,.field_3=187,.field_4=244}},.field_3=(t3) {.field_1=238917205884928,.field_2=234,.field_3=(t1) {.field_1=148,.field_2=-0.65625,.field_3=148,.field_4=82}}},.field_2=(t3) {.field_1=13348047552512,.field_2=206,.field_3=(t1) {.field_1=236,.field_2=-0.390625,.field_3=159,.field_4=22}},.field_3=-1.203125,.field_4=E_6_3},.field_3=3263768112,.field_4=(t4) {.field_1=(t2) {.field_1=0.890625,.field_2=(t1) {.field_1=223,.field_2=-0.78125,.field_3=76,.field_4=85}},.field_2=(t3) {.field_1=165112780685312,.field_2=206,.field_3=(t1) {.field_1=4,.field_2=1.078125,.field_3=200,.field_4=49}},.field_3=(t3) {.field_1=107450067189760,.field_2=25,.field_3=(t1) {.field_1=89,.field_2=-1.203125,.field_3=14,.field_4=51}}}},.field_2=E_19_1,.field_3=178689565917184,.field_4=E_20_4,.field_5=E_21_6,.field_6=(t22) {.field_1=269113953746944,.field_2=2315358406}};
    t21 a_3_3 = E_21_9;
    uint8_t a_3_4 = 248;
    uint8_t a_3_5 = 86;
    t27 a_3_6 = (t27) {.field_1=85770339090432,.field_2=(t24) {.field_1=-0.671875},.field_3=(t25) {.field_1=-1.15234375,.field_2=E_20_1,.field_3=E_21_1,.field_4=(t12) {.field_1=0.46875,.field_2=(t10) {.field_1=3865086263296,.field_2=(t7) {.field_1=(t5) {.field_1=62745143410688,.field_2=444241798,.field_3=148,.field_4=38,.field_5=186451756843008},.field_2=253,.field_3=E_6_6},.field_3=(t9) {.field_1=(t1) {.field_1=106,.field_2=0.875,.field_3=169,.field_4=132},.field_2=(t3) {.field_1=193552010969088,.field_2=134,.field_3=(t1) {.field_1=209,.field_2=0.390625,.field_3=6,.field_4=157}}}},.field_3=(t11) {.field_1=(t2) {.field_1=-1.015625,.field_2=(t1) {.field_1=122,.field_2=1.40625,.field_3=38,.field_4=208}},.field_2=55544454578176,.field_3=-2.609375}}},.field_4=(t26) {.field_1=33366908207104,.field_2=136001504280576,.field_3=132}};
    uint32_t ret_3 = fn_3_myr(a_3_1, a_3_2, a_3_3, a_3_4, a_3_5, a_3_6);
    if (!(ret_3==2527618668)) {
        return -1;
    }

    t30 a_4_1 = (t30) {.field_1=(t28) {.field_1=(t23) {.field_1=(t18) {.field_1=(t17) {.field_1=(t16) {.field_1=44445459546112,.field_2=(t1) {.field_1=83,.field_2=-0.453125,.field_3=82,.field_4=113},.field_3=235939541549056,.field_4=1.078125},.field_2=0.3125},.field_2=(t8) {.field_1=(t4) {.field_1=(t2) {.field_1=-0.234375,.field_2=(t1) {.field_1=236,.field_2=0.828125,.field_3=60,.field_4=26}},.field_2=(t3) {.field_1=143671663132672,.field_2=50,.field_3=(t1) {.field_1=243,.field_2=-0.796875,.field_3=3,.field_4=53}},.field_3=(t3) {.field_1=214366877515776,.field_2=126,.field_3=(t1) {.field_1=252,.field_2=1.453125,.field_3=128,.field_4=149}}},.field_2=(t3) {.field_1=223779870998528,.field_2=137,.field_3=(t1) {.field_1=203,.field_2=0.953125,.field_3=12,.field_4=243}},.field_3=1.453125,.field_4=E_6_3},.field_3=1314332054,.field_4=(t4) {.field_1=(t2) {.field_1=-0.671875,.field_2=(t1) {.field_1=209,.field_2=-1.125,.field_3=32,.field_4=106}},.field_2=(t3) {.field_1=160715212455936,.field_2=12,.field_3=(t1) {.field_1=45,.field_2=0.96875,.field_3=39,.field_4=176}},.field_3=(t3) {.field_1=2142336450560,.field_2=26,.field_3=(t1) {.field_1=239,.field_2=-1.5,.field_3=80,.field_4=178}}}},.field_2=E_19_5,.field_3=84526311014400,.field_4=E_20_6,.field_5=E_21_1,.field_6=(t22) {.field_1=248535774265344,.field_2=1964811016}},.field_2=0.1875,.field_3=E_6_5,.field_4=-1.87109375},.field_2=(t29) {.field_1=155,.field_2=2693759888}};
    uint64_t ret_4 = fn_4_myr(a_4_1);
    if (!(ret_4==276275068796928)) {
        return -1;
    }

    t33 a_5_1 = (t33) {.field_1=E_31_3,.field_2=E_32_5};
    t34 a_5_2 = (t34) {.field_1=E_32_5,.field_2=66};
    t36 a_5_3 = (t36) {.field_1=E_35_4,.field_2=214};
    t37 a_5_4 = E_37_1;
    uint64_t a_5_5 = 210077727850496;
    double ret_5 = fn_5_myr(a_5_1, a_5_2, a_5_3, a_5_4, a_5_5);
    if (!(ret_5==-2.5234375)) {
        return -1;
    }

    t40 a_6_1 = (t40) {.field_1=(t8) {.field_1=(t4) {.field_1=(t2) {.field_1=0.71875,.field_2=(t1) {.field_1=162,.field_2=-1.375,.field_3=12,.field_4=208}},.field_2=(t3) {.field_1=208257259929600,.field_2=224,.field_3=(t1) {.field_1=202,.field_2=-0.015625,.field_3=86,.field_4=251}},.field_3=(t3) {.field_1=253976550440960,.field_2=123,.field_3=(t1) {.field_1=146,.field_2=0.828125,.field_3=120,.field_4=62}}},.field_2=(t3) {.field_1=26298971062272,.field_2=103,.field_3=(t1) {.field_1=239,.field_2=1.28125,.field_3=152,.field_4=89}},.field_3=0.734375,.field_4=E_6_3},.field_2=E_6_1,.field_3=E_38_6,.field_4=(t39) {.field_1=(t27) {.field_1=152549047009280,.field_2=(t24) {.field_1=0.859375},.field_3=(t25) {.field_1=-3.7109375,.field_2=E_20_7,.field_3=E_21_5,.field_4=(t12) {.field_1=1.26171875,.field_2=(t10) {.field_1=257906686427136,.field_2=(t7) {.field_1=(t5) {.field_1=154973503488000,.field_2=2040012194,.field_3=167,.field_4=109,.field_5=74787062284288},.field_2=39,.field_3=E_6_6},.field_3=(t9) {.field_1=(t1) {.field_1=63,.field_2=-1.390625,.field_3=63,.field_4=209},.field_2=(t3) {.field_1=230983872282624,.field_2=184,.field_3=(t1) {.field_1=15,.field_2=1.296875,.field_3=31,.field_4=255}}}},.field_3=(t11) {.field_1=(t2) {.field_1=1.015625,.field_2=(t1) {.field_1=183,.field_2=-0.484375,.field_3=142,.field_4=16}},.field_2=137163913822208,.field_3=-2.80859375}}},.field_4=(t26) {.field_1=170983975682048,.field_2=72796805398528,.field_3=42}},.field_2=1990287632,.field_3=(t29) {.field_1=194,.field_2=3552591666},.field_4=(t8) {.field_1=(t4) {.field_1=(t2) {.field_1=-0.375,.field_2=(t1) {.field_1=79,.field_2=-0.625,.field_3=9,.field_4=188}},.field_2=(t3) {.field_1=159828972142592,.field_2=48,.field_3=(t1) {.field_1=128,.field_2=1.0,.field_3=62,.field_4=192}},.field_3=(t3) {.field_1=179052505464832,.field_2=16,.field_3=(t1) {.field_1=120,.field_2=0.625,.field_3=250,.field_4=151}}},.field_2=(t3) {.field_1=196274257788928,.field_2=205,.field_3=(t1) {.field_1=79,.field_2=-1.4375,.field_3=91,.field_4=96}},.field_3=0.09375,.field_4=E_6_4},.field_5=1.515625},.field_5=2.265625};
    t41 a_6_2 = E_41_5;
    t34 a_6_3 = (t34) {.field_1=E_32_8,.field_2=203};
    t42 a_6_4 = (t42) {.field_1=-1.58984375,.field_2=217,.field_3=3500137676,.field_4=101686982737920};
    double a_6_5 = 0.10546875;
    double a_6_6 = 3.21875;
    uint64_t a_6_7 = 40133181046784;
    uint64_t a_6_8 = 71741835313152;
    t26 a_6_9 = (t26) {.field_1=221313136459776,.field_2=104460582912,.field_3=149};
    t43 ret_6 = fn_6_myr(a_6_1, a_6_2, a_6_3, a_6_4, a_6_5, a_6_6, a_6_7, a_6_8, a_6_9);
    if (!(ret_6==E_43_1)) {
        return -1;
    }

    t48 a_7_1 = (t48) {.field_1=(t44) {.field_1=6386332991488,.field_2=-3.90625,.field_3=163,.field_4=155193554763776,.field_5=(t36) {.field_1=E_35_3,.field_2=18}},.field_2=(t45) {.field_1=(t44) {.field_1=112229799297024,.field_2=-2.8984375,.field_3=216,.field_4=44483204612096,.field_5=(t36) {.field_1=E_35_2,.field_2=195}},.field_2=39665754832896,.field_3=114,.field_4=776337414},.field_3=E_46_5,.field_4=146729052536832,.field_5=(t47) {.field_1=(t9) {.field_1=(t1) {.field_1=153,.field_2=1.234375,.field_3=43,.field_4=174},.field_2=(t3) {.field_1=26828980748288,.field_2=242,.field_3=(t1) {.field_1=236,.field_2=-1.078125,.field_3=180,.field_4=30}}},.field_2=266092824363008},.field_6=(t27) {.field_1=13831859994624,.field_2=(t24) {.field_1=0.390625},.field_3=(t25) {.field_1=0.0703125,.field_2=E_20_1,.field_3=E_21_6,.field_4=(t12) {.field_1=-2.765625,.field_2=(t10) {.field_1=175411747749888,.field_2=(t7) {.field_1=(t5) {.field_1=262729443049472,.field_2=932333724,.field_3=5,.field_4=16,.field_5=17637135220736},.field_2=46,.field_3=E_6_2},.field_3=(t9) {.field_1=(t1) {.field_1=211,.field_2=-0.5625,.field_3=171,.field_4=1},.field_2=(t3) {.field_1=12764915826688,.field_2=49,.field_3=(t1) {.field_1=154,.field_2=1.375,.field_3=93,.field_4=73}}}},.field_3=(t11) {.field_1=(t2) {.field_1=-1.46875,.field_2=(t1) {.field_1=79,.field_2=-0.90625,.field_3=49,.field_4=4}},.field_2=93216219463680,.field_3=-2.83203125}}},.field_4=(t26) {.field_1=18454498639872,.field_2=269237052506112,.field_3=165}}};
    uint32_t ret_7 = fn_7_myr(a_7_1);
    if (!(ret_7==2499773006)) {
        return -1;
    }

    t1 a_8_1 = (t1) {.field_1=200,.field_2=0.25,.field_3=18,.field_4=246};
    t50 a_8_2 = (t50) {.field_1=-0.734375,.field_2=(t44) {.field_1=8592161505280,.field_2=2.32421875,.field_3=77,.field_4=95804356820992,.field_5=(t36) {.field_1=E_35_1,.field_2=126}},.field_3=-1.12109375,.field_4=E_49_3};
    t51 a_8_3 = E_51_3;
    float a_8_4 = 0.1875;
    t52 a_8_5 = (t52) {.field_1=-0.81640625};
    uint32_t a_8_6 = 441584498;
    t53 a_8_7 = (t53) {.field_1=198};
    uint8_t a_8_8 = 89;
    float ret_8 = fn_8_myr(a_8_1, a_8_2, a_8_3, a_8_4, a_8_5, a_8_6, a_8_7, a_8_8);
    if (!(ret_8==1.421875)) {
        return -1;
    }

    t51 a_9_1 = E_51_8;
    float a_9_2 = -0.796875;
    float a_9_3 = -0.71875;
    t56 a_9_4 = (t56) {.field_1=E_54_7,.field_2=255,.field_3=(t55) {.field_1=(t39) {.field_1=(t27) {.field_1=130105287114752,.field_2=(t24) {.field_1=-1.0},.field_3=(t25) {.field_1=3.53125,.field_2=E_20_2,.field_3=E_21_1,.field_4=(t12) {.field_1=-0.67578125,.field_2=(t10) {.field_1=23617654620160,.field_2=(t7) {.field_1=(t5) {.field_1=12594182750208,.field_2=2889081194,.field_3=84,.field_4=84,.field_5=265251503210496},.field_2=17,.field_3=E_6_7},.field_3=(t9) {.field_1=(t1) {.field_1=141,.field_2=-0.1875,.field_3=10,.field_4=118},.field_2=(t3) {.field_1=24302215233536,.field_2=170,.field_3=(t1) {.field_1=64,.field_2=0.953125,.field_3=156,.field_4=112}}}},.field_3=(t11) {.field_1=(t2) {.field_1=0.3125,.field_2=(t1) {.field_1=168,.field_2=0.3125,.field_3=140,.field_4=197}},.field_2=149682231705600,.field_3=0.3828125}}},.field_4=(t26) {.field_1=66190105247744,.field_2=201243456045056,.field_3=128}},.field_2=1608787508,.field_3=(t29) {.field_1=208,.field_2=3925244758},.field_4=(t8) {.field_1=(t4) {.field_1=(t2) {.field_1=-0.125,.field_2=(t1) {.field_1=219,.field_2=0.265625,.field_3=201,.field_4=48}},.field_2=(t3) {.field_1=148885215510528,.field_2=219,.field_3=(t1) {.field_1=41,.field_2=-1.3125,.field_3=75,.field_4=52}},.field_3=(t3) {.field_1=153670362464256,.field_2=97,.field_3=(t1) {.field_1=222,.field_2=0.9375,.field_3=47,.field_4=122}}},.field_2=(t3) {.field_1=66828912033792,.field_2=201,.field_3=(t1) {.field_1=35,.field_2=1.046875,.field_3=86,.field_4=233}},.field_3=-0.421875,.field_4=E_6_7},.field_5=-1.109375},.field_2=350527626,.field_3=98,.field_4=(t7) {.field_1=(t5) {.field_1=146849136115712,.field_2=3845172994,.field_3=76,.field_4=250,.field_5=212141542211584},.field_2=227,.field_3=E_6_6}}};
    t58 a_9_5 = (t58) {.field_1=E_57_1,.field_2=(t53) {.field_1=77},.field_3=-2.5};
    t61 a_9_6 = (t61) {.field_1=E_59_2,.field_2=-0.984375,.field_3=(t60) {.field_1=-1.09375,.field_2=-1.53125,.field_3=20793480314880,.field_4=36073398206464}};
    t65 a_9_7 = (t65) {.field_1=(t62) {.field_1=-2.32421875,.field_2=208},.field_2=(t63) {.field_1=-0.0625,.field_2=(t47) {.field_1=(t9) {.field_1=(t1) {.field_1=224,.field_2=-0.984375,.field_3=131,.field_4=218},.field_2=(t3) {.field_1=209069555974144,.field_2=100,.field_3=(t1) {.field_1=82,.field_2=1.4375,.field_3=89,.field_4=16}}},.field_2=273030717767680},.field_3=(t23) {.field_1=(t18) {.field_1=(t17) {.field_1=(t16) {.field_1=62938097254400,.field_2=(t1) {.field_1=107,.field_2=0.609375,.field_3=97,.field_4=70},.field_3=77561457147904,.field_4=1.265625},.field_2=-1.515625},.field_2=(t8) {.field_1=(t4) {.field_1=(t2) {.field_1=-0.828125,.field_2=(t1) {.field_1=174,.field_2=1.390625,.field_3=142,.field_4=121}},.field_2=(t3) {.field_1=266278944636928,.field_2=1,.field_3=(t1) {.field_1=57,.field_2=-0.1875,.field_3=53,.field_4=10}},.field_3=(t3) {.field_1=258630503497728,.field_2=22,.field_3=(t1) {.field_1=58,.field_2=-0.203125,.field_3=241,.field_4=248}}},.field_2=(t3) {.field_1=232005730828288,.field_2=67,.field_3=(t1) {.field_1=237,.field_2=-1.03125,.field_3=83,.field_4=229}},.field_3=-0.328125,.field_4=E_6_6},.field_3=2541087240,.field_4=(t4) {.field_1=(t2) {.field_1=0.875,.field_2=(t1) {.field_1=4,.field_2=1.15625,.field_3=173,.field_4=8}},.field_2=(t3) {.field_1=29310895783936,.field_2=92,.field_3=(t1) {.field_1=250,.field_2=0.515625,.field_3=214,.field_4=236}},.field_3=(t3) {.field_1=188538419019776,.field_2=16,.field_3=(t1) {.field_1=92,.field_2=1.1875,.field_3=26,.field_4=72}}}},.field_2=E_19_9,.field_3=93123907813376,.field_4=E_20_6,.field_5=E_21_9,.field_6=(t22) {.field_1=84919533043712,.field_2=2312961846}},.field_4=738749758},.field_3=(t64) {.field_1=(t61) {.field_1=E_59_2,.field_2=0.671875,.field_3=(t60) {.field_1=-0.01953125,.field_2=-1.171875,.field_3=30820079632384,.field_4=68216701976576}},.field_2=E_21_7,.field_3=227,.field_4=E_31_2}};
    t66 a_9_8 = E_66_3;
    t67 a_9_9 = E_67_3;
    double a_9_10 = 2.06640625;
    uint64_t ret_9 = fn_9_myr(a_9_1, a_9_2, a_9_3, a_9_4, a_9_5, a_9_6, a_9_7, a_9_8, a_9_9, a_9_10);
    if (!(ret_9==6316575686656)) {
        return -1;
    }

    t74 a_10_1 = (t74) {.field_1=(t68) {.field_1=1.21875,.field_2=-0.890625,.field_3=E_54_5,.field_4=-1.140625},.field_2=(t69) {.field_1=-2.86328125,.field_2=(t16) {.field_1=272734424006656,.field_2=(t1) {.field_1=168,.field_2=-0.3125,.field_3=7,.field_4=96},.field_3=194650230882304,.field_4=1.3125},.field_3=2832640246,.field_4=248902425116672,.field_5=3177491204,.field_6=E_20_7},.field_3=(t70) {.field_1=(t9) {.field_1=(t1) {.field_1=242,.field_2=1.0,.field_3=118,.field_4=202},.field_2=(t3) {.field_1=130511054045184,.field_2=60,.field_3=(t1) {.field_1=149,.field_2=1.25,.field_3=209,.field_4=211}}},.field_2=65},.field_4=E_71_4,.field_5=E_72_7,.field_6=(t73) {.field_1=1065245434,.field_2=0.359375,.field_3=(t22) {.field_1=182449277763584,.field_2=2003708228}}};
    float a_10_2 = 0.890625;
    t75 a_10_3 = E_75_4;
    uint8_t a_10_4 = 73;
    t78 a_10_5 = (t78) {.field_1=100724679639040,.field_2=(t44) {.field_1=197606249070592,.field_2=0.3515625,.field_3=45,.field_4=111205168709632,.field_5=(t36) {.field_1=E_35_6,.field_2=168}},.field_3=(t76) {.field_1=(t55) {.field_1=(t39) {.field_1=(t27) {.field_1=273706513334272,.field_2=(t24) {.field_1=0.625},.field_3=(t25) {.field_1=3.27734375,.field_2=E_20_4,.field_3=E_21_5,.field_4=(t12) {.field_1=1.125,.field_2=(t10) {.field_1=228193040859136,.field_2=(t7) {.field_1=(t5) {.field_1=197736696643584,.field_2=2597012506,.field_3=181,.field_4=213,.field_5=107853796474880},.field_2=110,.field_3=E_6_2},.field_3=(t9) {.field_1=(t1) {.field_1=122,.field_2=1.296875,.field_3=218,.field_4=48},.field_2=(t3) {.field_1=13448229421056,.field_2=244,.field_3=(t1) {.field_1=47,.field_2=1.453125,.field_3=167,.field_4=113}}}},.field_3=(t11) {.field_1=(t2) {.field_1=-0.5625,.field_2=(t1) {.field_1=91,.field_2=1.53125,.field_3=127,.field_4=192}},.field_2=208774694567936,.field_3=-2.68359375}}},.field_4=(t26) {.field_1=203837156753408,.field_2=124751440248832,.field_3=19}},.field_2=949240352,.field_3=(t29) {.field_1=233,.field_2=3093393614},.field_4=(t8) {.field_1=(t4) {.field_1=(t2) {.field_1=-1.46875,.field_2=(t1) {.field_1=157,.field_2=-0.546875,.field_3=189,.field_4=115}},.field_2=(t3) {.field_1=199305275375616,.field_2=43,.field_3=(t1) {.field_1=171,.field_2=-0.984375,.field_3=22,.field_4=134}},.field_3=(t3) {.field_1=105055744950272,.field_2=34,.field_3=(t1) {.field_1=122,.field_2=0.1875,.field_3=25,.field_4=33}}},.field_2=(t3) {.field_1=1198625914880,.field_2=107,.field_3=(t1) {.field_1=125,.field_2=1.546875,.field_3=235,.field_4=61}},.field_3=0.734375,.field_4=E_6_1},.field_5=1.453125},.field_2=763193548,.field_3=39,.field_4=(t7) {.field_1=(t5) {.field_1=52338931924992,.field_2=391110206,.field_3=223,.field_4=55,.field_5=198983641202688},.field_2=50,.field_3=E_6_7}},.field_2=673599174},.field_4=(t77) {.field_1=(t55) {.field_1=(t39) {.field_1=(t27) {.field_1=254957327745024,.field_2=(t24) {.field_1=-1.171875},.field_3=(t25) {.field_1=2.5546875,.field_2=E_20_1,.field_3=E_21_2,.field_4=(t12) {.field_1=-1.171875,.field_2=(t10) {.field_1=130672052011008,.field_2=(t7) {.field_1=(t5) {.field_1=97871770550272,.field_2=3555186718,.field_3=6,.field_4=114,.field_5=270228958085120},.field_2=7,.field_3=E_6_7},.field_3=(t9) {.field_1=(t1) {.field_1=114,.field_2=-1.5,.field_3=200,.field_4=176},.field_2=(t3) {.field_1=212862159159296,.field_2=220,.field_3=(t1) {.field_1=167,.field_2=1.359375,.field_3=3,.field_4=215}}}},.field_3=(t11) {.field_1=(t2) {.field_1=-1.484375,.field_2=(t1) {.field_1=226,.field_2=-1.390625,.field_3=187,.field_4=21}},.field_2=4365063815168,.field_3=3.0390625}}},.field_4=(t26) {.field_1=278530325086209,.field_2=40351778734080,.field_3=183}},.field_2=345942008,.field_3=(t29) {.field_1=91,.field_2=281298906},.field_4=(t8) {.field_1=(t4) {.field_1=(t2) {.field_1=0.140625,.field_2=(t1) {.field_1=180,.field_2=1.234375,.field_3=145,.field_4=38}},.field_2=(t3) {.field_1=240181903360000,.field_2=153,.field_3=(t1) {.field_1=4,.field_2=-0.625,.field_3=158,.field_4=204}},.field_3=(t3) {.field_1=279255610818561,.field_2=96,.field_3=(t1) {.field_1=169,.field_2=0.46875,.field_3=79,.field_4=172}}},.field_2=(t3) {.field_1=137219642097664,.field_2=85,.field_3=(t1) {.field_1=143,.field_2=0.140625,.field_3=17,.field_4=164}},.field_3=0.1875,.field_4=E_6_7},.field_5=0.421875},.field_2=3026647616,.field_3=171,.field_4=(t7) {.field_1=(t5) {.field_1=200920828215296,.field_2=266056950,.field_3=188,.field_4=66,.field_5=215864985649152},.field_2=161,.field_3=E_6_6}},.field_2=234,.field_3=-0.12890625,.field_4=109,.field_5=(t70) {.field_1=(t9) {.field_1=(t1) {.field_1=238,.field_2=0.890625,.field_3=11,.field_4=187},.field_2=(t3) {.field_1=218973062561792,.field_2=108,.field_3=(t1) {.field_1=100,.field_2=-0.234375,.field_3=187,.field_4=17}}},.field_2=233},.field_6=296769572}};
    t79 a_10_6 = E_79_2;
    float ret_10 = fn_10_myr(a_10_1, a_10_2, a_10_3, a_10_4, a_10_5, a_10_6);
    if (!(ret_10==-1.515625)) {
        return -1;
    }

    return 0;
}
