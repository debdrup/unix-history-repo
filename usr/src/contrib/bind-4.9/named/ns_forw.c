#if !defined(lint) && !defined(SABER)
static char sccsid[] = "@(#)ns_forw.c	4.32 (Berkeley) 3/3/91";
static char rcsid[] = "$Id: ns_forw.c,v 4.9.1.1 1993/05/02 22:43:03 vixie Rel $";
#endif /* not lint */

/*
 * ++Copyright++ 1986
 * -
 * Copyright (c) 1986 Regents of the University of California.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * -
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 * -
 * --Copyright--
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <resolv.h>
#include <stdio.h>
#include <errno.h>
#include "../conf/portability.h"
#include "../conf/options.h"
#include "ns.h"
#include "db.h"

struct	qinfo *qhead = QINFO_NULL;	/* head of allocated queries */
struct	qinfo *retryqp = QINFO_NULL;	/* list of queries to retry */
struct	fwdinfo *fwdtab;		/* list of forwarding hosts */

int	nsid;				/* next forwarded query id */
extern int forward_only;		/* you are only a slave */
extern u_short ns_port;

#ifdef SLAVE_FORWARD
extern int slave_retry;			/* retry time when a slave */
#endif

void	qfree(), qflush(), qremove(),
	schedretry(), unsched(), retry();
time_t	retrytime();
int	nslookup();

/*
 * Forward the query to get the answer since its not in the database.
 * Returns FW_OK if a request struct is allocated and the query sent.
 * Returns FW_DUP if this is a duplicate of a pending request. 
 * Returns FW_NOSERVER if there were no addresses for the nameservers.
 * Returns FW_SERVFAIL on malloc error or if asked to do something
 * dangerous, such as fwd to ourselves or fwd to the host that asked us.
 * (no action is taken on errors and qpp is not filled in.)
 */
int
ns_forw(nsp, msg, msglen, fp, qsp, dfd, qpp, dname)
	struct databuf *nsp[];
	u_char *msg;
	int msglen;
	struct sockaddr_in *fp;
	struct qstream *qsp;
	int dfd;
	struct qinfo **qpp;
	char *dname;
{
	register struct qinfo *qp;
	HEADER *hp;
	u_short id;
	int n;

#ifdef DEBUG
	if (debug >= 3)
		fprintf(ddt,"ns_forw()\n");
#endif

	/* Don't forward if we're already working on it. */
	hp = (HEADER *) msg;
	id = hp->id;
	/* Look at them all */
	for (qp = qhead; qp!=QINFO_NULL; qp = qp->q_link) {
		if (qp->q_id == id &&
		    bcmp((char *)&qp->q_from, fp, sizeof(qp->q_from)) == 0 &&
		    ((qp->q_cmsglen == 0 && qp->q_msglen == msglen &&
		     bcmp((char *)qp->q_msg+2, msg+2, msglen-2) == 0) ||
		    (qp->q_cmsglen == msglen &&
		     bcmp((char *)qp->q_cmsg+2, msg+2, msglen-2) == 0))) {
#ifdef DEBUG
			if (debug >= 3)
				fprintf(ddt,"forw: dropped DUP id=%d\n", ntohs(id));
#endif
#ifdef STATS
			stats[S_DUPQUERIES].cnt++;
#endif
			return (FW_DUP);
		}
	}

	qp = qnew();
	qp->q_from = *fp;	/* nslookup wants to know this */
	if ((n = nslookup(nsp, qp, dname, "ns_forw")) < 0) {
#ifdef DEBUG
		if (debug >= 2)
			fprintf(ddt,"forw: nslookup reports danger\n");
#endif
		qfree(qp);
		return (FW_SERVFAIL);
	} else if (n == 0 && !(forward_only && fwdtab)) {
#ifdef DEBUG
		if (debug >= 2)
			fprintf(ddt,"forw: no nameservers found\n");
#endif
		qfree(qp);
		return (FW_NOSERVER);
	}
	qp->q_stream = qsp;
	qp->q_curaddr = 0;
	qp->q_fwd = fwdtab;
	qp->q_dfd = dfd;
	qp->q_id = id;
	qp->q_expire = tt.tv_sec + RETRY_TIMEOUT*2;
	hp->id = qp->q_nsid = htons((u_short)++nsid);
	hp->ancount = 0;
	hp->nscount = 0;
	hp->arcount = 0;
	if ((qp->q_msg = malloc((unsigned)msglen)) == NULL) {
		syslog(LOG_ERR, "forw: %m");
		qfree(qp);
		return (FW_SERVFAIL);
	}
	bcopy(msg, qp->q_msg, qp->q_msglen = msglen);
	if (!qp->q_fwd) {
		hp->rd = 0;
		qp->q_addr[0].stime = tt;
	}

#ifdef SLAVE_FORWARD
	if(forward_only)
		schedretry(qp, (time_t)slave_retry);
	else
#endif /* SLAVE_FORWARD */
	schedretry(qp, qp->q_fwd ?(2*RETRYBASE) :retrytime(qp));

#ifdef DEBUG
	if (debug)
		fprintf(ddt,
		  "forw: forw -> %s %d (%d) nsid=%d id=%d %dms retry %d sec\n",
			inet_ntoa(Q_NEXTADDR(qp,0)->sin_addr),
			ds, ntohs(Q_NEXTADDR(qp,0)->sin_port),
			ntohs(qp->q_nsid), ntohs(qp->q_id),
			(qp->q_addr[0].nsdata != NULL)
				? qp->q_addr[0].nsdata->d_nstime
				: -1,
			qp->q_time - tt.tv_sec);
	if (debug >= 10)
		fp_query((char *)msg, ddt);
#endif
	if (sendto(ds, msg, msglen, 0, (struct sockaddr *)Q_NEXTADDR(qp,0),
		   sizeof(struct sockaddr_in)) < 0){
#ifdef DEBUG
		if (debug >= 5)
			fprintf(ddt,"error returning msg errno=%d\n",errno);
#endif
	}
#ifdef STATS
	stats[S_OUTPKTS].cnt++;
#endif
	if (qpp)
		*qpp = qp;
	hp->rd = 1;
	return (0);
}

