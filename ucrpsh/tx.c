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
#include <sys/wait.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <time.h>
#include <unistd.h>

#include <ucrp.h>

#include "main.h"
#include "cle.h"
#include "extern.h"
#include "termios.h"
#include "tx.h"

static int interrupt = 0;
static int suspend  = 0;

int  tx_loop(void);
void tx_ask(UCRP *);
void tx_busy(void);
void tx_getln(UCRP *);
void tx_interrupt(UCRP *);
void tx_suspend(UCRP *);
void tx_sighdlr(int);
void tx_checkchild(void);
void tx_exec(UCRP *);

static void  tx_exit(int, char *);

/*
 * tx_main()
 */
void
tx_main(void)
{
	/*
	 * set signal handlers
	 */
	signal(SIGALRM, tx_sighdlr);
	signal(SIGCHLD, tx_sighdlr);
	signal(SIGINT, tx_sighdlr);
	signal(SIGHUP, SIG_IGN);

	if (login_shell)
		signal(SIGTSTP, tx_sighdlr);

	return tx_exit(tx_loop(), "tx_loop returned.");
}

/*
 * tx_exit()
 *
 */
static void
tx_exit(int e, char *str)
{
	if (str == NULL)
		ucrp_log(LOG_NOTICE, "%s: reason unknown.\n", __func__);
	else
		ucrp_log(LOG_NOTICE, "%s: %s\n", __func__, str);

	ctl->exit = 1; /* tell rx thread we are done */

	/* 
	 * if our parent is init and we are the session leader, kill
	 * all process in our process group as something REALLY bad
	 * has happened.
	 */
	if (getppid() == 1) {
		ucrp_log(LOG_CRIT, "%s: parent is init.", __func__);

		if (getpid() == getpgrp()) {
			ucrp_log(LOG_CRIT,
				 "%s: sending TERM signal "
				 "to my process group.",
				 __func__);
			kill(0, SIGTERM);
		}
	}

	exit(e);
}

/*
 * tx_sighdlr()
 */
void
tx_sighdlr(int sig)
{
	switch (sig) {
	case SIGALRM:
		break;
	case SIGCHLD:
		tx_checkchild();
		break;
	case SIGINT:
		interrupt = 1;
		break;
	case SIGTSTP:
		suspend = 1;
		break;
	}

	return;
}

/*
 * tx_interrupt()
 *
 * send a UCRP_INTERRUPT message.
 */
void
tx_interrupt(UCRP *sm)
{
	ucrp_msg_interrupt(sm);
        if (ucrp_send(server, sm) == -1)  
                tx_exit(-1, "ucrp_send");  

	return;
}

/*
 * tx_suspend()
 *
 * send a UCRP_SUSPEND message.
 */
void
tx_suspend(UCRP *sm)
{
	ucrp_msg_suspend(sm);
        if (ucrp_send(server, sm) == -1)  
                tx_exit(-1, "ucrp_send");  

	return;
}

/*
 * tx_loop() -- transmit loop
 *
 * handle interrupt and suspend events
 *
 * if a UCRP_PROMPT message is received, display a prompt
 * to the user.
 *
 * if a UCRP_BUSY message is received, let the user know 
 * that the system is busy
 *
 * if the rx thread is dead, die.
 *
 * otherwise sleep
 */
int
tx_loop(void)
{
	struct timespec rqtp;

	memset(&rqtp, 0, sizeof rqtp);
        rqtp.tv_nsec = 10000000;

	if ((sm = malloc(UCRP_MAX_MSGSIZE)) == NULL) { 
		ucrp_log(LOG_ERR, "%s: %s\n", __func__, strerror(errno));
		tx_exit(EX_UNAVAILABLE, "malloc failed.");
        }

	cle_setup();

	for (;;) {

		if (interrupt) {
			tx_interrupt(sm); /* send UCRP_INTERRUPT */
			interrupt = 0;
		}

		if (suspend) {
			tx_suspend(sm);   /* send UCRP_SUSPEND */
			suspend = 0;
		}

		ucrp_mutex_lock(&ctl_mutex);
		if (ctl->busy) {
			ucrp_mutex_unlock(&ctl_mutex);
			tx_busy(); /* display busy */
			continue;
		}

		if (ctl->ask) {
			ucrp_mutex_unlock(&ctl_mutex);
			tx_ask(sm); /* ask the user a question */
			continue;
		}

		if (ctl->exec) {
			ucrp_mutex_unlock(&ctl_mutex);
			tx_exec(sm); /* exec an local file */
			continue;
		}

		if (ctl->prompt) {
			ucrp_mutex_unlock(&ctl_mutex);
			tx_getln(sm); /* get command from user */
			continue;
		}

		if (ctl->exit)
			tx_exit(EX_OK, "tx: ctl->exit is set.");

		ucrp_mutex_unlock(&ctl_mutex);

		nanosleep(&rqtp, NULL);

		/* if our parent is init, something really bad happened. */
		if (getppid() == 1)
			tx_exit(-1, "tx: parent is init.");

	}

	return 0;
}

/*
 * tx_getln()
 *
 * get a command line from the user
 */
