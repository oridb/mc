/* CFLAGS: -I/usr/include/ */
/* CFLAGS: -I/usr/include */
/* LIBS: c */

#include <stdint.h>
#include "004_types.h"

extern t2 fn_1_myr(double a1);
extern uint64_t fn_2_myr(double a1, t4 a2, t1 a3, t5 a4);
extern t13 fn_3_myr(t1 a1, t3 a2, t7 a3, t4 a4, t9 a5, uint8_t a6, float a7, float a8);
extern t19 fn_4_myr(t16 a1);
extern t29 fn_5_myr(uint32_t a1, t11 a2, t21 a3, t23 a4, t24 a5, uint32_t a6, double a7, t26 a8, t27 a9, t28 a10);
extern uint32_t fn_6_myr(t20 a1, t15 a2);
extern t37 fn_7_myr(double a1, double a2, t31 a3, t21 a4, double a5, float a6, t35 a7, uint64_t a8);
extern t42 fn_8_myr(t38 a1, t39 a2, double a3, uint8_t a4);
extern uint64_t fn_9_myr(t46 a1, float a2, t26 a3, t47 a4, t48 a5, t54 a6, uint32_t a7, float a8, t55 a9, t56 a10);
extern t3 fn_10_myr(uint64_t a1, double a2, t61 a3, uint8_t a4, t62 a5, uint8_t a6, double a7, uint64_t a8);

t2
fn_1_c(double a1)
{
    if (!(a1==-0.26171875)) {
        goto bad;
    }

    return (t2) {.field_1=12459410325504,.field_2=98659448520704,.field_3=(t1) {.field_1=166,.field_2=107,.field_3=0.140625,.field_4=92389009653760},.field_4=-3.140625,.field_5=(t1) {.field_1=157,.field_2=191,.field_3=1.125,.field_4=644801363968}};

bad:
    return (t2) {.field_1=238850629042176,.field_2=48799761170432,.field_3=(t1) {.field_1=120,.field_2=183,.field_3=1.109375,.field_4=182937149505536},.field_4=3.46875,.field_5=(t1) {.field_1=140,.field_2=106,.field_3=-0.4375,.field_4=114576194142208}};
}

uint64_t
fn_2_c(double a1, t4 a2, t1 a3, t5 a4)
{
    if (!(a1==-3.80078125)) {
        goto bad;
    }

    if (!((a2.field_1==2.8203125) && (a2.field_2==1.40625) && ((a2.field_3.field_1==281155744301057) && ((a2.field_3.field_2.field_1==201484396396544) && (a2.field_3.field_2.field_2==167610483867648) && ((a2.field_3.field_2.field_3.field_1==241) && (a2.field_3.field_2.field_3.field_2==67) && (a2.field_3.field_2.field_3.field_3==-1.546875) && (a2.field_3.field_2.field_3.field_4==85217436499968)) && (a2.field_3.field_2.field_4==1.33203125) && ((a2.field_3.field_2.field_5.field_1==115) && (a2.field_3.field_2.field_5.field_2==174) && (a2.field_3.field_2.field_5.field_3==0.578125) && (a2.field_3.field_2.field_5.field_4==108879745908736))) && ((a2.field_3.field_3.field_1==46) && (a2.field_3.field_3.field_2==0) && (a2.field_3.field_3.field_3==0.484375) && (a2.field_3.field_3.field_4==41200816095232)) && (a2.field_3.field_4==1.640625) && (a2.field_3.field_5==4163457034)))) {
        goto bad;
    }

    if (!((a3.field_1==179) && (a3.field_2==82) && (a3.field_3==0.796875) && (a3.field_4==149846806626304))) {
        goto bad;
    }

    if (!((a4.field_1==132525092503552) && (a4.field_2==-1.3125))) {
        goto bad;
    }

    return 132205860093952;

bad:
    return 224271076950016;
}

t13
fn_3_c(t1 a1, t3 a2, t7 a3, t4 a4, t9 a5, uint8_t a6, float a7, float a8)
{
    if (!((a1.field_1==211) && (a1.field_2==183) && (a1.field_3==0.71875) && (a1.field_4==77205130182656))) {
        goto bad;
    }

    if (!((a2.field_1==186084876091392) && ((a2.field_2.field_1==162500698177536) && (a2.field_2.field_2==117147867938816) && ((a2.field_2.field_3.field_1==81) && (a2.field_2.field_3.field_2==185) && (a2.field_2.field_3.field_3==-1.171875) && (a2.field_2.field_3.field_4==80629570863104)) && (a2.field_2.field_4==-2.453125) && ((a2.field_2.field_5.field_1==49) && (a2.field_2.field_5.field_2==253) && (a2.field_2.field_5.field_3==0.03125) && (a2.field_2.field_5.field_4==266183655817216))) && ((a2.field_3.field_1==133) && (a2.field_3.field_2==78) && (a2.field_3.field_3==-1.15625) && (a2.field_3.field_4==208468516143104)) && (a2.field_4==0.625) && (a2.field_5==2593151244))) {
        goto bad;
    }

    if (!((a3.field_1==104) && (a3.field_2==E_6_7) && ((a3.field_3.field_1==168319363710976) && (a3.field_3.field_2==1.015625)) && ((a3.field_4.field_1==105329353555968) && (a3.field_4.field_2==49345085177856) && ((a3.field_4.field_3.field_1==61) && (a3.field_4.field_3.field_2==176) && (a3.field_4.field_3.field_3==1.25) && (a3.field_4.field_3.field_4==102963779993600)) && (a3.field_4.field_4==3.734375) && ((a3.field_4.field_5.field_1==21) && (a3.field_4.field_5.field_2==143) && (a3.field_4.field_5.field_3==1.46875) && (a3.field_4.field_5.field_4==167234555215872))))) {
        goto bad;
    }

    if (!((a4.field_1==3.12890625) && (a4.field_2==0.4375) && ((a4.field_3.field_1==63982214447104) && ((a4.field_3.field_2.field_1==135072305512448) && (a4.field_3.field_2.field_2==67998052515840) && ((a4.field_3.field_2.field_3.field_1==12) && (a4.field_3.field_2.field_3.field_2==71) && (a4.field_3.field_2.field_3.field_3==-0.578125) && (a4.field_3.field_2.field_3.field_4==187813731500032)) && (a4.field_3.field_2.field_4==3.07421875) && ((a4.field_3.field_2.field_5.field_1==205) && (a4.field_3.field_2.field_5.field_2==88) && (a4.field_3.field_2.field_5.field_3==-0.53125) && (a4.field_3.field_2.field_5.field_4==674692464640))) && ((a4.field_3.field_3.field_1==193) && (a4.field_3.field_3.field_2==11) && (a4.field_3.field_3.field_3==0.3125) && (a4.field_3.field_3.field_4==162599899496448)) && (a4.field_3.field_4==-0.62890625) && (a4.field_3.field_5==3331590902)))) {
        goto bad;
    }

    if (!((a5.field_1==E_8_5) && (a5.field_2==3187538484))) {
        goto bad;
    }

    if (!(a6==90)) {
        goto bad;
    }

    if (!(a7==0.125)) {
        goto bad;
    }

    if (!(a8==1.203125)) {
        goto bad;
    }

    return (t13) {.field_1=650661816,.field_2=(t10) {.field_1=-0.015625,.field_2=2851726420},.field_3=(t11) {.field_1=-3.625,.field_2=(t2) {.field_1=74772813709312,.field_2=112035052126208,.field_3=(t1) {.field_1=41,.field_2=21,.field_3=-0.28125,.field_4=170067893944320},.field_4=0.52734375,.field_5=(t1) {.field_1=81,.field_2=216,.field_3=-0.328125,.field_4=156496708894720}}},.field_4=(t12) {.field_1=(t4) {.field_1=-3.19140625,.field_2=1.515625,.field_3=(t3) {.field_1=236308757348352,.field_2=(t2) {.field_1=263713238548480,.field_2=40608255049728,.field_3=(t1) {.field_1=138,.field_2=142,.field_3=-1.1875,.field_4=100184688558080},.field_4=2.76171875,.field_5=(t1) {.field_1=73,.field_2=65,.field_3=1.28125,.field_4=53591441145856}},.field_3=(t1) {.field_1=54,.field_2=81,.field_3=-0.5,.field_4=200208867524608},.field_4=3.7734375,.field_5=1624457196}},.field_2=(t1) {.field_1=250,.field_2=97,.field_3=0.125,.field_4=16842727882752},.field_3=(t5) {.field_1=222985090629632,.field_2=-1.109375}}};

bad:
    return (t13) {.field_1=2694026068,.field_2=(t10) {.field_1=-1.21875,.field_2=4260115106},.field_3=(t11) {.field_1=-3.71484375,.field_2=(t2) {.field_1=187395190947840,.field_2=23501456670720,.field_3=(t1) {.field_1=1,.field_2=129,.field_3=-1.5,.field_4=42547396608000},.field_4=-3.53515625,.field_5=(t1) {.field_1=148,.field_2=70,.field_3=-0.296875,.field_4=121552673308672}}},.field_4=(t12) {.field_1=(t4) {.field_1=0.47265625,.field_2=0.5,.field_3=(t3) {.field_1=96003926654976,.field_2=(t2) {.field_1=211416389124096,.field_2=246412730040320,.field_3=(t1) {.field_1=168,.field_2=23,.field_3=0.265625,.field_4=181009723555840},.field_4=-0.2265625,.field_5=(t1) {.field_1=144,.field_2=6,.field_3=-1.53125,.field_4=175496053391360}},.field_3=(t1) {.field_1=208,.field_2=40,.field_3=-0.78125,.field_4=260664153079808},.field_4=-2.7421875,.field_5=1183476516}},.field_2=(t1) {.field_1=151,.field_2=59,.field_3=-0.6875,.field_4=27338362322944},.field_3=(t5) {.field_1=71252360822784,.field_2=-0.203125}}};
}

