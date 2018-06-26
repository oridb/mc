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

{ find_M(f, n, v, a, b) = my(X, gran, l, lnext, len, xprev, xcur, xnext, yprev, ycur, ynext, thisa, thisb, k, j);
        gran = 100 * n;
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

                if(ycur > yprev && ycur > ynext, listput(l, [xcur, abs(ycur)]),);
        );

        vecsort(l, 2);
        if(length(l) < n + 2 || l[1][2] < 2^(-100), return("q"),);
        len = length(l);

        lnext = listcreate();
        for(j = 1, n + 2,
                thisa = l[j][1] - (b-a)/gran;
                thisb = l[j][1] + (b-a)/gran;

                xprev = thisa - (thisb - a)/gran;
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

                        if(ycur > yprev && ycur > ynext, listput(lnext, xcur),);
                );
        );
        vecsort(lnext, cmp);
        listkill(l);
        X = vector(n + 2);
        for (k = 1, min(n + 2, length(lnext)), X[k] = lnext[k]);
        listkill(lnext);
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
                c = find_M(f, n, v, a, b);
                if(c == "q", return,);
                print("c = ", c);
        );
}

print("\n");
print("Minimaxing sin(x), degree 7, on [-Pi/(4 * 256), Pi/(4 * 256)]:");
find_minimax(sin, 7, -Pi/1024, Pi/1024)
print("\n");
print("Minimaxing cos(x), degree 7, on [-Pi/(4 * 256), Pi/(4 * 256)]:");
find_minimax(cos, 7, -Pi/1024, Pi/1024)
print("\n");

