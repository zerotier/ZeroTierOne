#
# ZeroTier One - Network Virtualization Everywhere
# Copyright (C) 2011-2015  ZeroTier, Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# --
#
# ZeroTier may be used and distributed under the terms of the GPLv3, which
# are available at: http://www.gnu.org/licenses/gpl-3.0.html
#
# If you would like to embed ZeroTier into a commercial application or
# redistribute it in a modified binary form, please contact ZeroTier Networks
# LLC. Start here: http://www.zerotier.com/
#

SHCC=gcc

intercept_CFLAGS = -c -fPIC -g -O2 -Wall -std=c99 -D_GNU_SOURCE -DNETCON_INTERCEPT
LIB_NAME = intercept
SHLIB_EXT=dylib
SHLIB_MAJOR = 1
SHLIB_MINOR = 8
COMMON = common
OBJS= intercept.o
SHLIB = ${LIB_NAME}.${SHLIB_EXT}.${SHLIB_MAJOR}.${SHLIB_MINOR}
SHLDFLAGS = -g -O2 -Wall -I. -nostdlib -shared
LIBS = -ldl -lc -lrt -lpthread

lib:
	${SHCC} $(intercept_CFLAGS) -I. intercept.c -o intercept.o
	${SHCC} $(intercept_CFLAGS) -I. common.c -o common.o
	${SHCC} $(intercept_CFLAGS) -I. sendfd.c -o sendfd.o
	${SHCC} $(SHLDFLAGS) intercept.o common.o sendfd.o -o libintercept.so.1.0 $(LIBS)

install:
	cp libintercept.so.1.0 /lib/libintercept.so.1.0
	ln -sf /lib/libintercept.so.1.0 /lib/libintercept
	/usr/bin/install -c netcon/intercept /usr/bin

uninstall:
	rm -r /lib/libintercept.so.1.0
	rm -r /lib/libintercept
	rm -r /usr/bin/intercept
