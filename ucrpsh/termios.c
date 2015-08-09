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

#include <sys/ioctl.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include <ucrp.h>

#include "main.h"
#include "extern.h"
#include "termios.h"

void  termios_save(FILE *, struct termios *);
void  termios_restore(FILE *, struct termios *);

static struct termios stdin_termios;
static struct termios stdout_termios;
static struct termios stderr_termios;

static struct termios tx_stdin_termios;
static struct termios tx_stdout_termios;
static struct termios tx_stderr_termios;

static struct termios rx_stdin_termios;
static struct termios rx_stdout_termios;
static struct termios rx_stderr_termios;

/*
 * termios_getln()
 *
 * this needs to be cleaned up, possible signal issues.
 * map ASK_* to TGETLN_* options.
 */
char *
termios_getln(char *buf, size_t bufsize, short options)
{
	struct termios t;
	int ch, bufdone;

	memset(buf, 0, bufsize);

	/* save terminal settings */
	termios_tx_save();

	/* turn off buffering & echo if needed */
	tcgetattr(fileno(stdin), &t);
	if (options & ASK_NOECHO || options & ASK_FEEDBACK)
		t.c_lflag &= ~ECHO;  /* turn off echo */

	t.c_lflag &= ~ICANON; /* turn off buffering */
	tcsetattr(fileno(stdin), TCSANOW, &t);

	bufdone = 0;
	while (bufdone < bufsize) {
		ch = fgetc(stdin);

		if (ch == EOF) {
			if (ferror(stdin))
				goto cleanup;
			
			break;
		}

		if (ch == 0x08 || ch == 0x7f) { /* bs, del */
			if (bufdone > 0) {
				bufdone--;
				if (options & ASK_FEEDBACK)
					fprintf(stdout, "\b \b");
			}
			continue;
		}

		if (ch == '\n' || ch == '\r')
			break;

		if (options & ASK_FEEDBACK)
			putc('*', stdout);

		if (isprint(ch))
		    buf[bufdone++] = (char)ch;
	}
	buf[bufdone] = '\0';

	if ((options & ASK_NOECHO || 
	     options & ASK_FEEDBACK ||
	     (options & ASK_CHAR && bufdone == bufsize))) { 
		putc('\n', stdout);
	}

	/* set back to buffered io */
 cleanup:
	termios_tx_restore();

	return buf;
}

void
termios_busy(void)
{
	struct termios t;
	struct timespec rqtp;
	int busy, pos;
	char *graphic[] = { "\b/", "\b-", "\b\\", "\b|" };

	pos = 0;

	memset(&rqtp, 0, sizeof rqtp);
	rqtp.tv_nsec = 100000000;

	ucrp_mutex_lock(&ctl_mutex);  
	busy = ctl->busy;
	ucrp_mutex_unlock(&ctl_mutex);

	/* save terminal settings */
	termios_tx_save();

	/* turn off echo && buffering */
	tcgetattr(fileno(stdin), &t);
	t.c_lflag &= ~ECHO;
	t.c_lflag &= ~ICANON;
	tcsetattr(fileno(stdin), TCSANOW, &t);

	while (busy) {

		write(fileno(stdout), graphic[pos], sizeof(char) * 2);

		if (pos == (sizeof(*graphic) - 1))
			pos = 0;
		else 
			pos++;

		/* sleep */
		nanosleep(&rqtp, NULL);

		ucrp_mutex_lock(&ctl_mutex);
		busy = ctl->busy;
		ucrp_mutex_unlock(&ctl_mutex);
	}

	write(fileno(stdout), "\b", sizeof(char));

	/* set orig terminal settings */
	termios_tx_restore();

	return;
}

/*
 * termios_setup()
 */
void
termios_setup(void)
{
	termios_save(stdin, &stdin_termios);
	termios_save(stdout, &stdout_termios);
	termios_save(stderr, &stderr_termios);

	stdin_termios.c_lflag &= ~ICANON;
	stdout_termios.c_lflag &= ~ICANON;
	stderr_termios.c_lflag &= ~ICANON;

	termios_reset();

	return;
}

/*
 * termios_tx_save()
 */
void
termios_tx_save(void)
{
	termios_save(stdin, &tx_stdin_termios);
	termios_save(stdout, &tx_stdout_termios);
	termios_save(stderr, &tx_stderr_termios);

	return;
}

/*
 * termios_rx_save()
 */
void
termios_rx_save(void)
{
	termios_save(stdin, &rx_stdin_termios);
	termios_save(stdout, &rx_stdout_termios);
	termios_save(stderr, &rx_stderr_termios);

	return;
}

/*
 * termios_tx_restore()
 */ 
void
termios_tx_restore(void)
{
	termios_restore(stdin, &tx_stdin_termios);
	termios_restore(stdout, &tx_stdout_termios);
	termios_restore(stderr, &tx_stderr_termios);

	return;
}

/*
 * termios_rx_restore()
 */ 
void
termios_rx_restore(void)
{
	termios_restore(stdin, &rx_stdin_termios);
	termios_restore(stdout, &rx_stdout_termios);
	termios_restore(stderr, &rx_stderr_termios);

	return;
}

/*
 * termios_reset()
 */ 
void
termios_reset(void)
{
	termios_restore(stdin, &stdin_termios);
	termios_restore(stdout, &stdout_termios);
	termios_restore(stderr, &stderr_termios);

	return;
}

/*
 * termios_save()
 */
void
termios_save(FILE *f, struct termios *t)
{
	if (tcgetattr(fileno(f), t) == -1)
		err(1, __func__);

	return;
}

/*
 * termios_restore()
 */
void
termios_restore(FILE *f, struct termios *t)
{
	if (tcsetattr(fileno(f), TCSADRAIN, t) == -1)
		err(1, __func__);

	return;
}

/*
 * termios_swinsz()
 */
void
termios_swinsz(unsigned short rows, unsigned short cols, 
	       unsigned short xpixel, unsigned short ypixel)
{
	struct winsize ws;

	ws.ws_row = rows;
	ws.ws_col = cols;
	ws.ws_xpixel = xpixel;
	ws.ws_ypixel = ypixel;

        if (ioctl(fileno(stdout), TIOCSWINSZ, &ws) == -1)
		ucrp_log(LOG_DEBUG, "%s: %s\n", __func__, strerror(errno));
	
	return;
}