t19
fn_4_c(t16 a1)
{
    if (!((a1.field_1==E_14_1) && (a1.field_2==-0.734375) && (a1.field_3==31855997878272) && (a1.field_4==E_15_5))) {
        goto bad;
    }

    return (t19) {.field_1=0.03125,.field_2=(t17) {.field_1=(t13) {.field_1=947051528,.field_2=(t10) {.field_1=0.96875,.field_2=241559198},.field_3=(t11) {.field_1=-0.125,.field_2=(t2) {.field_1=22236939157504,.field_2=74920569470976,.field_3=(t1) {.field_1=22,.field_2=25,.field_3=-0.46875,.field_4=99222545367040},.field_4=-2.25,.field_5=(t1) {.field_1=6,.field_2=66,.field_3=0.9375,.field_4=72999681130496}}},.field_4=(t12) {.field_1=(t4) {.field_1=-2.41015625,.field_2=-0.328125,.field_3=(t3) {.field_1=33373226926080,.field_2=(t2) {.field_1=264437028749312,.field_2=41455185559552,.field_3=(t1) {.field_1=58,.field_2=157,.field_3=-1.40625,.field_4=172834173747200},.field_4=2.96484375,.field_5=(t1) {.field_1=47,.field_2=72,.field_3=-0.8125,.field_4=161994014326784}},.field_3=(t1) {.field_1=98,.field_2=191,.field_3=0.9375,.field_4=54448237051904},.field_4=2.390625,.field_5=1201643174}},.field_2=(t1) {.field_1=69,.field_2=18,.field_3=-0.421875,.field_4=104603486781440},.field_3=(t5) {.field_1=119279618490368,.field_2=0.6875}}},.field_2=0.65625,.field_3=125926830637056,.field_4=250},.field_3=E_18_3,.field_4=(t2) {.field_1=198926511898624,.field_2=48406520135680,.field_3=(t1) {.field_1=198,.field_2=211,.field_3=-1.21875,.field_4=259336007450624},.field_4=-3.48828125,.field_5=(t1) {.field_1=185,.field_2=248,.field_3=0.53125,.field_4=170021025611776}}};

bad:
    return (t19) {.field_1=-1.078125,.field_2=(t17) {.field_1=(t13) {.field_1=4079464194,.field_2=(t10) {.field_1=1.09375,.field_2=3151538724},.field_3=(t11) {.field_1=-1.30078125,.field_2=(t2) {.field_1=167739336687616,.field_2=149124248371200,.field_3=(t1) {.field_1=134,.field_2=102,.field_3=0.96875,.field_4=224961458864128},.field_4=-0.203125,.field_5=(t1) {.field_1=45,.field_2=43,.field_3=1.25,.field_4=252713962045440}}},.field_4=(t12) {.field_1=(t4) {.field_1=1.9921875,.field_2=-0.484375,.field_3=(t3) {.field_1=247192255463424,.field_2=(t2) {.field_1=38160966352896,.field_2=14664874459136,.field_3=(t1) {.field_1=12,.field_2=232,.field_3=1.1875,.field_4=45492371193856},.field_4=2.5859375,.field_5=(t1) {.field_1=238,.field_2=226,.field_3=0.953125,.field_4=168620418269184}},.field_3=(t1) {.field_1=125,.field_2=246,.field_3=0.15625,.field_4=94751226331136},.field_4=1.5234375,.field_5=3789142650}},.field_2=(t1) {.field_1=238,.field_2=57,.field_3=-0.28125,.field_4=94165903343616},.field_3=(t5) {.field_1=9631686328320,.field_2=-0.921875}}},.field_2=2.4140625,.field_3=59577473957888,.field_4=97},.field_3=E_18_5,.field_4=(t2) {.field_1=30816459292672,.field_2=37695797854208,.field_3=(t1) {.field_1=13,.field_2=252,.field_3=-0.71875,.field_4=29337515720704},.field_4=-3.61328125,.field_5=(t1) {.field_1=45,.field_2=251,.field_3=-0.9375,.field_4=2851418144768}}};
}

t29
fn_5_c(uint32_t a1, t11 a2, t21 a3, t23 a4, t24 a5, uint32_t a6, double a7, t26 a8, t27 a9, t28 a10)
{
    if (!(a1==3491579916)) {
        goto bad;
    }

    if (!((a2.field_1==2.140625) && ((a2.field_2.field_1==17107741573120) && (a2.field_2.field_2==257014155051008) && ((a2.field_2.field_3.field_1==120) && (a2.field_2.field_3.field_2==241) && (a2.field_2.field_3.field_3==1.078125) && (a2.field_2.field_3.field_4==195206002114560)) && (a2.field_2.field_4==-1.15625) && ((a2.field_2.field_5.field_1==46) && (a2.field_2.field_5.field_2==186) && (a2.field_2.field_5.field_3==0.1875) && (a2.field_2.field_5.field_4==278368438321153))))) {
        goto bad;
    }

    if (!((((a3.field_1.field_1.field_1==66) && (a3.field_1.field_1.field_2==239) && (a3.field_1.field_1.field_3==-1.203125) && (a3.field_1.field_1.field_4==89848418926592)) && ((a3.field_1.field_2.field_1==239095841685504) && (a3.field_1.field_2.field_2==165469104898048) && ((a3.field_1.field_2.field_3.field_1==67) && (a3.field_1.field_2.field_3.field_2==177) && (a3.field_1.field_2.field_3.field_3==-0.6875) && (a3.field_1.field_2.field_3.field_4==172891013120000)) && (a3.field_1.field_2.field_4==2.93359375) && ((a3.field_1.field_2.field_5.field_1==2) && (a3.field_1.field_2.field_5.field_2==44) && (a3.field_1.field_2.field_5.field_3==1.140625) && (a3.field_1.field_2.field_5.field_4==266978004566016)))) && (((a3.field_2.field_1.field_1==488880992) && ((a3.field_2.field_1.field_2.field_1==0.1875) && (a3.field_2.field_1.field_2.field_2==4283670802)) && ((a3.field_2.field_1.field_3.field_1==-2.4453125) && ((a3.field_2.field_1.field_3.field_2.field_1==86642904793088) && (a3.field_2.field_1.field_3.field_2.field_2==9550056259584) && ((a3.field_2.field_1.field_3.field_2.field_3.field_1==135) && (a3.field_2.field_1.field_3.field_2.field_3.field_2==40) && (a3.field_2.field_1.field_3.field_2.field_3.field_3==-1.234375) && (a3.field_2.field_1.field_3.field_2.field_3.field_4==14565778391040)) && (a3.field_2.field_1.field_3.field_2.field_4==3.859375) && ((a3.field_2.field_1.field_3.field_2.field_5.field_1==97) && (a3.field_2.field_1.field_3.field_2.field_5.field_2==149) && (a3.field_2.field_1.field_3.field_2.field_5.field_3==0.75) && (a3.field_2.field_1.field_3.field_2.field_5.field_4==208889540378624)))) && (((a3.field_2.field_1.field_4.field_1.field_1==3.3515625) && (a3.field_2.field_1.field_4.field_1.field_2==1.5625) && ((a3.field_2.field_1.field_4.field_1.field_3.field_1==190971227013120) && ((a3.field_2.field_1.field_4.field_1.field_3.field_2.field_1==13441050607616) && (a3.field_2.field_1.field_4.field_1.field_3.field_2.field_2==89440160972800) && ((a3.field_2.field_1.field_4.field_1.field_3.field_2.field_3.field_1==135) && (a3.field_2.field_1.field_4.field_1.field_3.field_2.field_3.field_2==162) && (a3.field_2.field_1.field_4.field_1.field_3.field_2.field_3.field_3==0.25) && (a3.field_2.field_1.field_4.field_1.field_3.field_2.field_3.field_4==62767721873408)) && (a3.field_2.field_1.field_4.field_1.field_3.field_2.field_4==3.2578125) && ((a3.field_2.field_1.field_4.field_1.field_3.field_2.field_5.field_1==49) && (a3.field_2.field_1.field_4.field_1.field_3.field_2.field_5.field_2==25) && (a3.field_2.field_1.field_4.field_1.field_3.field_2.field_5.field_3==1.34375) && (a3.field_2.field_1.field_4.field_1.field_3.field_2.field_5.field_4==103776287981568))) && ((a3.field_2.field_1.field_4.field_1.field_3.field_3.field_1==246) && (a3.field_2.field_1.field_4.field_1.field_3.field_3.field_2==223) && (a3.field_2.field_1.field_4.field_1.field_3.field_3.field_3==-0.0625) && (a3.field_2.field_1.field_4.field_1.field_3.field_3.field_4==147071279038464)) && (a3.field_2.field_1.field_4.field_1.field_3.field_4==2.90625) && (a3.field_2.field_1.field_4.field_1.field_3.field_5==2875920810))) && ((a3.field_2.field_1.field_4.field_2.field_1==212) && (a3.field_2.field_1.field_4.field_2.field_2==231) && (a3.field_2.field_1.field_4.field_2.field_3==-0.9375) && (a3.field_2.field_1.field_4.field_2.field_4==278720009601025)) && ((a3.field_2.field_1.field_4.field_3.field_1==4008885223424) && (a3.field_2.field_1.field_4.field_3.field_2==-0.78125)))) && (a3.field_2.field_2==3.78125) && (a3.field_2.field_3==111604690845696) && (a3.field_2.field_4==213)) && (a3.field_3==0.703125) && (a3.field_4==E_15_1) && (a3.field_5==2.0625) && (a3.field_6==1.73828125))) {
        goto bad;
    }

    if (!((a4.field_1==208) && (a4.field_2==1.30078125) && (a4.field_3==E_14_3) && (a4.field_4==E_22_2))) {
        goto bad;
    }

    if (!(a5==E_24_4)) {
        goto bad;
    }

    if (!(a6==157825342)) {
        goto bad;
    }

    if (!(a7==3.765625)) {
        goto bad;
    }

    if (!((((a8.field_1.field_1.field_1==E_8_6) && (a8.field_1.field_1.field_2==3815456600)) && (a8.field_1.field_2==-1.58203125) && (a8.field_1.field_3==16877321453568) && (a8.field_1.field_4==145567904956416) && (a8.field_1.field_5==-0.125) && (a8.field_1.field_6==1.76171875)) && (a8.field_2==171) && (a8.field_3==140444787539968) && (a8.field_4==15))) {
        goto bad;
    }

    if (!(a9==E_27_5)) {
        goto bad;
    }

    if (!(a10==E_28_3)) {
        goto bad;
    }

    return E_29_1;

bad:
    return E_29_3;
}

