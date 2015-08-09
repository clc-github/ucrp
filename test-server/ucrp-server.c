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
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/socket.h> 
#include <sys/wait.h>

#include <netinet/in.h>

#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include <ucrp.h>

extern char *__progname;

static void service_clients(void);
int ucrp_listen4(void);
int ucrp_listen6(void);
void ucrp_client(int);

void process_message(int, UCRP *, UCRP *);
void sig_alrm(int);
void xmit_msg(int, UCRP *);

void do_complete(int, UCRP *, UCRP *);
void do_help(int, UCRP *, UCRP *);
void do_command(int, UCRP *, UCRP *);

void do_askc(int, char **, int, UCRP *, UCRP *);
void do_aske(int, char **, int, UCRP *, UCRP *);
void do_askf(int, char **, int, UCRP *, UCRP *);
void do_askn(int, char **, int, UCRP *, UCRP *);
void do_busy(int, char **, int, UCRP *, UCRP *);
void do_exec(int, char **, int, UCRP *, UCRP *);
void do_ftp(int, char **, int, UCRP *, UCRP *);
void do_pager(int, char **, int, UCRP *, UCRP *);
void do_show(int, char **, int, UCRP *, UCRP *);
void do_term(int, char **, int, UCRP *, UCRP *);
void do_quit(int, char **, int, UCRP *, UCRP *);

void do_show_version(int, char **, int, UCRP *, UCRP *);
void do_show_time(int, char **, int, UCRP *, UCRP *);

static int display_logmsg = 0;
static int prompt = 0;

/*
 * command data
 */
char help_askc[] = "ask [char    ]";
char help_aske[] = "ask [echo    ]";
char help_askf[] = "ask [feedback]";
char help_askn[] = "ask [no echo ]";
char help_busy[] = "get busy";
char help_exec[] = "exec local process.";
char help_ftp[] = "ftp dummy messages.";
char help_pager[] = "show lots of lines";
char help_show[] = "show something";
char help_term[] = "set terminal size";
char help_quit[] = "exit out of here";
char help_cr[] = "<cr>";

typedef void (function_t)(int, char**, int, UCRP *, UCRP *);

typedef struct _cmd { 
        char *name;
        char *help;
        struct _cmd *next;
        function_t (*f);
} CMD;

CMD cmd_show[] = { 
        { "version", help_cr, NULL, do_show_version }, 
        { "time", help_cr, NULL, do_show_time }, 
};
#define CMD_SHOW_SIZE ((sizeof(cmd_show) / sizeof(cmd_show[0])))

CMD cmd_main[] = { 
        { "askc", help_askc, NULL, do_askc },
        { "aske", help_aske, NULL, do_aske },
        { "askf", help_askf, NULL, do_askf },
        { "askn", help_askn, NULL, do_askn },
        { "busy", help_busy, NULL, do_busy },
        { "exec", help_exec, NULL, do_exec },
        { "ftp", help_ftp, NULL, do_ftp },
        { "pager", help_pager, NULL, do_pager },
        { "show", help_show, cmd_show, do_show }, 
        { "term", help_term, NULL, do_term }, 
        { "quit", help_quit, NULL, do_quit}, 
};
#define CMD_MAIN_SIZE ((sizeof(cmd_main) / sizeof(cmd_main[0])))


/*
 * command functions
 */
void
do_askc(int argc, char *argv[], int s, UCRP *rm, UCRP *sm)
{
	ucrp_msg_ask(sm, ASK_CHAR, "Hack the planet? [Y/n]: ", "Y");
	xmit_msg(s, sm);

	return;
}

void
do_aske(int argc, char *argv[], int s, UCRP *rm, UCRP *sm)
{
	ucrp_msg_ask(sm, ASK_NONE, "Hack the planet? [Y/n]: ", "Y");
	xmit_msg(s, sm);

	return;
}

void
do_askf(int argc, char *argv[], int s, UCRP *rm, UCRP *sm)
{
	ucrp_msg_ask(sm, ASK_FEEDBACK, "Password: ", "");
	xmit_msg(s, sm);

	return;
}

void
do_askn(int argc, char *argv[], int s, UCRP *rm, UCRP *sm)
{
	ucrp_msg_ask(sm, ASK_NOECHO, "Password: ", "");
	xmit_msg(s, sm);

	return;
}

void
do_busy(int argc, char *argv[], int s, UCRP *rm, UCRP *sm)
{
	ucrp_msg_busy(sm);
	xmit_msg(s, sm);
	sleep(5);

	return;
}

void
do_exec(int argc, char *argv[], int s, UCRP *rm, UCRP *sm)
{
	char *exec_str;

	if ((exec_str = strstr(UCRP_PAYLOAD(rm), " ")) == NULL)
		return;

	exec_str++;
	ucrp_msg_exec(sm, exec_str);
	xmit_msg(s, sm);

	return;
}

