/*
 * Huffandpuff minimal Huffman coder
 *
 * (c)2013 Adam Ierymenko <adam.ierymenko@zerotier.com>
 * This code is in the public domain and is distributed with NO WARRANTY.
 */

#ifndef ____HUFFMAN_H
#define ____HUFFMAN_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Required size of huffheap parameter to compress and decompress
 *
 * Note: if you change any of the data types in the _huffman_node
 * or _huffman_encode_table structs in huffman.c, this also must be
 * changed.
 */
#define HUFFHEAP_SIZE ((sizeof(double) * 257) + (((sizeof(void *) * 4) + sizeof(double) + sizeof(unsigned long)) * (257 * 3)) + ((sizeof(unsigned long) + sizeof(unsigned long)) * 257))

/**
 * Huffman encode a block of data
 *
 * @param in Input data
 * @param inlen Input data length
 * @param out Output buffer
 * @param outlen Output buffer length
 * @param huffheap Heap memory to use for compression (must be HUFFHEAP_SIZE in size)
 * @return Size of encoded result or 0 on out buffer overrun
 */
extern unsigned long huffman_compress(const unsigned char *in,unsigned long inlen,unsigned char *out,unsigned long outlen,void *huffheap);

/**
 * Huffman decode a block of data
 *
 * @param in Input data
 * @param inlen Length of input data
 * @param out Output buffer
 * @param outlen Length of output buffer
 * @param huffheap Heap memory to use for decompression (must be HUFFHEAP_SIZE in size)
 * @return Size of decoded result or 0 on out buffer overrun or corrupt input data
 */
extern unsigned long huffman_decompress(const unsigned char *in,unsigned long inlen,unsigned char *out,unsigned long outlen,void *huffheap);

#ifdef __cplusplus
}
#endif

#endif
