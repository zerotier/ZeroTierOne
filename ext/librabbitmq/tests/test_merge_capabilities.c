/*
 * Copyright 2015 Alan Antonuk. All Rights Reserved.
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
 */

#include "amqp_socket.h"
#include "amqp_table.h"

#include <stdio.h>
#include <stdlib.h>

static int compare_bytes(amqp_bytes_t l, amqp_bytes_t r);
static int compare_amqp_table_entry(amqp_table_entry_t result,
                                    amqp_table_entry_t expect);
static int compare_field_value(amqp_field_value_t result,
                               amqp_field_value_t expect);
static int compare_amqp_table(amqp_table_t* result, amqp_table_t* expect);

static int compare_bytes(amqp_bytes_t l, amqp_bytes_t r) {
  if (l.len == r.len &&
      (l.bytes == r.bytes || 0 == memcmp(l.bytes, r.bytes, l.len))) {
    return 1;
  }
  return 0;
}

static int compare_amqp_table_entry(amqp_table_entry_t result,
                                    amqp_table_entry_t expect) {
  if (!compare_bytes(result.key, expect.key)) {
    return 0;
  }
  return compare_field_value(result.value, expect.value);
}

static int compare_field_value(amqp_field_value_t result,
                               amqp_field_value_t expect) {
  if (result.kind != expect.kind) {
    return 0;
  }
  switch (result.kind) {
    case AMQP_FIELD_KIND_BOOLEAN:
      return result.value.boolean == expect.value.boolean;
    case AMQP_FIELD_KIND_I8:
      return result.value.i8 == expect.value.i8;
    case AMQP_FIELD_KIND_U8:
      return result.value.u8 == expect.value.u8;
    case AMQP_FIELD_KIND_I16:
      return result.value.i16 == expect.value.i16;
    case AMQP_FIELD_KIND_U16:
      return result.value.u16 == expect.value.u16;
    case AMQP_FIELD_KIND_I32:
      return result.value.i32 == expect.value.i32;
    case AMQP_FIELD_KIND_U32:
      return result.value.u32 == expect.value.u32;
    case AMQP_FIELD_KIND_I64:
      return result.value.i64 == expect.value.i64;
    case AMQP_FIELD_KIND_U64:
    case AMQP_FIELD_KIND_TIMESTAMP:
      return result.value.u64 == expect.value.u64;
    case AMQP_FIELD_KIND_F32:
      return result.value.f32 == expect.value.f32;
    case AMQP_FIELD_KIND_F64:
      return result.value.f64 == expect.value.f64;
    case AMQP_FIELD_KIND_DECIMAL:
      return !memcmp(&result.value.decimal, &expect.value.decimal,
                     sizeof(expect.value.decimal));
    case AMQP_FIELD_KIND_UTF8:
    case AMQP_FIELD_KIND_BYTES:
      return compare_bytes(result.value.bytes, expect.value.bytes);
    case AMQP_FIELD_KIND_ARRAY: {
      int i;
      if (result.value.array.num_entries != expect.value.array.num_entries) {
        return 0;
      }
      for (i = 0; i < result.value.array.num_entries; ++i) {
        if (!compare_field_value(result.value.array.entries[i],
                                 expect.value.array.entries[i])) {
          return 0;
        }
      }
      return 1;
    }
    case AMQP_FIELD_KIND_TABLE:
      return compare_amqp_table(&result.value.table, &expect.value.table);
    case AMQP_FIELD_KIND_VOID:
      return 1;
  }
  return 1;
}

static int compare_amqp_table(amqp_table_t* result, amqp_table_t* expect) {
  int i;

  if (result->num_entries != expect->num_entries) {
    return 0;
  }

  for (i = 0; i < expect->num_entries; ++i) {
    if (!compare_amqp_table_entry(expect->entries[i], result->entries[i])) {
      return 0;
    }
  }
  return 1;
}

static void test_merge_capabilities(amqp_table_t* base, amqp_table_t* add,
                                    amqp_table_t* expect) {
  amqp_pool_t pool;
  amqp_table_t result;
  int res;
  init_amqp_pool(&pool, 4096);

  res = amqp_merge_capabilities(base, add, &result, &pool);
  if (AMQP_STATUS_OK != res) {
    fprintf(stderr, "amqp_merge_capabilities returned !ok: %d\n", res);
    abort();
  }

  if (!compare_amqp_table(&result, expect)) {
    fprintf(stderr, "amqp_merge_capabilities incorrect result.\n");
    abort();
  }
  empty_amqp_pool(&pool);
  return;
}

int main(void) {
  {
    amqp_table_t sub_base;
    amqp_table_t sub_add;
    amqp_table_t sub_expect;
    amqp_table_t base;
    amqp_table_t add;
    amqp_table_t expect;

    amqp_table_entry_t sub_base_entries[1];
    amqp_table_entry_t sub_add_entries[2];
    amqp_table_entry_t sub_expect_entries[2];

    amqp_table_entry_t base_entries[3];
    amqp_table_entry_t add_entries[3];
    amqp_table_entry_t expect_entries[4];

    sub_base_entries[0] = amqp_table_construct_utf8_entry("foo", "bar");
    sub_base.num_entries =
        sizeof(sub_base_entries) / sizeof(amqp_table_entry_t);
    sub_base.entries = sub_base_entries;

    sub_add_entries[0] = amqp_table_construct_utf8_entry("something", "else");
    sub_add_entries[1] = amqp_table_construct_utf8_entry("foo", "baz");
    sub_add.num_entries = sizeof(sub_add_entries) / sizeof(amqp_table_entry_t);
    sub_add.entries = sub_add_entries;

    sub_expect_entries[0] = amqp_table_construct_utf8_entry("foo", "baz");
    sub_expect_entries[1] =
        amqp_table_construct_utf8_entry("something", "else");
    sub_expect.num_entries =
        sizeof(sub_expect_entries) / sizeof(amqp_table_entry_t);
    sub_expect.entries = sub_expect_entries;

    base_entries[0] = amqp_table_construct_utf8_entry("product", "1.0");
    base_entries[1] = amqp_table_construct_utf8_entry("nooverride", "yeah");
    base_entries[2] = amqp_table_construct_table_entry("props", &sub_base);
    base.num_entries = sizeof(base_entries) / sizeof(amqp_table_entry_t);
    base.entries = base_entries;

    add_entries[0] = amqp_table_construct_bool_entry("bool_entry", 1);
    add_entries[1] = amqp_table_construct_utf8_entry("product", "2.0");
    add_entries[2] = amqp_table_construct_table_entry("props", &sub_add);
    add.num_entries = sizeof(add_entries) / sizeof(amqp_table_entry_t);
    add.entries = add_entries;

    expect_entries[0] = amqp_table_construct_utf8_entry("product", "2.0"),
    expect_entries[1] = amqp_table_construct_utf8_entry("nooverride", "yeah"),
    expect_entries[2] = amqp_table_construct_table_entry("props", &sub_expect);
    expect_entries[3] = amqp_table_construct_bool_entry("bool_entry", 1);
    expect.num_entries = sizeof(expect_entries) / sizeof(amqp_table_entry_t);
    expect.entries = expect_entries;

    test_merge_capabilities(&base, &add, &expect);
  }
  fprintf(stderr, "ok\n");
  return 0;
}
