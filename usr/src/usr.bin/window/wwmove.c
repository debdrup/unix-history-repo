#ifndef lint
static	char *sccsid = "@(#)wwmove.c	3.2 83/09/15";
#endif

#include "ww.h"

/*
 * Move a window.  Should be unattached.
 */
wwmove(w, row, col)
register struct ww *w;
{
	register dr, dc;
	register i;

	if (w->ww_forw != 0 || w->ww_back != 0)
		return;				/* sanity */

	dr = row - w->ww_w.t;
	dc = col - w->ww_w.l;

	w->ww_w.t += dr;
	w->ww_w.b += dr;
	w->ww_w.l += dc;
	w->ww_w.r += dc;

	w->ww_b.t += dr;
	w->ww_b.b += dr;
	w->ww_b.l += dc;
	w->ww_b.r += dc;

	w->ww_i.t = MAX(w->ww_w.t, 0);
	w->ww_i.b = MIN(w->ww_w.b, wwnrow);
	w->ww_i.nr = w->ww_i.b - w->ww_i.t;
	w->ww_i.l = MAX(w->ww_w.l, 0);
	w->ww_i.r = MIN(w->ww_w.r, wwncol);
	w->ww_i.nc = w->ww_i.r - w->ww_i.l;

	w->ww_cur.r += dr;
	w->ww_cur.c += dc;

	w->ww_win -= dr;
	for (i = w->ww_w.t; i < w->ww_w.b; i++)
		w->ww_win[i] -= dc;
	w->ww_cov -= dr;
	for (i = w->ww_w.t; i < w->ww_w.b; i++)
		w->ww_cov[i] -= dc;
	if (w->ww_fmap != 0) {
		w->ww_fmap -= dr;
		for (i = w->ww_w.t; i < w->ww_w.b; i++)
			w->ww_fmap[i] -= dc;
	}
	w->ww_nvis -= dr;
	w->ww_buf -= dr;
	for (i = w->ww_b.t; i < w->ww_b.b; i++)
		w->ww_buf[i] -= dc;
}