void
do_ftp(int argc, char *argv[], int s, UCRP *rm, UCRP *sm)
{
	int i, c;

	ucrp_msg_display(sm, "Using FTP to locate remote file...\n"); 
	xmit_msg(s, sm);
	sleep(1);

	ucrp_msg_display(sm, "Preparing local system for download..\n"); 
	xmit_msg(s, sm);
	sleep(1);

	ucrp_msg_display(sm, "Downloading image file..\n"); 
	xmit_msg(s, sm);
	sleep(1);

	c = 300;
	for (i = 0; i < c; i++) {
		ucrp_msg_display(sm, "#"); 
		xmit_msg(s, sm);
		usleep(5000);
	}

	ucrp_msg_display(sm, "[OK]\n"); 
	xmit_msg(s, sm);
	sleep(1);

	ucrp_msg_display(sm, "Verifying downloaded image file...\n"); 
	xmit_msg(s, sm);
	sleep(1);

	ucrp_msg_display(sm, "Blah Blah Blah...\n"); 
	xmit_msg(s, sm);
	sleep(1);

	return;
}

void
do_pager(int argc, char *argv[], int s, UCRP *rm, UCRP *sm)
{
	int i;
	char *line, *line2;

	line = NULL;
	for (i = 0; i < 10000; i++) {
		asprintf(&line, "%-10d ooga booga\n", i);
		asprintf(&line2, "%-10d wowy zowy wowy zowy wowy zowy wowy zowy wowy zowy wowy zowy wowy zowy wowy zowy wowy zowy wowy zowy wowy zowy wowy zowy wowy zowy wowy zowy wowy zowy \n", i);
		ucrp_msg_display(sm, line); 
		ucrp_msg_display(sm, line2); 
		xmit_msg(s, sm);

		if (line)
			free(line);

		if (line2)
			free(line2);
	}

	return;
}

void
do_show(int argc, char *argv[], int s, UCRP *rm, UCRP *sm)
{
	ucrp_msg_display(sm, "Version ?.?\n"); 
	xmit_msg(s, sm);

	return;
}

void
do_term(int argc, char *argv[], int s, UCRP *rm, UCRP *sm)
{
	ucrp_msg_swinsz(sm, 30, 85, 0, 0); 
	xmit_msg(s, sm);

	return;
}

void
do_quit(int argc, char *argv[], int s, UCRP *rm, UCRP *sm)
{
	ucrp_msg_display(sm, "goodbye...\n"); 
	xmit_msg(s, sm);
	close(s);
	_exit(0);
	return;
}

void
do_show_version(int argc, char *argv[], int s, UCRP *rm, UCRP *sm)
{
	ucrp_log(LOG_NOTICE, "%s: ok\n", __func__);
	return;
}

void
do_show_time(int argc, char *argv[], int s, UCRP *rm, UCRP *sm)
{
	ucrp_log(LOG_NOTICE, "%s: ok\n", __func__);
	return;
}

/*
 *
 */
int
ucrp_listen4(void)
{
	struct sockaddr_in sin; 
	struct servent *sp; 
	int s;
	int on = 1;

	sp = getservbyname(UCRP_SERVICE, "tcp");
	if (sp == NULL) {  
                printf("%s: tcp/%s: unknown service\n",
		       __func__, UCRP_SERVICE);  
                return -1;  
        }

	memset(&sin, 0, sizeof(sin)); 
        sin.sin_family = AF_INET;  
        sin.sin_addr.s_addr = htonl(INADDR_ANY); 
        sin.sin_port = sp->s_port;

	if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1) {   
                perror(NULL);   
                return -1; 
        }

	/* socket options */ 
        if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) != 0) { 
                close(s);
                perror(__func__); 
                return -1; 
        } 
 
        if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) == -1) { 
                close(s); 
                perror(__func__);   
                return -1; 
        } 
  
        if (listen(s, 5) == -1) { 
                close(s); 
                perror(__func__); 
                return -1; 
        }

	return s;
}

/*
 *
 */
#ifndef __linux__
int
ucrp_listen6(void)
{
	struct sockaddr_in6 sin; 
	struct servent *sp; 
	int s;
	int on = 1;

	sp = getservbyname(UCRP_SERVICE, "tcp");
	if (sp == NULL) {  
                printf("%s: tcp/%s: unknown service\n",
		       __func__, UCRP_SERVICE);  
                return -1;  
        }

	memset(&sin, 0, sizeof(sin)); 
        sin.sin6_family = AF_INET6;  
        sin.sin6_addr = in6addr_any; 
        sin.sin6_port = sp->s_port;

	if ((s = socket(AF_INET6, SOCK_STREAM, 0)) == -1) {   
                perror(NULL);   
                return -1; 
        }

	/* socket options */ 
        if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) != 0) { 
                close(s);
                perror(__func__); 
                return -1; 
        } 
 
        if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) == -1) { 
                close(s); 
                perror(__func__);   
                return -1; 
        } 
  
        if (listen(s, 5) == -1) { 
                close(s); 
                perror(__func__); 
                return -1; 
        }

	return s;
}
#endif /* __linux__ */

