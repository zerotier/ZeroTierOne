/*
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MIT
 *
 * Portions created by Alan Antonuk are Copyright (c) 2012-2013
 * Alan Antonuk. All Rights Reserved.
 *
 * Portions created by VMware are Copyright (c) 2007-2012 VMware, Inc.
 * All Rights Reserved.
 *
 * Portions created by Tony Garnock-Jones are Copyright (c) 2009-2010
 * VMware, Inc. and Tony Garnock-Jones. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * ***** END LICENSE BLOCK *****
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "amqp_private.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

char const *amqp_version(void) { return AMQP_VERSION_STRING; }

uint32_t amqp_version_number(void) { return AMQP_VERSION; }

void init_amqp_pool(amqp_pool_t *pool, size_t pagesize) {
  pool->pagesize = pagesize ? pagesize : 4096;

  pool->pages.num_blocks = 0;
  pool->pages.blocklist = NULL;

  pool->large_blocks.num_blocks = 0;
  pool->large_blocks.blocklist = NULL;

  pool->next_page = 0;
  pool->alloc_block = NULL;
  pool->alloc_used = 0;
}

static void empty_blocklist(amqp_pool_blocklist_t *x) {
  int i;

  if (x->blocklist != NULL) {
    for (i = 0; i < x->num_blocks; i++) {
      free(x->blocklist[i]);
    }
    free(x->blocklist);
  }
  x->num_blocks = 0;
  x->blocklist = NULL;
}

void recycle_amqp_pool(amqp_pool_t *pool) {
  empty_blocklist(&pool->large_blocks);
  pool->next_page = 0;
  pool->alloc_block = NULL;
  pool->alloc_used = 0;
}

void empty_amqp_pool(amqp_pool_t *pool) {
  recycle_amqp_pool(pool);
  empty_blocklist(&pool->pages);
}

/* Returns 1 on success, 0 on failure */
static int record_pool_block(amqp_pool_blocklist_t *x, void *block) {
  size_t blocklistlength = sizeof(void *) * (x->num_blocks + 1);

  if (x->blocklist == NULL) {
    x->blocklist = malloc(blocklistlength);
    if (x->blocklist == NULL) {
      return 0;
    }
  } else {
    void *newbl = realloc(x->blocklist, blocklistlength);
    if (newbl == NULL) {
      return 0;
    }
    x->blocklist = newbl;
  }

  x->blocklist[x->num_blocks] = block;
  x->num_blocks++;
  return 1;
}

void *amqp_pool_alloc(amqp_pool_t *pool, size_t amount) {
  if (amount == 0) {
    return NULL;
  }

  amount = (amount + 7) & (~7); /* round up to nearest 8-byte boundary */

  if (amount > pool->pagesize) {
    void *result = calloc(1, amount);
    if (result == NULL) {
      return NULL;
    }
    if (!record_pool_block(&pool->large_blocks, result)) {
      free(result);
      return NULL;
    }
    return result;
  }

  if (pool->alloc_block != NULL) {
    assert(pool->alloc_used <= pool->pagesize);

    if (pool->alloc_used + amount <= pool->pagesize) {
      void *result = pool->alloc_block + pool->alloc_used;
      pool->alloc_used += amount;
      return result;
    }
  }

  if (pool->next_page >= pool->pages.num_blocks) {
    pool->alloc_block = calloc(1, pool->pagesize);
    if (pool->alloc_block == NULL) {
      return NULL;
    }
    if (!record_pool_block(&pool->pages, pool->alloc_block)) {
      return NULL;
    }
    pool->next_page = pool->pages.num_blocks;
  } else {
    pool->alloc_block = pool->pages.blocklist[pool->next_page];
    pool->next_page++;
  }

  pool->alloc_used = amount;

  return pool->alloc_block;
}

void amqp_pool_alloc_bytes(amqp_pool_t *pool, size_t amount,
                           amqp_bytes_t *output) {
  output->len = amount;
  output->bytes = amqp_pool_alloc(pool, amount);
}

amqp_bytes_t amqp_cstring_bytes(char const *cstr) {
  amqp_bytes_t result;
  result.len = strlen(cstr);
  result.bytes = (void *)cstr;
  return result;
}

amqp_bytes_t amqp_bytes_malloc_dup(amqp_bytes_t src) {
  amqp_bytes_t result;
  result.len = src.len;
  result.bytes = malloc(src.len);
  if (result.bytes != NULL) {
    memcpy(result.bytes, src.bytes, src.len);
  }
  return result;
}

amqp_bytes_t amqp_bytes_malloc(size_t amount) {
  amqp_bytes_t result;
  result.len = amount;
  result.bytes = malloc(amount); /* will return NULL if it fails */
  return result;
}

void amqp_bytes_free(amqp_bytes_t bytes) { free(bytes.bytes); }

amqp_pool_t *amqp_get_or_create_channel_pool(amqp_connection_state_t state,
                                             amqp_channel_t channel) {
  amqp_pool_table_entry_t *entry;
  size_t index = channel % POOL_TABLE_SIZE;

  entry = state->pool_table[index];

  for (; NULL != entry; entry = entry->next) {
    if (channel == entry->channel) {
      return &entry->pool;
    }
  }

  entry = malloc(sizeof(amqp_pool_table_entry_t));
  if (NULL == entry) {
    return NULL;
  }

  entry->channel = channel;
  entry->next = state->pool_table[index];
  state->pool_table[index] = entry;

  init_amqp_pool(&entry->pool, state->frame_max);

  return &entry->pool;
}

amqp_pool_t *amqp_get_channel_pool(amqp_connection_state_t state,
                                   amqp_channel_t channel) {
  amqp_pool_table_entry_t *entry;
  size_t index = channel % POOL_TABLE_SIZE;

  entry = state->pool_table[index];

  for (; NULL != entry; entry = entry->next) {
    if (channel == entry->channel) {
      return &entry->pool;
    }
  }

  return NULL;
}

int amqp_bytes_equal(amqp_bytes_t r, amqp_bytes_t l) {
  if (r.len == l.len &&
      (r.bytes == l.bytes || 0 == memcmp(r.bytes, l.bytes, r.len))) {
    return 1;
  }
  return 0;
}
