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
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <time.h>
#include <unistd.h>

#include <ucrp.h>

#include "main.h"
#include "rx.h"
#include "tx.h"
#include "termios.h"

typedef void (function_t)(void);

static void  command_string(void);
static pid_t fork_th(function_t *);
static void  usage(void);

SH_CTL *ctl;                /* shell control structure      */
ucrp_mutex_t ctl_mutex;     /* shell control structure lock */
ucrp_mutex_t termios_mutex; /* termios lock                 */
UCRP *sm = NULL;            /* send message                 */
UCRP *rm = NULL;            /* recv message                 */
int server;                 /* server socket                */
int login_shell = 0;        /* login shell ?                */

extern char *__progname;    /* from crt0.o                  */

/*
 * fork_th()
 *
 * returns pid of child thread or -1 on error
 */
static pid_t
fork_th(function_t *thp)
{
	pid_t pid;

	if ((pid = fork()) != 0) /* parent or error */
		return pid;

	/* child */
	thp();

	exit(EX_OK);

	/* NOTREACHED */
	return -1;
}

/*
 * command_string()
 *
 * we don't support command strings; display an error.
 */
static void
command_string(void)
{
	fprintf(stderr, "%s: access denied\n", __progname);
	exit(EX_USAGE);
}

/*
 * usage()
 *
 * display usage information
 */
static void
usage(void)
{
	fprintf(stderr, "usage: %s [-c command-string] [-h host] [-p port]\n",
		__progname);
	exit(EX_USAGE);
}

/*
 * main()
 */
int
main(int argc, char *argv[])
{
	extern char *optarg; 
        extern int optind; 
        int ch;
	char *nodename, *servname;

	nodename = servname = NULL;

	/* are we a login shell ? */
	if (argv[0] != NULL && strlen(argv[0]) > 1)
		if (*argv[0] == '-')
			login_shell = 1;

	/* process command line arguments */
	while ((ch = getopt(argc, argv, "c:h:p:")) != -1)
		switch (ch) {
		case 'c':
			command_string();
			/* NOTREACHED */
		case 'h':
			nodename = optarg;
			break;
		case 'p':
			servname = optarg;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	argc -= optind;
	argv += optind;

	/* make sure we are not being run from a script */
	if (isatty(fileno(stdin)) == 0)
		errx(EX_USAGE, "stdin is not a tty.");

	/* shared resources */
	if (ucrp_mutex_init(&ctl_mutex) == -1)
		err(EX_IOERR, "ctl mutex");

	if (ucrp_mutex_init(&termios_mutex) == -1)
		err(EX_IOERR, "termios mutex");

	if (ucrp_mmap((void *)&ctl, sizeof(SH_CTL)) == -1)
		err(EX_IOERR, "mmap");

	ctl->usesyslog = 1;
	ucrp_setlogstream(stdout);

	termios_setup();
	setsid(); /* we should already be session leader, this is jik */

	/* connect */
	if ((server = ucrp_connect(nodename, servname)) == -1) {
		emenu_main();
		exit(EX_UNAVAILABLE);
		/* NOTREACHED */
	}

        /* ignore SIGALRM until real handerls are setup */
	if (signal(SIGALRM, SIG_IGN) == SIG_ERR)
		err(1, "signal");

	/* start receive thread */
	fork_th(rx_main);

	/* start transmit thread */
	tx_main();

	return EX_OK;
}
