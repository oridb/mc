/* cc -o generate-triples-for-GB91 generate-triples-for-GB91.c -lmpfr */
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include <mpfr.h>

/*
   Find triples (xi, sin(xi), cos(xi)) very close to machine numbers
   for use in the algorithm of [GB91]. See [Lut95] for a better way
   of calculating these, which we don't follow.

   The idea is that, by good fortune (or more persuasive arguments),
   there exist floating-point numbers xi that are very close to
   numbers of the form (i/N)(π/4). They are so close, in fact, that
   the exact binary expansion of (i/N)(π/4) is xi, followed by a
   bunch of zeroes, then the irrational, unpredictable part.

   Then, when we discretize the input for sin(x) to some xi + delta,
   the precomputed sin(xi) and cos(xi) can be stored as single
   floating-point numbers; the extra zeroes in the infinite-precision
   result mean that we get some precision for free. Compare with
   the constants for log-impl.myr or exp-impl.myr, where the constants
   require more space to be stored.
 */

/* Something something -fno-strict-aliasing */
#define FLT64_TO_UINT64(f) (*((uint64_t *) ((char *) &f)))
#define UINT64_TO_FLT64(u) (*((double *) ((char *) &u)))
#define xmin(a, b) ((a) < (b) ? (a) : (b))
#define xmax(a, b) ((a) > (b) ? (a) : (b))

#define EXP_OF_FLT64(f) (((FLT64_TO_UINT64(f)) >> 52) & 0x7ff)

static int N = 256;

/* Returns >= zero iff successful */
static int find_triple_64(int i, int min_leeway, int perfect_leeway)
{
        /*
           Using mpfr is entirely overkill for this; [Lut95] includes
           PASCAL fragments that use almost entirely integer
           arithmetic, and the initial calculation doesn't need to
           be so precise. No matter.
         */
        mpfr_t xi;
        mpfr_t xip1;
        mpfr_t cos;
        mpfr_t sin;
        double xip1_d;
        double t;
        uint64_t sin_u;
        uint64_t cos_u;
        int e1;
        int e2;
        uint64_t xip1_u;
        double xi_initial;
        uint64_t xi_initial_u;
        double xi_current;
        uint64_t xi_current_u;
        long int r = 0;
        long int best_r = 0;
        int sgn = 1;
        int ml = min_leeway;
        int best_l = 0;
        uint64_t best_xi_u;
        uint64_t best_sin_u;
        uint64_t best_cos_u;
        time_t start;
        time_t end;

        start = time(0);
        mpfr_init2(xi, 100);
        mpfr_init2(xip1, 100);
        mpfr_init2(cos, 100);
        mpfr_init2(sin, 100);

        /* start out at xi = πi/(4N) */
        mpfr_const_pi(xi, MPFR_RNDN);
        mpfr_mul_si(xip1, xi, (long int) (i + 1), MPFR_RNDN);
        mpfr_mul_si(xi, xi, (long int) i, MPFR_RNDN);
        mpfr_div_si(xi, xi, (long int) 4 * N, MPFR_RNDN);
        mpfr_div_si(xip1, xip1, (long int) 4 * N, MPFR_RNDN);
        xip1_d = mpfr_get_d(xip1, MPFR_RNDN);
        xip1_u = FLT64_TO_UINT64(xip1_d);
        xi_initial = mpfr_get_d(xi, MPFR_RNDN);
        xi_initial_u = FLT64_TO_UINT64(xi_initial);

        while (1) {
                xi_current_u = xi_initial_u + (sgn * r);
                xi_current = UINT64_TO_FLT64(xi_current_u);
                mpfr_set_d(xi, xi_current, MPFR_RNDN);

                /* Test if cos(xi) has enough zeroes */
                mpfr_cos(cos, xi, MPFR_RNDN);
                t = mpfr_get_d(cos, MPFR_RNDN);
                cos_u = FLT64_TO_UINT64(t);
                e1 = EXP_OF_FLT64(t);
                mpfr_sub_d(cos, cos, t, MPFR_RNDN);
                t = mpfr_get_d(cos, MPFR_RNDN);
                e2 = EXP_OF_FLT64(t);

                if (e2 == -1024) {

                        /* Damn; this is too close to a subnormal. i = 0 or N? */
                        return -1;
                }

                if (e1 - e2 < (52 + min_leeway)) {
                        goto inc;
                }

                ml = xmax(min_leeway, e1 - e2 - 52);

                /* Test if sin(xi) has enough zeroes */
                mpfr_sin(sin, xi, MPFR_RNDN);
                t = mpfr_get_d(sin, MPFR_RNDN);
                sin_u = FLT64_TO_UINT64(t);
                e1 = EXP_OF_FLT64(t);
                mpfr_sub_d(sin, sin, t, MPFR_RNDN);
                t = mpfr_get_d(sin, MPFR_RNDN);
                e2 = EXP_OF_FLT64(t);

                if (e2 == -1024) {

                        /* Damn; this is too close to a subnormal. i = 0 or N? */
                        return -1;
                }

                if (e1 - e2 < (52 + min_leeway)) {
                        goto inc;
                }

                ml = xmin(ml, e1 - e2 - 52);

                /* Hurrah, this is valid */
                if (ml > best_l) {
                        best_l = ml;
                        best_xi_u = xi_current_u;
                        best_cos_u = cos_u;
                        best_sin_u = sin_u;
                        best_r = sgn * r;

                        /* If this is super-good, don't bother finding more */
                        if (best_l >= perfect_leeway) {
                                break;
                        }
                }

inc:

                /* Increment */
                sgn *= -1;

                if (sgn < 0) {
                        r++;
                } else if (r > (1 << 29) || xi_current_u > xip1_u) {
                        /*
                           This is taking too long, give up looking
                           for perfection and take the best we've
                           got. A sweep of 1 << 28 finishes in ~60
                           hrs on my personal machine as I write
                           this.
                         */
                        break;
                }
        }

        end = time(0);

        if (best_l > min_leeway) {
                printf(
                        "[ .a = %#018lx, .c = %#018lx, .s = %#018lx ], /* i = %03d, l = %02d, r = %010ld, t = %ld */ \n",
                        best_xi_u, best_cos_u, best_sin_u, i, best_l, best_r,
                        end -
                        start);

                return 0;
        } else {
                return -1;
        }
}

int main(void)
{
        for (int i = 190; i <= N; ++i) {
                if (find_triple_64(i, 11, 20) < 0) {
                        printf("CANNOT FIND SUITABLE CANDIDATE FOR i = %03d\n", i);
                }
        }

        return 0;
}
