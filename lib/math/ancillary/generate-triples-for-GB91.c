/* cc -o generate-triples-for-GB91 generate-triples-for-GB91.c -lmpfr # -fno-strict-aliasing */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <mpfr.h>

/*
   Find triples (xi, sin(xi), cos(xi)) very close to machine numbers
   for use in the algorithm of [GB91]. See [Lut95] for a better way
   of calculating these, which we don't follow.

   The idea is that, by good fortune (or more persuasive arguments),
   there exist floating-point numbers xi that are close to numbers
   of the form (i/N)(π/4), such that, letting f(xi) = yi, the binary
   expansion of yi has a bunch of zeroes after the 53rd bit.

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

typedef int (*mpfr_fn)(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);

static int N = 256;

/* Returns >= zero iff successful */
static int find_triple_64(int i, int min_leeway, int perfect_leeway, mpfr_fn
                          sin_fn, mpfr_fn cos_fn)
{
        /*
           Using mpfr is not entirely overkill for this; [Lut95]
           includes PASCAL fragments that use almost entirely integer
           arithmetic... but the error term in that only handles
           up to 13 extra bits of zeroes or so. We proudly boast
           at least 16 bits of extra zeroes in all cases.
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
                cos_fn(cos, xi, MPFR_RNDN);
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
                sin_fn(sin, xi, MPFR_RNDN);
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
                } else if (r > (1 << 29) ||
                           xi_current_u > xip1_u) {
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
                        "(%#018lx, %#018lx, %#018lx), /* i = %03d, l = %02d, r = %010ld, t = %ld */ \n",
                        best_xi_u, best_cos_u, best_sin_u, i, best_l, best_r,
                        end -
                        start);

                return 0;
        } else {
                return -1;
        }
}

static void usage(void)
{
        printf("generate-triples-for-GB91\n");
        printf("                          [-i start_idx]\n");
        printf("                          [-j end_idx]\n");
        printf("                          -f sin|tan\n");
}

int main(int argc, char **argv)
{
        int c = 0;
        long i_start_arg = 1;
        long i_end_arg = N;
        int i_start = 1;
        int i_end = N;
        mpfr_fn sin_fn = 0;
        mpfr_fn cos_fn = 0;

        for (int k = 0; k < argc; ++k) {
                printf("%s ", argv[k]);
        }

        printf("\n");

        while ((c = getopt(argc, argv, "i:j:f:")) != -1) {
                switch (c) {
                case 'i':
                        errno = 0;
                        i_start_arg = strtoll(optarg, 0, 0);

                        if (errno) {
                                fprintf(stderr, "bad start index %s\n", optarg);

                                return 1;
                        }

                        break;
                case 'j':
                        errno = 0;
                        i_end_arg = strtoll(optarg, 0, 0);

                        if (errno) {
                                fprintf(stderr, "bad end index %s\n", optarg);

                                return 1;
                        }

                        break;
                case 'f':

                        if (!strcmp(optarg, "sin")) {
                                sin_fn = mpfr_sin;
                                cos_fn = mpfr_cos;
                        } else if (!strcmp(optarg, "tan")) {
                                sin_fn = mpfr_tan;
                                cos_fn = mpfr_cot;
                        } else {
                                fprintf(stderr, "unknown function %s\n",
                                        optarg);

                                return 1;
                        }

                        break;
                default:
                        usage();
                        break;
                }
        }

        if (i_start_arg <= 0 ||
            i_end_arg > N) {
                printf("truncating start to (0, %d]\n", N);
                i_start_arg = xmin(xmax(i_start_arg, 1), N);
        }

        if (i_end_arg <= 0 ||
            i_end_arg > N) {
                printf("truncating end to (0, %d]\n", N);
                i_end_arg = xmin(xmax(i_end_arg, 1), N);
        }

        i_start = i_start_arg;
        i_end = i_end_arg;

        if (!sin_fn ||
            !cos_fn) {
                fprintf(stderr, "-f required\n");

                return 1;
        }

        for (int i = i_start; i <= i_end; ++i) {
                if (find_triple_64(i, 11, 20, sin_fn, cos_fn) < 0) {
                        /*
                           This indicates you should drop the range
                           limitations on r, re-run, and come back
                           in a week.
                         */
                        printf("CANNOT FIND SUITABLE CANDIDATE FOR i = %03d\n",
                               i);
                }
        }

        return 0;
}
