" Vim syntax file
" Language: Flow (Flow Control Language)
" Maintainer: Christian Parpart
" Latest Revision: 26 May 2018

if exists("b:current_syntax")
	finish
endif

" ---------------------------------------------------------------------------------
"
" comments
syn keyword flowTodo contained TODO FIXME XXX NOTE BUG
syn match flowComment "#.*$" contains=flowTodo
syn region flowComment start="\/\*" end="\*\/" contains=flowTodo

" blocks
syn region flowBlock start="{" end="}" transparent fold

" keywords
syn keyword flowKeywords import from
syn keyword flowKeywords handler match on do extern var if then else for unless

" symbols
syn keyword flowOperator and or xor nand not shl shr in
syn match flowOperator '(\|)'
syn match flowOperator '{\|}'
syn match flowOperator '\[\|\]'
syn match flowOperator '[,;]'
syn match flowOperator '[^=<>~\$\^\/\*]\zs\(=>\|=\^\|=\$\|=\~\|==\|!=\|<=\|>=\|<\|>\|!\|=\|+\|-\|\*\*\|\*\)\ze[^=<>~\$\^\/\*]'
" FIXME the '/' operator is not mentioned in the operator list, currently, to
" properly highlight the /regex/ operator.

syn keyword flowCast bool int string

" regular expressions (/things.like.that/)
"syn match flowRegExpEscape "\\\[" contained display
syn match flowRegExpSpecial "\(\[\|?\|\]\|(\|)\|\.\|*\||\)" contained display
syn region flowRegExp matchgroup=Delimiter start="\(=\|=\~\|(\)\s*/"ms=e,me=e end="/" skip="\\/" oneline contains=flowRegExpEscape,flowRegExpSpecial
syn region flowRegExp matchgroup=Delimiter start="/"ms=e,me=e end="/" skip="\\/" oneline contains=flowRegExpEscape,flowRegExpSpecial
syn match flowRegExpGroup "$\%(0\|[1-9]\d*\)"

" numbers
syn match flowNumber /\d\+/
syn match flowNumber /\d\+.\d/
syn keyword flowNumber true false

syn match flowIPv4 /\d\{1,3}\.\d\{1,3}\.\d\{1,3}\.\d\{1,3}/

" IPv6 address
syn match flowIPv6 /[a-fA-F0-9:]*::[a-fA-F0-9:.]*/
syn match flowIPv6 /[a-fA-F0-9:]\+:[a-fA-F0-9:]\+:[a-fA-F0-9:.]\+/

" identifiers
syn match flowIdent /[a-zA-Z_][a-zA-Z0-9_.]*\([a-zA-Z0-9_.]*:\)\@!/

" units (singular)
syn keyword flowUnit bit kbit mbit gbit tbit
syn keyword flowUnit byte kbyte mbyte gbyte tbyte
syn keyword flowUnit sec min hour day week month year

" units (plural)
syn keyword flowUnit bits kbits mbits gbits tbits
syn keyword flowUnit bytes kbytes mbytes gbytes tbytes
syn keyword flowUnit secs mins hours days weeks months years

" strings
syn match flowSpecial display contained "\\\(u\x\{4}\|U\x\{8}\)"
syn match flowFormat display contained "\\\(r\|n\|t\)"
syn match flowFormat display contained "%\(\d\+\$\)\=[-+' #0*]*\(\d*\|\*\|\*\d\+\$\)\(\.\(\d*\|\*\|\*\d\+\$\)\)\=\([hlL]\|ll\)\=\([bdiuoxXDOUfeEgGcCsSpn]\|\[\^\=.[^]]*\]\)"
syn region flowString start=+L\="+ skip=+\\\\\|\\"+ end=+"+ contains=flowSpecial,flowFormat
syn region flowRawString start="'" end="'"

" ---------------------------------------------------------------------------------

let b:current_syntax = "flow"

hi def link flowIdent         Identifier
hi def link flowTodo          Todo
hi def link flowComment       Comment
hi def link flowString        Constant
hi def link flowRawString     Constant
hi def link flowNumber        Constant
hi def link flowIPv4          Constant
hi def link flowIPv6          Constant
hi def link flowRegExp        Constant
hi def link flowBlock         Statement
hi def link flowKeywords      Keyword
hi def link flowOperator      Operator
hi def link flowCast          Type
hi def link flowCoreVar       Statement
hi def link flowSpecial       Special
hi def link flowUnit          Special
hi def link flowFormat        Special
hi def link flowRegExpEscape  Special
hi def link flowRegExpSpecial Special
hi def link flowRegExpGroup   Constant

" possible link targets Todo, Comment, Constant, Type, Keyword, Statement, Special, PreProc, Identifier
