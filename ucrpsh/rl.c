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

#ifdef HAVE_READLINE

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <readline/readline.h> 
#include <readline/history.h> 

#include <ucrp.h>

#include "main.h"
#include "extern.h"
#include "termios.h"
#include "cle.h"

int cle_rl_complete(int, int);
int cle_rl_help(int, int);
int cle_rl_emenu(int, int);

static char *line = (char *)NULL;

/*
 * GNU Readline wrapper functions
 */
int
cle_rl_help(int count, int key)
{
	char *hstr;
	int len, display;

	hstr = rl_copy_text(0, rl_end);
	len = strlen(hstr);

	ucrp_mutex_lock(&ctl_mutex);
	display = ctl->display;
	ctl->prompt = 0;
	ucrp_mutex_unlock(&ctl_mutex);

	/* 
	 * need to display newline here so
	 * help does not start on the same line as
	 * our cle line
	 */
	putc('\n', stdout);

	/* send UCRP_HELP */
	ucrp_msg_help(sm, hstr);

	if (ucrp_send(server, sm) == -1) 
                exit(-1);

	termios_tx_save();
	ucrp_mutex_lock(&ctl_mutex);
	ctl->usepager = 1;
	ucrp_mutex_unlock(&ctl_mutex);
	ucrp_mutex_unlock(&termios_mutex);

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

	ucrp_mutex_lock(&termios_mutex);
	termios_tx_restore();

	ctl->helped = 0;
	ctl->usepager = 0;
	rl_on_new_line();
	ucrp_mutex_unlock(&ctl_mutex);

	return 0;
}

int
cle_rl_complete(int count, int key)
{
	char *cstr;
	int len, display;

	cstr = rl_copy_text(0, rl_end);
	len = strlen(cstr);

	ucrp_mutex_lock(&ctl_mutex);
	display = ctl->display;
	ucrp_mutex_unlock(&ctl_mutex);

	/* send UCRP_COMPLETE */
	ucrp_msg_complete(sm, cstr);

	if (ucrp_send(server, sm) == -1) 
                exit(-1);

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

	ucrp_mutex_lock(&termios_mutex);
	termios_tx_restore();

	/* I am not sure why, but rl_delete_text does not reset rl_point */
 	rl_delete_text(0, rl_end);
	rl_point = 0;
	rl_insert_text(ctl->completed_str);

	/*
	 * redisplay prompt if one or more UCRP_DISPLAY messages have
	 * been received since we sent our UCRP_COMPLETE message
	 */
	if (ctl->display != display)
		rl_on_new_line();

	ctl->completed = 0;
	ucrp_mutex_unlock(&ctl_mutex);

	return 0;
}

int
cle_rl_emenu(int count, int key)
{
	putc('\n', stdout);
	termios_tx_save();
	emenu_main();
	termios_tx_restore();
	putc('\n', stdout);
	rl_on_new_line();

	return 0;
}

void
cle_setup(void)
{
        rl_bind_key('\t', cle_rl_complete); 
        rl_bind_key('?', cle_rl_help); 
        rl_bind_key(CTRL('b'), cle_rl_emenu);
	rl_unbind_key(ESC);
	return;
}

char *
cle_getln(char *prompt)
{
	HIST_ENTRY *ch;
	int cle;
	void *pintsigh; /* previous SIGINT handler */

	if (line) {
                free(line);  
                line = (char *)NULL;  
        }  

	/* ignore SIGINT for now */
	pintsigh = signal(SIGINT, SIG_IGN);

	cle = 1;
	while (cle) {
		line = readline(prompt);  
		
		if (line && *line) {
			cle = 0;

			/*
			 * command history
			 *
			 * don't add to history if current line is
			 * equal to the last line.
			 *
			 */

			/* advance to the end of the history list */
			for (ch = current_history(); ch != NULL;
			     ch = next_history());

			ch = previous_history(); /* last history entry */

			if (ch == NULL)
				add_history(line); /* no history yet */
			else
				if (strcmp(ch->line, line) != 0)
                                        add_history(line);
		}

		/*
		 * EOF (ctrl-d or connection closed)
		 */
		if (line == NULL)
			return NULL;
	}

	/* restore SIGINT handler */
	signal(SIGINT, pintsigh);

        return line;
}

#endif /* HAVE_READLINE */
