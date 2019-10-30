/*
  Attempts to find a minimax polynomial of degree n for f via Remez
  algorithm.
 */

{ chebyshev_node(a, b, k, n) = my(p, m, c);
        p = (b + a)/2;
        m = (b - a)/2;
        c = cos(Pi * (2*k - 1)/(2*n));
        return(p + m*c);
}

{ remez_step(f, n, a, b, x) = my(M, xx, bvec, k);
        M = matrix(n + 2, n + 2);
        bvec = vector(n + 2);
        for (k = 1, n + 2,
                xx = x[k];
                for (j = 1, n + 1,
                        M[k,j] = xx^(j - 1);
                );
                M[k, n + 2] = (-1)^k;
                bvec[k] = f(xx);
        );
        return(mattranspose(M^-1 * mattranspose(bvec)));
}

{ p_eval(n, v, x) = my(s, k);
        s = 0;
        for (k = 1, n + 1,
                s = s + v[k]*x^(k - 1)
        );

        return(s);
}

{ err(f, n, v, x) =
        return(abs(f(x) - p_eval(n, v, x)));
}

{ find_M(f, n, v, a, b, depth) = my(X, gran, l, lnext, len, xprev, xcur, xnext, yprev, ycur, ynext, thisa, thisb, k, j);
        gran = 10000 * depth;
        l = listcreate();

        xprev = a - (b - a)/gran;
        yprev = err(f, n, v, xprev);

        xcur = a;
        ycur = err(f, n, v, xprev);
        
        xnext = a + (b - a)/gran;
        ynext = err(f, n, v, xprev);

        for (k = 2, gran,
                xprev = xcur;
                yprev = ycur;
                
                xcur = xnext;
                ycur = ynext;

                xnext = a + k*(b - a)/gran;
                ynext = err(f, n, v, xnext);

                if(ycur > yprev && ycur > ynext, listput(l, [xcur, ycur]),);
        );

        vecsort(l, 2);
        if(length(l) < n + 2 || l[1][2] < 2^(-2000), return("q"),);
        len = length(l);

        X = vector(n + 2);

        for(j = 1, n + 2,
                lnext = listcreate();
                thisa = l[j][1] - (b-a)/gran;
                thisb = l[j][1] + (b-a)/gran;

                xprev = thisa - (thisb - thisa)/gran;
                yprev = err(f, n, v, xprev);

                xcur = thisa;
                ycur = err(f, n, v, xprev);
        
                xnext = thisa + (thisb - thisa)/gran;
                ynext = err(f, n, v, xprev);

                for (k = 2, gran,
                        xprev = xcur;
                        yprev = ycur;
                
                        xcur = xnext;
                        ycur = ynext;

                        xnext = thisa + k*(thisb - thisa)/gran;
                        ynext = abs(f(xnext) - p_eval(n, v, xnext));

                        if(ycur > yprev && ycur > ynext, listput(lnext, [xcur, ycur]),);
                );

                vecsort(lnext, 2);
                if(length(lnext) < 1, return("q"),);
                X[j] = lnext[1][1];
                listkill(lnext);
        );
        listkill(l);
        vecsort(X);
        return(X);
}

{ find_minimax(f, n, a, b) = my(c, k, j);
        c = vector(n + 2);
        for (k = 1, n + 2,
                c[k] = chebyshev_node(a, b, k, n + 2);
        );
        for(j = 1, 100,
                v = remez_step(f, n, a, b, c);
                print("v = ", v);
                c = find_M(f, n, v, a, b, j);
                if(c == "q", return,);
                print("c = ", c);
        );
}

{ sinoverx(x) =
        return(if(x == 0, 1, sin(x)/x));
}

{ tanoverx(x) =
        return(if(x == 0, 1, tan(x)/x));
}

{ atanxoverx(x) =
        return(if(x == 0, 1, atan(x)/x));
}

{ cotx(x) =
        return(1/tanoverx(x));
}

{ log2xoverx(x) =
        return(if(x == 1,1,log(x)/(x-1))/log(2));
}

{ log1p(x) =
        return(log(1 + x));
}

print("\n");
print("Minimaxing sin(x) / x, degree 6, on [-Pi/(4 * 256), Pi/(4 * 256)]:");
find_minimax(sinoverx, 6, -Pi/1024, Pi/1024)
print("\n");
print("(You'll need to add a 0x0 at the beginning to make a degree 7...\n");
print("\n");
print("---\n");
print("\n");
print("Minimaxing cos(x), degree 7, on [-Pi/(4 * 256), Pi/(4 * 256)]:");
find_minimax(cos, 7, -Pi/1024, Pi/1024)
print("\n");
print("---\n");
print("\n");
print("Minmimaxing tan(x) / x, degree 6, on [-Pi/(4 * 256), Pi/(4 * 256)]:");
find_minimax(tanoverx, 6, -Pi/1024, Pi/1024)
print("\n");
print("(You'll need to add a 0x0 at the beginning to make a degree 7...\n");
print("\n");
print("---\n");
print("\n");
print("Minmimaxing x*cot(x), degree 8, on [-Pi/(4 * 256), Pi/(4 * 256)]:");
find_minimax(cotx, 8, -Pi/1024, Pi/1024)
print("\n");
print("(Remember to divide by x)\n");
print("\n");
print("---\n");
print("\n");
print("Minmimaxing tan(x) / x, degree 10, on [0, 15.5/256]:");
find_minimax(tanoverx, 10, 0, 15.5/256)
print("\n");
print("(You'll need to add a 0x0 at the beginning to make a degree 11...\n");
print("\n");
print("---\n");
print("\n");
print("Minmimaxing atan(x) / x, degree 12, on [0, 15.5/256]:");
find_minimax(atanxoverx, 12, 0, 1/16)
print("\n");
print("(You'll need to add a 0x0 at the beginning to make a degree 13...\n");
print("\n");
print("---\n");
print("Minmimaxing log_2(x) / (x - 1), degree 7, on [1, 2^(1/8)]:");
find_minimax(log2xoverx, 7, 1, 2^(1/8))
print("\n");
/* print("(You'll need to add a 0x0 at the beginning to make a degree 13...\n"); */
/* print("\n"); */
print("---\n");
print("Minmimaxing log(1 + x), degree 5, on [0, 2^-20) [it's just going to give the Taylor expansion]:");
find_minimax(log1p, 5, 0, 2^-20)
print("\n");
/* print("(You'll need to add a 0x0 at the beginning to make a degree 13...\n"); */
/* print("\n"); */
print("---\n");
print("Remember that there's that extra, ugly E term at the end of the vector that you want to lop off.\n");
