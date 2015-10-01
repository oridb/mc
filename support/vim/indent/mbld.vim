" Vim Indentation file
" Language: Myrddin
" Maintainer: Ori Bernstein

if exists("b:did_indent")
    finish
endif

function! s:CheckMatch(line, pats)
    for p in a:pats
        let idx = match(a:line, p)
        if idx >= 0
            return 1
        endif
    endfor
    return 0
endfunction


function! GetMbldIndent(ln)
    let ln = a:ln

    if ln == 1
        let ind = 0
    else
        let i = 1
        let prevln = ''
        while prevln =~ '^\s*$'
            let prevln = getline(ln - i)
            let ind = indent(ln - i)
            let i = i + 1
        endwhile
        echo "IND IS " ind
        let curln = getline(ln)
        let inpat = ['^\s*=\s*$']
        let level = 0
        if s:CheckMatch(prevln, inpat)
            let level = level + 1
        endif

        if match(curln, ';;\s*$') > 0
            let level = level - 1
        endif
        echo "TABSTOP IS " &tabstop
        let ind = ind + (level * &tabstop)
    endif
    return ind
endfunction

setlocal indentkeys+=,;
setlocal indentexpr=GetMbldIndent(v:lnum)
let b:did_indent = 1