/* aIsUs(addr)
 *	scan the datagramq (our list of interface addresses) for "addr"
 * returns:
 *	boolean: is "addr" on the datagramq list?
 * author:
 *	Paul Vixie (DECWRL) April 1991
 */
int
aIsUs(addr)
	struct in_addr addr;
{
	struct qdatagram *dqp;

	for (dqp = datagramq; dqp != QDATAGRAM_NULL; dqp = dqp->dq_next) {
		if (addr.s_addr == dqp->dq_addr.s_addr) {
			return 1;
		}
	}
	return 0;
}

/* haveComplained(tag1, tag2)
 *	check to see if we have complained about (tag1,tag2) recently
 *	(note that these are declared as pointers but are never deref'd)
 * returns:
 *	boolean: have we complained recently?
 * side-effects:
 *	outdated complaint records removed from our static list
 * author:
 *	Paul Vixie (DECWRL) April 1991
 */
int
haveComplained(tag1, tag2)
	char *tag1, *tag2;
{
	struct complaint {
		char *tag1, *tag2;
		time_t expire;
		struct complaint *next;
	};
	static struct complaint *List = NULL;
	struct complaint *cur, *next, *prev;
	int r = 0;

	for (cur = List, prev = NULL;  cur;  prev = cur, cur = next) {
		next = cur->next;
		if (tt.tv_sec > cur->expire) {
			if (prev)
				prev->next = next;
			else
				List = next;
			free((char*) cur);
			cur = prev;
		} else if ((tag1 == cur->tag1) && (tag2 == cur->tag2)) {
			r++;
		}
	}
	if (!r) {
		cur = (struct complaint *)malloc(sizeof(struct complaint));
		cur->tag1 = tag1;
		cur->tag2 = tag2;
		cur->expire = tt.tv_sec + INIT_REFRESH;	/* "10 minutes" */
		cur->next = NULL;
		if (prev)
			prev->next = cur;
		else
			List = cur;
	}
	return r;
}

/* void
 * nslookupComplain(sysloginfo, queryname, complaint, dname, a_rr)
 *	Issue a complaint about a dangerous situation found by nslookup().
 * params:
 *	sysloginfo is a string identifying the complainant.
 *	queryname is the domain name associated with the problem.
 *	complaint is a string describing what is wrong.
 *	dname and a_rr are the problematic other name server.
 */
