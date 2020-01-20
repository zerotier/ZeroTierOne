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

#ifndef ZT_RINGBUFFER_H
#define ZT_RINGBUFFER_H

#include <typeinfo>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <algorithm>

namespace ZeroTier {

/**
 * A circular buffer
 *
 * For fast handling of continuously-evolving variables (such as path quality metrics).
 * Using this, we can maintain longer sliding historical windows for important path
 * metrics without the need for potentially expensive calls to memcpy/memmove.
 *
 * Some basic statistical functionality is implemented here in an attempt
 * to reduce the complexity of code needed to interact with this type of buffer.
 */
template <class T,size_t S>
class RingBuffer
{
private:
	T buf[S];
	size_t begin;
	size_t end;
	bool wrap;

public:
	inline RingBuffer() :
		begin(0),
		end(0),
		wrap(false)
	{
		memset(buf,0,sizeof(T)*S);
	}

	/**
	 * @return A pointer to the underlying buffer
	 */
	inline T *get_buf()
	{
		return buf + begin;
	}

	/**
	 * Adjust buffer index pointer as if we copied data in
	 * @param n Number of elements to copy in
	 * @return Number of elements we copied in
	 */
	inline size_t produce(size_t n)
	{
		n = std::min(n, getFree());
		if (n == 0) {
			return n;
		}
		const size_t first_chunk = std::min(n, S - end);
		end = (end + first_chunk) % S;
		if (first_chunk < n) {
			const size_t second_chunk = n - first_chunk;
			end = (end + second_chunk) % S;
		}
		if (begin == end) {
			wrap = true;
		}
		return n;
	}

	/**
	 * Fast erase, O(1).
	 * Merely reset the buffer pointer, doesn't erase contents
	 */
	inline void reset() { consume(count()); }

	/**
	 * adjust buffer index pointer as if we copied data out
	 * @param n Number of elements we copied from the buffer
	 * @return Number of elements actually available from the buffer
	 */
	inline size_t consume(size_t n)
	{
		n = std::min(n, count());
		if (n == 0) {
			return n;
		}
		if (wrap) {
			wrap = false;
		}
		const size_t first_chunk = std::min(n, S - begin);
		begin = (begin + first_chunk) % S;
		if (first_chunk < n) {
			const size_t second_chunk = n - first_chunk;
			begin = (begin + second_chunk) % S;
		}
		return n;
	}

	/**
	 * @param data Buffer that is to be written to the ring
	 * @param n Number of elements to write to the buffer
	 */
	inline size_t write(const T * data, size_t n)
	{
		n = std::min(n, getFree());
		if (n == 0) {
			return n;
		}
		const size_t first_chunk = std::min(n, S - end);
		memcpy(buf + end, data, first_chunk * sizeof(T));
		end = (end + first_chunk) % S;
		if (first_chunk < n) {
			const size_t second_chunk = n - first_chunk;
			memcpy(buf + end, data + first_chunk, second_chunk * sizeof(T));
			end = (end + second_chunk) % S;
		}
		if (begin == end) {
			wrap = true;
		}
		return n;
	}

	/**
	 * Place a single value on the buffer. If the buffer is full, consume a value first.
	 *
	 * @param value A single value to be placed in the buffer
	 */
	inline void push(const T value)
	{
		if (count() == S) {
			consume(1);
		}
		const size_t first_chunk = std::min((size_t)1, S - end);
		*(buf + end) = value;
		end = (end + first_chunk) % S;
		if (begin == end) {
			wrap = true;
		}
	}

	/**
	 * @return The most recently pushed element on the buffer
	 */
	inline T get_most_recent() { return *(buf + end); }