uint32_t
fn_6_c(t20 a1, t15 a2)
{
    if (!(((a1.field_1.field_1==141) && (a1.field_1.field_2==149) && (a1.field_1.field_3==-1.25) && (a1.field_1.field_4==58279295057920)) && ((a1.field_2.field_1==63572928888832) && (a1.field_2.field_2==46145049067520) && ((a1.field_2.field_3.field_1==109) && (a1.field_2.field_3.field_2==59) && (a1.field_2.field_3.field_3==-1.4375) && (a1.field_2.field_3.field_4==89364507131904)) && (a1.field_2.field_4==0.23828125) && ((a1.field_2.field_5.field_1==25) && (a1.field_2.field_5.field_2==213) && (a1.field_2.field_5.field_3==1.546875) && (a1.field_2.field_5.field_4==232592606363648))))) {
        goto bad;
    }

    if (!(a2==E_15_6)) {
        goto bad;
    }

    return 4008488864;

bad:
    return 2072402048;
}

t37
fn_7_c(double a1, double a2, t31 a3, t21 a4, double a5, float a6, t35 a7, uint64_t a8)
{
    if (!(a1==2.84375)) {
        goto bad;
    }

    if (!(a2==-3.25390625)) {
        goto bad;
    }

    if (!((a3.field_1==-2.56640625) && (a3.field_2==-0.59375) && (a3.field_3==-0.34375) && ((a3.field_4.field_1==83) && (a3.field_4.field_2==-0.8125) && (a3.field_4.field_3==245451030659072) && (a3.field_4.field_4==E_24_1) && (a3.field_4.field_5==0.58203125) && (a3.field_4.field_6==0.734375)))) {
        goto bad;
    }

    if (!((((a4.field_1.field_1.field_1==154) && (a4.field_1.field_1.field_2==16) && (a4.field_1.field_1.field_3==1.53125) && (a4.field_1.field_1.field_4==240450872147968)) && ((a4.field_1.field_2.field_1==93431282270208) && (a4.field_1.field_2.field_2==243719039352832) && ((a4.field_1.field_2.field_3.field_1==176) && (a4.field_1.field_2.field_3.field_2==179) && (a4.field_1.field_2.field_3.field_3==1.421875) && (a4.field_1.field_2.field_3.field_4==211285330100224)) && (a4.field_1.field_2.field_4==-2.0546875) && ((a4.field_1.field_2.field_5.field_1==120) && (a4.field_1.field_2.field_5.field_2==11) && (a4.field_1.field_2.field_5.field_3==0.984375) && (a4.field_1.field_2.field_5.field_4==85609800400896)))) && (((a4.field_2.field_1.field_1==3734886520) && ((a4.field_2.field_1.field_2.field_1==-0.03125) && (a4.field_2.field_1.field_2.field_2==4178676130)) && ((a4.field_2.field_1.field_3.field_1==1.4609375) && ((a4.field_2.field_1.field_3.field_2.field_1==278857533489153) && (a4.field_2.field_1.field_3.field_2.field_2==40762444611584) && ((a4.field_2.field_1.field_3.field_2.field_3.field_1==255) && (a4.field_2.field_1.field_3.field_2.field_3.field_2==97) && (a4.field_2.field_1.field_3.field_2.field_3.field_3==-0.09375) && (a4.field_2.field_1.field_3.field_2.field_3.field_4==67201348272128)) && (a4.field_2.field_1.field_3.field_2.field_4==-1.9375) && ((a4.field_2.field_1.field_3.field_2.field_5.field_1==145) && (a4.field_2.field_1.field_3.field_2.field_5.field_2==208) && (a4.field_2.field_1.field_3.field_2.field_5.field_3==1.53125) && (a4.field_2.field_1.field_3.field_2.field_5.field_4==6521111838720)))) && (((a4.field_2.field_1.field_4.field_1.field_1==-0.2265625) && (a4.field_2.field_1.field_4.field_1.field_2==0.28125) && ((a4.field_2.field_1.field_4.field_1.field_3.field_1==24215899865088) && ((a4.field_2.field_1.field_4.field_1.field_3.field_2.field_1==128440090099712) && (a4.field_2.field_1.field_4.field_1.field_3.field_2.field_2==124968190476288) && ((a4.field_2.field_1.field_4.field_1.field_3.field_2.field_3.field_1==106) && (a4.field_2.field_1.field_4.field_1.field_3.field_2.field_3.field_2==82) && (a4.field_2.field_1.field_4.field_1.field_3.field_2.field_3.field_3==-1.15625) && (a4.field_2.field_1.field_4.field_1.field_3.field_2.field_3.field_4==33900307021824)) && (a4.field_2.field_1.field_4.field_1.field_3.field_2.field_4==-1.75390625) && ((a4.field_2.field_1.field_4.field_1.field_3.field_2.field_5.field_1==226) && (a4.field_2.field_1.field_4.field_1.field_3.field_2.field_5.field_2==91) && (a4.field_2.field_1.field_4.field_1.field_3.field_2.field_5.field_3==0.765625) && (a4.field_2.field_1.field_4.field_1.field_3.field_2.field_5.field_4==261485102891008))) && ((a4.field_2.field_1.field_4.field_1.field_3.field_3.field_1==43) && (a4.field_2.field_1.field_4.field_1.field_3.field_3.field_2==12) && (a4.field_2.field_1.field_4.field_1.field_3.field_3.field_3==0.9375) && (a4.field_2.field_1.field_4.field_1.field_3.field_3.field_4==185189970870272)) && (a4.field_2.field_1.field_4.field_1.field_3.field_4==-3.73046875) && (a4.field_2.field_1.field_4.field_1.field_3.field_5==2085882880))) && ((a4.field_2.field_1.field_4.field_2.field_1==166) && (a4.field_2.field_1.field_4.field_2.field_2==42) && (a4.field_2.field_1.field_4.field_2.field_3==-0.0625) && (a4.field_2.field_1.field_4.field_2.field_4==8114197757952)) && ((a4.field_2.field_1.field_4.field_3.field_1==179406660042752) && (a4.field_2.field_1.field_4.field_3.field_2==0.6875)))) && (a4.field_2.field_2==-1.7109375) && (a4.field_2.field_3==58423380148224) && (a4.field_2.field_4==136)) && (a4.field_3==-0.71875) && (a4.field_4==E_15_2) && (a4.field_5==-3.87109375) && (a4.field_6==2.8046875))) {
        goto bad;
    }

    if (!(a5==-0.3046875)) {
        goto bad;
    }

    if (!(a6==0.734375)) {
        goto bad;
    }

    if (!(((a7.field_1.field_1==66690558)) && (((a7.field_2.field_1.field_1==201975036641280) && (a7.field_2.field_1.field_2==191791705096192) && ((a7.field_2.field_1.field_3.field_1==188) && (a7.field_2.field_1.field_3.field_2==255) && (a7.field_2.field_1.field_3.field_3==1.21875) && (a7.field_2.field_1.field_3.field_4==75962620575744)) && (a7.field_2.field_1.field_4==-1.81640625) && ((a7.field_2.field_1.field_5.field_1==30) && (a7.field_2.field_1.field_5.field_2==70) && (a7.field_2.field_1.field_5.field_3==-1.171875) && (a7.field_2.field_1.field_5.field_4==122844992372736))) && (a7.field_2.field_2==E_18_6)) && (a7.field_3==E_34_3))) {
        goto bad;
    }

    if (!(a8==145924444127232)) {
        goto bad;
    }

    return (t37) {.field_1=E_36_2};

bad:
    return (t37) {.field_1=E_36_3};
}

