/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */
#pragma once
#include "..\Math\superfastmath.h"
#include "..\Random\superrandom.hpp"
#include <optional>
#include <atomic>

template<typename T, uint32_t const max_size>  // *supporting only trivial types*  *max_size should be a power of 2*
class alignas(64) trivial_cache
{
	static_assert(std::is_trivial<T>::value, "trivial_cache requires type to be trivial");
	static_assert(std::atomic<T>::is_always_lock_free, "trivial_cache requires trivial type to be lock free");

public:
	__declspec(safebuffers) inline void						push(T const key, T const value);
	__declspec(safebuffers) inline std::optional<T> const	fetch(T const key);
	__declspec(safebuffers) inline void						clear();

private:
	static inline void make_sequence();
private:
	static inline thread_local bool		_hasSequence = false;;
	static inline thread_local uint32_t _sequence[max_size] = {};
	
	// structure of arrays //
	std::atomic<T>			_keys[max_size] = {};
	std::atomic<T>			_values[max_size] = {};
	std::atomic<uint32_t>	_life[max_size] = { max_size };

public:
	// static public methods that should be used for construction/destruction of bit_volume on the heap. These provide alignment support on a cacheline to avoid some false sharing.
	static inline trivial_cache<T, max_size>* const __restrict create()
	{
		tbb::cache_aligned_allocator< trivial_cache<T, max_size> > allocator;
		return(static_cast<trivial_cache<T, max_size>* const __restrict>(allocator.allocate(1)));
	}
	static inline void destroy(trivial_cache<T, max_size>* cache)
	{
		if (cache) {

			tbb::cache_aligned_allocator< trivial_cache<T, max_size> > allocator;
			allocator.deallocate(cache, 0);
			cache = nullptr;
		}
	}

	trivial_cache() = default;
	trivial_cache(trivial_cache const& rhs)
	{
		for (uint32_t i = 0; i < max_size; ++i) {
			
			uint32_t const
				keys(rhs._keys[i]),
				values(rhs._values[i]),
				life(rhs._life[i]);
			
			_keys[i] = keys;
			_values[i] = values;
			_life[i] = life;
		}
	}

};

template<typename T, uint32_t const max_size>
__declspec(safebuffers) inline void trivial_cache<T, max_size>::push(T const key, T const value)
{
	[[unlikely]] if (!_hasSequence) { // has thread local sequence for iteration ?
		make_sequence();
		_hasSequence = true;
	}

	uint32_t min_life(max_size);
	uint32_t min_index(0);

#pragma loop( ivdep )
	for (uint32_t index = 0; index < max_size; ++index) {

		uint32_t const i(_sequence[index]); // get shuffled index

		if (key == _keys[i].load(std::memory_order_relaxed)) { // match
			_life[i].fetch_add(max_size, std::memory_order_relaxed); // increase life for this cached item
			_values[i].store(value, std::memory_order_relaxed); // update value
			return; // no need to change key or value
		}
		else if (0 == _life[i].load(std::memory_order_relaxed)) { // if dead cached item
			_life[i].store(max_size, std::memory_order_relaxed);
			_values[i].store(value, std::memory_order_relaxed);
			_keys[i].store(key, std::memory_order_relaxed);
			return; // cache item in the first dead spot
		}
		else {
			// life is not zero (see above) so no worries about underun with decrement
			uint32_t const last = _life[i].fetch_sub(1, std::memory_order_relaxed) - 1;
			if (last < min_life) { // find cached item with least amount of life left
				min_life = last;   // any read from life de
				min_index = i;
			}
		}
	}

	if (min_life < max_size) { // only potentially replace a cached item if the minimum life is under the maximum size threshold
							   // this keeps items in the cache that are greater than the maximum size threshold (popular items)
							   // *if this test fails continously, max_size is not large enough, consider increasing max_size
		if (0 == min_life || (uint32_t)PsuedoRandomNumber32(0, max_size) > min_life) { // cache item if life for cached item is zero or if a random number up-to max_size is greater than the cached items life.
			// replace with new item
			_life[min_index].store(max_size, std::memory_order_relaxed);
			_values[min_index].store(value, std::memory_order_relaxed);
			_keys[min_index].store(key, std::memory_order_relaxed);
		}
	}

	// otherwise the item is dropped and not added to the cache.
}

template<typename T, uint32_t const max_size>
__declspec(safebuffers) inline std::optional<T> const trivial_cache<T, max_size>::fetch(T const key)
{
	[[unlikely]] if (!_hasSequence) { // has thread local sequence for iteration ?
		make_sequence();
		_hasSequence = true;
	}

#pragma loop( ivdep )
	for (uint32_t index = 0; index < max_size; ++index) {

		uint32_t const i(_sequence[index]); // get shuffled index

		if (key == _keys[i].load(std::memory_order_relaxed)) {
			_life[i].fetch_add(max_size, std::memory_order_relaxed);
			return(_values[i].load(std::memory_order_relaxed));
		}
	}

	return(std::nullopt);
}

template<typename T, uint32_t const max_size>
__declspec(safebuffers) inline void trivial_cache<T, max_size>::clear()
{
	// sequences, if made, stay the same //
	// everything else is cleared //
	for (uint32_t i = 0; i < max_size; ++i) {
		_life[i] = {};
		_values[i] = {};
		_keys[i] = {};
	}
}

// the sequence used for iteration is randomly shuffled from 0 to max_size
// this is done to add literally another dimension to the existing dimension of probability that accessed cached items happen at different times in parallel.
// time of fetch or push and the iteration thru the cache is "random" with respect to time.
// this additonal dimension to the iteration thru the cache is "random" with respect to the maximum size.
// point is to have less probability that an specific item in the cache is fetched or pushed simultaneously, resulting in less contention on the atomic operations performed.

// a sequence is randomly shuffled only on first fetch or push, local to the thread the operation was invoked from.
// each thread has a unique sequence to iterate
template<typename T, uint32_t const max_size>
inline void trivial_cache<T, max_size>::make_sequence()
{
	for (uint32_t i = 0; i < max_size; ++i) {
		_sequence[i] = i;
	}

	random_shuffle(&_sequence[0], &_sequence[max_size - 1]);
}

