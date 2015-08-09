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

#include <sys/types.h>

#include <netinet/in.h>

#include <string.h>

#include <ucrp.h>

/*
 * ucrp_msg_hton()
 *
 * convert ucrp message header to network byte order
 */
void
ucrp_msg_hton(UCRP *msg)
{
	msg->type = htons(msg->type);
	msg->options = htons(msg->options);
	msg->length = htons(msg->length);

	return;
}

/*
 * ucrp_msg_ntoh()
 *
 * convert ucrp message header to host byte order
 */
void
ucrp_msg_ntoh(UCRP *msg)
{
	msg->type = ntohs(msg->type);
	msg->options = ntohs(msg->options);
	msg->length = ntohs(msg->length);

	return;
}

/*
 * ucrp_msg_getln()
 *
 * return current UCRP_SEPARATOR line.  If it is not available,
 * returns NULL.
 */
char *
ucrp_msg_getln(char **line)
{
	char *bol;

	bol = *line;
	if ((*line = strstr(bol, UCRP_SEPARATOR)) == NULL)
		return NULL;

	**line = '\0';
	*line += strlen(UCRP_SEPARATOR);
 
        return bol;
}

/*
 * UCRP servers MAY send the following message types.
 */

/*
 * ucrp_msg_ask()
 *
 * format ucrp message
 */
void
ucrp_msg_ask(UCRP *msg, uint16_t options, char *pstr, char *dstr)
{
	msg->type = UCRP_ASK; 
        msg->options = options; 
        snprintf(UCRP_PAYLOAD(msg), UCRP_MAX_PAYLOAD,
		 "%s\r\n%s\r\n", pstr, dstr); 
        msg->length = strlen(UCRP_PAYLOAD(msg)); 

	return;
}

/*
 * ucrp_msg_busy()
 *
 * format ucrp message
 */
void
ucrp_msg_busy(UCRP *msg)
{
	msg->type = UCRP_BUSY; 
        msg->options = 0; 
        msg->length = 0;

	return;
}

/*
 * ucrp_msg_completed()
 *
 * format ucrp message
 */
void
ucrp_msg_completed(UCRP *msg, char *cstr)
{
	msg->type = UCRP_COMPLETED; 
        msg->options = 0; 
        snprintf(UCRP_PAYLOAD(msg), UCRP_MAX_PAYLOAD,
		 "%s\r\n", cstr); 
        msg->length = strlen(UCRP_PAYLOAD(msg)); 

	return;
}

/*
 * ucrp_msg_display()
 *
 * format ucrp message
 */
void
ucrp_msg_display(UCRP *msg, char *fstr)
{
	msg->type = UCRP_DISPLAY; 
        msg->options = 0; 
        snprintf(UCRP_PAYLOAD(msg), UCRP_MAX_PAYLOAD, "%s", fstr); 
        msg->length = strlen(UCRP_PAYLOAD(msg)); 

	return;
}

/*
 * ucrp_msg_prompt()
 *
 * format ucrp message
 */
void
ucrp_msg_prompt(UCRP *msg, char *pstr)
{
	msg->type = UCRP_PROMPT; 
        msg->options = 0; 
        snprintf(UCRP_PAYLOAD(msg), UCRP_MAX_PAYLOAD,
		 "%s\r\n", pstr); 
        msg->length = strlen(UCRP_PAYLOAD(msg)); 

	return;
}

/*
 * ucrp_msg_helped()
 *
 * format ucrp message
 */
void
ucrp_msg_helped(UCRP *msg)
{
	msg->type = UCRP_HELPED; 
        msg->options = 0; 
        msg->length = 0;

	return;
}

/*
 * ucrp_msg_swinsz()
 *
 * format ucrp message
 */
void
ucrp_msg_swinsz(UCRP *msg, uint rows, uint cols, uint xpixel, uint ypixel)
{
	msg->type = UCRP_SWINSZ; 
        msg->options = 0; 
        snprintf(UCRP_PAYLOAD(msg), UCRP_MAX_PAYLOAD,
		 "%u\r\n%u\r\n%u\r\n%u\r\n",
		 rows, cols, xpixel, ypixel); 
        msg->length = strlen(UCRP_PAYLOAD(msg)); 

	return;
}

/*
 * ucrp_msg_exec()
 *
 * format ucrp message
 */
void
ucrp_msg_exec(UCRP *msg, char *command)
{
	msg->type = UCRP_EXEC; 
        msg->options = 0; 
        snprintf(UCRP_PAYLOAD(msg), UCRP_MAX_PAYLOAD, "%s\r\n", command); 
        msg->length = strlen(UCRP_PAYLOAD(msg)); 

	return;
}

/*
 * UCRP clients MAY send the following message types.
 */

/*
 * ucrp_msg_command()
 *
 * format ucrp message
 */
void
ucrp_msg_command(UCRP *msg, char *command)
{
	msg->type = UCRP_COMMAND; 
        msg->options = 0; 
        snprintf(UCRP_PAYLOAD(msg), UCRP_MAX_PAYLOAD,
		 "%s\r\n", command); 
        msg->length = strlen(UCRP_PAYLOAD(msg)); 

	return;
}

/*
 * ucrp_msg_complete()
 *
 * format ucrp message
 */
void
ucrp_msg_complete(UCRP *msg, char *str)
{
	msg->type = UCRP_COMPLETE; 
        msg->options = 0; 
        snprintf(UCRP_PAYLOAD(msg), UCRP_MAX_PAYLOAD,
		 "%s\r\n", str); 
        msg->length = strlen(UCRP_PAYLOAD(msg)); 

	return;
}

/*
 * ucrp_msg_help()
 *
 * format ucrp message
 */
void
ucrp_msg_help(UCRP *msg, char *str)
{
	msg->type = UCRP_HELP; 
        msg->options = 0; 
        snprintf(UCRP_PAYLOAD(msg), UCRP_MAX_PAYLOAD,
		 "%s\r\n", str); 
        msg->length = strlen(UCRP_PAYLOAD(msg)); 

	return;
}

/*
 * ucrp_msg_interrupt()
 *
 * format ucrp message
 */
void
ucrp_msg_interrupt(UCRP *msg)
{
	msg->type = UCRP_INTERRUPT; 
        msg->options = 0; 
        msg->length = 0;

	return;
}

/*
 * ucrp_msg_tell()
 *
 * format ucrp message
 */
void
ucrp_msg_tell(UCRP *msg, char *str)
{
	msg->type = UCRP_TELL; 
        msg->options = 0; 
        snprintf(UCRP_PAYLOAD(msg), UCRP_MAX_PAYLOAD,
		 "%s\r\n", str); 
        msg->length = strlen(UCRP_PAYLOAD(msg)); 

	return;
}

/*
 * ucrp_msg_suspend()
 *
 * format ucrp message
 */
void
ucrp_msg_suspend(UCRP *msg)
{
	msg->type = UCRP_SUSPEND; 
        msg->options = 0; 
        msg->length = 0;

	return;
}

/*
 * ucrp_msg_wait()
 *
 * format ucrp message
 */
void
ucrp_msg_wait(UCRP *msg, uint16_t options, int status)
{
	msg->type = UCRP_WAIT; 
        msg->options = options; 
	msg->length = 0;

	if (options & WAIT_STATUS) {
		snprintf(UCRP_PAYLOAD(msg), UCRP_MAX_PAYLOAD, 
			 "%d\r\n", status);  
		msg->length = strlen(UCRP_PAYLOAD(msg));
	}

	return;
}