t42
fn_8_c(t38 a1, t39 a2, double a3, uint8_t a4)
{
    if (!(a1==E_38_4)) {
        goto bad;
    }

    if (!(a2==E_39_1)) {
        goto bad;
    }

    if (!(a3==-2.69140625)) {
        goto bad;
    }

    if (!(a4==17)) {
        goto bad;
    }

    return (t42) {.field_1=86,.field_2=(t40) {.field_1=(t13) {.field_1=3264522662,.field_2=(t10) {.field_1=1.125,.field_2=3024392},.field_3=(t11) {.field_1=2.90625,.field_2=(t2) {.field_1=246587710898176,.field_2=98684395847680,.field_3=(t1) {.field_1=7,.field_2=232,.field_3=-0.25,.field_4=243343254224896},.field_4=1.3828125,.field_5=(t1) {.field_1=123,.field_2=151,.field_3=0.671875,.field_4=230200040620032}}},.field_4=(t12) {.field_1=(t4) {.field_1=-0.5,.field_2=-1.484375,.field_3=(t3) {.field_1=78257571102720,.field_2=(t2) {.field_1=202195772637184,.field_2=21507053977600,.field_3=(t1) {.field_1=42,.field_2=43,.field_3=0.453125,.field_4=259622561644544},.field_4=1.18359375,.field_5=(t1) {.field_1=201,.field_2=220,.field_3=-0.40625,.field_4=87732745404416}},.field_3=(t1) {.field_1=61,.field_2=181,.field_3=-1.34375,.field_4=27713052475392},.field_4=1.640625,.field_5=4052472574}},.field_2=(t1) {.field_1=249,.field_2=15,.field_3=1.484375,.field_4=248514888728576},.field_3=(t5) {.field_1=134716506374144,.field_2=1.046875}}},.field_2=2404422220},.field_3=246,.field_4=(t41) {.field_1=(t20) {.field_1=(t1) {.field_1=109,.field_2=70,.field_3=0.875,.field_4=242714521763840},.field_2=(t2) {.field_1=85381772738560,.field_2=16488017559552,.field_3=(t1) {.field_1=148,.field_2=97,.field_3=-0.859375,.field_4=211742737956864},.field_4=-3.73046875,.field_5=(t1) {.field_1=37,.field_2=103,.field_3=0.96875,.field_4=3251610451968}}},.field_2=E_29_7,.field_3=34127014920192}};

bad:
    return (t42) {.field_1=64,.field_2=(t40) {.field_1=(t13) {.field_1=2072157872,.field_2=(t10) {.field_1=-0.96875,.field_2=1499873962},.field_3=(t11) {.field_1=-2.40234375,.field_2=(t2) {.field_1=38436406689792,.field_2=91121526767616,.field_3=(t1) {.field_1=64,.field_2=27,.field_3=-0.921875,.field_4=205924771299328},.field_4=3.46484375,.field_5=(t1) {.field_1=196,.field_2=177,.field_3=-0.40625,.field_4=11710038278144}}},.field_4=(t12) {.field_1=(t4) {.field_1=-0.1953125,.field_2=-0.84375,.field_3=(t3) {.field_1=97091811147776,.field_2=(t2) {.field_1=150148618125312,.field_2=228580951588864,.field_3=(t1) {.field_1=185,.field_2=194,.field_3=0.203125,.field_4=210398628937728},.field_4=3.1875,.field_5=(t1) {.field_1=247,.field_2=142,.field_3=1.3125,.field_4=208583715848192}},.field_3=(t1) {.field_1=173,.field_2=43,.field_3=-0.875,.field_4=245248454164480},.field_4=0.15234375,.field_5=1786378194}},.field_2=(t1) {.field_1=2,.field_2=215,.field_3=0.53125,.field_4=32483101376512},.field_3=(t5) {.field_1=14001190862848,.field_2=-0.3125}}},.field_2=251181130},.field_3=208,.field_4=(t41) {.field_1=(t20) {.field_1=(t1) {.field_1=24,.field_2=109,.field_3=1.125,.field_4=160345918668800},.field_2=(t2) {.field_1=185513129279488,.field_2=57065717104640,.field_3=(t1) {.field_1=26,.field_2=120,.field_3=1.34375,.field_4=242789147082752},.field_4=-3.62890625,.field_5=(t1) {.field_1=172,.field_2=197,.field_3=-1.5625,.field_4=65287928938496}}},.field_2=E_29_7,.field_3=209522592579584}};
}

uint64_t
fn_9_c(t46 a1, float a2, t26 a3, t47 a4, t48 a5, t54 a6, uint32_t a7, float a8, t55 a9, t56 a10)
{
    if (!((((a1.field_1.field_1.field_1==0.171875) && (a1.field_1.field_1.field_2==3226754646)) && (((a1.field_1.field_2.field_1.field_1==2.765625) && (a1.field_1.field_2.field_1.field_2==0.1875) && ((a1.field_1.field_2.field_1.field_3.field_1==276756521549824) && ((a1.field_1.field_2.field_1.field_3.field_2.field_1==152999296499712) && (a1.field_1.field_2.field_1.field_3.field_2.field_2==85664820494336) && ((a1.field_1.field_2.field_1.field_3.field_2.field_3.field_1==228) && (a1.field_1.field_2.field_1.field_3.field_2.field_3.field_2==103) && (a1.field_1.field_2.field_1.field_3.field_2.field_3.field_3==-0.71875) && (a1.field_1.field_2.field_1.field_3.field_2.field_3.field_4==189995730337792)) && (a1.field_1.field_2.field_1.field_3.field_2.field_4==2.2109375) && ((a1.field_1.field_2.field_1.field_3.field_2.field_5.field_1==167) && (a1.field_1.field_2.field_1.field_3.field_2.field_5.field_2==118) && (a1.field_1.field_2.field_1.field_3.field_2.field_5.field_3==-1.546875) && (a1.field_1.field_2.field_1.field_3.field_2.field_5.field_4==199486233247744))) && ((a1.field_1.field_2.field_1.field_3.field_3.field_1==94) && (a1.field_1.field_2.field_1.field_3.field_3.field_2==71) && (a1.field_1.field_2.field_1.field_3.field_3.field_3==-1.0625) && (a1.field_1.field_2.field_1.field_3.field_3.field_4==93572022140928)) && (a1.field_1.field_2.field_1.field_3.field_4==-1.046875) && (a1.field_1.field_2.field_1.field_3.field_5==3049108056))) && ((a1.field_1.field_2.field_2.field_1==185) && (a1.field_1.field_2.field_2.field_2==79) && (a1.field_1.field_2.field_2.field_3==1.03125) && (a1.field_1.field_2.field_2.field_4==217666636349440)) && ((a1.field_1.field_2.field_3.field_1==129819186561024) && (a1.field_1.field_2.field_3.field_2==-1.421875))) && ((a1.field_1.field_3.field_1==-1.60546875) && (a1.field_1.field_3.field_2==0.125) && ((a1.field_1.field_3.field_3.field_1==37068096536576) && ((a1.field_1.field_3.field_3.field_2.field_1==239555475144704) && (a1.field_1.field_3.field_3.field_2.field_2==81311387615232) && ((a1.field_1.field_3.field_3.field_2.field_3.field_1==252) && (a1.field_1.field_3.field_3.field_2.field_3.field_2==105) && (a1.field_1.field_3.field_3.field_2.field_3.field_3==-0.71875) && (a1.field_1.field_3.field_3.field_2.field_3.field_4==148899603939328)) && (a1.field_1.field_3.field_3.field_2.field_4==1.68359375) && ((a1.field_1.field_3.field_3.field_2.field_5.field_1==42) && (a1.field_1.field_3.field_3.field_2.field_5.field_2==238) && (a1.field_1.field_3.field_3.field_2.field_5.field_3==1.53125) && (a1.field_1.field_3.field_3.field_2.field_5.field_4==236603073363968))) && ((a1.field_1.field_3.field_3.field_3.field_1==183) && (a1.field_1.field_3.field_3.field_3.field_2==164) && (a1.field_1.field_3.field_3.field_3.field_3==-0.609375) && (a1.field_1.field_3.field_3.field_3.field_4==202904325324800)) && (a1.field_1.field_3.field_3.field_4==-1.15234375) && (a1.field_1.field_3.field_3.field_5==2895375692)))) && (a1.field_2==E_44_7) && ((a1.field_3.field_1==2208687568)) && (a1.field_4==1848386781184) && ((a1.field_5.field_1==-1.05859375) && (a1.field_5.field_2==-0.859375) && ((a1.field_5.field_3.field_1==205448639807488) && ((a1.field_5.field_3.field_2.field_1==190494723801088) && (a1.field_5.field_3.field_2.field_2==14572431867904) && ((a1.field_5.field_3.field_2.field_3.field_1==128) && (a1.field_5.field_3.field_2.field_3.field_2==35) && (a1.field_5.field_3.field_2.field_3.field_3==-1.25) && (a1.field_5.field_3.field_2.field_3.field_4==224517446828032)) && (a1.field_5.field_3.field_2.field_4==1.37109375) && ((a1.field_5.field_3.field_2.field_5.field_1==59) && (a1.field_5.field_3.field_2.field_5.field_2==166) && (a1.field_5.field_3.field_2.field_5.field_3==1.453125) && (a1.field_5.field_3.field_2.field_5.field_4==61154056208384))) && ((a1.field_5.field_3.field_3.field_1==15) && (a1.field_5.field_3.field_3.field_2==60) && (a1.field_5.field_3.field_3.field_3==0.765625) && (a1.field_5.field_3.field_3.field_4==218183195164672)) && (a1.field_5.field_3.field_4==-0.765625) && (a1.field_5.field_3.field_5==2915453416))) && (a1.field_6==3275953006))) {
        goto bad;
    }

    if (!(a2==-0.328125)) {
        goto bad;
    }

    if (!((((a3.field_1.field_1.field_1==E_8_1) && (a3.field_1.field_1.field_2==3258536312)) && (a3.field_1.field_2==-2.17578125) && (a3.field_1.field_3==31978917462016) && (a3.field_1.field_4==76825007489024) && (a3.field_1.field_5==-0.859375) && (a3.field_1.field_6==-0.171875)) && (a3.field_2==127) && (a3.field_3==269734934740992) && (a3.field_4==39))) {
        goto bad;
    }

    if (!(a4==E_47_3)) {
        goto bad;
    }

    if (!(a5==E_48_7)) {
        goto bad;
    }

    if (!(((a6.field_1.field_1==613960680)) && ((a6.field_2.field_1==-0.125) && ((a6.field_2.field_2.field_1==469993662)) && (a6.field_2.field_3==201131156307968) && (a6.field_2.field_4==3.79296875)) && (a6.field_3==E_51_4) && ((a6.field_4.field_1==0.09375) && (a6.field_4.field_2==E_51_6) && (a6.field_4.field_3==134) && (a6.field_4.field_4==1.234375) && (a6.field_4.field_5==1.359375)) && (a6.field_5==E_51_2) && (a6.field_6==E_53_4))) {
        goto bad;
    }

    if (!(a7==604879132)) {
        goto bad;
    }

    if (!(a8==0.640625)) {
        goto bad;
    }

    if (!(a9==E_55_3)) {
        goto bad;
    }

    if (!(a10==E_56_4)) {
        goto bad;
    }

    return 219771358019584;

bad:
    return 168173477429248;
}

