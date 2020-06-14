/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2024-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#if !defined(_WIN32) && !defined(_WIN64)
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#endif

#include "zerotier_cgo.h"

int main(int argc,char **argv)
{
	// Fork into background if run with 'service -d'. This is best done prior
	// to launching the Go code, since Go likes to start thread pools and stuff
	// that don't play nice with fork. This is obviously unsupported on Windows.
#if !defined(_WIN32) && !defined(_WIN64)
	for(int i=1;i<argc;) {
		if (strcmp(argv[i++], "service") == 0) {
			for(;i<argc;) {
				if (strcmp(argv[i++], "-d") == 0) {
					long p = (long)fork();
					if (p < 0) {
						fprintf(stderr,"FATAL: fork() failed!\n");
						return -1;
					} else if (p > 0) {
						return 0;
					}
					break;
				}
			}
			break;
		}
	}
#endif

	ZeroTierMain();

	return 0;
}
