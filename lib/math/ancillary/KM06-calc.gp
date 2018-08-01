/*
 Implementations of some functions from [KM06]. The exact ranges
 were applied by manual fiddling.
 */

{ betap(amin, amax, p, n) = my(l, s);
        l = amax^(1/p);
        s = amin^(1/p);
        return(polroots(l^(1 - 1/(2^(n-1))) * (3/s - (x - s) * (p+1)/(s^2)) * (x - s)^2 - s^(1 - 1/(2^(n-1))) * (3/l - (x - l) * (p+1)/(l^2)) * (x - l)^2));
}

{ betam(amin, amax, p, n) = my(l, s);
        l = amax^(1/p);
        s = amin^(1/p);
        return(polroots(l^(1 - 1/(2^(n-1))) * (3/s - (x - s) * (p+1)/(s^2)) * (x - s)^2 + s^(1 - 1/(2^(n-1))) * (3/l - (x - l) * (p+1)/(l^2)) * (x - l)^2));
}

{ beta(amin, amax, p, n) = my(plus, minus, alsmaller, allarger, l, r);
        plus = betap(amin, amax, p, n);
        minus = betam(amin, amax, p, n);
        alsmaller = min(amax^(1/p), amin^(1/p));
        allarger = max(amax^(1/p), amin^(1/p));
        l = List();
        for(i=1, length(plus), if(imag(plus[i]) < 0.001 && imag(plus[i]) > -0.001, listput(l, real(plus[i])), ));
        for(i=1, length(minus), if(imag(minus[i]) < 0.001 && imag(minus[i]) > -0.001, listput(l, real(minus[i])), ));
        r=0.0;
        for(i=1, length(l), if(l[i] <= allarger && l[i] >= alsmaller, r=l[i], ));
        return(r);
}

{ maxerr(amin, amax, p, n) = my(x1, x2, e1, e2);
        x1 = List([beta(amin, amax, p, n)]);
        x2 = List([beta(amin, amax, p, n)]);
        for(i=1, n, listput(x1, x1[i]/p * (p - 1 + amin * x1[i]^(-p))));
        for(i=1, n, listput(x2, x2[i]/p * (p - 1 + amax * x2[i]^(-p))));
        e1 = abs(amin^(1/p) - x1[n + 1]);
        e2 = abs(amax^(1/p) - x2[n + 1]);
        return(max(e1, e2));
}
