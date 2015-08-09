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
#include <sys/socket.h>

#include <errno.h> 
#include <stdio.h> 
#include <string.h>

#include <ucrp.h>

/*
 * ucrp_recv()
 *
 * returns the number of bytes received or -1 on error.
 */
ssize_t
ucrp_recv(int s, UCRP *msg)
{
	ssize_t ret;
	uint32_t todo, done;

	memset(msg, 0, UCRP_MAX_MSGSIZE);

	/* read headers */
	done = 0;
	todo = UCRP_HDR_SIZE;
	while (done != todo) {
		ret = recv(s, (uint8_t *)msg + done, todo - done, 0);
		UCRP_DEBUG((LOG_DEBUG, "%s: ret=%d todo=%u done=%u\n",
			    __func__, ret, todo, done));

		if (ret < 1)
			return ret;

		done += ret;
	}

	ucrp_msg_ntoh(msg);

	/* check payload size */
	if (msg->length > UCRP_MAX_PAYLOAD) {
		ucrp_log(LOG_NOTICE, 
			 "%s: msg->length=%hu > UCRP_MAX_PAYLOAD=%hu\n", 
			 __func__, msg->length, UCRP_MAX_PAYLOAD);
		errno = EINVAL;
		return -1;
	}

	/* read payload */
	todo += msg->length;
	while (done != todo) {
		ret = recv(s, (uint8_t *)msg + done, todo - done, 0);
		UCRP_DEBUG((LOG_DEBUG, "%s: ret=%d todo=%u done=%u\n",
			    __func__, ret, todo, done));

		if (ret < 1)
			return ret;

		done += ret;
	}
	
	UCRP_DEBUG((LOG_DEBUG, "%s: todo=%u done=%u\n", __func__, todo, done));

	return done;
}
