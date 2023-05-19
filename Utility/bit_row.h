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
#pragma warning( disable : 4804 )	// unsafe use of type bool in write_bit() (its ok for this case)
#include <tbb\cache_aligned_allocator.h>
#include <Utility/mem.h>
#include <Utility/class_helper.h>
#include <atomic>

#pragma intrinsic(memset)

#define bit_function __declspec(safebuffers) __forceinline

 ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 // bit_row
template<size_t const Length>  // must be divisable by 64 //
struct no_vtable bit_row
{
#ifdef BIT_ROW_ATOMIC
	using bits = std::atomic_size_t;
#else
	using bits = size_t;
#endif

	static constexpr size_t const element_bits = 6;
	static constexpr size_t const element_count = (1 << element_bits);	// block size (64 = (1 << 6))
	static_assert((0 == Length % element_count), "bit_row length invalid");

	static constexpr size_t const Stride = { Length / element_count };	// number of blocks for each dimension
public:
	static constexpr size_t const total_bit_count = Length;
	static constexpr size_t const Size = Stride * sizeof(bits);

	static constexpr size_t const length() { return(Length); }
	static constexpr size_t const stride() { return(Stride); }
	bit_function size_t const     population_count() const;

	bit_function bool const   read_bit(size_t const index) const;
	bit_function void		  set_bit(size_t const index);
	bit_function void		  clear_bit(size_t const index);
	bit_function void		  write_bit(size_t const index, bool const state);  // *use set_bit / clear_bit for best performance - avoids branching.

	bit_function bits const* const __restrict data() const { return(_bits); }
	bit_function bits* const __restrict		  data() { return(_bits); }

	constexpr size_t const size() const { return(Stride * sizeof(bits)); }
	void clear();

private:
	alignas(CACHE_LINE_BYTES) bits	_bits[Stride] = {};
public:
	// static public methods that should be used for construction/destruction of bit_row on the heap. These provide alignment support on a cacheline to avoid some false sharing.
	__declspec(safebuffers) static inline bit_row<Length>* const __restrict create(size_t const row_count = 1)
	{
		tbb::cache_aligned_allocator< bit_row<Length> > allocator;
		bit_row<Length>* const __restrict pReturn(static_cast<bit_row<Length>*const __restrict>(allocator.allocate(row_count)));

		pReturn->clear();

		return(pReturn);
	}
	__declspec(safebuffers) static inline void destroy(bit_row<Length>* row)
	{
		if (row) {

			tbb::cache_aligned_allocator< bit_row<Length> > allocator;
			allocator.deallocate(row, 0);
			row = nullptr;
		}
	}
	// ops
	__declspec(safebuffers) static inline bit_row<Length>& __restrict and_bits(bit_row<Length>& __restrict a, bit_row<Length> const& __restrict b)
	{
		for (size_t bits = 0; bits < Length; bits += 256) { // by total bits (length), 256 bits / iteration

			size_t const block(bits >> element_bits);	// 4 elements per 256 bits

			__m256i const 
				xmA(_mm256_load_si256((__m256i const* const __restrict)(a.data() + block))),
				xmB(_mm256_load_si256((__m256i const* const __restrict)(b.data() + block)));

			_mm256_store_si256((__m256i* const __restrict)(a.data() + block), _mm256_and_si256(xmA, xmB));
		}

		return(a);
	}
	__declspec(safebuffers) static inline bit_row<Length>& __restrict or_bits(bit_row<Length>& __restrict a, bit_row<Length> const& __restrict b)
	{
		for (size_t bits = 0; bits < Length; bits += 256) { // by total bits (length), 256 bits / iteration

			size_t const block(bits >> element_bits);	// 4 elements per 256 bits

			__m256i const 
				xmA(_mm256_load_si256((__m256i const* const __restrict)(a.data() + block))),
				xmB(_mm256_load_si256((__m256i const* const __restrict)(b.data() + block)));

			_mm256_store_si256((__m256i* const __restrict)(a.data() + block), _mm256_or_si256(xmA, xmB));
		}

		return(a);
	}
	__declspec(safebuffers) static inline bit_row<Length>& __restrict xor_bits(bit_row<Length>& __restrict a, bit_row<Length> const& __restrict b)
	{
		for (size_t bits = 0; bits < Length; bits += 256) { // by total bits (length), 256 bits / iteration

			size_t const block(bits >> element_bits);	// 4 elements per 256 bits

			__m256i const 
				xmA(_mm256_load_si256((__m256i const* const __restrict)(a.data() + block))),
				xmB(_mm256_load_si256((__m256i const* const __restrict)(b.data() + block)));

			_mm256_store_si256((__m256i* const __restrict)(a.data() + block), _mm256_xor_si256(xmA, xmB));
		}

		return(a);
	}
};

template<size_t const Length>
bit_function size_t const bit_row<Length>::population_count() const
{
	size_t count(0);
	for (size_t block = 0; block < Stride; ++block) {

		count += _mm_popcnt_u64(_bits[block]);
	}
	return(count);
}

