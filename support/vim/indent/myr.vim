" Vim Indentation file
" Language: Myrddin
" Maintainer: Ori Bernstein

if exists("b:did_indent")
    finish
endif

function! s:CountMatches(line, pats)
    let matches = 0
    for p in a:pats
        let idx = 0
        while idx >= 0
            let idx = match(a:line, p, idx)
            if idx >= 0
                let matches = matches + 1
                let idx = idx + strlen(p)
            endif
        endwhile
    endfor
    return matches
endfunction

function! s:LineMatch(line, pats)
    for p in a:pats
        let pat = '^\s*'.p.'\s*$'
        if match(a:line, pat, 0) >= 0
            return 1
        endif
    endfor
    return 0
endfunction

function! GetMyrIndent(ln)
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

        let curln = getline(ln)

        let inpat = ['\<if\>', '\<elif\>', '\<else\>',
                \    '\<while\>','\<for\>', '\<match\>',
                \    '\<struct\>', '\<union\>',
                \    '{', '^\s*|', '=\s*$']
        let outpat = ['}', ';;']
        let outalone = ['\<else\>', '\<elif\>', '}', ';;', '|.*']
        let width = &tabstop

        let n_in = s:CountMatches(prevln, inpat)
        let n_out = s:CountMatches(prevln, outpat)
        if s:LineMatch(curln, outalone) != 0
            let ind = n_in * &tabstop + ind - &tabstop
        " avoid double-counting outdents from outalones.
        elseif s:LineMatch(prevln, outpat) == 0
            let ind = ind + (n_in - n_out) * &tabstop
        endif
    endif
    return ind
endfunction

setlocal indentkeys+=,;\|,=elif
setlocal indentexpr=GetMyrIndent(v:lnum)

let b:did_indent = 1
