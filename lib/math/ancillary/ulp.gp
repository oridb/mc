/*
  I always end up need this for debugging
 */

{ ulp32(a) = my(aa, q);
        aa = abs(a);
        if(aa < 2^(-150),return(2^(-126 - 23)),);
        q = floor(log(aa)/log(2));
        if(q < -126,q=-126,);
        return(2^(q-23));
}

{ ulp64(a) = my(aa, q);
        aa = abs(a);
        if(aa < 2^(-2000),return(2^(-1022 - 52)),);
        q = floor(log(aa)/log(2));
        if(q < -1022,q=-1022,);
        return(2^(q-52));
}

{ err32(x, y) =
        return(abs(x-y)/ulp32(x));
}

{ err64(x, y) =
        return(abs(x-y)/ulp64(x));
}

{ flt32fromuint32(a) = my(n, e, s);
        n = bitand(a, 0x80000000) != 0;
        e = bitand(a, 0x7f800000) >> 23;
        s = bitand(a, 0x007fffff);

        if(e != 0, s = bitor(s, 0x00800000), s = 2.0 * s);
        s = s * 2.0^(-23);
        e = e - 127;
        return((-1)^n * s * 2^(e));
}

{ flt64fromuint64(a) = my(n, e, s);
        n = bitand(a, 0x8000000000000000) != 0;
        e = bitand(a, 0x7ff0000000000000) >> 52;
        s = bitand(a, 0x000fffffffffffff);

        if(e != 0, s = bitor(s, 0x0010000000000000), s = 2.0 * s);
        s = s * 2.0^(-52);
        e = e - 1023;
        return((-1)^n * 2^(e) * s);
}
