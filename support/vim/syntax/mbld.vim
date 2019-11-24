" Vim Syntax file
" Language: Myrddin Build
" Maintainer: Ori Bernstein

if exists("b:current_syntax")
    finish
endif

syn region mbldComment start=+#+ end=+$+
syn region mbldString start=+"+ skip=+\\"+ end=+"+ extend
syn keyword mbldKeyword
            \ bin
            \ lib
            \ gen
            \ test
            \ man
            \ sub

hi def link mbldComment  Comment
hi def link mbldString   String

