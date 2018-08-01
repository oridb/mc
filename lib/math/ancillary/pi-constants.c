/* cc -o pi-constants pi-constants.c -lmpfr */
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include <mpfr.h>

#define FLT64_TO_UINT64(f) (*((uint64_t *) ((char *) &f)))
#define UINT64_TO_FLT64(u) (*((double *) ((char *) &u)))

int main(void)
{
        mpfr_t pi;
        mpfr_t two_pi;
        mpfr_t t;
        mpfr_t t2;
        mpfr_t perfect_n;
        double d = 0;
        uint64_t u = 0;

        mpfr_init2(pi, 10000);
        mpfr_init2(two_pi, 10000);
        mpfr_init2(t, 10000);
        mpfr_init2(t2, 10000);
        mpfr_init2(perfect_n, 10000);
        mpfr_const_pi(pi, MPFR_RNDN);
        mpfr_mul_si(two_pi, pi, 2, MPFR_RNDN);

        for (long e = 25; e <= 1023; e += 50) {
                mpfr_set_si(t, e, MPFR_RNDN);
                mpfr_exp2(t, t, MPFR_RNDN);
                mpfr_fmod(t2, t, two_pi, MPFR_RNDN);
                mpfr_set(t, t2, MPFR_RNDN);
                d = mpfr_get_d(t, MPFR_RNDN);
                u = FLT64_TO_UINT64(d);
                printf("(%#018lx, ", u);
                mpfr_set_d(t2, d, MPFR_RNDN);
                mpfr_sub(t, t, t2, MPFR_RNDN);
                d = mpfr_get_d(t, MPFR_RNDN);
                u = FLT64_TO_UINT64(d);
                printf("%#018lx, ", u);
                mpfr_set_d(t2, d, MPFR_RNDN);
                mpfr_sub(t, t, t2, MPFR_RNDN);
                d = mpfr_get_d(t, MPFR_RNDN);
                u = FLT64_TO_UINT64(d);
                printf("%#018lx), /* 2^%ld mod 2pi */\n", u, e);
        }

        printf("\n");
        printf("1000 bits of pi/2:\n");
        mpfr_const_pi(pi, MPFR_RNDN);
        mpfr_div_si(pi, pi, 2, MPFR_RNDN);

        for (int bits_obtained = 0; bits_obtained < 1000; bits_obtained += 53) {
                d = mpfr_get_d(pi, MPFR_RNDN);
                u = FLT64_TO_UINT64(d);
                printf("%#018lx\n", u);
                mpfr_set_d(t, d, MPFR_RNDN);
                mpfr_sub(pi, pi, t, MPFR_RNDN);
        }

        printf("\n");
        printf("1000 bits of pi/4:\n");
        mpfr_const_pi(pi, MPFR_RNDN);
        mpfr_div_si(pi, pi, 4, MPFR_RNDN);

        for (int bits_obtained = 0; bits_obtained < 1000; bits_obtained += 53) {
                d = mpfr_get_d(pi, MPFR_RNDN);
                u = FLT64_TO_UINT64(d);
                printf("%#018lx\n", u);
                mpfr_set_d(t, d, MPFR_RNDN);
                mpfr_sub(pi, pi, t, MPFR_RNDN);
        }

        printf("\n");
        printf("Pre-computed 2/pi:\n");
        mpfr_const_pi(pi, MPFR_RNDN);
        mpfr_set_si(t, 2, MPFR_RNDN);
        mpfr_div(pi, t, pi, MPFR_RNDN);
        d = mpfr_get_d(pi, MPFR_RNDN);
        u = FLT64_TO_UINT64(d);
        printf("%#018lx\n", u);
        printf("\n");
        printf("Pre-computed 1/(pi/1024):\n");
        mpfr_const_pi(pi, MPFR_RNDN);
        mpfr_set_si(t, 1024, MPFR_RNDN);
        mpfr_div(pi, t, pi, MPFR_RNDN);
        d = mpfr_get_d(pi, MPFR_RNDN);
        u = FLT64_TO_UINT64(d);
        printf("%#018lx\n", u);

        return 0;
}