static
void
nslookupComplain(sysloginfo, queryname, complaint, dname, a_rr)
	char *sysloginfo, *queryname, *complaint, *dname;
	register struct databuf *a_rr;
{
#ifdef DEBUG
	if (debug >= 2) {
		fprintf(ddt,"NS '%s' %s\n", dname, complaint);
	}
#endif
	if (sysloginfo && queryname && !haveComplained(queryname, complaint))
	{
		char buf[512];

		/* syslog only takes 5 params */
		sprintf(buf, "%s: query(%s) %s (%s:%s)",
			sysloginfo, queryname,
			complaint, dname, inet_ntoa(
			    *(struct in_addr*)a_rr->d_data
			));
		syslog(LOG_INFO, buf);
	}
}

/*
 * nslookup(nsp, qp, syslogdname, sysloginfo)
 *	Lookup the address for each nameserver in `nsp' and add it to
 * 	the list saved in the qinfo structure pointed to by `qp'.
 *	Omits information about nameservers that we shouldn't ask.
 *	Detects the following dangerous operations:
 *		One of the A records for one of the nameservers in nsp
 *		refers to the address of one of our own interfaces;
 *		One of the A records refers to the nameserver port on
 *		the host that asked us this question.
 * returns: the number of addresses added, or -1 if a dangerous operation
 *	is detected.
 * side effects:
 *	if a dangerous situation is detected and (syslogdname && sysloginfo),
 *	calls syslog.
 */