/*
 *
 */
void sig_alrm(int s)
{
	display_logmsg = 1;
	return;
}

/*
 *
 */
void
ucrp_client(int s)
{
	struct sockaddr_storage addr;
	socklen_t addrlen = sizeof(addr);
	fd_set read_set, read_set_orig;
	int c, todo;
	pid_t pid;
	UCRP *sm, *rm;

	/* accept connection */
	c = accept(s, (struct sockaddr *)&addr, &addrlen);

	/* fork */
	if ((pid = fork()) < 0) {
		perror(__func__);
		close(c);
		return;
	} else if (pid > 0) { /* parent */
		close(c);
		return;
	} 

	/* 
	 * in child now
	 */
	ucrp_setlogprio(LOG_NOTICE);
	ucrp_setlogstream(stdout);

	if (c == -1) {
		perror(__func__);
		exit(c);
	}

	if ((sm = malloc(UCRP_MAX_MSGSIZE)) == NULL) {
		perror(__func__);
		exit(EX_UNAVAILABLE);
	}

	if ((rm = malloc(UCRP_MAX_MSGSIZE)) == NULL) {
		perror(__func__);
		exit(EX_UNAVAILABLE);
	}

	/* display something */
	ucrp_msg_display(sm, "\r\n\r\nUser Access Verification\r\n\r\n");
	xmit_msg(c, sm);

	/*
	ucrp_msg_ask(sm, ASK_NOECHO, "Password: ", "");
	xmit_msg(c, sm);
	*/

	/* XXX -- check password here */

	/* send busy */
	ucrp_msg_busy(sm);
	xmit_msg(c, sm);
	sleep(1);

	/* setup timer for UCRP_DISPLAY messages */
	signal(SIGALRM, sig_alrm);
	{
		struct itimerval it;

		memset(&it, 0, sizeof(it));
		it.it_interval.tv_sec = 60;
		it.it_value.tv_sec = 60;

		setitimer(ITIMER_REAL, &it, NULL);
	}

	/* send prompt */
	ucrp_msg_prompt(sm, "cli> ");
	xmit_msg(c, sm);

	/* service */
	FD_ZERO(&read_set_orig);
	FD_SET(c, &read_set_orig);
	for (;;) {
		ucrp_log(LOG_NOTICE, "%s: selecting...\n", __func__);
		read_set = read_set_orig;
		todo = select(c + 1, &read_set, NULL, NULL, NULL);

		if (todo == -1) {
                        perror(__func__);

			if (display_logmsg == 1) {
				ucrp_msg_display(sm, "ALERT: look at me!\n");
				xmit_msg(c, sm);
				display_logmsg = 0;
			}

			continue;
		}


		if (FD_ISSET(c, &read_set)) {
			/* process message */
                        ucrp_log(LOG_NOTICE, "%s: new message.\n", __func__);
			if (ucrp_recv(c, rm) < 1) {
				ucrp_log(LOG_NOTICE, "%s: exiting...\n",
					 __func__);
				exit(-1);
			}

			process_message(c, rm, sm);
                }

		if (prompt == 1) {
			ucrp_msg_prompt(sm, "cli> ");
			xmit_msg(c, sm);
			prompt = 0;
		}
	}

	exit(0);
}

/*
 *
 */
static void
service_clients(void)
{
	struct timeval timeout;
	fd_set read_set, read_set_orig;
	int s4, s6, maxfd, todo;

	FD_ZERO(&read_set_orig);
	maxfd = -1;

	/* setup socket */
	s4 = ucrp_listen4();
	if (s4 != -1) {
		if (s4 > maxfd)
			maxfd = s4;

		fprintf(stderr, "%s: ipv4 ready.\n", __progname);
		FD_SET(s4, &read_set_orig);
	}

#ifndef __linux__
	s6 = ucrp_listen6();
#else
	s6 = -1;
#endif /* __linux__ */

	if (s6 != -1) {
		if (s6 > maxfd)
			maxfd = s6;

		fprintf(stderr, "%s: ipv6 ready.\n", __progname);
		FD_SET(s6, &read_set_orig);
	}

	if (maxfd == -1) {
		fprintf(stderr, "%s: socket setup failed.\n", __progname);
		exit(EX_UNAVAILABLE);
	}

	for (;;) {
		timeout.tv_sec = 5; 
		timeout.tv_usec = 0;
		read_set = read_set_orig;
		todo = select(maxfd + 1, &read_set, NULL, NULL, &timeout);

		if (todo == -1)
			perror(__func__);

		/* new connection */
		if (s4 != -1 && FD_ISSET(s4, &read_set)) {
			printf("%s: ipv4 connection.\n", __progname);
			ucrp_client(s4);
		}

		if (s6 != -1 && FD_ISSET(s6, &read_set)) {
			printf("%s: ipv6 connection.\n", __progname);
			ucrp_client(s6);
		}

		/* wait for dead children */
		wait4(-1, NULL, WNOHANG, NULL);

	}

	/* NOTREACHED */
	return;
}

