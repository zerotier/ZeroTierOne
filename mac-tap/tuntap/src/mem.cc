/*
 * ip tunnel/ethertap device for MacOSX. Common functionality of tap_interface and tun_interface.
 *
 * Memory management implementation.
 */
/*
 * Copyright (c) 2011 Mattias Nissler <mattias.nissler@gmx.de>
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice, this list of
 *      conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright notice, this list of
 *      conditions and the following disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *   3. The name of the author may not be used to endorse or promote products derived from this
 *      software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "mem.h"

extern "C" {

#include <libkern/OSMalloc.h>

}

#if 0
#define dprintf(...)			log(LOG_INFO, __VA_ARGS__)
#else
#define dprintf(...)
#endif

static int inited = 0;
static OSMallocTag tag;

void
mem_initialize(const char* name) {
	
	if (!inited) {
		tag = OSMalloc_Tagalloc(name, OSMT_DEFAULT);
		inited = 1;
	}
}

void
mem_shutdown() {
	
	if (inited) {
		OSMalloc_Tagfree(tag);
		inited = 0;
	}
}

void *
mem_alloc(uint32_t size) {

	return OSMalloc(size, tag);
}

void
mem_free(void *addr, uint32_t size) {

	OSFree(addr, size, tag);
}

