/* cc -o log-overkill-tests log-overkill-tests.c -lmpfr */
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include <mpfr.h>

#define FLT64_TO_UINT64(f) (*((uint64_t *) ((char *) &f)))
#define UINT64_TO_FLT64(u) (*((double *) ((char *) &u)))

#define FLT32_TO_UINT32(f) (*((uint32_t *) ((char *) &f)))
#define UINT32_TO_FLT32(u) (*((float *) ((char *) &u)))

/*
   xoroshiro64**, from http://xoshiro.di.unimi.it/xoroshiro64starstar.c
   and in public domain.
 */
static inline uint32_t
rotl(const uint32_t x, int k)
{
        return (x << k) | (x >> (32 - k));
}

static uint32_t s[2];

uint32_t
next(void)
{
        const uint32_t s0 = s[0];
        uint32_t s1 = s[1];
        const uint32_t result_starstar = rotl(s0 * 0x9E3779BB, 5) * 5;

        s1 ^= s0;
        s[0] = rotl(s0, 26) ^ s1 ^ (s1 << 9);
        s[1] = rotl(s1, 13);

        return result_starstar;
}

int
main(void)
{
        mpfr_t x;
        mpfr_t lnx;
        mpfr_t t;
        double d = 0;
        float f = 0;
        double t1 = 0;
        double t2 = 0;
        float t3 = 0;
        float t4 = 0;
        uint64_t u = 0;
        uint32_t v = 0;

        /* Seed xoroshiro64** */
        s[1] = 1234;
        s[2] = 1234;

        mpfr_init2(x, 10000);
        mpfr_init2(t, 10000);
        mpfr_init2(lnx, 10000);

        size_t n = 0;
        while (n < 100) {
                u = (((uint64_t)next()) << 32) | ((uint64_t)next());
                d = UINT64_TO_FLT64(u);
                if (d < 0.001 || d > 100) {
                        continue;
                }

                printf("(%#018lx, ", u);
                mpfr_set_d(x, d, MPFR_RNDN);
                mpfr_log(lnx, x, MPFR_RNDN);

                t1 = mpfr_get_d(lnx, MPFR_RNDN);
                mpfr_set_d(t, t1, MPFR_RNDN);
                mpfr_sub(lnx, lnx, t, MPFR_RNDN);
                t2 = mpfr_get_d(lnx, MPFR_RNDN);

                printf("%#018lx, %#018lx),\n", FLT64_TO_UINT64(t1), FLT64_TO_UINT64(t2));
                n++;
        }

        n = 0;
        while (n < 100) {
                v = next();
                f = UINT32_TO_FLT32(v);
                if (f < 0.001 || f > 100) {
                        continue;
                }

                printf("(%#010x, ", v);
                mpfr_set_d(x, (double) f, MPFR_RNDN);
                mpfr_log(lnx, x, MPFR_RNDN);

                t3 = (float)mpfr_get_d(lnx, MPFR_RNDN);
                mpfr_set_d(t, (double) t3, MPFR_RNDN);
                mpfr_sub(lnx, lnx, t, MPFR_RNDN);
                t4 = (float)mpfr_get_d(lnx, MPFR_RNDN);

                printf("%#010x, %#010x),\n", FLT32_TO_UINT32(t3), FLT32_TO_UINT32(t4));
                n++;
        }

        return 0;
}
