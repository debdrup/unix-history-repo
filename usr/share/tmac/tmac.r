.\" Copyright (c) 1979, 1993
.\"	The Regents of the University of California.  All rights reserved.
.\"
.\" This module is believed to contain source code proprietary to AT&T.
.\" Use and redistribution is subject to the Berkeley Software License
.\" Agreement and your Software Agreement with AT&T (Western Electric).
.\"
.\"	@(#)tmac.r	8.1 (Berkeley) 6/8/93
.\"

.de HD
.ps 10
.ft 1
.if t .tl '\(rn'''
.if t 'sp  \\n(m1-1
.if n 'sp \\n(m1
.if e .1e
.if o .1o
.ps
.ft
'sp \\n(m2
.if \\n(:n .nm 1 1 2
.ns
..
.wh 0 HD
.de FT
'sp \\n(m3
.ps 10
.ft 1
.if e .2e
.if o .2o
.ps
.ft
'bp
..
.wh -1i FT
.de m1
.nr m1 \\$1
..
.de m2
.nr m2 \\$1
..
.de m3
.nr m3 \\$1
.ch FT -\\n(m3-\\n(m4
..
.de m4
.nr m4 \\$1
.ch FT -\\n(m3-\\n(m4
..
.m1 3
.m2 2
.m3 2
.m4 3
.de he
.de 1e
.tl \\$1 \\$2 \\$3 \\$4 \\$5 \\$6 \\$7 \\$8 \\$9
\\..
.de 1o
.tl \\$1 \\$2 \\$3 \\$4 \\$5 \\$6 \\$7 \\$8 \\$9
\\..
..
.de fo
.de 2e
.tl \\$1 \\$2 \\$3 \\$4 \\$5 \\$6 \\$7 \\$8 \\$9
\\..
.de 2o
.tl \\$1 \\$2 \\$3 \\$4 \\$5 \\$6 \\$7 \\$8 \\$9
\\..
..
.de eh
.de 1e
.tl \\$1 \\$2 \\$3 \\$4 \\$5 \\$6 \\$7 \\$8 \\$9
\\..
..
.de oh
.de 1o
.tl \\$1 \\$2 \\$3 \\$4 \\$5 \\$6 \\$7 \\$8 \\$9
\\..
..
.de ef
.de 2e
.tl \\$1 \\$2 \\$3 \\$4 \\$5 \\$6 \\$7 \\$8 \\$9
\\..
..
.de of
.de 2o
.tl \\$1 \\$2 \\$3 \\$4 \\$5 \\$6 \\$7 \\$8 \\$9
\\..
..
.he ''''
.fo ''''
.de bl
.rs
.sp \\$1
..
.de n1
.n2 \\$1
.nr :n 0
.if \\n(.$ .nr :n 1
..
.de n2
.if \\n(.$ .if \\$1=0 .nm
.if \\n(.$ .if !\\$1=0 .nm \\$1 1 2
.if !\\n(.$ .nm 1 1 2
..
.rn ds :d
.de ds
.if \\n(.$ .:d \\$1 "\\$2\\$3\\$4\\$5\\$6\\$7\\$8\\$9
.if !\\n(.$ .ls 2
..
.de ss
.ls 1
..
.de EQ
.nf
.sp
..
.de EN
.sp
.fi
..
