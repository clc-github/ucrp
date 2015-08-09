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

#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <ucrp.h>

extern char *__progname;

/*
 * ucrp_mutex_init()
 *
 * returns 0 or -1 on error
 */
int
ucrp_mutex_init(ucrp_mutex_t *mutex)
{
	char *mutex_file;

	if (asprintf(&mutex_file, "/tmp/%s.XXXXXXXXXX", __progname) == -1) {
		ucrp_log(LOG_WARNING, "%s: %s\n", strerror(errno));
		return -1;
	}

	*mutex = mkstemp(mutex_file);
	if (*mutex == -1)
		ucrp_log(LOG_WARNING, "%s: %s\n", __func__, strerror(errno));

	if (unlink(mutex_file) == -1)
		ucrp_log(LOG_WARNING, "%s: %s\n", __func__, strerror(errno));

	free(mutex_file);

	return (*mutex == -1) ? -1 : 0;
}

/*
 * ucrp_mutex_lock()
 *
 * returns 0 or -1 on error
 */
int
ucrp_mutex_lock(ucrp_mutex_t *mutex)
{
        int i, ret;

        for (i = 0;; i++) {
		ret = lockf(*mutex, F_LOCK, 0);

                if (ret == 0)
                        break;  /* got the lock */
                else
                        sched_yield();
        }

	/* UCRP_DEBUG((LOG_DEBUG, "%s: lock [%d tries]\n", __func__, i)); */

	return ret;
}

/*
 * ucrp_mutex_trylock()
 *
 * returns 0 or -1 on error
 */
int
ucrp_mutex_trylock(ucrp_mutex_t *mutex)
{
	return lockf(*mutex, F_TLOCK, 0);
}

/*
 * ucrp_mutex_unlock()
 *
 * returns 0 or -1 on error
 */
int
ucrp_mutex_unlock(ucrp_mutex_t *mutex)
{
	int ret;

	ret = lockf(*mutex, F_ULOCK, 0);
	
        if (ret != 0)
                ucrp_log(LOG_ERR, "%s: %s\n", __func__, strerror(errno));
	
	/* UCRP_DEBUG((LOG_DEBUG, "%s: unlock\n", __func__)); */

	return ret;
}

#ifndef sched_yield
int
sched_yield(void)
{
	struct timespec rqtp;

        memset(&rqtp, 0, sizeof rqtp);
        rqtp.tv_nsec = 10000000;
        nanosleep(&rqtp, NULL);

	return 0;
}
#endif
