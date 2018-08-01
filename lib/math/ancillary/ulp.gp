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