	/**
	 * @param dest Destination buffer
	 * @param n Size (in terms of number of elements) of the destination buffer
	 * @return Number of elements read from the buffer
	 */
	inline size_t read(T *dest,size_t n)
	{
		n = std::min(n, count());
		if (n == 0) {
			return n;
		}
		if (wrap) {
			wrap = false;
		}
		const size_t first_chunk = std::min(n, S - begin);
		memcpy(dest, buf + begin, first_chunk * sizeof(T));
		begin = (begin + first_chunk) % S;
		if (first_chunk < n) {
			const size_t second_chunk = n - first_chunk;
			memcpy(dest + first_chunk, buf + begin, second_chunk * sizeof(T));
			begin = (begin + second_chunk) % S;
		}
		return n;
	}

	/**
	 * Return how many elements are in the buffer, O(1).
	 *
	 * @return The number of elements in the buffer
	 */
	inline size_t count()
	{
		if (end == begin) {
			return wrap ? S : 0;
		}
		else if (end > begin) {
			return end - begin;
		}
		else {
			return S + end - begin;
		}
	}

	/**
	 * @return The number of slots that are unused in the buffer
	 */
	inline size_t getFree() { return S - count(); }

	/**
	 * @return The arithmetic mean of the contents of the buffer
	 */
	inline float mean()
	{
		size_t iterator = begin;
		float subtotal = 0;
		size_t curr_cnt = count();
		for (size_t i=0; i<curr_cnt; i++) {
			iterator = (iterator + S - 1) % curr_cnt;
			subtotal += (float)*(buf + iterator);
		}
		return curr_cnt ? subtotal / (float)curr_cnt : 0;
	}

	/**
	 * @return The arithmetic mean of the most recent 'n' elements of the buffer
	 */
	inline float mean(size_t n)
	{
		n = n < S ? n : S;
		size_t iterator = begin;
		float subtotal = 0;
		size_t curr_cnt = count();
		for (size_t i=0; i<n; i++) {
			iterator = (iterator + S - 1) % curr_cnt;
			subtotal += (float)*(buf + iterator);
		}
		return curr_cnt ? subtotal / (float)curr_cnt : 0;
	}

	/**
	 * @return The sample standard deviation of element values
	 */
	inline float stddev() { return sqrt(variance()); }

	/**
	 * @return The variance of element values
	 */
	inline float variance()
	{
		size_t iterator = begin;
		float cached_mean = mean();
		size_t curr_cnt = count();
		T sum_of_squared_deviations = 0;
		for (size_t i=0; i<curr_cnt; i++) {
			iterator = (iterator + S - 1) % curr_cnt;
			float deviation = (buf[i] - cached_mean);
			sum_of_squared_deviations += (T)(deviation*deviation);
		}
		float variance = (float)sum_of_squared_deviations / (float)(S - 1);
		return variance;
	}

	/**
	 * @return The number of elements of zero value
	 */
	inline size_t zeroCount()
	{
		size_t iterator = begin;
		size_t zeros = 0;
		size_t curr_cnt = count();
		for (size_t i=0; i<curr_cnt; i++) {
			iterator = (iterator + S - 1) % curr_cnt;
			if (*(buf + iterator) == 0) {
				zeros++;
			}
		}
		return zeros;
	}

	/**
	 * @param value Value to match against in buffer
	 * @return The number of values held in the ring buffer which match a given value
	 */
	inline size_t countValue(T value)
	{
		size_t iterator = begin;
		size_t cnt = 0;
		size_t curr_cnt = count();
		for (size_t i=0; i<curr_cnt; i++) {
			iterator = (iterator + S - 1) % curr_cnt;
			if (*(buf + iterator) == value) {
				cnt++;
			}
		}
		return cnt;
	}

	/**
	 * Print the contents of the buffer
	 */
	/*
	inline void dump()
	{
		size_t iterator = begin;
		for (size_t i=0; i<S; i++) {
			iterator = (iterator + S - 1) % S;
			if (typeid(T) == typeid(int)) {
				 //DEBUG_INFO("buf[%2zu]=%2d", iterator, (int)*(buf + iterator));
			}
			else {
				 //DEBUG_INFO("buf[%2zu]=%2f", iterator, (float)*(buf + iterator));
			}
		}
	}
	*/
};

} // namespace ZeroTier

#endif
