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

#ifndef _UCRP_H
#define _UCRP_H

#include <sys/types.h>

#include <syslog.h>
#include <stdarg.h>
#ifdef __linux__
#include <stdint.h>
#endif
#include <stdio.h>


#define UCRP_SERVICE "ucrp"
#define UCRP_SEPARATOR "\r\n"

/* server sends, client receives */
#define UCRP_ASK       100
#define      ASK_NONE      0x0
#define      ASK_NOECHO    0x1
#define      ASK_FEEDBACK  0x2
#define      ASK_CHAR      0x4
#define UCRP_BUSY      101
#define UCRP_COMPLETED 102
#define UCRP_DISPLAY   103
#define UCRP_PROMPT    104
#define UCRP_HELPED    105
#define UCRP_SWINSZ    106
#define UCRP_EXEC      107

/* client sends, server receives */
#define UCRP_COMMAND   200
#define UCRP_COMPLETE  201
#define UCRP_HELP      202
#define UCRP_INTERRUPT 203
#define UCRP_TELL      204
#define UCRP_SUSPEND   205
#define UCRP_WAIT      206
#define      WAIT_STATUS   0x1
#define      WAIT_SIGNAL   0x2
#define      WAIT_ERROR    0x4

typedef struct _ucrp {
	uint16_t type;
	uint16_t options;
	uint16_t length;
} UCRP;

typedef int ucrp_mutex_t;

#define UCRP_HDR_SIZE    sizeof(UCRP)
#define UCRP_MAX_MSGSIZE (1500 + sizeof(char)) /* don't forget a '\0' */
#define UCRP_MAX_PAYLOAD ((UCRP_MAX_MSGSIZE - sizeof(char)) - UCRP_HDR_SIZE)
#define UCRP_PAYLOAD(x) ((uint8_t *)x + UCRP_HDR_SIZE)

#define UCRP_LOG_DEFAULT LOG_WARNING

#ifndef NDEBUG
#define UCRP_DEBUG(x) ucrp_log x
#define UCRP_PMSG(x)  ucrp_pmsg x
#else
#define UCRP_DEBUG(x)
#define UCRP_PMSG(x)
#endif

__BEGIN_DECLS
int     ucrp_connect(char *, char *);
ssize_t ucrp_recv(int, UCRP *);
ssize_t ucrp_send(int, UCRP *);

/*
 * logging functions
 */
void ucrp_log(uint32_t, const char *, ...);
uint32_t ucrp_setlogprio(uint32_t);
void ucrp_setusesyslog(uint32_t);
void ucrp_setlogstream(FILE *);

/*
 * util functions
 */
void  ucrp_pmsg(FILE *, UCRP *);
char *ucrp_strtype(uint16_t);

/*
 * mutex functions
 */
int ucrp_mutex_init(ucrp_mutex_t *);
int ucrp_mutex_lock(ucrp_mutex_t *);
int ucrp_mutex_unlock(ucrp_mutex_t *);
int ucrp_mutex_trylock(ucrp_mutex_t *);

/*
 * memory map functions
 */
int ucrp_mmap(void **, size_t);
int ucrp_munmap(void *, size_t);

/*
 * message format functions
 */
void ucrp_msg_hton(UCRP *);
void ucrp_msg_ntoh(UCRP *);
char *ucrp_msg_getln(char **);

void ucrp_msg_ask(UCRP *, uint16_t, char *, char *);
void ucrp_msg_busy(UCRP *);
void ucrp_msg_completed(UCRP *, char *);
void ucrp_msg_display(UCRP *, char *);
void ucrp_msg_prompt(UCRP *, char *);
void ucrp_msg_helped(UCRP *);
void ucrp_msg_swinsz(UCRP *, uint, uint, uint, uint);
void ucrp_msg_exec(UCRP *, char *);

void ucrp_msg_command(UCRP *, char *);
void ucrp_msg_complete(UCRP *, char *);
void ucrp_msg_help(UCRP *, char *);
void ucrp_msg_interrupt(UCRP *);
void ucrp_msg_tell(UCRP *, char *);
void ucrp_msg_suspend(UCRP *);
void ucrp_msg_wait(UCRP *, uint16_t, int);
__END_DECLS

#endif /* _UCRP_H */
