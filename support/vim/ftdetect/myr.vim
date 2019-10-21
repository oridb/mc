au BufRead,BufNewFile *.myr
            \ setlocal ft=myr |
            \ setlocal errorformat=%f:%l:\ %m,%D%f/%[a-z]%#...,%Dproject\ base\ %f:
