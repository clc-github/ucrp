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
#include <sys/mman.h>

#include <errno.h>
#include <string.h>

#include <ucrp.h>

/*
 * ucrp_mmap()
 *
 * create an anonymous shared memory map of size len
 *
 * returns 0 or -1 on error
 */
int
ucrp_mmap(void **map, size_t len)
{
	*map = mmap(0, len, PROT_READ|PROT_WRITE, MAP_ANON|MAP_SHARED, -1, 0);

	if (*map == MAP_FAILED) {
		ucrp_log(LOG_WARNING, "%s: %s\n", __func__, strerror(errno));
		return -1;
	}

	memset(*map, 0, len);

	return 0;
}

/*
 * ucrp_munmap()
 *
 * wrapper around munmap()
 *
 * returns 0 or -1 on error
 */
int
ucrp_munmap(void *addr, size_t len)
{
	return munmap(addr, len);
}