t3
fn_10_c(uint64_t a1, double a2, t61 a3, uint8_t a4, t62 a5, uint8_t a6, double a7, uint64_t a8)
{
    if (!(a1==160708767514624)) {
        goto bad;
    }

    if (!(a2==0.36328125)) {
        goto bad;
    }

    if (!((a3.field_1==E_57_5) && ((a3.field_2.field_1==-0.27734375) && (a3.field_2.field_2==E_36_3)) && ((a3.field_3.field_1==97) && (a3.field_3.field_2==33865353396224)) && ((a3.field_4.field_1==216701139681280) && (a3.field_4.field_2==-1.03125) && ((a3.field_4.field_3.field_1==-0.8515625) && (a3.field_4.field_3.field_2==0.71875) && ((a3.field_4.field_3.field_3.field_1==244705840726016) && ((a3.field_4.field_3.field_3.field_2.field_1==53149469900800) && (a3.field_4.field_3.field_3.field_2.field_2==161998735409152) && ((a3.field_4.field_3.field_3.field_2.field_3.field_1==127) && (a3.field_4.field_3.field_3.field_2.field_3.field_2==47) && (a3.field_4.field_3.field_3.field_2.field_3.field_3==1.25) && (a3.field_4.field_3.field_3.field_2.field_3.field_4==59620799152128)) && (a3.field_4.field_3.field_3.field_2.field_4==-1.41015625) && ((a3.field_4.field_3.field_3.field_2.field_5.field_1==130) && (a3.field_4.field_3.field_3.field_2.field_5.field_2==161) && (a3.field_4.field_3.field_3.field_2.field_5.field_3==-1.453125) && (a3.field_4.field_3.field_3.field_2.field_5.field_4==49656372789248))) && ((a3.field_4.field_3.field_3.field_3.field_1==136) && (a3.field_4.field_3.field_3.field_3.field_2==83) && (a3.field_4.field_3.field_3.field_3.field_3==-1.15625) && (a3.field_4.field_3.field_3.field_3.field_4==278799917907969)) && (a3.field_4.field_3.field_3.field_4==2.5234375) && (a3.field_4.field_3.field_3.field_5==3744252854))) && (a3.field_4.field_4==E_38_1)))) {
        goto bad;
    }

    if (!(a4==202)) {
        goto bad;
    }

    if (!(a5==E_62_3)) {
        goto bad;
    }

    if (!(a6==0)) {
        goto bad;
    }

    if (!(a7==2.234375)) {
        goto bad;
    }

    if (!(a8==215484511158272)) {
        goto bad;
    }

    return (t3) {.field_1=268633981059072,.field_2=(t2) {.field_1=24949413642240,.field_2=153583813787648,.field_3=(t1) {.field_1=35,.field_2=252,.field_3=0.8125,.field_4=128878867382272},.field_4=-0.03125,.field_5=(t1) {.field_1=98,.field_2=126,.field_3=0.546875,.field_4=258416610377728}},.field_3=(t1) {.field_1=209,.field_2=206,.field_3=1.28125,.field_4=180894750212096},.field_4=1.390625,.field_5=4104417000};

bad:
    return (t3) {.field_1=194724296130560,.field_2=(t2) {.field_1=131606075277312,.field_2=177386963992576,.field_3=(t1) {.field_1=80,.field_2=184,.field_3=0.453125,.field_4=88961125056512},.field_4=-3.15625,.field_5=(t1) {.field_1=110,.field_2=126,.field_3=1.140625,.field_4=266037895757824}},.field_3=(t1) {.field_1=137,.field_2=208,.field_3=-1.46875,.field_4=23546486325248},.field_4=3.55078125,.field_5=90406856};
}

