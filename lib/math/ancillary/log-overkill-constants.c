/* cc -o log-overkill-constants log-overkill-constants.c -lmpfr */
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include <mpfr.h>

#define FLT64_TO_UINT64(f) (*((uint64_t *) ((char *) &f)))
#define UINT64_TO_FLT64(u) (*((double *) ((char *) &u)))

int
main(void)
{
        mpfr_t t1;
        mpfr_t t2;
        mpfr_t t3;
        double d = 0;
        uint64_t u = 0;
        size_t j = 0;
        long int k = 0;

        mpfr_init2(t1, 10000);
        mpfr_init2(t2, 10000);
        mpfr_init2(t3, 10000);

        /* C1 */
        for (k = -5; k >= -20; k -= 5) {
                printf(
                        "const C%ld : (uint64, uint64, uint64, uint64)[32] = [\n",
                        (k /
                         (
                                 -5)));

                for (j = 0; j < 32; ++j) {
                        mpfr_set_si(t1, 1, MPFR_RNDN);
                        mpfr_set_si(t2, k, MPFR_RNDN);
                        mpfr_exp2(t2, t2, MPFR_RNDN);
                        mpfr_mul_si(t2, t2, j, MPFR_RNDN);
                        mpfr_add(t1, t1, t2, MPFR_RNDN);

                        /* first, log(1 + ...) */
                        mpfr_log(t2, t1, MPFR_RNDN);
                        d = mpfr_get_d(t2, MPFR_RNDN);
                        u = FLT64_TO_UINT64(d);
                        printf("	(%#018lx, ", u);
                        mpfr_set_d(t3, d, MPFR_RNDN);
                        mpfr_sub(t2, t2, t3, MPFR_RNDN);
                        d = mpfr_get_d(t2, MPFR_RNDN);
                        u = FLT64_TO_UINT64(d);
                        printf("%#018lx, ", u);

                        /* now, 1/(1 + ...) */
                        mpfr_pow_si(t2, t1, -1, MPFR_RNDN);
                        d = mpfr_get_d(t2, MPFR_RNDN);
                        u = FLT64_TO_UINT64(d);
                        printf("    %#018lx, ", u);
                        mpfr_set_d(t3, d, MPFR_RNDN);
                        mpfr_sub(t2, t2, t3, MPFR_RNDN);
                        d = mpfr_get_d(t2, MPFR_RNDN);
                        u = FLT64_TO_UINT64(d);
                        printf("%#018lx), /* j = %zu */\n", u, j);
                }

                printf("]\n\n");
        }

        return 0;
}
