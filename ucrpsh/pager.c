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

#define PAGER_PROMPT "--More--"

#include <sys/ioctl.h>
#include <sys/types.h>

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <ucrp.h>

#include "main.h"
#include "extern.h"
#include "rx.h"
#include "termios.h"

static int session = 0;
static struct winsize ws;
static unsigned int lines_out;
static unsigned int chars_out;

/*
 * pager_reset()
 *
 * reset the pager, starting a new pager session
 *
 * returns 0 or -1 on error
 */
int
pager_reset(void)
{
	if (ioctl(fileno(stdout), TIOCGWINSZ, &ws) == -1)
		return -1;

	/* unknown terminal size */
	if (ws.ws_row == 0)
		ws.ws_row = 24;

	if (ws.ws_col == 0)
		ws.ws_col = 80;

	session = 1;

	chars_out = 0;
	lines_out = 0;

	if (ws.ws_row >= 2)
		ws.ws_row -= 2; /* correct for window size */

	if (ws.ws_col >= 2)
		ws.ws_col -= 2; /* correct for window size */

	ucrp_log(LOG_DEBUG, "%s: ws.ws_row=%u ws.ws_col=%u\n", __func__,
		 ws.ws_row, ws.ws_col);

	return 0;
}

/*
 * pager_write()
 *
 * very simple (read shity) pager.  i wanted to use 'less' but:
 *  ouput is buffered
 *  reads to much on stdin
 *  allows exiting to a shell
 *
 * input:
 *  <space> -- advance one page
 *  \n|j    -- advance one line
 *  q       -- quit (throw away input until session reset)
 *
 * returns the number of bytes displayed or -1 on error
 * (note that 0 is not an error)
 */
int
pager_write(const void *buf, size_t nbytes)
{
	int i, nl, p, pplen;
	char ch, *pprompt;

	if (session == 0)
		return 0; /* no pager session, don't display */

	pprompt = PAGER_PROMPT;
	nl = 0; /* new line */

	for (i = 0; i < nbytes; i++) {
		ch = *((char *)buf + i);

		if (write(fileno(stdout), &ch, sizeof(ch)) == -1)
			return -1;

		chars_out++;

		if (ch == '\n')
			nl = 1;
		else 
			nl = 0;

		if (nl) {
			chars_out = 0;
			lines_out++;
		} else if (chars_out > ws.ws_col) {
			chars_out = 0;
			lines_out++;
			fprintf(stdout, "\n");
		}


		if (lines_out > ws.ws_row) {
			struct termios t;
			char chin;

			/* display pager prompt */
			fprintf(stdout, pprompt);
			fflush(stdout);

			/* save terminal settings */
			termios_tx_save();

			/* turn off buffering echo if needed */
			tcgetattr(fileno(stdin), &t);
			t.c_lflag &= ~ECHO;
			t.c_lflag &= ~ICANON;
			tcsetattr(fileno(stdin), TCSANOW, &t);

			for (;;) {
				chin = fgetc(stdin);

				if (chin == '\n' || chin == 'j' || 
				    chin == '\r' || chin == 0) {
					lines_out--;
					break;
				}

				if (chin == 'q') {
					session = 0;
					/* send UCRP_INTERRUPT */
					kill(rx_getppid(), SIGINT);
					nbytes = i; /* break outer for() */
					break;
				}

				if (chin == ' ') {
					pager_reset();
					break;
				}
			}

			/* erase pager prompt */
			pplen = strlen(pprompt);

			for (p = 0; p < pplen; p++)
				fprintf(stdout, "\b \b");
				
			fflush(stdout);

			termios_tx_restore();
		}
	}

	return nbytes;
}