/*
 * 
 */
int
main(int argc, char *argv[])
{
	service_clients();
	return EX_OK;
}


void
xmit_msg(int s, UCRP *sm)
{
	int ret;

	ret = ucrp_send(s, sm);

	if (ret == -1 || ret == 0) {
		close(s);
		printf("%s: %s\n", __func__, strerror(errno));
		exit(-1);
	}

	return;
}

void
process_message(int s, UCRP *rm, UCRP *sm)
{
	UCRP_PMSG((stdout, rm));

	switch (rm->type) {
	case UCRP_COMMAND:
		do_command(s, rm, sm);
		break;
	case UCRP_COMPLETE:
		do_complete(s, rm, sm);
		break;
	case UCRP_HELP:
		do_help(s, rm, sm);
		break;
	case UCRP_INTERRUPT:
		ucrp_log(LOG_NOTICE, "%s: ignoring UCRP_INTERRUPT\n",
			 __func__);
		break;
	case UCRP_TELL:
		ucrp_pmsg(stdout, rm);
		break;
	case UCRP_SUSPEND:
		ucrp_log(LOG_NOTICE, "%s: ignoring UCRP_SUSPEND\n",
			 __func__);
		break;
	case UCRP_WAIT:
	{
		char *lp = UCRP_PAYLOAD(rm);
		fprintf(stdout, "%s: UCRP_WAIT: "
			"WAIT_SIGNAL=%d "
			"WAIT_ERROR=%d "
			"WAIT_STATUS=%s\n",
			__func__,
			rm->options & WAIT_SIGNAL ? 1 : 0,
			rm->options & WAIT_ERROR  ? 1 : 0,
			rm->options & WAIT_STATUS ? ucrp_msg_getln(&lp) :
			"N/A");
		break;
	}
	default:
		ucrp_log(LOG_NOTICE, "unknown message type=%u\n", rm->type);
		break;
	}

	return;
}

/*
 * do_complete()
 *
 */
void
do_complete(int s, UCRP *rm, UCRP *sm)
{
	ucrp_msg_completed(sm, "busy");
	xmit_msg(s, sm);
	return;
}

/*
 * do_help()
 *
 */
void
do_help(int s, UCRP *rm, UCRP *sm)
{
	char *line;
	int i, ret;

	ucrp_msg_display(sm, "\n\n");
	xmit_msg(s, sm);

	for (i = 0; i < CMD_MAIN_SIZE; i++) {
		ret = asprintf(&line, " %-10s\t%-40s\n",
			       cmd_main[i].name, cmd_main[i].help);
		if (ret == -1)
			_exit(ret);

		ucrp_msg_display(sm, line);
		xmit_msg(s, sm);

		free(line);
	}

	ucrp_msg_display(sm, "\n\n");
	xmit_msg(s, sm);

	ucrp_msg_helped(sm);
	xmit_msg(s, sm);

	return;
}

/*
 * do_command()
 *
 */
void
do_command(int s, UCRP *rm, UCRP *sm)
{
	function_t *doit;
	char *str;
	int i;

	ucrp_msg_busy(sm);
	xmit_msg(s, sm);

	str = strstr(UCRP_PAYLOAD(rm), UCRP_SEPARATOR);
	if (str == NULL) {
		ucrp_msg_display(sm, "invalid message.\n");
		xmit_msg(s, sm);
		_exit(-1);
	}
	*str = '\0';

	/* run command */
	for (i = 0; i < CMD_MAIN_SIZE; i++) {
		if (strncmp(UCRP_PAYLOAD(rm), cmd_main[i].name,
			    strlen(cmd_main[i].name)) == 0) {

			doit = cmd_main[i].f;
			doit(0, NULL, s, rm, sm);

			ucrp_msg_prompt(sm, "cli> "); 
			xmit_msg(s, sm);

			return;
		}
	}

	ucrp_msg_display(sm, "% Unknown Command\n");
	xmit_msg(s, sm);

	ucrp_msg_prompt(sm, "cli> "); 
	xmit_msg(s, sm);

	return;
}