int
nslookup(nsp, qp, syslogdname, sysloginfo)
	struct databuf *nsp[];
	register struct qinfo *qp;
	char *syslogdname;
	char *sysloginfo;
{
	register struct namebuf *np;
	register struct databuf *dp, *nsdp;
	register struct qserv *qs;
	register int n, i;
	struct hashbuf *tmphtp;
	char *dname, *fname;
	int oldn, naddr, class, found_arr;
	time_t curtime;
	int qcomp();

#ifdef DEBUG
	if (debug >= 3)
		fprintf(ddt, "nslookup(nsp=x%x,qp=x%x,'%s')\n",
			nsp, qp, syslogdname);
#endif

	naddr = n = qp->q_naddr;
	curtime = (u_long) tt.tv_sec;
	while ((nsdp = *nsp++) != NULL) {
		class = nsdp->d_class;
		dname = (char *)nsdp->d_data;
#ifdef DEBUG
		if (debug >= 3)
			fprintf(ddt,"nslookup: NS %s c%d t%d (x%x)\n",
				dname, class, nsdp->d_type, nsdp->d_flags);
#endif
		/* don't put in people we have tried */
		for (i = 0; i < qp->q_nusedns; i++)
			if (qp->q_usedns[i] == nsdp) {
#ifdef DEBUG
				if (debug >= 2)
					fprintf(ddt,
						"skipping used NS w/name %s\n",
						nsdp->d_data);
#endif
				goto skipserver;
			}

		tmphtp = ((nsdp->d_flags & DB_F_HINT) ? fcachetab : hashtab);
		np = nlookup(dname, &tmphtp, &fname, 1);
		if (np == NULL || fname != dname) {
#ifdef DEBUG
			if (debug >= 3)
			    fprintf(ddt, "%s: not found %s %x\n",
				    dname, fname, np);
#endif
			continue;
		}
		found_arr = 0;
		oldn = n;
		/* look for name server addresses */
		for (dp = np->n_data;  dp != NULL;  dp = dp->d_next) {
			if (dp->d_type == T_CNAME && dp->d_class == class)
				goto skipserver;
			if (dp->d_type != T_A || dp->d_class != class)
				continue;
			/*
			 * Don't use records that may become invalid to
			 * reference later when we do the rtt computation.
			 * Never delete our safety-belt information!
			 */
			if ((dp->d_zone == 0) &&
			    (dp->d_ttl < (curtime+900)) &&
			    !(dp->d_flags & DB_F_HINT) )
		        {
#ifdef DEBUG
				if (debug >= 3)
					fprintf(ddt,"nslookup: stale entry '%s'\n",
					    np->n_dname);
#endif
				/* Cache invalidate the NS RR's */
				if (dp->d_ttl < curtime)
					delete_all(np, class, T_A);
				n = oldn;
				break;
			}

			found_arr++;
			/* don't put in duplicates */
			qs = qp->q_addr;
			for (i = 0; i < n; i++, qs++)
				if (qs->ns_addr.sin_addr.s_addr ==
				    ((struct in_addr *)dp->d_data)->s_addr)
					goto skipaddr;
			qs->ns_addr.sin_family = AF_INET;
			qs->ns_addr.sin_port = ns_port;
			qs->ns_addr.sin_addr = *(struct in_addr *)dp->d_data;
			qs->ns = nsdp;
			qs->nsdata = dp;
			qp->q_addr[n].nretry = 0;
			/*
			 * if we are being asked to fwd a query whose
			 * nameserver list includes our own name/address(es),
			 * then we have detected a lame delegation and rather
			 * than melt down the network and hose down the other
			 * servers (who will hose us in return), we'll return
			 * -1 here which will cause SERVFAIL to be sent to
			 * the client's resolver which will hopefully then
			 * shut up.
			 *
			 * (originally done in nsContainsUs by vix@dec mar92;
			 * moved into nslookup by apb@und jan1993)
			 */
			if (aIsUs(*(struct in_addr *)dp->d_data)) {
			    static char *complaint = "contains our address";
			    nslookupComplain(sysloginfo, syslogdname,
					complaint, dname, dp);
			    return -1;
			}
			/*
			 * If we want to forward to a host that asked us
			 * this question then either we or they are sick
			 * (unless they asked from some port other than
			 * their nameserver port).  (apb@und jan1993)
			 */
			if (bcmp((char *)&qp->q_from, &qs->ns_addr,
			    sizeof(qp->q_from)) == 0)
			{
			    static char *complaint = "forwarding loop";
			    nslookupComplain(sysloginfo, syslogdname,
					complaint, dname, dp);
			    return -1;
			}
			n++;
			if (n >= NSMAX)
				goto out;
	skipaddr:	;
		}
#ifdef DEBUG
		if (debug >= 3)
			fprintf(ddt,"nslookup: %d ns addrs\n", n);
#endif
		if (found_arr == 0 && qp->q_system == 0)
			(void) sysquery(dname, class, T_A);
skipserver:	;
	}
out:
#ifdef DEBUG
	if (debug >= 3)
		fprintf(ddt,"nslookup: %d ns addrs total\n", n);
#endif
	qp->q_naddr = n;
	if (n > 1) {
		qsort((char *)qp->q_addr, n, sizeof(struct qserv), qcomp);
	}
	return (n - naddr);
}

/*
 * qcomp - compare two NS addresses, and return a negative, zero, or
 *	   positive value depending on whether the first NS address is
 *	   "better than", "equally good as", or "inferior to" the second
 *	   NS address.
 *
 * How "goodness" is defined (for the purposes of this routine):
 *  - If the estimated round trip times differ by an amount deemed significant
 *    then the one with the smaller estimate is preferred; else
 *  - If we can determine which one is topologically closer then the
 *    closer one is preferred; else
 *  - The one with the smaller estimated round trip time is preferred
 *    (zero is returned if the two estimates are identical).
 *
 * How "topological closeness" is defined (for the purposes of this routine):
 *    Ideally, named could consult some magic map of the Internet and
 *    determine the length of the path to an arbitrary destination.  Sadly,
 *    no such magic map exists.  However, named does have a little bit of
 *    topological information in the form of the sortlist (which includes
 *    the directly connected subnet(s), the directly connected net(s), and
 *    any additional nets that the administrator has added using the "sortlist"
 *    directive in the bootfile.  Thus, if only one of the addresses matches
 *    something in the sortlist then it is considered to be topologically
 *    closer.  If both match, but match different entries in the sortlist,
 *    then the one that matches the entry closer to the beginning of the
 *    sorlist is considered to be topologically closer.  In all other cases,
 *    topological closeness is ignored because it's either indeterminate or
 *    equal.
 *
 * How times are compared:
 *    Both times are rounded to the closest multiple of the NOISE constant
 *    defined below and then compared.  If the rounded values are equal
 *    then the difference in the times is deemed insignificant.  Rounding
 *    is used instead of merely taking the absolute value of the difference
 *    because doing the latter would make the ordering defined by this
 *    routine be incomplete in the mathematical sense (e.g. A > B and
 *    B > C would not imply A > C).  The mathematics are important in
 *    practice to avoid core dumps in qsort().
 *
 * XXX: this doesn't solve the European root nameserver problem very well.
 * XXX: we should detect and mark as inferior nameservers that give bogus
 *      answers
 *
 * (this was originally vixie's stuff but almquist fixed fatal bugs in it
 * and wrote the above documentation)
 */

