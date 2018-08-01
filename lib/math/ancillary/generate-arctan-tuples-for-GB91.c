/* cc -o generate-arctan-tuples-for-GB91 generate-arctan-tuples-for-GB91.c -lmpfr # -fno-strict-aliasing */
/* cc -static -std=c99 -D_POSIX_C_SOURCE=999999999 -fno-strict-aliasing -O2 -o generate-arctan-tuples-for-GB91 generate-arctan-tuples-for-GB91.c -lmpfr -lgmp */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <mpfr.h>

/*
   [GB91] treat arctan by a table + minimax method: the range [0,1]
   is partitioned up, and on each partition arctan is approximated
   by a minimax polynomial pi(x - xi) =~= arctan(x - xi).

   The twist of [GB91], that of Highly Accurate Tables, here applies
   to the first two coefficients of the minimax polynomial: by
   adjusting xi and computing different polynomials, we obtain
   coefficients cij for pi such that ci0 and ci1, in perfect accuracy,
   have a bunch of zeroes in the binary expansion after the 53rd
   bit. This gives the stored cij a bit more accuracy for free.

   Note that there's a sign flip somewhere: so the even-degree
   elements need to be negated for use in atan-impl.myr.
 */

/* Something something -fno-strict-aliasing */
#define FLT64_TO_UINT64(f) (*((uint64_t *) ((char *) &f)))
#define UINT64_TO_FLT64(u) (*((double *) ((char *) &u)))
#define xmin(a, b) ((a) < (b) ? (a) : (b))
#define xmax(a, b) ((a) > (b) ? (a) : (b))

#define EXP_OF_FLT64(f) (((FLT64_TO_UINT64(f)) >> 52) & 0x7ff)

typedef int (*mpfr_fn)(mpfr_ptr, mpfr_srcptr, mpfr_rnd_t);

#define N 5

static int leeway_of(mpfr_t temp, mpfr_t f)
{
        double d1 = mpfr_get_d(f, MPFR_RNDN);
        double d2 = 0.0;

        mpfr_set_d(temp, d1, MPFR_RNDN);
        mpfr_sub(temp, f, temp, MPFR_RNDN);
        d2 = mpfr_get_d(temp, MPFR_RNDN);

        return EXP_OF_FLT64(d1) - 52 - EXP_OF_FLT64(d2);
}

static void determinant_poorly(mpfr_t det, mpfr_t A[][N + 2])
{
        int sgn = 1;
        int sigma[N + 2];
        mpfr_t prod;

        for (int j = 0; j < (N + 2); ++j) {
                sigma[j] = j;
        }

        mpfr_set_si(det, 0, MPFR_RNDN);
        mpfr_init2(prod, 200);

        while (1) {
                /* ∏ a_{j, sigma[j]} */
                mpfr_set_si(prod, sgn, MPFR_RNDN);

                for (int j = 0; j < (N + 2); ++j) {
                        mpfr_mul(prod, prod, A[j][sigma[j]], MPFR_RNDN);
                }

                mpfr_add(det, det, prod, MPFR_RNDN);

                /* increment sigma: algorithm K/L/something... */
                int k = N + 1;
                int j = N + 1;
                int t;

                while (k > 0 &&
                       sigma[k - 1] >= sigma[k]) {
                        k--;
                }

                if (!k) {
                        break;
                }

                while (sigma[j] <= sigma[k - 1]) {
                        j--;
                }

                if (k - 1 != j) {
                        t = sigma[k - 1];
                        sigma[k - 1] = sigma[j];
                        sigma[j] = t;
                        sgn *= -1;
                }

                for (int l = N + 1; l > k; --l, ++k) {
                        t = sigma[l];
                        sigma[l] = sigma[k];
                        sigma[k] = t;
                        sgn *= -1;
                }
        }
}

