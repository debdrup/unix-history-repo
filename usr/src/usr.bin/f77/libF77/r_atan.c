/*-
 * Copyright (c) 1980 The Regents of the University of California.
 * All rights reserved.
 *
 * This module is believed to contain source code proprietary to AT&T.
 * Use and redistribution is subject to the Berkeley Software License
 * Agreement and your Software Agreement with AT&T (Western Electric).
 */

#ifndef lint
static char sccsid[] = "@(#)r_atan.c	5.3 (Berkeley) 4/12/91";
#endif /* not lint */

float r_atan(x)
float *x;
{
double atan();
return( atan(*x) );
}