template<size_t const Length>
bit_function bool const bit_row<Length>::read_bit(size_t const index) const
{
	size_t const block(index >> element_bits);
	size_t const bit(1ull << (index & (element_count - 1ull))); // remainder, divisor is always power of two (element_count) "(index % 64) == (index & (64-1))"

	return((_bits[block] & bit) == bit);
}

template<size_t const Length>
bit_function void bit_row<Length>::set_bit(size_t const index)
{
	size_t const block(index >> element_bits);
	size_t const bit(1ull << (index & (element_count - 1ull))); // remainder, divisor is always power of two (element_count)

	// set
	_bits[block] |= bit;
}

template<size_t const Length>
bit_function void bit_row<Length>::clear_bit(size_t const index)
{
	size_t const block(index >> element_bits);
	size_t const bit(1ull << (index & (element_count - 1ull))); // remainder, divisor is always power of two (element_count)

	// clear
	_bits[block] &= (~bit);
}

template<size_t const Length>
bit_function void bit_row<Length>::write_bit(size_t const index, bool const state)
{
	size_t const block(index >> element_bits);
	size_t const bit(1ull << (index & (element_count - 1ull))); // remainder, divisor is always power of two (element_count)

	// Conditionally set or clear bits without branching
	// https://graphics.stanford.edu/~seander/bithacks.html#ConditionalSetOrClearBitsWithoutBranching
	_bits[block] = (_bits[block] & ~bit) | (-state & bit);
}

template<size_t const Length>
void bit_row<Length>::clear()
{
	if constexpr (Size > 4096) {
		___memset_threaded<CACHE_LINE_BYTES>(_bits, 0, size());
	}
	else {
		memset(_bits, 0, size());
	}
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// bit_row_reference
template<size_t const Length>  // must be divisable by 64 //
struct no_vtable bit_row_reference
{
#ifdef BIT_ROW_ATOMIC
	using bits = std::atomic_size_t;
#else
	using bits = size_t;
#endif
	static constexpr size_t const element_bits = bit_row<Length>::element_bits;
	static constexpr size_t const element_count = bit_row<Length>::element_count;	// block size (64 = (1 << 6))

	static constexpr size_t const Stride = bit_row<Length>::Stride;	// number of blocks for each dimension
public:
	static constexpr size_t const total_bit_count = Length;
	static constexpr size_t const Size = Stride * sizeof(bits);

	static constexpr size_t const length() { return(Length); }
	static constexpr size_t const stride() { return(Stride); }

	bit_function bool const   read_bit(size_t const index) const;
	bit_function void		  set_bit(size_t const index);
	bit_function void		  clear_bit(size_t const index);
	bit_function void		  write_bit(size_t const index, bool const state);  // *use set_bit / clear_bit for best performance - avoids branching.

	bit_function bits const* const __restrict data() const { return(_bits); }
	bit_function bits* const __restrict		  data() { return(_bits); }

	constexpr size_t const size() const { return(stride * sizeof(bits)); }
private:
	bit_row_reference(bit_row<Length>& __restrict a, size_t const offset = 0)
		: _bits(a.data() + (offset >> element_bits)), _offset(offset & (element_count - 1ull)) // remainder, remeber a single bit represents a single index
	{}
private:
	bits* const            _bits;
	size_t const           _offset;  // *this is like a count, not like an index
public:
	// static public methods that should be used for construction
	__declspec(safebuffers) static inline bit_row_reference<Length> create(bit_row<Length>& __restrict a, size_t const offset)
	{
		return{ a, offset };
	}
};

template<size_t const Length>
bit_function bool const bit_row_reference<Length>::read_bit(size_t const index) const
{
	size_t const relative_index(index + _offset);
	size_t const block(relative_index >> element_bits);
	size_t const bit(1ull << (relative_index & (element_count - 1ull))); // remainder, divisor is always power of two (element_count) "(index % 64) == (index & (64-1))"

	return((_bits[block] & bit) == bit);
}

template<size_t const Length>
bit_function void bit_row_reference<Length>::set_bit(size_t const index)
{
	size_t const relative_index(index + _offset);
	size_t const block(relative_index >> element_bits);
	size_t const bit(1ull << (relative_index & (element_count - 1ull))); // remainder, divisor is always power of two (element_count)

	// set
	_bits[block] |= bit;
}

template<size_t const Length>
bit_function void bit_row_reference<Length>::clear_bit(size_t const index)
{
	size_t const relative_index(index + _offset);
	size_t const block(relative_index >> element_bits);
	size_t const bit(1ull << (relative_index & (element_count - 1ull))); // remainder, divisor is always power of two (element_count)

	// clear
	_bits[block] &= (~bit);
}

template<size_t const Length>
bit_function void bit_row_reference<Length>::write_bit(size_t const index, bool const state)
{
	size_t const relative_index(index + _offset);
	size_t const block(relative_index >> element_bits);
	size_t const bit(1ull << (relative_index & (element_count - 1ull))); // remainder, divisor is always power of two (element_count)

	// Conditionally set or clear bits without branching
	// https://graphics.stanford.edu/~seander/bithacks.html#ConditionalSetOrClearBitsWithoutBranching
	_bits[block] = (_bits[block] & ~bit) | (-state & bit);
}