static void invert_poorly(mpfr_t A[][N + 2], mpfr_t Ainv[][N + 2])
{
        mpfr_t Mij[N + 2][N + 2];
        mpfr_t det;
        mpfr_t Mijdet;

        mpfr_init2(det, 200);
        mpfr_init2(Mijdet, 200);
        determinant_poorly(det, A);

        for (int i = 0; i < N + 2; ++i) {
                for (int j = 0; j < N + 2; ++j) {
                        mpfr_init2(Mij[i][j], 200);

                        if (i == (N + 1) &&
                            j == (N + 1)) {
                                mpfr_set_si(Mij[i][j], 1, MPFR_RNDN);
                        } else if (i == (N + 1) ||
                                   j == (N + 1)) {
                                mpfr_set_si(Mij[i][j], 0, MPFR_RNDN);
                        }
                }
        }

        /* Construct transpose adjugate poorly */
        for (int i = 0; i < N + 2; ++i) {
                for (int j = 0; j < N + 2; ++j) {
                        /* Copy over A, sans i, j, to Mij */
                        for (int ii = 0; ii < N + 2; ii++) {
                                if (ii == i) {
                                        continue;
                                }

                                int ri = ii > i ? ii - 1 : ii;

                                for (int jj = 0; jj < N + 2; jj++) {
                                        if (jj == j) {
                                                continue;
                                        }

                                        int rj = jj > j ? jj - 1 : jj;

                                        mpfr_set(Mij[ri][rj], A[ii][jj],
                                                 MPFR_RNDN);
                                }
                        }

                        /* Ainv[j][i] = | Mij | / det */
                        determinant_poorly(Mijdet, Mij);
                        mpfr_div(Ainv[j][i], Mijdet, det, MPFR_RNDN);

                        if ((i + j) % 2) {
                                mpfr_mul_si(Ainv[j][i], Ainv[j][i], -1,
                                            MPFR_RNDN);
                        }
                }
        }
}

static int find_tuple(int ii, int min_leeway)
{
        int64_t r = 0;
        double xi_orig_d = ii / 256.0;
        uint64_t xi_orig = FLT64_TO_UINT64(xi_orig_d);
        double range_a = -1 / 512.0;
        double range_b = 1 / 512.0;
        uint64_t xi;
        double xi_d;
        mpfr_t xi_m;
        int best_lee = 0;
        long int best_r = 0;
        mpfr_t t[10];
        mpfr_t cn[N + 2];
        mpfr_t bi[N + 2];
        mpfr_t best_bi[N + 2];
        mpfr_t best_xi;
        mpfr_t xij[N + 2][N + 2];
        mpfr_t xijinv[N + 2][N + 2];
        mpfr_t fxi[N + 2];
        double t_d = 0.0;
        uint64_t t_u = 0;
        long ec = 1;
        long start = time(0);
        long end = start;

        mpfr_init2(xi_m, 200);
        mpfr_init2(best_xi, 200);

        for (int i = 0; i < 10; ++i) {
                mpfr_init2(t[i], 200);
        }

        mpfr_set_d(t[1], range_a, MPFR_RNDN);
        mpfr_set_d(t[2], range_b, MPFR_RNDN);
        mpfr_add(t[3], t[2], t[1], MPFR_RNDN);
        mpfr_sub(t[4], t[2], t[1], MPFR_RNDN);
        mpfr_div_si(t[3], t[3], 2, MPFR_RNDN);
        mpfr_div_si(t[4], t[4], 2, MPFR_RNDN);

        /* Calculate Chebyshev nodes for the range */
        for (int i = 0; i < (N + 2); ++i) {
                mpfr_init2(cn[i], 200);
                mpfr_init2(bi[i], 200);
                mpfr_init2(best_bi[i], 200);
                mpfr_set_si(best_bi[i], 0, MPFR_RNDN);
                mpfr_init2(fxi[i], 200);
                mpfr_set_si(cn[i], 2 * i - 1, MPFR_RNDN);
                mpfr_div_si(cn[i], cn[i], 2 * (N + 2), MPFR_RNDN);
                mpfr_cos(cn[i], cn[i], MPFR_RNDN);
                mpfr_mul(cn[i], cn[i], t[4], MPFR_RNDN);
                mpfr_add(cn[i], cn[i], t[3], MPFR_RNDN);
        }

        /*
           Set up M×M (M = N+2) matrix for one step of Remez
           algorithm: the cnI^Js in

           b0 + b1·cn1 + ⋯ + bN·cn1^n + (-1)^1·E = f(cn1)
           b0 + b1·cn2 + ⋯ + bN·cn2^n + (-1)^2·E = f(cn2)
            ⋮     ⋮      ⋱     ⋮              ⋮       ⋮
           b0 + b1·cnM + ⋯ + bN·cnM^n + (-1)^M·E = f(cnM)
         */
        for (int i = 0; i < (N + 2); ++i) {
                mpfr_set_si(t[1], 1, MPFR_RNDN);

                for (int j = 0; j < (N + 1); ++j) {
                        mpfr_init2(xij[i][j], 200);
                        mpfr_init2(xijinv[i][j], 200);
                        mpfr_set(xij[i][j], t[1], MPFR_RNDN);
                        mpfr_mul(t[1], t[1], cn[i], MPFR_RNDN);
                }

                mpfr_init2(xij[i][N + 1], 200);
                mpfr_init2(xijinv[i][N + 1], 200);
                mpfr_set_si(xij[i][N + 1], ec, MPFR_RNDN);
                ec *= -1;
        }

        /* Compute (xij)^(-1) */
        invert_poorly(xij, xijinv);

        while (r < (1 << 28)) {
                xi = xi_orig + r;
                xi_d = UINT64_TO_FLT64(xi);
                mpfr_set_d(xi_m, xi_d, MPFR_RNDN);

                /* compute f(cn[i]) = atan(cn[i] - xi) */
                for (int i = 0; i < (N + 2); ++i) {
                        mpfr_sub(fxi[i], cn[i], xi_m, MPFR_RNDN);
                        mpfr_atan(fxi[i], fxi[i], MPFR_RNDN);
                }

                /* Now solve the linear system above for bi */
                for (int i = 0; i < (N + 2); ++i) {
                        mpfr_set_si(bi[i], 0, MPFR_RNDN);

                        for (int j = 0; j < (N + 2); ++j) {
                                mpfr_mul(t[i], xijinv[i][j], fxi[j], MPFR_RNDN);
                                mpfr_add(bi[i], bi[i], t[i], MPFR_RNDN);
                        }
                }

                /*
                   If the error term isn't within close to 0, we
                   should, by all rights, try a few more iterations
                   of Remez. But that's incredibly slow, and we're
                   in a tight loop, so let's just bail.
                 */
                double e = mpfr_get_d(bi[N + 1], MPFR_RNDN);

                if (FLT64_TO_UINT64(e) & 0x7fffffffffffffff > 0x08) {
                        goto next_r;
                }

                /* Test if b[0] and b[1] are very precise */
                int leeA = leeway_of(t[0], bi[0]);
                int leeB = 0;
                int lee = 0;

                if (leeA <= min_leeway) {
                        goto next_r;
                }

                leeB = leeway_of(t[0], bi[1]);

                if (leeB + 4 <= min_leeway) {
                        goto next_r;
                }

                lee = xmin(leeA, leeB + 4);

                if (lee <= best_lee) {
                        goto next_r;
                }

                best_lee = lee;
                best_r = r;
                mpfr_set(best_xi, xi_m, MPFR_RNDN);

                for (int i = 0; i < (N + 2); ++i) {
                        mpfr_set(best_bi[i], bi[i], MPFR_RNDN);
                }

next_r:

                /* increment r */
                if (r <= 0) {
                        r = 1 - r;
                } else {
                        r = -r;
                }
        }

        end = time(0);

        if (best_lee < min_leeway) {
                return -1;
        }

        /* Recall the N+1 entry in output is the error, which we don't care about */
        t_d = mpfr_get_d(best_xi, MPFR_RNDN);
        t_u = FLT64_TO_UINT64(t_d);
        printf("(%#018lx, ", t_u);

        for (int i = 0; i < N; ++i) {
                t_d = mpfr_get_d(best_bi[i], MPFR_RNDN);
                printf("%#018lx, ", FLT64_TO_UINT64(t_d));
        }

        t_d = mpfr_get_d(best_bi[N], MPFR_RNDN);
        t_u = FLT64_TO_UINT64(t_d);
        printf("%#018lx), ", t_u);
        printf("/* i = %03d, l = %02d, r = %010ld, t = %ld */\n", ii, best_lee,
               best_r, end - start);

        return 0;
}

