#
# Copyright (c) 2003 Christopher L. Cousins <clc@sparf.com>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

SUBDIRS= lib ucrpsh test-server
RANLIB?= ranlib
SETENV?= /usr/bin/env -i

CFLAGS=  -O2 -I../include -g -Wall -Wuninitialized
#CFLAGS+= -D_GNU_SOURCE
#CFLAGS+= -DNDEBUG

MAKE_ENV+= CC="${CC}" CFLAGS="${CFLAGS}" RANLIB="${RANLIB}" PATH="${PATH}"

all clean distclean:
	@for dir in ${SUBDIRS}; do                       \
		set -e;                                  \
		cd $${dir};                              \
		${SETENV} ${MAKE_ENV} ${MAKE} ${@};      \
		cd ..;                                   \
	done
	@rm -f TAGS

TAGS:
	@rm -f TAGS
	@find . -type f -name \*.[ch] -print | xargs etags -a

