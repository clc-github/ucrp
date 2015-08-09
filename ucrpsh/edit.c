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

#ifdef HAVE_LIBEDIT
#include <err.h>
#include <histedit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include <ucrp.h>

#include "main.h"
#include "extern.h"
#include "termios.h"
#include "cle.h"

unsigned char cle_edit_complete(EditLine *, int); 
unsigned char cle_edit_help(EditLine *, int);
unsigned char cle_edit_emenu(EditLine *, int);
char *cle_edit_setprompt(EditLine *);

EditLine *el; 
History *hist; 
HistEvent hev;
int cle_cnt; 
char *cmdline = NULL;
char *setprompt;

char *
cle_edit_setprompt(EditLine *el)
{
	return setprompt;
}

unsigned char 
cle_edit_complete(EditLine *el, int foo) 
{
	const LineInfo *li = (LineInfo *)NULL;
	char *cstr;
	int len;

	li = el_line(el);
	len = li->lastchar - li->buffer;

	if ((cstr = malloc(sizeof(char) * len + 1)) == NULL)
		err(EX_OSERR, "malloc");
	memcpy(cstr, li->buffer, sizeof(char) * len);
	cstr[len] = '\0';

	/* send UCRP_COMPLETE */
	ucrp_msg_complete(sm, cstr);
	free(cstr);

	if (ucrp_send(server, sm) == -1) 
		err(EX_UNAVAILABLE, "ucrp_send");

	termios_tx_save();
	ucrp_mutex_unlock(&termios_mutex);

	/* wait for UCRP_COMPLETED message */
	for (;;) {
		ucrp_mutex_lock(&ctl_mutex);
		if (ctl->completed)
			break;

		if (ctl->exit)
			exit(1);

		ucrp_mutex_unlock(&ctl_mutex);
		sleep(1);
	}

	ctl->completed = 0;
	el_deletestr(el, len);
	el_insertstr(el, ctl->completed_str);
	ucrp_mutex_unlock(&ctl_mutex);

	ucrp_mutex_lock(&termios_mutex);
	termios_tx_restore();

        return CC_REDISPLAY; 
}

unsigned char 
cle_edit_help(EditLine *el, int foo) 
{ 
	const LineInfo *li = (LineInfo *)NULL;
	char *hstr;
	int len;

	li = el_line(el);
	len = li->lastchar - li->buffer;

	if ((hstr = malloc(sizeof(char) * len + 1)) == NULL)
		err(EX_OSERR, "malloc");
	memcpy(hstr, li->buffer, sizeof(char) * len);
	hstr[len] = '\0';

	/* 
	 * need to display newline here so help does not start on the
	 * same line as our cle line
	 *
	 * XXX - I am not sure if I want this done here or in the cli
	 *       server...
	 */
	putc('\n', stdout);

	/* send UCRP_HELP */
	ucrp_msg_help(sm, hstr);
	free(hstr);

	if (ucrp_send(server, sm) == -1) 
		err(EX_UNAVAILABLE, "ucrp_send"); 

	termios_tx_save();
	ucrp_mutex_unlock(&termios_mutex);

	ucrp_mutex_lock(&ctl_mutex);
	ctl->usepager = 1;
	ucrp_mutex_unlock(&ctl_mutex);

	/* wait for UCRP_HELPED message */
	for (;;) {
		ucrp_mutex_lock(&ctl_mutex);
		if (ctl->helped)
			break;

		if (ctl->exit)
			exit(1);

		ucrp_mutex_unlock(&ctl_mutex);
		sleep(1);
	}

	ctl->helped = 0;
	ctl->usepager = 0;
	ucrp_mutex_unlock(&ctl_mutex);

	ucrp_mutex_lock(&termios_mutex);
	termios_tx_restore();

        return CC_REDISPLAY; 
}

unsigned char 
cle_edit_emenu(EditLine *el, int foo) 
{ 
	putc('\n', stdout);
	termios_tx_save();
	emenu_main();
	termios_tx_restore();
	putc('\n', stdout);

        return CC_REDISPLAY; 
}

void 
cle_setup(void)
{
	/* init editline */
	cle_cnt = 0;

	el = el_init(__progname, stdin, stdout, stderr);

	/* setup history */
	hist = history_init();           
        history(hist, &hev, H_SETSIZE, 100); 
        el_set(el, EL_HIST, history, hist); 

	/* prompt function */
        el_set(el, EL_PROMPT, cle_edit_setprompt);

	/* default cle style */
	el_set(el, EL_EDITOR, "emacs");

	/* handle resize */
	el_set(el, EL_SIGNAL);

	/* bind keys */
	el_set(el, EL_ADDFN, "complete", "Complete command",
	       cle_edit_complete); 
        el_set(el, EL_BIND, "\t", "complete", NULL);

	el_set(el, EL_ADDFN, "help", "Help for command",
	       cle_edit_help); 
        el_set(el, EL_BIND, "?", "help", NULL);

	el_set(el, EL_ADDFN, "emenu", "Escape Menu",
	       cle_edit_emenu); 
        el_set(el, EL_BIND, "^B", "emenu", NULL);

	return;
}

char * 
cle_getln(char *prompt) 
{
	int ret;
	const char *line;

	/* set prompt; see cle_edit_setprompt() */ 
	setprompt = prompt;

	if (cmdline) { 
                free(cmdline);   
                cmdline = (char *)NULL;   
        }   
 
	for (;;) {
		line = el_gets(el, &cle_cnt);

		if (line == NULL || cle_cnt == 0)
			return NULL; /* EOF (ctrl-d or connection closed) */

                if (line[--cle_cnt] == '\n') 
                        if (cle_cnt == 0)  
                                continue; 

		/* 
		 * don't add to history if current line is equal to
		 * the last line.
		 */
		ret = history(hist, &hev, H_FIRST);
		if ((ret == -1) ||
		    (ret != -1 && strcmp(hev.str, line) != 0))
			history(hist, &hev, H_ENTER, line);

		if ((asprintf(&cmdline, "%s", line)) == -1)
			err(EX_OSERR, "asprintf");
		cmdline[cle_cnt] = '\0';
		break;
	}

	return cmdline;
}

#endif /* HAVE_LIBEDIT */