static void usage(void)
{
        printf("generate-arctan-tuples-for-GB91\n");
        printf("                                [-i start_idx]\n");
        printf("                                [-j end_idx]\n");
}

int main(int argc, char **argv)
{
        int c = 0;
        long i_start_arg = 1;
        long i_end_arg = 256;
        int i_start = 1;
        int i_end = 256;

        for (int k = 0; k < argc; ++k) {
                printf("%s ", argv[k]);
        }

        printf("\n");

        while ((c = getopt(argc, argv, "i:j:")) != -1) {
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
                default:
                        usage();
                        break;
                }
        }

        if (i_start_arg <= 0 ||
            i_start_arg > 256) {
                printf("truncating start to (0, %d]\n", 256);
                i_start_arg = xmin(xmax(i_start_arg, 1), 256);
        }

        if (i_end_arg <= 0 ||
            i_end_arg > 256) {
                printf("truncating end to (0, %d]\n", 256);
                i_end_arg = xmin(xmax(i_end_arg, 1), 256);
        }

        i_start = i_start_arg;
        i_end = i_end_arg;

        for (int i = i_start; i <= i_end; ++i) {
                if (find_tuple(i, 1) < 0) {
                        printf("CANNOT FIND SUITABLE CANDIDATE FOR i = %03d\n",
                               i);
                }
        }

        return 0;
}
