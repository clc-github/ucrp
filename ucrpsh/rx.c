/*
 * Copyright (c) 2003 Christopher L. Cousins <clc@sparf.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/time.h>
#include <sys/types.h>

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <time.h>
#include <unistd.h>

#include <ucrp.h>

#include "main.h"
#include "extern.h"
#include "termios.h"
#include "rx.h"

static void  rx_exit(int, char *);

static int pager = 0;

/*
 * rx_getppid()
 *
 */
pid_t
rx_getppid(void)
{
	pid_t ppid;

	ppid = getppid();
	if (ppid == 1)
		rx_exit(-1, "parent is init.");

	return ppid;
}

/*
 * rx_exit()
 *
 */
static void
rx_exit(int e, char *str)
{
	pid_t ppid;

	if (str == NULL)
		ucrp_log(LOG_NOTICE, "%s: reason unknown.\n", __func__);
	else
		ucrp_log(LOG_NOTICE, "%s: %s\n", __func__, str);

	ctl->exit = 1;

	if ((ppid = getppid()) != 1)
		kill(ppid, SIGTERM);

	_exit(e);
}

/*
 * rx_main()
 *
 */
void
rx_main(void)
{
	signal(SIGINT, SIG_IGN);     /* ignore crtl-c    */
	signal(SIGHUP, SIG_IGN);     /* we get them from sshd sometimes */

	if (login_shell)
		signal(SIGTSTP, SIG_IGN);    /* ignore crtl-z    */

	return rx_exit(rx_loop(), "rx_loop returned.");
}

/*
 * rx_proc_msg()
 *
 */
