" Vim Indentation file
" Language: Myrddin
" Maintainer: Ori Bernstein

if exists("b:did_indent")
    finish
endif

function! Quoted(lnum, col)
    let stk = synstack(a:lnum, a:col)
    for id in stk
        let a = synIDattr(id, "name") 
        if a == "myrComment" || a == "myrString" || a == "myrChar"
            return 1
        endif
    endfor
    return 0
endfunction

function! s:CountMatches(line, lnum, pats)
    let matches = 0
    for p in a:pats
        let idx = 0
        while idx >= 0
            let idx = match(a:line, p, idx)
            if idx >= 0
                let ic = Quoted(a:lnum, idx+1)
                if !ic
                    let matches = matches + 1
                endif
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
        let i = i - 1

        let curln = getline(ln)

        let inpat = ['\<if\>', '\<elif\>', '\<else\>',
                \    '\<while\>','\<for\>', '\<match\>',
                \    '\<struct\>', '\<union\>',
                \    '{', '\[', '(', '^\s*|.*:', '=\s*$']
        let outpat = ['}', ']', ')']
        let outalone = ['\<else\>', '\<elif\>.*', ';;', '|.*',
                    \ '].*', '}.*']
        let width = &tabstop

        let n_in = s:CountMatches(prevln, ln - i, inpat)
        let n_out = s:CountMatches(prevln, ln - i, outpat)

        " indent escaped line endings
        if prevln =~ '\\\s*$' && getline(ln - i - 1) !~ '\\\s*$'
            let n_in = n_in + 1
        endif

        " if we break the set of indented line endings, outdent
        if getline(ln - i - 1) =~ '\\\s*$' && getline(ln - i) !~ '\\\s*$'
            let n_out = n_out + 1
        endif

        " we already outdented if we had an outalone, but since all
        " outpats are also outalones, we don't want to double count.
        if s:LineMatch(prevln, outalone) != 0 && n_out > 0
            let n_out = n_out - 1
        endif
        let n_out = n_out + s:LineMatch(curln, outalone)

        let change = 0
        if n_in - n_out > 0
            let change = 1
        elseif n_in - n_out < 0
            let change = -1
        endif
        let ind = ind + change * &tabstop
    endif
    return ind
endfunction

setlocal indentkeys+=,;\|],=elif
setlocal indentexpr=GetMyrIndent(v:lnum)

let b:did_indent = 1
