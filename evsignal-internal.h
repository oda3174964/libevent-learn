/*
 * Copyright 2000-2007 Niels Provos <provos@citi.umich.edu>
 * Copyright 2007-2012 Niels Provos and Nick Mathewson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef EVSIGNAL_INTERNAL_H_INCLUDED_
#define EVSIGNAL_INTERNAL_H_INCLUDED_

#ifndef evutil_socket_t
#include "event2/util.h"
#endif
#include <signal.h>

typedef void (*ev_sighandler_t)(int);

/* Data structure for the default signal-handling implementation in signal.c
 */
struct evsig_info {
	/* Event watching ev_signal_pair[1] */
	//用于监听socketpair读端的event. ev_signal_pair[1]为读端
	struct event ev_signal;
	/* Socketpair used to send notifications from the signal handler */
	evutil_socket_t ev_signal_pair[2];
	/* True iff we've added the ev_signal event yet. */
	//用来标志是否已经将ev_signal这个event加入到event_base中了
	int ev_signal_added;
	/* Count of the number of signals we're currently watching. */
	//用户一共要监听多少个信号
	int ev_n_signals_added;

	/* Array of previous signal handler objects before Libevent started
	 * messing with them.  Used to restore old signal handlers. */
	//数组。用户可能已经设置过某个信号的信号捕抓函数。但
	//Libevent还是要为这个信号设置另外一个信号捕抓函数，
	//此时，就要保存用户之前设置的信号捕抓函数。当用户不要
	//监听这个信号时，就能够恢复用户之前的捕抓函数。
	//因为是有多个信号，所以得用一个数组保存
#ifdef EVENT__HAVE_SIGACTION
	struct sigaction **sh_old;
#else
	ev_sighandler_t **sh_old; //保存的是捕抓函数的函数指针，又因为是数组。所以是二级指针
#endif
	/* Size of sh_old. */
	int sh_old_max;
};
int evsig_init_(struct event_base *);
void evsig_dealloc_(struct event_base *);

void evsig_set_base_(struct event_base *base);
void evsig_free_globals_(void);

#endif /* EVSIGNAL_INTERNAL_H_INCLUDED_ */