void
rx_proc_msg(UCRP *rm)
{
	int usepager;
	static int usesyslog;

	UCRP_PMSG((stdout, rm));

	/* clear busy flag as the ucrp server is obviously no longer busy */
	ucrp_mutex_lock(&ctl_mutex);
	ctl->busy = 0;
	usepager = ctl->usepager;
	ucrp_mutex_unlock(&ctl_mutex);

	/* check our logging level */
	if (usesyslog != ctl->usesyslog) {
		ucrp_mutex_lock(&ctl_mutex);

		usesyslog = ctl->usesyslog;
		ucrp_setusesyslog(usesyslog);
		ucrp_setlogprio(ctl->logprio);

		ucrp_mutex_unlock(&ctl_mutex);
	}

	/* setup pager session if needed */
	if (rm->type == UCRP_DISPLAY && pager == 0) {
		if (usepager) {
			ucrp_mutex_lock(&termios_mutex);
			termios_rx_save();

			if (pager_reset() == -1)
				rx_exit(-1, "pager_reset failed.");

			pager = 1;
		} else {
			pager = 0;
		}

	} else if (rm->type != UCRP_DISPLAY && pager != 0) {
		termios_rx_restore();
		ucrp_mutex_unlock(&termios_mutex);
		pager = 0;
	}

	switch (rm->type) {
	case UCRP_DISPLAY:
	{
		int i, ret;

		ucrp_mutex_lock(&ctl_mutex);
		ctl->display++;
		ucrp_mutex_unlock(&ctl_mutex);

		if (pager) {
			ret = pager_write(UCRP_PAYLOAD(rm), rm->length);
			if (ret == -1) {
				ucrp_log(LOG_ERR, "%s: %s\n",
					 __func__, strerror(errno));
				rx_exit(-1, "pager_write failed.");
			}
		} else {
			for (i = 0; i != rm->length; i += ret) {
				ret = write(fileno(stdout),
					    UCRP_PAYLOAD(rm) + i,
					    rm->length);

				if (ret == -1) {
					ucrp_log(LOG_ERR, "%s: %s\n",
						 __func__, strerror(errno));
					rx_exit(-1, "write failed.");
				}
			}
		}

	}
		break;
	case UCRP_ASK:
		ucrp_mutex_lock(&ctl_mutex);
		ctl->ask = 1;
		memcpy(ctl->am, rm, UCRP_MAX_MSGSIZE);
		ucrp_mutex_unlock(&ctl_mutex);
		break;
	case UCRP_BUSY:
		ucrp_mutex_lock(&ctl_mutex);
		ctl->busy = 1;
		ucrp_mutex_unlock(&ctl_mutex);
		break;
	case UCRP_COMPLETED:
		ucrp_mutex_lock(&ctl_mutex);
		ctl->completed = 1;
		memset(ctl->completed_str, 0, UCRP_MAX_PAYLOAD);
		memcpy(ctl->completed_str, UCRP_PAYLOAD(rm),
		       rm->length - strlen(UCRP_SEPARATOR));
		ucrp_mutex_unlock(&ctl_mutex);
		break;
	case UCRP_EXEC:
		ucrp_mutex_lock(&ctl_mutex);
		ctl->exec = 1;
		ctl->usepager = 0;
		memset(ctl->exec_str, 0, UCRP_MAX_PAYLOAD);
		memcpy(ctl->exec_str, UCRP_PAYLOAD(rm),
		       rm->length - strlen(UCRP_SEPARATOR));
		ucrp_mutex_unlock(&ctl_mutex);
		break;
	case UCRP_PROMPT:
		ucrp_mutex_lock(&ctl_mutex);
		ctl->prompt = 1;
		memset(ctl->prompt_str, 0, UCRP_MAX_PAYLOAD);
		memcpy(ctl->prompt_str, UCRP_PAYLOAD(rm), rm->length - 2);
		ucrp_mutex_unlock(&ctl_mutex);
		break;
	case UCRP_HELPED:
		ucrp_mutex_lock(&ctl_mutex);
		ctl->helped = 1;
		ucrp_mutex_unlock(&ctl_mutex);
		break;
	case UCRP_SWINSZ:
	{
		unsigned short rows, cols, xpixel, ypixel;
		char *ln, *lp;

		lp = UCRP_PAYLOAD(rm);

		if ((ln = ucrp_msg_getln(&lp)) == NULL)
			break;
		else
			rows = (unsigned short)strtol(ln, (char **)NULL, 10);

		if ((ln = ucrp_msg_getln(&lp)) == NULL)
			break;
		else
			cols = (unsigned short)strtol(ln, (char **)NULL, 10);

		if ((ln = ucrp_msg_getln(&lp)) == NULL)
			break;
		else
			xpixel = (unsigned short)strtol(ln, (char **)NULL,
							10);

		if ((ln = ucrp_msg_getln(&lp)) == NULL)
			break;
		else
			ypixel = (unsigned short)strtol(ln, (char **)NULL,
							10);

		ucrp_mutex_lock(&termios_mutex);
		termios_swinsz(rows, cols, xpixel, ypixel);
		ucrp_mutex_unlock(&termios_mutex);
	}
		break;
	default:
		ucrp_log(LOG_INFO, "%s: unknown message type=%u\n",
			 __func__, rm->type);
	}

	/* let tx thread know a new message has arrived */
	kill(rx_getppid(), SIGALRM);

	return;
}

/*
 * rx_loop() -- receive loop
 *
 */
int
rx_loop(void)
{
	fd_set read_set, read_set_orig;
	int todo;
	struct timeval timeout;

	if ((rm = malloc(UCRP_MAX_MSGSIZE)) == NULL) {
		ucrp_log(LOG_ERR, "%s: %s\n", __func__, strerror(errno));
		rx_exit(EX_UNAVAILABLE, "malloc failed.");
        }

	FD_ZERO(&read_set_orig);
	FD_SET(server, &read_set_orig);

	for (;;) {
		read_set = read_set_orig;
		timeout.tv_sec = 5;
		timeout.tv_usec = 0;
                todo = select(server + 1, &read_set, NULL, NULL, &timeout);

		if (todo == -1)
			ucrp_log(LOG_DEBUG, "%s: %s\n",
				 __func__, strerror(errno));

		if (FD_ISSET(server, &read_set)) {

                        /* process message */
                        if (ucrp_recv(server, rm) < 1) {
				ucrp_log(LOG_DEBUG, "%s: %s\n",
					 __func__, strerror(errno));
				rx_exit(EX_OK, "remote connection closed.\n");
			}

			rx_proc_msg(rm);
                }

		/* make sure our parent is alive */
		rx_getppid();

		/* exit quietly if asked to */
		if (ctl->exit != 0)
			_exit(EX_OK);
	}

	return 0;
}