void
tx_getln(UCRP *sm)
{
	char *prompt, *buf, *line;

	buf = NULL;
	prompt = "(?) ";

        ucrp_mutex_lock(&ctl_mutex);
	if (strlen(ctl->prompt_str) > 0) {
		if (asprintf(&buf, "%s", ctl->prompt_str) == -1)
			warn("asprintf");
		else
			prompt = buf;
	}

	/* force UCRP_DISPLAY messages to not use a pager */
	ctl->usepager = 0;
        ucrp_mutex_unlock(&ctl_mutex);

	ucrp_mutex_lock(&termios_mutex);

	if ((line = cle_getln(prompt)) == NULL)
		tx_exit(-1, "EOF on stdin");

	/* ok to use a pager again; clear prompt */
	ucrp_mutex_lock(&ctl_mutex);
	ctl->usepager = 1;
	ctl->prompt = 0; 
	ucrp_mutex_unlock(&ctl_mutex);

	/* format message */
	ucrp_msg_command(sm, line);

	ucrp_mutex_unlock(&termios_mutex);

	/* send message */
	if (ucrp_send(server, sm) == -1)
                tx_exit(-1, "ucrp_send");  

	if (buf != NULL)
		free(buf);

	return;
}

/*
 * tx_ask()
 *
 * print 'prompt [default]' to user and get a line of input.  If no
 * input from user, return 'default' ucrp server.
 */
void
tx_ask(UCRP *sm)
{
	uint8_t abuf[UCRP_MAX_MSGSIZE];
	char buf[UCRP_MAX_PAYLOAD];
	int buflen;
	UCRP *am;
	char *prompt, *prompt_default, *lp;

	am = (UCRP *)abuf;
	lp = UCRP_PAYLOAD(am);

	ucrp_mutex_lock(&ctl_mutex);
	ctl->ask = 0;
	memcpy(am, ctl->am, UCRP_MAX_MSGSIZE);
	ucrp_mutex_unlock(&ctl_mutex);

	prompt = ucrp_msg_getln(&lp);
	prompt_default = ucrp_msg_getln(&lp);

	if (prompt != NULL) {
		fprintf(stdout, "%s", prompt);
		fflush(stdout);
	}

	/* get users response */
	ucrp_mutex_lock(&termios_mutex);
	termios_getln(buf, am->options & ASK_CHAR ? 1 : UCRP_MAX_PAYLOAD,
		      am->options);
	ucrp_mutex_unlock(&termios_mutex);

	buflen = strlen(buf);

	if (buflen == 0)
		ucrp_msg_tell(sm, prompt_default);
	else 
		ucrp_msg_tell(sm, buf);
	
	if (ucrp_send(server, sm) == -1) 
                tx_exit(-1, "ucrp_send");  

	return;
}

/*
 * tx_busy()
 * write spin loop to stdout
 */
void
tx_busy(void)
{
	ucrp_mutex_lock(&termios_mutex);
	termios_busy();
	ucrp_mutex_unlock(&termios_mutex);
	return;
}

/* must be async-signal safe */
void
tx_checkchild(void)
{
	int status, options;
        pid_t pid;

	status = 0;
        options = WNOHANG;

	if ((pid = waitpid(-1, &status, options)) <= 0)
		return;

	if (WIFEXITED(status))
		ctl->exit = 1; /* rx thread has exited */
	
	if (WIFSIGNALED(status))
		ctl->exit = 1; /* rx thread died by signal */

	return;
}

void
tx_exec(UCRP *sm)
{
        pid_t pid;
        int status = 0;
        sigset_t nmask, omask;
        char *argp[] = { "sh", "-c", NULL, NULL };

        ucrp_mutex_lock(&ctl_mutex); 
        ctl->exec = 0;
        ctl->usepager = 0;  /* should be off already */
	if (asprintf(&argp[2], "exec %s", ctl->exec_str) == -1)
		tx_exit(-1, "asprintf");

	sigemptyset(&nmask);
	sigaddset(&nmask, SIGCHLD);
	sigaddset(&nmask, SIGINT);
	sigaddset(&nmask, SIGQUIT);
	sigprocmask(SIG_BLOCK, &nmask, &omask);

	switch (pid = fork()) {
	case -1:
		ucrp_log(LOG_NOTICE, "%s: %s\n", __func__, strerror(errno));
		ucrp_msg_wait(sm, WAIT_ERROR, 0);
		break;
	case 0:
		execv(_PATH_BSHELL, argp);
		tx_exit(127, "execv");
		/* NOTREACHED */
	default:
		if ((pid = waitpid(pid, &status, 0)) < 1) {
			ucrp_log(LOG_ERR, "%s: waitpid: %s\n", __func__,
				 strerror(errno));
			tx_exit(-1, "waitpid");
		}

		if (WIFEXITED(status))
			ucrp_msg_wait(sm, WAIT_STATUS, WEXITSTATUS(status));
		else if (WIFSIGNALED(status))
			ucrp_msg_wait(sm, WAIT_SIGNAL, 0);
		else
			tx_exit(-1, "exit status");
	}

	sigprocmask(SIG_SETMASK, &omask, NULL);

	free(argp[2]);

	if (ucrp_send(server, sm) == -1)  
                tx_exit(-1, "ucrp_send");

        ucrp_mutex_unlock(&ctl_mutex); 

	/* UCRP_DISPLAY messages will appear now, but the
	   pager is still off.  It will get turned on again
	   after the next UCRP_PROMPT is done */

	return;
}