/*
 * RTT delta deemed to be significant, in milliseconds.  With the current
 * definition of RTTROUND it must be a power of 2.
 */
#define NOISE 1024		/* ~1 second */

#define sign(x) (((x) < 0) ? -1 : ((x) > 0) ? 1 : 0)
#define RTTROUND(rtt) (((rtt) + (NOISE >> 1)) & ~(NOISE - 1))

int
qcomp(qs1, qs2)
	struct qserv *qs1, *qs2;
{
	int pos1, pos2, pdiff;
	u_long rtt1, rtt2;
	long tdiff;
	extern struct netinfo *nettab;		/* sortlist */

	rtt1 = qs1->nsdata->d_nstime;
	rtt2 = qs2->nsdata->d_nstime;

#ifdef DEBUG
	if (debug >= 10) {
		fprintf(ddt, "qcomp(%s, ", inet_ntoa(qs1->ns_addr.sin_addr));
		fprintf(ddt, "%s) ", inet_ntoa(qs2->ns_addr.sin_addr));
		fprintf(ddt, "%lu (%lu) - %lu (%lu) = %lu",
			rtt1, RTTROUND(rtt1), rtt2, RTTROUND(rtt2),
			rtt1 - rtt2);
	}
#endif
	if (RTTROUND(rtt1) == RTTROUND(rtt2)) {
		pos1 = position_on_netlist(qs1->ns_addr.sin_addr, nettab);
		pos2 = position_on_netlist(qs2->ns_addr.sin_addr, nettab);
		pdiff = pos1 - pos2;
#ifdef DEBUG
		if (debug >= 10)
			fprintf(ddt, ", pos1=%d, pos2=%d\n", pos1, pos2);
#endif
		if (pdiff)
			return (pdiff);
	}
#ifdef DEBUG
	else if (debug >= 10)
		fprintf(ddt, "\n");
#endif
	tdiff = rtt1 - rtt2;
	return (sign(tdiff));
}
#undef sign
#undef RTTROUND

/*
 * Arrange that forwarded query (qp) is retried after t seconds.
 */
void
schedretry(qp, t)
	struct qinfo *qp;
	time_t t;
{
	register struct qinfo *qp1, *qp2;

#ifdef DEBUG
	if (debug > 3) {
		fprintf(ddt,"schedretry(%#x, %dsec)\n", qp, t);
		if (qp->q_time)
		    fprintf(ddt,
			    "WARNING: schedretry(%#x, %d) q_time already %d\n",
			    qp, t, qp->q_time);
	}
#endif
	t += (u_long) tt.tv_sec;
	qp->q_time = t;

	if ((qp1 = retryqp) == NULL) {
		retryqp = qp;
		qp->q_next = NULL;
		return;
	}
	if (t < qp1->q_time) {
		qp->q_next = qp1;
		retryqp = qp;
		return;
	}
	while ((qp2 = qp1->q_next) != NULL && qp2->q_time < t)
		qp1 = qp2;
	qp1->q_next = qp;
	qp->q_next = qp2;
}

/*
 * Unsched is called to remove a forwarded query entry.
 */
void
unsched(qp)
	struct qinfo *qp;
{
	register struct qinfo *np;

#ifdef DEBUG
	if (debug > 3) {
		fprintf(ddt,"unsched(%#x, %d )\n", qp, ntohs(qp->q_id));
	}
#endif
	if (retryqp == qp) {
		retryqp = qp->q_next;
	} else {
		for (np=retryqp;  np->q_next != QINFO_NULL;  np = np->q_next) {
			if (np->q_next != qp)
				continue;
			np->q_next = qp->q_next;	/* dequeue */
			break;
		}
	}
	qp->q_next = QINFO_NULL;		/* sanity check */
	qp->q_time = 0;
}

