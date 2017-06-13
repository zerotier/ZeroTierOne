/* libanode: the Anode C reference implementation
 * Copyright (C) 2009 Adam Ierymenko <adam.ierymenko@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>. */

#ifndef _SPARK_STREAMENCODER_H
#define _SPARK_STREAMENCODER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  unsigned char *input_buf;
  unsigned long input_buf_capacity;
  unsigned long input_length;

  unsigned char *stream_out_buf;
  unsigned long stream_out_buf_capacity;
  unsigned long stream_out_length;

  void (*data_segment_add_func)(const void *data,unsigned long len,const void *global_hash,unsigned long global_hash_len);
} SparkStreamEncoder;

/**
 * Initialize a spark stream encoder
 *
 * @param enc Encoder structure to initialize
 * @param data_segment_add_func Function to call to store or cache data
 */
void SparkStreamEncoder_init(
  SparkStreamEncoder *enc,
  void (*data_segment_add_func)(
    const void *data,
    unsigned long len,
    const void *global_hash,
    unsigned long global_hash_len));

/**
 * Clean up a spark stream encoder structure
 *
 * @param enc Structure to clear
 */
void SparkStreamEncoder_destroy(SparkStreamEncoder *enc);

/**
 * Add data to encode
 *
 * @param enc Encoder structure
 * @param data Data to encode
 * @param len Length of data in bytes
 * @return Number of bytes of result stream now available
 */
unsigned long SparkStreamEncoder_put(
  SparkStreamEncoder *enc,
  const void *data,
  unsigned long len);

/**
 * Flush all data currently in input buffer
 *
 * @param enc Encoder structure to flush
 */
void SparkStreamEncoder_flush(SparkStreamEncoder *enc);

/**
 * @return Number of bytes of output stream available
 */
static inline unsigned long SparkStreamEncoder_available(SparkStreamEncoder *enc)
{
  return enc->stream_out_length;
}

/**
 * @return Pointer to result stream bytes (may return null if none available)
 */
static inline const void *SparkStreamEncoder_get(SparkStreamEncoder *enc)
{
  return (const void *)(enc->stream_out_buf);
}

/**
 * @return "Consume" result stream bytes after they're read or sent
 */
static inline void SparkStreamEncoder_consume(SparkStreamEncoder *enc,unsigned long len)
{
  unsigned long i;
  for(i=len;i<enc->stream_out_length;++i)
    enc->stream_out_buf[i - len] = enc->stream_out_buf[i];
}

#ifdef __cplusplus
}
#endif

#endif
