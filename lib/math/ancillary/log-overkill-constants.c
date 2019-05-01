/* cc -o log-overkill-constants log-overkill-constants.c -lmpfr */
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include <mpfr.h>

#define FLT64_TO_UINT64(f) (*((uint64_t *) ((char *) &f)))
#define UINT64_TO_FLT64(u) (*((double *) ((char *) &u)))

#define FLT32_TO_UINT32(f) (*((uint32_t *) ((char *) &f)))
#define UINT32_TO_FLT32(u) (*((double *) ((char *) &u)))

int main(void)
{
        mpfr_t one;
        mpfr_t two_to_the_minus_k;
        mpfr_t one_plus_two_to_the_minus_k;
        mpfr_t ln_one_plus_two_to_the_minus_k;
        mpfr_t one_minus_two_to_the_minus_k;
        mpfr_t ln_one_minus_two_to_the_minus_k;
        mpfr_t t1;
        mpfr_t t2;
        double d = 0;
        uint64_t u = 0;

        mpfr_init2(one, 10000);
        mpfr_init2(two_to_the_minus_k, 10000);
        mpfr_init2(one_plus_two_to_the_minus_k, 10000);
        mpfr_init2(ln_one_plus_two_to_the_minus_k, 10000);
        mpfr_init2(one_minus_two_to_the_minus_k, 10000);
        mpfr_init2(ln_one_minus_two_to_the_minus_k, 10000);
        mpfr_init2(t1, 10000);
        mpfr_init2(t2, 10000);

        printf("/* C_plus */\n");
        printf("(0x0000000000000000, 0x0000000000000000, 0x0000000000000000), /* dummy */\n");

        for (size_t k = 1; k <= 27; ++k) {
                mpfr_set_si(one, 1, MPFR_RNDN);
                mpfr_mul_2si(two_to_the_minus_k, one, -k, MPFR_RNDN);
                mpfr_add(one_plus_two_to_the_minus_k, one, two_to_the_minus_k, MPFR_RNDN);
                mpfr_log(ln_one_plus_two_to_the_minus_k, one_plus_two_to_the_minus_k, MPFR_RNDN);

                mpfr_set(t1, ln_one_plus_two_to_the_minus_k, MPFR_RNDN);
                d = mpfr_get_d(t1, MPFR_RNDN);
                u = FLT64_TO_UINT64(d);
                printf("(%#018lx, ", u);
                mpfr_set_d(t2, d, MPFR_RNDN);
                mpfr_sub(t1, t1, t2, MPFR_RNDN);
                d = mpfr_get_d(t1, MPFR_RNDN);
                u = FLT64_TO_UINT64(d);
                printf("%#018lx, ", u);
                mpfr_set_d(t2, d, MPFR_RNDN);
                mpfr_sub(t1, t1, t2, MPFR_RNDN);
                d = mpfr_get_d(t1, MPFR_RNDN);
                u = FLT64_TO_UINT64(d);
                printf("%#018lx), /* k = %zu */\n", u, k);
        }

        printf("\n");
        printf("/* C_minus */\n");
        printf("(0x0000000000000000, 0x0000000000000000, 0x0000000000000000), /* dummy */\n");

        for (size_t k = 1; k <= 27; ++k) {
                mpfr_set_si(one, 1, MPFR_RNDN);
                mpfr_mul_2si(two_to_the_minus_k, one, -k, MPFR_RNDN);
                mpfr_sub(one_minus_two_to_the_minus_k, one, two_to_the_minus_k, MPFR_RNDN);
                mpfr_log(ln_one_minus_two_to_the_minus_k, one_minus_two_to_the_minus_k, MPFR_RNDN);

                mpfr_set(t1, ln_one_minus_two_to_the_minus_k, MPFR_RNDN);
                d = mpfr_get_d(t1, MPFR_RNDN);
                u = FLT64_TO_UINT64(d);
                printf("(%#018lx, ", u);
                mpfr_set_d(t2, d, MPFR_RNDN);
                mpfr_sub(t1, t1, t2, MPFR_RNDN);
                d = mpfr_get_d(t1, MPFR_RNDN);
                u = FLT64_TO_UINT64(d);
                printf("%#018lx, ", u);
                mpfr_set_d(t2, d, MPFR_RNDN);
                mpfr_sub(t1, t1, t2, MPFR_RNDN);
                d = mpfr_get_d(t1, MPFR_RNDN);
                u = FLT64_TO_UINT64(d);
                printf("%#018lx), /* k = %zu */\n", u, k);
        }

        return 0;
}