/*
 * Retry is called to retransmit query 'qp'.
 */
void
retry(qp)
	register struct qinfo *qp;
{
	register int n;
	register HEADER *hp;

#ifdef DEBUG
	if (debug > 3)
		fprintf(ddt, "retry(x%x) id=%d\n", qp, ntohs(qp->q_id));
#endif
	if((HEADER *)qp->q_msg == NULL) {		/*** XXX ***/
		qremove(qp);
		return;
	}						/*** XXX ***/

	if (qp->q_expire && (qp->q_expire < tt.tv_sec)) {
#ifdef DEBUG
		if (debug)
			fprintf(ddt,
		        "retry(x%x): expired @ %d (%d secs before now (%d))\n",
				qp, qp->q_expire, tt.tv_sec - qp->q_expire,
				tt.tv_sec);
#endif
		qremove(qp);
		return;
	}

	/* try next address */
	n = qp->q_curaddr;
	if (qp->q_fwd) {
		qp->q_fwd = qp->q_fwd->next;
		if (qp->q_fwd)
			goto found;
		/* out of forwarders, try direct queries */
	} else
		++qp->q_addr[n].nretry;
	if (!forward_only) {
		do {
			if (++n >= qp->q_naddr)
				n = 0;
			if (qp->q_addr[n].nretry < MAXRETRY)
				goto found;
		} while (n != qp->q_curaddr);
	}
	/*
	 * Give up. Can't reach destination.
	 */
	hp = (HEADER *)(qp->q_cmsg ? qp->q_cmsg : qp->q_msg);
	if (qp->q_system == PRIMING_CACHE) {
		/* Can't give up priming */
		unsched(qp);
		schedretry(qp, (time_t)60*60);	/* 1 hour */
		hp->rcode = NOERROR;	/* Lets be safe, reset the query */
		hp->qr = hp->aa = 0;
		qp->q_fwd = fwdtab;
		for (n = 0; n < qp->q_naddr; n++)
			qp->q_addr[n].nretry = 0;
		return;
	}
#ifdef DEBUG
	if (debug >= 5)
		fprintf(ddt,"give up\n");
#endif
	n = ((HEADER *)qp->q_cmsg ? qp->q_cmsglen : qp->q_msglen);
	hp->id = qp->q_id;
	hp->qr = 1;
	hp->ra = 1;
	hp->rd = 1;
	hp->rcode = SERVFAIL;
#ifdef DEBUG
	if (debug >= 10)
		fp_query(qp->q_msg, ddt);
#endif
	if (send_msg((char *)hp, n, qp)) {
#ifdef DEBUG
		if (debug)
			fprintf(ddt,"gave up retry(x%x) nsid=%d id=%d\n",
				qp, ntohs(qp->q_nsid), ntohs(qp->q_id));
#endif
	}
	qremove(qp);
#ifdef STATS
	stats[S_RESPFAIL].cnt++;
#endif
	return;

found:
	if (qp->q_fwd == 0 && qp->q_addr[n].nretry == 0)
		qp->q_addr[n].stime = tt;
	qp->q_curaddr = n;
	hp = (HEADER *)qp->q_msg;
	hp->rd = (qp->q_fwd ? 1 : 0);
#ifdef DEBUG
	if (debug)
		fprintf(ddt,
			"%s(addr=%d n=%d) -> %s %d (%d) nsid=%d id=%d %dms\n",
			(qp->q_fwd ? "reforw" : "resend"),
			n, qp->q_addr[n].nretry,
			inet_ntoa(Q_NEXTADDR(qp,n)->sin_addr),
			ds, ntohs(Q_NEXTADDR(qp,n)->sin_port),
			ntohs(qp->q_nsid), ntohs(qp->q_id),
			qp->q_addr[n].nsdata->d_nstime);
	if (debug >= 10)
		fp_query(qp->q_msg, ddt);
#endif
	/* NOSTRICT */
	if (sendto(ds, qp->q_msg, qp->q_msglen, 0,
	    (struct sockaddr *)Q_NEXTADDR(qp,n),
	    sizeof(struct sockaddr_in)) < 0)
	{
#ifdef DEBUG
		if (debug > 3)
			fprintf(ddt,"error resending msg errno=%d\n",errno);
#endif
	}
	hp->rd = 1;	/* leave set to 1 for dup detection */
#ifdef STATS
	stats[S_OUTPKTS].cnt++;
#endif
	unsched(qp);
#ifdef SLAVE_FORWARD
	if(forward_only)
		schedretry(qp, (time_t)slave_retry);
	else
#endif /* SLAVE_FORWARD */
	schedretry(qp, qp->q_fwd ? (2*RETRYBASE) : retrytime(qp));
}

