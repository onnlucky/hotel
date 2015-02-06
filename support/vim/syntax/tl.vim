" Vim syntax file
" Language: Hotel
" Author: Onne Gorter
" Last Change: 2015

if version < 600
  syntax clear
elseif exists("b:current_syntax")
  finish
endif

syn case match
syn sync minlines=50

syn match  tlTodo "[tT][oO][dD][oO]" contained
syn match  tlLineComment "#[^#].*" contains=tlTodo
syn region tlComment start="^\s*###" end="^\s*###" contains=tlTodo
syn region tlNestingComment start="(#" end="#)" contains=tlTodo,tlNestingComment

syntax region tlString start=/"/ skip=/\\\\\|\\"/ end=/"/ contains=@tlInterpString
syntax region tlInterpolation matchgroup=tlInterpDelim start=/\$(/ end=/)/ contained contains=TOP
syntax match tlInterpolation /\$\w\+/
syntax match tlEscape /\\\d\d\d\|\\x\x\{2\}\|\\u\x\{4\}\|\\./ contained
syntax cluster tlSimpleString contains=@Spell,tlEscape
syntax cluster tlInterpString contains=@tlSimpleString,tlInterpolation

syn match tlSpecial "="
syn match tlSpecial ":"
syn match tlSpecial "->"
syn keyword tlBool undefined null true false
syn keyword tlOperator and or xor not
syn keyword tlBuildin print assert if while loop catch break continue return goto throw

syntax match tlDot /\.\@<!\.\i\+/ transparent contains=ALLBUT,tlBuildin,tlOperator,tlBool

hi link tlTodo Todo
hi link tlLineComment Comment
hi link tlComment Comment
hi link tlNestingComment Comment

hi link tlString   String
hi link tlInterpDelim Delimiter
hi link tlEscape SpecialChar

hi link tlOperator Keyword
hi link tlSpecial  Special
hi link tlBool     Boolean
hi link tlBuildin  Conditional

let b:current_syntax = "tl"

