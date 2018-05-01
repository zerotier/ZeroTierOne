/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2018  ZeroTier, Inc.  https://www.zerotier.com/
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

#ifndef ZT_RINGBUFFER_H
#define ZT_RINGBUFFER_H

#include <typeinfo>
#include <cstdint>
#include <stdlib.h>
#include <memory.h>
#include <algorithm>
#include <math.h>

namespace ZeroTier {

/**
 * A revolving (ring) buffer. 
 *
 * For fast handling of continuously-evolving variables (such as path quality metrics).
 * Using this, we can maintain longer sliding historical windows for important path 
 * metrics without the need for potentially expensive calls to memcpy/memmove.
 *
 * Some basic statistical functionality is implemented here in an attempt
 * to reduce the complexity of code needed to interact with this type of buffer.
 */

template <class T>
class RingBuffer
{
private:
	T * buf;
	size_t size;
	size_t begin;
	size_t end;
	bool wrap;

public:

	/**
	 * create a RingBuffer with space for up to size elements.
	 */
	explicit RingBuffer(size_t size)
		: size(size),
		begin(0),
		end(0),
		wrap(false)
	{
		buf = new T[size];
		memset(buf, 0, sizeof(T) * size);
	}

	/**
	 * @return A pointer to the underlying buffer
	 */
	T* get_buf()
	{
		return buf + begin;
	}

	/** 
	 * Adjust buffer index pointer as if we copied data in
	 * @param n Number of elements to copy in
	 * @return Number of elements we copied in
	 */
	size_t produce(size_t n)
	{
		n = std::min(n, getFree());
		if (n == 0) {
			return n;
		}
		const size_t first_chunk = std::min(n, size - end);
		end = (end + first_chunk) % size;
		if (first_chunk < n) {
			const size_t second_chunk = n - first_chunk;
			end = (end + second_chunk) % size;
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
	void reset()
	{
		consume(count());
	}

	/** 
	 * adjust buffer index pointer as if we copied data out
	 * @param n Number of elements we copied from the buffer
	 * @return Number of elements actually available from the buffer
	 */
	size_t consume(size_t n)
	{
		n = std::min(n, count());
		if (n == 0) {
			return n;
		}
		if (wrap) {
			wrap = false;
		}
		const size_t first_chunk = std::min(n, size - begin);
		begin = (begin + first_chunk) % size;
		if (first_chunk < n) {
			const size_t second_chunk = n - first_chunk;
			begin = (begin + second_chunk) % size;
		}
		return n;
	}

	/**
	 * @param data Buffer that is to be written to the ring
	 * @param n Number of elements to write to the buffer
	 */
	size_t write(const T * data, size_t n)
	{
		n = std::min(n, getFree());
		if (n == 0) {
			return n;
		}
		const size_t first_chunk = std::min(n, size - end);
		memcpy(buf + end, data, first_chunk * sizeof(T));
		end = (end + first_chunk) % size;
		if (first_chunk < n) {
			const size_t second_chunk = n - first_chunk;
			memcpy(buf + end, data + first_chunk, second_chunk * sizeof(T));
			end = (end + second_chunk) % size;
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
	void push(const T value)
	{
		if (count() == size) {
			consume(1);
		}
		write(&value, 1);
	}

	/**
	 * @param dest Destination buffer
	 * @param n Size (in terms of number of elements) of the destination buffer
	 * @return Number of elements read from the buffer
	 */
	size_t read(T * dest, size_t n)
	{
		n = std::min(n, count());
		if (n == 0) {
			return n;
		}
		if (wrap) {
			wrap = false;
		}
		const size_t first_chunk = std::min(n, size - begin);
		memcpy(dest, buf + begin, first_chunk * sizeof(T));
		begin = (begin + first_chunk) % size;
		if (first_chunk < n) {
			const size_t second_chunk = n - first_chunk;
			memcpy(dest + first_chunk, buf + begin, second_chunk * sizeof(T));
			begin = (begin + second_chunk) % size;
		}
		return n;
	}

	/**
	 * Return how many elements are in the buffer, O(1).
	 *
	 * @return The number of elements in the buffer
	 */
	size_t count()
	{
		if (end == begin) {
			return wrap ? size : 0;
		}
		else if (end > begin) {
			return end - begin;
		}
		else {
			return size + end - begin;
		}
	}

	/**
	 * @return The number of slots that are unused in the buffer
	 */
	size_t getFree()
	{
		return size - count();
	}

	/**
	 * @return The arithmetic mean of the contents of the buffer
	 */
	T mean()
	{
		size_t iterator = begin;
		T mean = 0;
		for (int i=0; i<size; i++) {
			iterator = (iterator + size - 1) % size;
			mean += *(buf + iterator);
		}
		return count() ? mean / (T)count() : 0;
	}

	/**
	 * @return The sample standard deviation of the contents of the ring buffer
	 */
	T stddev()
	{
		size_t iterator = begin;
		T cached_mean = mean();
		if (size) {
			T sum_of_squared_deviations = 0;
			for (int i=0; i<size; i++) {
				iterator = (iterator + size - 1) % size;
				T deviation = (buf[i] - cached_mean);
				T sdev = deviation*deviation;
				sum_of_squared_deviations += sdev;
			}
			T variance = sum_of_squared_deviations / (size - 1);
			T sd = sqrt(variance);
			return sd;
		}
		return 0;
	}

	/**
	 * @return The number of elements of zero value, O(n)
	 */
	size_t zeroCount()
	{
		size_t iterator = begin;
		size_t zeros = 0;
		for (int i=0; i<size; i++) {
			iterator = (iterator + size - 1) % size;
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
	size_t countValue(T value)
	{
		size_t iterator = begin;
		size_t count = 0;
		for (int i=0; i<size; i++) {
			iterator = (iterator + size - 1) % size;
			if (*(buf + iterator) == value) {
				count++;
			}
		}
		return count;
	}

	/**
	 * Print the contents of the buffer
	 */
	void dump()
	{
		size_t iterator = begin;
		for (int i=0; i<size; i++) {
			iterator = (iterator + size - 1) % size;
			if (typeid(T) == typeid(int)) {
				// DEBUG_INFO("buf[%2zu]=%2d", iterator, (int)*(buf + iterator));
			}
			else {
				// DEBUG_INFO("buf[%2zu]=%2f", iterator, (float)*(buf + iterator));
			}
		}
	}
};

} // namespace ZeroTier

#endif
