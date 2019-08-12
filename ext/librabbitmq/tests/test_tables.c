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

#ifdef _MSC_VER
#define _USE_MATH_DEFINES
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <inttypes.h>

#include <amqp.h>

#include <math.h>

void die(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fprintf(stderr, "\n");
  abort();
}

static void dump_indent(int indent, FILE *out) {
  int i;

  for (i = 0; i < indent; i++) {
    fputc(' ', out);
  }
}

static void dump_value(int indent, amqp_field_value_t v, FILE *out) {
  int i;

  dump_indent(indent, out);
  fputc(v.kind, out);

  switch (v.kind) {
    case AMQP_FIELD_KIND_BOOLEAN:
      fputs(v.value.boolean ? " true\n" : " false\n", out);
      break;

    case AMQP_FIELD_KIND_I8:
      fprintf(out, " %" PRId8 "\n", v.value.i8);
      break;

    case AMQP_FIELD_KIND_U8:
      fprintf(out, " %" PRIu8 "\n", v.value.u8);
      break;

    case AMQP_FIELD_KIND_I16:
      fprintf(out, " %" PRId16 "\n", v.value.i16);
      break;

    case AMQP_FIELD_KIND_U16:
      fprintf(out, " %" PRIu16 "\n", v.value.u16);
      break;

    case AMQP_FIELD_KIND_I32:
      fprintf(out, " %" PRId32 "\n", v.value.i32);
      break;

    case AMQP_FIELD_KIND_U32:
      fprintf(out, " %" PRIu32 "\n", v.value.u32);
      break;

    case AMQP_FIELD_KIND_I64:
      fprintf(out, " %" PRId64 "\n", v.value.i64);
      break;

    case AMQP_FIELD_KIND_F32:
      fprintf(out, " %g\n", (double)v.value.f32);
      break;

    case AMQP_FIELD_KIND_F64:
      fprintf(out, " %g\n", v.value.f64);
      break;

    case AMQP_FIELD_KIND_DECIMAL:
      fprintf(out, " %u:::%u\n", v.value.decimal.decimals,
              v.value.decimal.value);
      break;

    case AMQP_FIELD_KIND_UTF8:
      fprintf(out, " %.*s\n", (int)v.value.bytes.len,
              (char *)v.value.bytes.bytes);
      break;

    case AMQP_FIELD_KIND_BYTES:
      fputc(' ', out);
      for (i = 0; i < (int)v.value.bytes.len; i++) {
        fprintf(out, "%02x", ((char *)v.value.bytes.bytes)[i]);
      }

      fputc('\n', out);
      break;

    case AMQP_FIELD_KIND_ARRAY:
      fputc('\n', out);
      for (i = 0; i < v.value.array.num_entries; i++) {
        dump_value(indent + 2, v.value.array.entries[i], out);
      }

      break;

    case AMQP_FIELD_KIND_TIMESTAMP:
      fprintf(out, " %" PRIu64 "\n", v.value.u64);
      break;

    case AMQP_FIELD_KIND_TABLE:
      fputc('\n', out);
      for (i = 0; i < v.value.table.num_entries; i++) {
        dump_indent(indent + 2, out);
        fprintf(out, "%.*s ->\n", (int)v.value.table.entries[i].key.len,
                (char *)v.value.table.entries[i].key.bytes);
        dump_value(indent + 4, v.value.table.entries[i].value, out);
      }

      break;

    case AMQP_FIELD_KIND_VOID:
      fputc('\n', out);
      break;

    default:
      fprintf(out, "???\n");
      break;
  }
}

