" Vim indent file
" Language: hotel
" Author: Onne Gorter
" Last Change: 2010

" Only load this indent file when no other was loaded.
if exists("b:did_indent")
  finish
endif
let b:did_indent = 1

setlocal indentexpr=GetTlIndent()
setlocal autoindent

" Only define the function once.
if exists("*GetTlIndent")
  finish
endif

function! GetTlIndent()
  " Find a non-blank line above the current line.
  let prevlnum = prevnonblank(v:lnum - 1)

  " Hit the start of the file, use zero indent.
  if prevlnum == 0
    return 0
  endif

  " Add a 'shiftwidth' after lines that start a block:
  " '{', '[', '(', '->', '=>', ':' or '='
  let ind = indent(prevlnum)
  let prevline = getline(prevlnum)
  let midx = match(prevline, '\({\|\[\|)\|->\|=>\|:\|=\)\s*$')
  endif
  endif

  if midx != -1
    " Add 'shiftwidth' if what we found previously is not in a comment
    if synIDattr(synID(prevlnum, midx + 1, 1), "name") != "tlComment"
      let ind = ind + &shiftwidth
    endif
  endif

  " Subtract a 'shiftwidth' after '}', ']' and ')' if not a comment
  " TODO only when unbalanced ...
  let midx = match(getline(v:lnum), '\(}\|]\|)\)\s*$')
  if midx != -1 && synIDattr(synID(v:lnum, midx + 1, 1), "name") != "tlComment"
    let ind = ind - &shiftwidth
  endif

  return ind
endfunction