/*
 * Compute retry time for the next server for a query.
 * Use a minimum time of RETRYBASE (4 sec.) or twice the estimated
 * service time; * back off exponentially on retries, but place a 45-sec.
 * ceiling on retry times for now.  (This is because we don't hold a reference
 * on servers or their addresses, and we have to finish before they time out.)
 */
time_t
retrytime(qp)
register struct qinfo *qp;
{
	time_t t;
	struct qserv *ns = &qp->q_addr[qp->q_curaddr];

#ifdef DEBUG
	if (debug > 3)
		fprintf(ddt,"retrytime: nstime %dms.\n",
		    ns->nsdata->d_nstime / 1000);
#endif
	t = (time_t) MAX(RETRYBASE, 2 * ns->nsdata->d_nstime / 1000);
	t <<= ns->nretry;
	t = MIN(t, RETRY_TIMEOUT);	/* max. retry timeout for now */
#ifdef notdef
	if (qp->q_system)
		return ((2 * t) + 5);	/* system queries can wait. */
#endif
	return (t);
}

void
qflush()
{
	while (qhead)
		qremove(qhead);
	qhead = QINFO_NULL;
}

void
qremove(qp)
	register struct qinfo *qp;
{
#ifdef DEBUG
	if(debug > 3)
		fprintf(ddt,"qremove(x%x)\n", qp);
#endif
	unsched(qp);			/* get off queue first */
	qfree(qp);
}

struct qinfo *
qfindid(id)
register u_short id;
{
	register struct qinfo *qp;

#ifdef DEBUG
	if(debug > 3)
		fprintf(ddt,"qfindid(%d)\n", ntohs(id));
#endif
	for (qp = qhead; qp!=QINFO_NULL; qp = qp->q_link) {
		if (qp->q_nsid == id)
			return(qp);
	}
#ifdef DEBUG
	if (debug >= 5)
		fprintf(ddt,"qp not found\n");
#endif
	return(NULL);
}

struct qinfo *
#ifdef DMALLOC
qnew_tagged(file, line)
	char *file;
	int line;
#else
qnew()
#endif
{
	register struct qinfo *qp;

	if ((qp = (struct qinfo *)
#ifdef DMALLOC
				  dcalloc(file, line,
#else
				  calloc(
#endif
					 1, sizeof(struct qinfo))) == NULL) {
#ifdef DEBUG
		if (debug >= 5)
			fprintf(ddt,"qnew: calloc error\n");
#endif
		syslog(LOG_ERR, "forw: %m");
		exit(12);
	}
#ifdef DEBUG
	if (debug >= 5)
		fprintf(ddt,"qnew(x%x)\n", qp);
#endif
	qp->q_link = qhead;
	qhead = qp;
	return( qp );
}

void
qfree(qp)
	struct qinfo *qp;
{
	register struct qinfo *np;

#ifdef DEBUG
	if(debug > 3)
		fprintf(ddt,"qfree( x%x )\n", qp);
	if(debug && qp->q_next)
		fprintf(ddt,"WARNING:  qfree of linked ptr x%x\n", qp);
#endif
	if (qp->q_msg)
	 	free(qp->q_msg);
 	if (qp->q_cmsg)
 		free(qp->q_cmsg);
	if( qhead == qp )  {
		qhead = qp->q_link;
	} else {
		for( np=qhead; np->q_link != QINFO_NULL; np = np->q_link )  {
			if( np->q_link != qp )  continue;
			np->q_link = qp->q_link;	/* dequeue */
			break;
		}
	}
	free((char *)qp);
}