static void test_dump_value(FILE *out) {
  amqp_table_entry_t entries[8];
  amqp_table_t table;
  amqp_field_value_t val;

  entries[0].key = amqp_cstring_bytes("zebra");
  entries[0].value.kind = AMQP_FIELD_KIND_UTF8;
  entries[0].value.value.bytes = amqp_cstring_bytes("last");

  entries[1].key = amqp_cstring_bytes("aardvark");
  entries[1].value.kind = AMQP_FIELD_KIND_UTF8;
  entries[1].value.value.bytes = amqp_cstring_bytes("first");

  entries[2].key = amqp_cstring_bytes("middle");
  entries[2].value.kind = AMQP_FIELD_KIND_UTF8;
  entries[2].value.value.bytes = amqp_cstring_bytes("third");

  entries[3].key = amqp_cstring_bytes("number");
  entries[3].value.kind = AMQP_FIELD_KIND_I32;
  entries[3].value.value.i32 = 1234;

  entries[4].key = amqp_cstring_bytes("decimal");
  entries[4].value.kind = AMQP_FIELD_KIND_DECIMAL;
  entries[4].value.value.decimal.decimals = 2;
  entries[4].value.value.decimal.value = 1234;

  entries[5].key = amqp_cstring_bytes("time");
  entries[5].value.kind = AMQP_FIELD_KIND_TIMESTAMP;
  entries[5].value.value.u64 = 1234123412341234;

  entries[6].key = amqp_cstring_bytes("beta");
  entries[6].value.kind = AMQP_FIELD_KIND_UTF8;
  entries[6].value.value.bytes = amqp_cstring_bytes("second");

  entries[7].key = amqp_cstring_bytes("wombat");
  entries[7].value.kind = AMQP_FIELD_KIND_UTF8;
  entries[7].value.value.bytes = amqp_cstring_bytes("fourth");

  table.num_entries = 8;
  table.entries = entries;

  qsort(table.entries, table.num_entries, sizeof(amqp_table_entry_t),
        &amqp_table_entry_cmp);

  val.kind = AMQP_FIELD_KIND_TABLE;
  val.value.table = table;

  dump_value(0, val, out);
}

static uint8_t pre_encoded_table[] = {
    0x00, 0x00, 0x00, 0xff, 0x07, 0x6c, 0x6f, 0x6e, 0x67, 0x73, 0x74, 0x72,
    0x53, 0x00, 0x00, 0x00, 0x15, 0x48, 0x65, 0x72, 0x65, 0x20, 0x69, 0x73,
    0x20, 0x61, 0x20, 0x6c, 0x6f, 0x6e, 0x67, 0x20, 0x73, 0x74, 0x72, 0x69,
    0x6e, 0x67, 0x09, 0x73, 0x69, 0x67, 0x6e, 0x65, 0x64, 0x69, 0x6e, 0x74,
    0x49, 0x00, 0x00, 0x30, 0x39, 0x07, 0x64, 0x65, 0x63, 0x69, 0x6d, 0x61,
    0x6c, 0x44, 0x03, 0x00, 0x01, 0xe2, 0x40, 0x09, 0x74, 0x69, 0x6d, 0x65,
    0x73, 0x74, 0x61, 0x6d, 0x70, 0x54, 0x00, 0x00, 0x63, 0xee, 0xa0, 0x53,
    0xc1, 0x94, 0x05, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x46, 0x00, 0x00, 0x00,
    0x1f, 0x03, 0x6f, 0x6e, 0x65, 0x49, 0x00, 0x00, 0xd4, 0x31, 0x03, 0x74,
    0x77, 0x6f, 0x53, 0x00, 0x00, 0x00, 0x0d, 0x41, 0x20, 0x6c, 0x6f, 0x6e,
    0x67, 0x20, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x04, 0x62, 0x79, 0x74,
    0x65, 0x62, 0xff, 0x04, 0x6c, 0x6f, 0x6e, 0x67, 0x6c, 0x00, 0x00, 0x00,
    0x00, 0x49, 0x96, 0x02, 0xd2, 0x05, 0x73, 0x68, 0x6f, 0x72, 0x74, 0x73,
    0x02, 0x8f, 0x04, 0x62, 0x6f, 0x6f, 0x6c, 0x74, 0x01, 0x06, 0x62, 0x69,
    0x6e, 0x61, 0x72, 0x79, 0x78, 0x00, 0x00, 0x00, 0x0f, 0x61, 0x20, 0x62,
    0x69, 0x6e, 0x61, 0x72, 0x79, 0x20, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67,
    0x04, 0x76, 0x6f, 0x69, 0x64, 0x56, 0x05, 0x61, 0x72, 0x72, 0x61, 0x79,
    0x41, 0x00, 0x00, 0x00, 0x17, 0x49, 0x00, 0x00, 0xd4, 0x31, 0x53, 0x00,
    0x00, 0x00, 0x0d, 0x41, 0x20, 0x6c, 0x6f, 0x6e, 0x67, 0x20, 0x73, 0x74,
    0x72, 0x69, 0x6e, 0x67, 0x05, 0x66, 0x6c, 0x6f, 0x61, 0x74, 0x66, 0x40,
    0x49, 0x0f, 0xdb, 0x06, 0x64, 0x6f, 0x75, 0x62, 0x6c, 0x65, 0x64, 0x40,
    0x09, 0x21, 0xfb, 0x54, 0x44, 0x2d, 0x18};

