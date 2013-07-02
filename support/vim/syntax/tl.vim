" Vim syntax file
" Language: Hotel
" Author: Onne Gorter
" Last Change: 2013

if version < 600
  syntax clear
elseif exists("b:current_syntax")
  finish
endif

syn case match
syn sync minlines=50

syn match  tlTodo "[tT][oO][dD][oO]" contained
syn match  tlLineComment "//.*" contains=tlTodo
syn region tlComment start="/\*" end="\*/" contains=tlTodo

syn match tlText "#\w\+"
syn region tlText start=+"+ skip=+\\"+ end=+"+ contains=tlTextEsc
syn match  tlTextEsc "&\d\+;" contained
syn match  tlTextEsc "&0x\x\+;" contained
syn match  tlTextEsc "\\[nrfvb\\\"]" contained
syn match  tlTextEsc "$\w\+" contained
syn match  tlTextEsc "$(.*)" contained

syn match tlField "\\\w\+"

syn match tlSpecial "="
syn match tlSpecial ":"
syn match tlSpecial "|"
syn match tlSpecial "=>"
syn match tlSpecial "->"
syn keyword tlBool     undefined null true false
syn keyword tlOperator and or xor not
syn keyword tlBuildin  print assert if while loop catch break continue return goto

hi link tlTodo        Todo
hi link tlLineComment Comment
hi link tlComment     Comment

hi link tlSymbol   String
hi link tlText     String
hi link tlTextEsc  Special

hi link tlArg      Identifier
hi link tlDef      Special
hi link tlField    Identifier

hi link tlOperator Keyword
hi link tlSpecial  Special
hi link tlBool     Boolean
hi link tlBuildin  Conditional

let b:current_syntax = "tl"