int
const check_c_to_myr_fns(void)
{
    double a_1_1 = -0.26171875;
    t2 ret_1 = fn_1_myr(a_1_1);
    if (!((ret_1.field_1==12459410325504) && (ret_1.field_2==98659448520704) && ((ret_1.field_3.field_1==166) && (ret_1.field_3.field_2==107) && (ret_1.field_3.field_3==0.140625) && (ret_1.field_3.field_4==92389009653760)) && (ret_1.field_4==-3.140625) && ((ret_1.field_5.field_1==157) && (ret_1.field_5.field_2==191) && (ret_1.field_5.field_3==1.125) && (ret_1.field_5.field_4==644801363968)))) {
        return -1;
    }

    double a_2_1 = -3.80078125;
    t4 a_2_2 = (t4) {.field_1=2.8203125,.field_2=1.40625,.field_3=(t3) {.field_1=281155744301057,.field_2=(t2) {.field_1=201484396396544,.field_2=167610483867648,.field_3=(t1) {.field_1=241,.field_2=67,.field_3=-1.546875,.field_4=85217436499968},.field_4=1.33203125,.field_5=(t1) {.field_1=115,.field_2=174,.field_3=0.578125,.field_4=108879745908736}},.field_3=(t1) {.field_1=46,.field_2=0,.field_3=0.484375,.field_4=41200816095232},.field_4=1.640625,.field_5=4163457034}};
    t1 a_2_3 = (t1) {.field_1=179,.field_2=82,.field_3=0.796875,.field_4=149846806626304};
    t5 a_2_4 = (t5) {.field_1=132525092503552,.field_2=-1.3125};
    uint64_t ret_2 = fn_2_myr(a_2_1, a_2_2, a_2_3, a_2_4);
    if (!(ret_2==132205860093952)) {
        return -1;
    }

    t1 a_3_1 = (t1) {.field_1=211,.field_2=183,.field_3=0.71875,.field_4=77205130182656};
    t3 a_3_2 = (t3) {.field_1=186084876091392,.field_2=(t2) {.field_1=162500698177536,.field_2=117147867938816,.field_3=(t1) {.field_1=81,.field_2=185,.field_3=-1.171875,.field_4=80629570863104},.field_4=-2.453125,.field_5=(t1) {.field_1=49,.field_2=253,.field_3=0.03125,.field_4=266183655817216}},.field_3=(t1) {.field_1=133,.field_2=78,.field_3=-1.15625,.field_4=208468516143104},.field_4=0.625,.field_5=2593151244};
    t7 a_3_3 = (t7) {.field_1=104,.field_2=E_6_7,.field_3=(t5) {.field_1=168319363710976,.field_2=1.015625},.field_4=(t2) {.field_1=105329353555968,.field_2=49345085177856,.field_3=(t1) {.field_1=61,.field_2=176,.field_3=1.25,.field_4=102963779993600},.field_4=3.734375,.field_5=(t1) {.field_1=21,.field_2=143,.field_3=1.46875,.field_4=167234555215872}}};
    t4 a_3_4 = (t4) {.field_1=3.12890625,.field_2=0.4375,.field_3=(t3) {.field_1=63982214447104,.field_2=(t2) {.field_1=135072305512448,.field_2=67998052515840,.field_3=(t1) {.field_1=12,.field_2=71,.field_3=-0.578125,.field_4=187813731500032},.field_4=3.07421875,.field_5=(t1) {.field_1=205,.field_2=88,.field_3=-0.53125,.field_4=674692464640}},.field_3=(t1) {.field_1=193,.field_2=11,.field_3=0.3125,.field_4=162599899496448},.field_4=-0.62890625,.field_5=3331590902}};
    t9 a_3_5 = (t9) {.field_1=E_8_5,.field_2=3187538484};
    uint8_t a_3_6 = 90;
    float a_3_7 = 0.125;
    float a_3_8 = 1.203125;
    t13 ret_3 = fn_3_myr(a_3_1, a_3_2, a_3_3, a_3_4, a_3_5, a_3_6, a_3_7, a_3_8);
    if (!((ret_3.field_1==650661816) && ((ret_3.field_2.field_1==-0.015625) && (ret_3.field_2.field_2==2851726420)) && ((ret_3.field_3.field_1==-3.625) && ((ret_3.field_3.field_2.field_1==74772813709312) && (ret_3.field_3.field_2.field_2==112035052126208) && ((ret_3.field_3.field_2.field_3.field_1==41) && (ret_3.field_3.field_2.field_3.field_2==21) && (ret_3.field_3.field_2.field_3.field_3==-0.28125) && (ret_3.field_3.field_2.field_3.field_4==170067893944320)) && (ret_3.field_3.field_2.field_4==0.52734375) && ((ret_3.field_3.field_2.field_5.field_1==81) && (ret_3.field_3.field_2.field_5.field_2==216) && (ret_3.field_3.field_2.field_5.field_3==-0.328125) && (ret_3.field_3.field_2.field_5.field_4==156496708894720)))) && (((ret_3.field_4.field_1.field_1==-3.19140625) && (ret_3.field_4.field_1.field_2==1.515625) && ((ret_3.field_4.field_1.field_3.field_1==236308757348352) && ((ret_3.field_4.field_1.field_3.field_2.field_1==263713238548480) && (ret_3.field_4.field_1.field_3.field_2.field_2==40608255049728) && ((ret_3.field_4.field_1.field_3.field_2.field_3.field_1==138) && (ret_3.field_4.field_1.field_3.field_2.field_3.field_2==142) && (ret_3.field_4.field_1.field_3.field_2.field_3.field_3==-1.1875) && (ret_3.field_4.field_1.field_3.field_2.field_3.field_4==100184688558080)) && (ret_3.field_4.field_1.field_3.field_2.field_4==2.76171875) && ((ret_3.field_4.field_1.field_3.field_2.field_5.field_1==73) && (ret_3.field_4.field_1.field_3.field_2.field_5.field_2==65) && (ret_3.field_4.field_1.field_3.field_2.field_5.field_3==1.28125) && (ret_3.field_4.field_1.field_3.field_2.field_5.field_4==53591441145856))) && ((ret_3.field_4.field_1.field_3.field_3.field_1==54) && (ret_3.field_4.field_1.field_3.field_3.field_2==81) && (ret_3.field_4.field_1.field_3.field_3.field_3==-0.5) && (ret_3.field_4.field_1.field_3.field_3.field_4==200208867524608)) && (ret_3.field_4.field_1.field_3.field_4==3.7734375) && (ret_3.field_4.field_1.field_3.field_5==1624457196))) && ((ret_3.field_4.field_2.field_1==250) && (ret_3.field_4.field_2.field_2==97) && (ret_3.field_4.field_2.field_3==0.125) && (ret_3.field_4.field_2.field_4==16842727882752)) && ((ret_3.field_4.field_3.field_1==222985090629632) && (ret_3.field_4.field_3.field_2==-1.109375))))) {
        return -1;
    }

    t16 a_4_1 = (t16) {.field_1=E_14_1,.field_2=-0.734375,.field_3=31855997878272,.field_4=E_15_5};
    t19 ret_4 = fn_4_myr(a_4_1);
    if (!((ret_4.field_1==0.03125) && (((ret_4.field_2.field_1.field_1==947051528) && ((ret_4.field_2.field_1.field_2.field_1==0.96875) && (ret_4.field_2.field_1.field_2.field_2==241559198)) && ((ret_4.field_2.field_1.field_3.field_1==-0.125) && ((ret_4.field_2.field_1.field_3.field_2.field_1==22236939157504) && (ret_4.field_2.field_1.field_3.field_2.field_2==74920569470976) && ((ret_4.field_2.field_1.field_3.field_2.field_3.field_1==22) && (ret_4.field_2.field_1.field_3.field_2.field_3.field_2==25) && (ret_4.field_2.field_1.field_3.field_2.field_3.field_3==-0.46875) && (ret_4.field_2.field_1.field_3.field_2.field_3.field_4==99222545367040)) && (ret_4.field_2.field_1.field_3.field_2.field_4==-2.25) && ((ret_4.field_2.field_1.field_3.field_2.field_5.field_1==6) && (ret_4.field_2.field_1.field_3.field_2.field_5.field_2==66) && (ret_4.field_2.field_1.field_3.field_2.field_5.field_3==0.9375) && (ret_4.field_2.field_1.field_3.field_2.field_5.field_4==72999681130496)))) && (((ret_4.field_2.field_1.field_4.field_1.field_1==-2.41015625) && (ret_4.field_2.field_1.field_4.field_1.field_2==-0.328125) && ((ret_4.field_2.field_1.field_4.field_1.field_3.field_1==33373226926080) && ((ret_4.field_2.field_1.field_4.field_1.field_3.field_2.field_1==264437028749312) && (ret_4.field_2.field_1.field_4.field_1.field_3.field_2.field_2==41455185559552) && ((ret_4.field_2.field_1.field_4.field_1.field_3.field_2.field_3.field_1==58) && (ret_4.field_2.field_1.field_4.field_1.field_3.field_2.field_3.field_2==157) && (ret_4.field_2.field_1.field_4.field_1.field_3.field_2.field_3.field_3==-1.40625) && (ret_4.field_2.field_1.field_4.field_1.field_3.field_2.field_3.field_4==172834173747200)) && (ret_4.field_2.field_1.field_4.field_1.field_3.field_2.field_4==2.96484375) && ((ret_4.field_2.field_1.field_4.field_1.field_3.field_2.field_5.field_1==47) && (ret_4.field_2.field_1.field_4.field_1.field_3.field_2.field_5.field_2==72) && (ret_4.field_2.field_1.field_4.field_1.field_3.field_2.field_5.field_3==-0.8125) && (ret_4.field_2.field_1.field_4.field_1.field_3.field_2.field_5.field_4==161994014326784))) && ((ret_4.field_2.field_1.field_4.field_1.field_3.field_3.field_1==98) && (ret_4.field_2.field_1.field_4.field_1.field_3.field_3.field_2==191) && (ret_4.field_2.field_1.field_4.field_1.field_3.field_3.field_3==0.9375) && (ret_4.field_2.field_1.field_4.field_1.field_3.field_3.field_4==54448237051904)) && (ret_4.field_2.field_1.field_4.field_1.field_3.field_4==2.390625) && (ret_4.field_2.field_1.field_4.field_1.field_3.field_5==1201643174))) && ((ret_4.field_2.field_1.field_4.field_2.field_1==69) && (ret_4.field_2.field_1.field_4.field_2.field_2==18) && (ret_4.field_2.field_1.field_4.field_2.field_3==-0.421875) && (ret_4.field_2.field_1.field_4.field_2.field_4==104603486781440)) && ((ret_4.field_2.field_1.field_4.field_3.field_1==119279618490368) && (ret_4.field_2.field_1.field_4.field_3.field_2==0.6875)))) && (ret_4.field_2.field_2==0.65625) && (ret_4.field_2.field_3==125926830637056) && (ret_4.field_2.field_4==250)) && (ret_4.field_3==E_18_3) && ((ret_4.field_4.field_1==198926511898624) && (ret_4.field_4.field_2==48406520135680) && ((ret_4.field_4.field_3.field_1==198) && (ret_4.field_4.field_3.field_2==211) && (ret_4.field_4.field_3.field_3==-1.21875) && (ret_4.field_4.field_3.field_4==259336007450624)) && (ret_4.field_4.field_4==-3.48828125) && ((ret_4.field_4.field_5.field_1==185) && (ret_4.field_4.field_5.field_2==248) && (ret_4.field_4.field_5.field_3==0.53125) && (ret_4.field_4.field_5.field_4==170021025611776))))) {
        return -1;
    }

    uint32_t a_5_1 = 3491579916;
    t11 a_5_2 = (t11) {.field_1=2.140625,.field_2=(t2) {.field_1=17107741573120,.field_2=257014155051008,.field_3=(t1) {.field_1=120,.field_2=241,.field_3=1.078125,.field_4=195206002114560},.field_4=-1.15625,.field_5=(t1) {.field_1=46,.field_2=186,.field_3=0.1875,.field_4=278368438321153}}};
    t21 a_5_3 = (t21) {.field_1=(t20) {.field_1=(t1) {.field_1=66,.field_2=239,.field_3=-1.203125,.field_4=89848418926592},.field_2=(t2) {.field_1=239095841685504,.field_2=165469104898048,.field_3=(t1) {.field_1=67,.field_2=177,.field_3=-0.6875,.field_4=172891013120000},.field_4=2.93359375,.field_5=(t1) {.field_1=2,.field_2=44,.field_3=1.140625,.field_4=266978004566016}}},.field_2=(t17) {.field_1=(t13) {.field_1=488880992,.field_2=(t10) {.field_1=0.1875,.field_2=4283670802},.field_3=(t11) {.field_1=-2.4453125,.field_2=(t2) {.field_1=86642904793088,.field_2=9550056259584,.field_3=(t1) {.field_1=135,.field_2=40,.field_3=-1.234375,.field_4=14565778391040},.field_4=3.859375,.field_5=(t1) {.field_1=97,.field_2=149,.field_3=0.75,.field_4=208889540378624}}},.field_4=(t12) {.field_1=(t4) {.field_1=3.3515625,.field_2=1.5625,.field_3=(t3) {.field_1=190971227013120,.field_2=(t2) {.field_1=13441050607616,.field_2=89440160972800,.field_3=(t1) {.field_1=135,.field_2=162,.field_3=0.25,.field_4=62767721873408},.field_4=3.2578125,.field_5=(t1) {.field_1=49,.field_2=25,.field_3=1.34375,.field_4=103776287981568}},.field_3=(t1) {.field_1=246,.field_2=223,.field_3=-0.0625,.field_4=147071279038464},.field_4=2.90625,.field_5=2875920810}},.field_2=(t1) {.field_1=212,.field_2=231,.field_3=-0.9375,.field_4=278720009601025},.field_3=(t5) {.field_1=4008885223424,.field_2=-0.78125}}},.field_2=3.78125,.field_3=111604690845696,.field_4=213},.field_3=0.703125,.field_4=E_15_1,.field_5=2.0625,.field_6=1.73828125};
    t23 a_5_4 = (t23) {.field_1=208,.field_2=1.30078125,.field_3=E_14_3,.field_4=E_22_2};
    t24 a_5_5 = E_24_4;
    uint32_t a_5_6 = 157825342;
    double a_5_7 = 3.765625;
    t26 a_5_8 = (t26) {.field_1=(t25) {.field_1=(t9) {.field_1=E_8_6,.field_2=3815456600},.field_2=-1.58203125,.field_3=16877321453568,.field_4=145567904956416,.field_5=-0.125,.field_6=1.76171875},.field_2=171,.field_3=140444787539968,.field_4=15};
    t27 a_5_9 = E_27_5;
    t28 a_5_10 = E_28_3;
    t29 ret_5 = fn_5_myr(a_5_1, a_5_2, a_5_3, a_5_4, a_5_5, a_5_6, a_5_7, a_5_8, a_5_9, a_5_10);
    if (!(ret_5==E_29_1)) {
        return -1;
    }

    t20 a_6_1 = (t20) {.field_1=(t1) {.field_1=141,.field_2=149,.field_3=-1.25,.field_4=58279295057920},.field_2=(t2) {.field_1=63572928888832,.field_2=46145049067520,.field_3=(t1) {.field_1=109,.field_2=59,.field_3=-1.4375,.field_4=89364507131904},.field_4=0.23828125,.field_5=(t1) {.field_1=25,.field_2=213,.field_3=1.546875,.field_4=232592606363648}}};
    t15 a_6_2 = E_15_6;
    uint32_t ret_6 = fn_6_myr(a_6_1, a_6_2);
    if (!(ret_6==4008488864)) {
        return -1;
    }

    double a_7_1 = 2.84375;
    double a_7_2 = -3.25390625;
    t31 a_7_3 = (t31) {.field_1=-2.56640625,.field_2=-0.59375,.field_3=-0.34375,.field_4=(t30) {.field_1=83,.field_2=-0.8125,.field_3=245451030659072,.field_4=E_24_1,.field_5=0.58203125,.field_6=0.734375}};
    t21 a_7_4 = (t21) {.field_1=(t20) {.field_1=(t1) {.field_1=154,.field_2=16,.field_3=1.53125,.field_4=240450872147968},.field_2=(t2) {.field_1=93431282270208,.field_2=243719039352832,.field_3=(t1) {.field_1=176,.field_2=179,.field_3=1.421875,.field_4=211285330100224},.field_4=-2.0546875,.field_5=(t1) {.field_1=120,.field_2=11,.field_3=0.984375,.field_4=85609800400896}}},.field_2=(t17) {.field_1=(t13) {.field_1=3734886520,.field_2=(t10) {.field_1=-0.03125,.field_2=4178676130},.field_3=(t11) {.field_1=1.4609375,.field_2=(t2) {.field_1=278857533489153,.field_2=40762444611584,.field_3=(t1) {.field_1=255,.field_2=97,.field_3=-0.09375,.field_4=67201348272128},.field_4=-1.9375,.field_5=(t1) {.field_1=145,.field_2=208,.field_3=1.53125,.field_4=6521111838720}}},.field_4=(t12) {.field_1=(t4) {.field_1=-0.2265625,.field_2=0.28125,.field_3=(t3) {.field_1=24215899865088,.field_2=(t2) {.field_1=128440090099712,.field_2=124968190476288,.field_3=(t1) {.field_1=106,.field_2=82,.field_3=-1.15625,.field_4=33900307021824},.field_4=-1.75390625,.field_5=(t1) {.field_1=226,.field_2=91,.field_3=0.765625,.field_4=261485102891008}},.field_3=(t1) {.field_1=43,.field_2=12,.field_3=0.9375,.field_4=185189970870272},.field_4=-3.73046875,.field_5=2085882880}},.field_2=(t1) {.field_1=166,.field_2=42,.field_3=-0.0625,.field_4=8114197757952},.field_3=(t5) {.field_1=179406660042752,.field_2=0.6875}}},.field_2=-1.7109375,.field_3=58423380148224,.field_4=136},.field_3=-0.71875,.field_4=E_15_2,.field_5=-3.87109375,.field_6=2.8046875};
    double a_7_5 = -0.3046875;
    float a_7_6 = 0.734375;
    t35 a_7_7 = (t35) {.field_1=(t32) {.field_1=66690558},.field_2=(t33) {.field_1=(t2) {.field_1=201975036641280,.field_2=191791705096192,.field_3=(t1) {.field_1=188,.field_2=255,.field_3=1.21875,.field_4=75962620575744},.field_4=-1.81640625,.field_5=(t1) {.field_1=30,.field_2=70,.field_3=-1.171875,.field_4=122844992372736}},.field_2=E_18_6},.field_3=E_34_3};
    uint64_t a_7_8 = 145924444127232;
    t37 ret_7 = fn_7_myr(a_7_1, a_7_2, a_7_3, a_7_4, a_7_5, a_7_6, a_7_7, a_7_8);
    if (!((ret_7.field_1==E_36_2))) {
        return -1;
    }

    t38 a_8_1 = E_38_4;
    t39 a_8_2 = E_39_1;
    double a_8_3 = -2.69140625;
    uint8_t a_8_4 = 17;
    t42 ret_8 = fn_8_myr(a_8_1, a_8_2, a_8_3, a_8_4);
    if (!((ret_8.field_1==86) && (((ret_8.field_2.field_1.field_1==3264522662) && ((ret_8.field_2.field_1.field_2.field_1==1.125) && (ret_8.field_2.field_1.field_2.field_2==3024392)) && ((ret_8.field_2.field_1.field_3.field_1==2.90625) && ((ret_8.field_2.field_1.field_3.field_2.field_1==246587710898176) && (ret_8.field_2.field_1.field_3.field_2.field_2==98684395847680) && ((ret_8.field_2.field_1.field_3.field_2.field_3.field_1==7) && (ret_8.field_2.field_1.field_3.field_2.field_3.field_2==232) && (ret_8.field_2.field_1.field_3.field_2.field_3.field_3==-0.25) && (ret_8.field_2.field_1.field_3.field_2.field_3.field_4==243343254224896)) && (ret_8.field_2.field_1.field_3.field_2.field_4==1.3828125) && ((ret_8.field_2.field_1.field_3.field_2.field_5.field_1==123) && (ret_8.field_2.field_1.field_3.field_2.field_5.field_2==151) && (ret_8.field_2.field_1.field_3.field_2.field_5.field_3==0.671875) && (ret_8.field_2.field_1.field_3.field_2.field_5.field_4==230200040620032)))) && (((ret_8.field_2.field_1.field_4.field_1.field_1==-0.5) && (ret_8.field_2.field_1.field_4.field_1.field_2==-1.484375) && ((ret_8.field_2.field_1.field_4.field_1.field_3.field_1==78257571102720) && ((ret_8.field_2.field_1.field_4.field_1.field_3.field_2.field_1==202195772637184) && (ret_8.field_2.field_1.field_4.field_1.field_3.field_2.field_2==21507053977600) && ((ret_8.field_2.field_1.field_4.field_1.field_3.field_2.field_3.field_1==42) && (ret_8.field_2.field_1.field_4.field_1.field_3.field_2.field_3.field_2==43) && (ret_8.field_2.field_1.field_4.field_1.field_3.field_2.field_3.field_3==0.453125) && (ret_8.field_2.field_1.field_4.field_1.field_3.field_2.field_3.field_4==259622561644544)) && (ret_8.field_2.field_1.field_4.field_1.field_3.field_2.field_4==1.18359375) && ((ret_8.field_2.field_1.field_4.field_1.field_3.field_2.field_5.field_1==201) && (ret_8.field_2.field_1.field_4.field_1.field_3.field_2.field_5.field_2==220) && (ret_8.field_2.field_1.field_4.field_1.field_3.field_2.field_5.field_3==-0.40625) && (ret_8.field_2.field_1.field_4.field_1.field_3.field_2.field_5.field_4==87732745404416))) && ((ret_8.field_2.field_1.field_4.field_1.field_3.field_3.field_1==61) && (ret_8.field_2.field_1.field_4.field_1.field_3.field_3.field_2==181) && (ret_8.field_2.field_1.field_4.field_1.field_3.field_3.field_3==-1.34375) && (ret_8.field_2.field_1.field_4.field_1.field_3.field_3.field_4==27713052475392)) && (ret_8.field_2.field_1.field_4.field_1.field_3.field_4==1.640625) && (ret_8.field_2.field_1.field_4.field_1.field_3.field_5==4052472574))) && ((ret_8.field_2.field_1.field_4.field_2.field_1==249) && (ret_8.field_2.field_1.field_4.field_2.field_2==15) && (ret_8.field_2.field_1.field_4.field_2.field_3==1.484375) && (ret_8.field_2.field_1.field_4.field_2.field_4==248514888728576)) && ((ret_8.field_2.field_1.field_4.field_3.field_1==134716506374144) && (ret_8.field_2.field_1.field_4.field_3.field_2==1.046875)))) && (ret_8.field_2.field_2==2404422220)) && (ret_8.field_3==246) && ((((ret_8.field_4.field_1.field_1.field_1==109) && (ret_8.field_4.field_1.field_1.field_2==70) && (ret_8.field_4.field_1.field_1.field_3==0.875) && (ret_8.field_4.field_1.field_1.field_4==242714521763840)) && ((ret_8.field_4.field_1.field_2.field_1==85381772738560) && (ret_8.field_4.field_1.field_2.field_2==16488017559552) && ((ret_8.field_4.field_1.field_2.field_3.field_1==148) && (ret_8.field_4.field_1.field_2.field_3.field_2==97) && (ret_8.field_4.field_1.field_2.field_3.field_3==-0.859375) && (ret_8.field_4.field_1.field_2.field_3.field_4==211742737956864)) && (ret_8.field_4.field_1.field_2.field_4==-3.73046875) && ((ret_8.field_4.field_1.field_2.field_5.field_1==37) && (ret_8.field_4.field_1.field_2.field_5.field_2==103) && (ret_8.field_4.field_1.field_2.field_5.field_3==0.96875) && (ret_8.field_4.field_1.field_2.field_5.field_4==3251610451968)))) && (ret_8.field_4.field_2==E_29_7) && (ret_8.field_4.field_3==34127014920192)))) {
        return -1;
    }

    t46 a_9_1 = (t46) {.field_1=(t43) {.field_1=(t10) {.field_1=0.171875,.field_2=3226754646},.field_2=(t12) {.field_1=(t4) {.field_1=2.765625,.field_2=0.1875,.field_3=(t3) {.field_1=276756521549824,.field_2=(t2) {.field_1=152999296499712,.field_2=85664820494336,.field_3=(t1) {.field_1=228,.field_2=103,.field_3=-0.71875,.field_4=189995730337792},.field_4=2.2109375,.field_5=(t1) {.field_1=167,.field_2=118,.field_3=-1.546875,.field_4=199486233247744}},.field_3=(t1) {.field_1=94,.field_2=71,.field_3=-1.0625,.field_4=93572022140928},.field_4=-1.046875,.field_5=3049108056}},.field_2=(t1) {.field_1=185,.field_2=79,.field_3=1.03125,.field_4=217666636349440},.field_3=(t5) {.field_1=129819186561024,.field_2=-1.421875}},.field_3=(t4) {.field_1=-1.60546875,.field_2=0.125,.field_3=(t3) {.field_1=37068096536576,.field_2=(t2) {.field_1=239555475144704,.field_2=81311387615232,.field_3=(t1) {.field_1=252,.field_2=105,.field_3=-0.71875,.field_4=148899603939328},.field_4=1.68359375,.field_5=(t1) {.field_1=42,.field_2=238,.field_3=1.53125,.field_4=236603073363968}},.field_3=(t1) {.field_1=183,.field_2=164,.field_3=-0.609375,.field_4=202904325324800},.field_4=-1.15234375,.field_5=2895375692}}},.field_2=E_44_7,.field_3=(t45) {.field_1=2208687568},.field_4=1848386781184,.field_5=(t4) {.field_1=-1.05859375,.field_2=-0.859375,.field_3=(t3) {.field_1=205448639807488,.field_2=(t2) {.field_1=190494723801088,.field_2=14572431867904,.field_3=(t1) {.field_1=128,.field_2=35,.field_3=-1.25,.field_4=224517446828032},.field_4=1.37109375,.field_5=(t1) {.field_1=59,.field_2=166,.field_3=1.453125,.field_4=61154056208384}},.field_3=(t1) {.field_1=15,.field_2=60,.field_3=0.765625,.field_4=218183195164672},.field_4=-0.765625,.field_5=2915453416}},.field_6=3275953006};
    float a_9_2 = -0.328125;
    t26 a_9_3 = (t26) {.field_1=(t25) {.field_1=(t9) {.field_1=E_8_1,.field_2=3258536312},.field_2=-2.17578125,.field_3=31978917462016,.field_4=76825007489024,.field_5=-0.859375,.field_6=-0.171875},.field_2=127,.field_3=269734934740992,.field_4=39};
    t47 a_9_4 = E_47_3;
    t48 a_9_5 = E_48_7;
    t54 a_9_6 = (t54) {.field_1=(t49) {.field_1=613960680},.field_2=(t50) {.field_1=-0.125,.field_2=(t45) {.field_1=469993662},.field_3=201131156307968,.field_4=3.79296875},.field_3=E_51_4,.field_4=(t52) {.field_1=0.09375,.field_2=E_51_6,.field_3=134,.field_4=1.234375,.field_5=1.359375},.field_5=E_51_2,.field_6=E_53_4};
    uint32_t a_9_7 = 604879132;
    float a_9_8 = 0.640625;
    t55 a_9_9 = E_55_3;
    t56 a_9_10 = E_56_4;
    uint64_t ret_9 = fn_9_myr(a_9_1, a_9_2, a_9_3, a_9_4, a_9_5, a_9_6, a_9_7, a_9_8, a_9_9, a_9_10);
    if (!(ret_9==219771358019584)) {
        return -1;
    }

    uint64_t a_10_1 = 160708767514624;
    double a_10_2 = 0.36328125;
    t61 a_10_3 = (t61) {.field_1=E_57_5,.field_2=(t58) {.field_1=-0.27734375,.field_2=E_36_3},.field_3=(t59) {.field_1=97,.field_2=33865353396224},.field_4=(t60) {.field_1=216701139681280,.field_2=-1.03125,.field_3=(t4) {.field_1=-0.8515625,.field_2=0.71875,.field_3=(t3) {.field_1=244705840726016,.field_2=(t2) {.field_1=53149469900800,.field_2=161998735409152,.field_3=(t1) {.field_1=127,.field_2=47,.field_3=1.25,.field_4=59620799152128},.field_4=-1.41015625,.field_5=(t1) {.field_1=130,.field_2=161,.field_3=-1.453125,.field_4=49656372789248}},.field_3=(t1) {.field_1=136,.field_2=83,.field_3=-1.15625,.field_4=278799917907969},.field_4=2.5234375,.field_5=3744252854}},.field_4=E_38_1}};
    uint8_t a_10_4 = 202;
    t62 a_10_5 = E_62_3;
    uint8_t a_10_6 = 0;
    double a_10_7 = 2.234375;
    uint64_t a_10_8 = 215484511158272;
    t3 ret_10 = fn_10_myr(a_10_1, a_10_2, a_10_3, a_10_4, a_10_5, a_10_6, a_10_7, a_10_8);
    if (!((ret_10.field_1==268633981059072) && ((ret_10.field_2.field_1==24949413642240) && (ret_10.field_2.field_2==153583813787648) && ((ret_10.field_2.field_3.field_1==35) && (ret_10.field_2.field_3.field_2==252) && (ret_10.field_2.field_3.field_3==0.8125) && (ret_10.field_2.field_3.field_4==128878867382272)) && (ret_10.field_2.field_4==-0.03125) && ((ret_10.field_2.field_5.field_1==98) && (ret_10.field_2.field_5.field_2==126) && (ret_10.field_2.field_5.field_3==0.546875) && (ret_10.field_2.field_5.field_4==258416610377728))) && ((ret_10.field_3.field_1==209) && (ret_10.field_3.field_2==206) && (ret_10.field_3.field_3==1.28125) && (ret_10.field_3.field_4==180894750212096)) && (ret_10.field_4==1.390625) && (ret_10.field_5==4104417000))) {
        return -1;
    }

    return 0;
}