static void test_table_codec(FILE *out) {
  amqp_pool_t pool;
  int result;

  amqp_table_entry_t inner_entries[2];
  amqp_table_t inner_table;

  amqp_field_value_t inner_values[2];
  amqp_array_t inner_array;

  amqp_table_entry_t entries[14];
  amqp_table_t table;

  inner_entries[0].key = amqp_cstring_bytes("one");
  inner_entries[0].value.kind = AMQP_FIELD_KIND_I32;
  inner_entries[0].value.value.i32 = 54321;

  inner_entries[1].key = amqp_cstring_bytes("two");
  inner_entries[1].value.kind = AMQP_FIELD_KIND_UTF8;
  inner_entries[1].value.value.bytes = amqp_cstring_bytes("A long string");

  inner_table.num_entries = 2;
  inner_table.entries = inner_entries;

  inner_values[0].kind = AMQP_FIELD_KIND_I32;
  inner_values[0].value.i32 = 54321;

  inner_values[1].kind = AMQP_FIELD_KIND_UTF8;
  inner_values[1].value.bytes = amqp_cstring_bytes("A long string");

  inner_array.num_entries = 2;
  inner_array.entries = inner_values;

  entries[0].key = amqp_cstring_bytes("longstr");
  entries[0].value.kind = AMQP_FIELD_KIND_UTF8;
  entries[0].value.value.bytes = amqp_cstring_bytes("Here is a long string");

  entries[1].key = amqp_cstring_bytes("signedint");
  entries[1].value.kind = AMQP_FIELD_KIND_I32;
  entries[1].value.value.i32 = 12345;

  entries[2].key = amqp_cstring_bytes("decimal");
  entries[2].value.kind = AMQP_FIELD_KIND_DECIMAL;
  entries[2].value.value.decimal.decimals = 3;
  entries[2].value.value.decimal.value = 123456;

  entries[3].key = amqp_cstring_bytes("timestamp");
  entries[3].value.kind = AMQP_FIELD_KIND_TIMESTAMP;
  entries[3].value.value.u64 = 109876543209876;

  entries[4].key = amqp_cstring_bytes("table");
  entries[4].value.kind = AMQP_FIELD_KIND_TABLE;
  entries[4].value.value.table = inner_table;

  entries[5].key = amqp_cstring_bytes("byte");
  entries[5].value.kind = AMQP_FIELD_KIND_I8;
  entries[5].value.value.i8 = (int8_t)-1;

  entries[6].key = amqp_cstring_bytes("long");
  entries[6].value.kind = AMQP_FIELD_KIND_I64;
  entries[6].value.value.i64 = 1234567890;

  entries[7].key = amqp_cstring_bytes("short");
  entries[7].value.kind = AMQP_FIELD_KIND_I16;
  entries[7].value.value.i16 = 655;

  entries[8].key = amqp_cstring_bytes("bool");
  entries[8].value.kind = AMQP_FIELD_KIND_BOOLEAN;
  entries[8].value.value.boolean = 1;

  entries[9].key = amqp_cstring_bytes("binary");
  entries[9].value.kind = AMQP_FIELD_KIND_BYTES;
  entries[9].value.value.bytes = amqp_cstring_bytes("a binary string");

  entries[10].key = amqp_cstring_bytes("void");
  entries[10].value.kind = AMQP_FIELD_KIND_VOID;

  entries[11].key = amqp_cstring_bytes("array");
  entries[11].value.kind = AMQP_FIELD_KIND_ARRAY;
  entries[11].value.value.array = inner_array;

  entries[12].key = amqp_cstring_bytes("float");
  entries[12].value.kind = AMQP_FIELD_KIND_F32;
  entries[12].value.value.f32 = (float)M_PI;

  entries[13].key = amqp_cstring_bytes("double");
  entries[13].value.kind = AMQP_FIELD_KIND_F64;
  entries[13].value.value.f64 = M_PI;

  table.num_entries = 14;
  table.entries = entries;

  fprintf(out, "AAAAAAAAAA\n");

  {
    amqp_field_value_t val;
    val.kind = AMQP_FIELD_KIND_TABLE;
    val.value.table = table;
    dump_value(0, val, out);
  }

  init_amqp_pool(&pool, 4096);

  {
    amqp_table_t decoded;
    size_t decoding_offset = 0;
    amqp_bytes_t decoding_bytes;
    decoding_bytes.len = sizeof(pre_encoded_table);
    decoding_bytes.bytes = pre_encoded_table;

    result =
        amqp_decode_table(decoding_bytes, &pool, &decoded, &decoding_offset);
    if (result < 0) {
      die("Table decoding failed: %s", amqp_error_string2(result));
    }

    fprintf(out, "BBBBBBBBBB\n");

    {
      amqp_field_value_t val;
      val.kind = AMQP_FIELD_KIND_TABLE;
      val.value.table = decoded;

      dump_value(0, val, out);
    }
  }

  {
    uint8_t encoding_buffer[4096];
    amqp_bytes_t encoding_result;
    size_t offset = 0;

    memset(&encoding_buffer[0], 0, sizeof(encoding_buffer));
    encoding_result.len = sizeof(encoding_buffer);
    encoding_result.bytes = &encoding_buffer[0];

    result = amqp_encode_table(encoding_result, &table, &offset);
    if (result < 0) {
      die("Table encoding failed: %s", amqp_error_string2(result));
    }

    if (offset != sizeof(pre_encoded_table))
      die("Offset should be %ld, was %ld", (long)sizeof(pre_encoded_table),
          (long)offset);

    result = memcmp(pre_encoded_table, encoding_buffer, offset);
    if (result != 0) {
      die("Table encoding differed", result);
    }
  }

  empty_amqp_pool(&pool);
}

