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

#ifndef LOGINPATH
#define LOGINPATH "/usr/bin/login"
#endif

#include <sys/types.h>
#include <sys/wait.h>

#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <ucrp.h>

#include "main.h"
#include "extern.h"
#include "termios.h"

/*
 * emenu_main() -- escape menu
 *
 * NOTE: can only be called by the tx thread.
 */
void
emenu_main(void)
{
	int ret;
	char ch;

	fprintf(stdout,
		"\n"
		"Supported escape menu options:\n"
		".  - terminate connection\n"
		"d  - turn on/off debug output\n"
		"l  - exec /bin/login\n"
		"\n? ");

	fflush(stdout);

	/* get command */
	ret = read(fileno(stdin), &ch, sizeof(ch));
	if (ret == sizeof(ch)) {
		fprintf(stdout, "\n");
		fflush(stdout);

		switch (ch) {
		case '.':
			raise(SIGTERM);
			break;
		case 'd':
			if (ucrp_setlogprio(LOG_DEBUG) == LOG_DEBUG) {
				/* turn off debug */
				ucrp_setlogprio(LOG_EMERG);
				ucrp_setusesyslog(1);

				ucrp_mutex_lock(&ctl_mutex);
				ctl->usesyslog = 1;
				ctl->logprio = UCRP_LOG_DEFAULT;
				ucrp_mutex_unlock(&ctl_mutex);
			} else {
				/* turn on debug */
				ucrp_setusesyslog(0);

				ucrp_mutex_lock(&ctl_mutex);
				ctl->usesyslog = 0;
				ctl->logprio = LOG_DEBUG;
				ucrp_mutex_unlock(&ctl_mutex);
			}
				
			break;
		case 'l':
			/* tell the rx thread to exit quietly */
			ctl->exit = 1; 

			/* reset console settings */
			termios_reset();

			execl(LOGINPATH, "login", (char *)NULL);
			err(1, "execl");
			/* NOTREAHCED */
			break;
		}
		write(fileno(stdout), &ch, sizeof(ch));
	}

	return;
}
