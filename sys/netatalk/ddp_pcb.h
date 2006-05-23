/*-
 * Copyright (c) 2004 Robert N. M. Watson
 * Copyright (c) 1990,1994 Regents of The University of Michigan.
 * All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation, and that the name of The University
 * of Michigan not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. This software is supplied as is without expressed or
 * implied warranties of any kind.
 *
 * This product includes software developed by the University of
 * California, Berkeley and its contributors.
 *
 *	Research Systems Unix Group
 *	The University of Michigan
 *	c/o Wesley Craig
 *	535 W. William Street
 *	Ann Arbor, Michigan
 *	+1-313-764-2278
 *	netatalk@umich.edu
 *
 * $FreeBSD: src/sys/netatalk/ddp_pcb.h,v 1.2.2.1 2005/01/31 23:26:25 imp Exp $
 */

#ifndef _NETATALK_DDP_PCB_H_
#define	_NETATALK_DDP_PCB_H_

int	at_pcballoc(struct socket *so);
int	at_pcbconnect(struct ddpcb *ddp, struct sockaddr *addr, 
	    struct thread *td);
void	at_pcbdetach(struct socket *so, struct ddpcb *ddp);
void	at_pcbdisconnect(struct ddpcb *ddp);
int	at_pcbsetaddr(struct ddpcb *ddp, struct sockaddr *addr,
	    struct thread *td);
void	at_sockaddr(struct ddpcb *ddp, struct sockaddr **addr);

/* Lock macros for per-pcb locks. */
#define	DDP_LOCK_INIT(ddp)	mtx_init(&(ddp)->ddp_mtx, "ddp_mtx",	\
				    NULL, MTX_DEF)
#define	DDP_LOCK_DESTROY(ddp)	mtx_destroy(&(ddp)->ddp_mtx)
#define	DDP_LOCK(ddp)		mtx_lock(&(ddp)->ddp_mtx)
#define	DDP_UNLOCK(ddp)		mtx_unlock(&(ddp)->ddp_mtx)
#define	DDP_LOCK_ASSERT(ddp)	mtx_assert(&(ddp)->ddp_mtx, MA_OWNED)

/* Lock macros for global pcb list lock. */
#define	DDP_LIST_LOCK_INIT()	mtx_init(&ddp_list_mtx, "ddp_list_mtx",	\
				    NULL, MTX_DEF)
#define	DDP_LIST_LOCK_DESTROY()	mtx_destroy(&ddp_list_mtx)
#define	DDP_LIST_XLOCK()	mtx_lock(&ddp_list_mtx)
#define	DDP_LIST_XUNLOCK()	mtx_unlock(&ddp_list_mtx)
#define	DDP_LIST_XLOCK_ASSERT()	mtx_assert(&ddp_list_mtx, MA_OWNED)
#define	DDP_LIST_SLOCK()	mtx_lock(&ddp_list_mtx)
#define	DDP_LIST_SUNLOCK()	mtx_unlock(&ddp_list_mtx)
#define	DDP_LIST_SLOCK_ASSERT()	mtx_assert(&ddp_list_mtx, MA_OWNED)

#endif