#define CHUNK_SIZE 4096

static int compare_files(FILE *f1_in, FILE *f2_in) {
  char f1_buf[CHUNK_SIZE];
  char f2_buf[CHUNK_SIZE];
  int res;

  rewind(f1_in);
  rewind(f2_in);

  for (;;) {
    size_t f1_got = fread(f1_buf, 1, CHUNK_SIZE, f1_in);
    size_t f2_got = fread(f2_buf, 1, CHUNK_SIZE, f2_in);
    res = memcmp(f1_buf, f2_buf, f1_got < f2_got ? f1_got : f2_got);

    if (res) {
      break;
    }

    if (f1_got < CHUNK_SIZE || f2_got < CHUNK_SIZE) {
      if (f1_got != f2_got) {
        res = (f1_got < f2_got ? -1 : 1);
      }
      break;
    }
  }

  return res;
}

const char *expected_file_name = "tests/test_tables.expected";

int main(void) {
  char *srcdir = getenv("srcdir");
  FILE *out, *expected = NULL;
  char *expected_path;

  out = tmpfile();
  if (out == NULL) {
    die("failed to create temporary file: %s", strerror(errno));
  }

  test_table_codec(out);
  fprintf(out, "----------\n");
  test_dump_value(out);

  if (srcdir == NULL) {
    srcdir = ".";
  }

  expected_path = malloc(strlen(srcdir) + strlen(expected_file_name) + 2);
  if (!expected_path) {
    die("out of memory");
  }
  sprintf(expected_path, "%s/%s", srcdir, expected_file_name);
  expected = fopen(expected_path, "r");
  if (!expected) {
    die("failed to open %s: %s", expected_path, strerror(errno));
  }

  if (compare_files(expected, out)) {
    die("output file did not have expected contents");
  }

  fclose(expected);
  free(expected_path);
  fclose(out);

  return 0;
}
