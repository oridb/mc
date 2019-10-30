" Vim Syntax file
" Language: Myrddin
" Maintainer: Ori Bernstein

if exists("b:current_syntax")
    finish
endif

if exists("myr_color_on")
  syn keyword myrConditional if elif else match
  syn keyword myrRepeat      while for
  syn keyword myrStructure   generic impl struct trait union type
  syn keyword myrItem        const var extern auto
  syn keyword myrPackage     pkg pkglocal use
  syn keyword myrControl     break continue goto
  syn keyword myrBool        true false
  syn keyword myrSizeOf      sizeof
  syn match   myrTerminate   ';;'
endif

syn region myrComment start=+/\*+ end=+\*/+ contains=myrComment
syn region myrComment start=+//+ end=+$+
syn match  myrSpecial display contained "\\\(x\x\+\|\o\{1,3}\|u{[a-zA-Z0-9_]*}\|.\|$\)"
syn match  myrFormat  display contained "{[^}]*}"
syn region myrString  start=+"+ skip=+\\"+ end=+"+ contains=myrSpecial,myrFormat extend
syn region myrChar    start=+'+ skip=+\\'+ end=+'+ contains=myrSpecial,myrFormat extend

hi def link myrComment  Comment
hi def link myrString   String
hi def link myrChar     String
hi def link myrSpecial  Special
hi def link myrFormat   Special

" Too much color makes my eyes hurt. Just highlight
" the most important and uncommon stuff.

if exists("myr_color_on")
  hi def link myrConditional Conditional
  hi def link myrRepeat      Repeat
  hi def link myrStructure   Structure
  hi def link myrItem        Constant
  hi def link myrPackage     Statement
  hi def link myrControl     Statement
  hi def link myrBool        Constant
  hi def link myrSizeOf      Identifier
  hi def link myrTerminate   SpecialChar
endif

let b:current_syntax = "myr"
