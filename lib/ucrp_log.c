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

#include <stdarg.h>
#include <stdio.h>
#include <syslog.h>
#include <unistd.h>

#include <ucrp.h>

static uint32_t  logprio = UCRP_LOG_DEFAULT;
static uint32_t  usesyslog = 1;
static FILE     *logstream = NULL;

/*
 * ucrp_log()
 */
void
ucrp_log(uint32_t msgprio, const char *format, ...)
{
	va_list ap;
 
	if (msgprio <= logprio) {
		va_start(ap, format); 

		if (usesyslog)
			vsyslog(msgprio, format, ap);
		else
			if (logstream)
				vfprintf(logstream, format, ap);

		va_end(ap);
	}

	return;
}

/*
 * ucrp_setlogprio()
 *
 * set the current log prio
 *
 * returns the previous log prio
 */
uint32_t
ucrp_setlogprio(uint32_t newprio)
{
	uint32_t oldprio;

	oldprio = logprio;
	logprio = newprio;

	return oldprio;
}

/*
 * ucrp_setusesyslog()
 *
 * log using syslog or stream
 */
void
ucrp_setusesyslog(uint32_t i)
{
	usesyslog = i;
	return;
}

/*
 * ucrp_setlogstream()
 *
 * use FILE stream for logging.
 */
void
ucrp_setlogstream(FILE *stream)
{
	logstream = stream;
	return;
}

/*
 * ucrp_pmsg()
 *
 * print ucrp message to the stream 'stream'
 */
void
ucrp_pmsg(FILE *stream, UCRP *msg)
{
	if (logprio < LOG_DEBUG)
		return;

	fprintf(stream,
		"--- UCRP SOM ---\n"
		"Type    =%s\n"
		"Options =%#x\n"
		"Length  =%hu\n",
		ucrp_strtype(msg->type), msg->options, msg->length);

	if (msg->length > 0) {
		fprintf(stream, "Payload =");
		fflush(stream);
		write(fileno(stream), UCRP_PAYLOAD(msg), msg->length);
	}

	fprintf(stream, "--- UCRP EOM ---\n");
	fflush(stream);

	return;
}
