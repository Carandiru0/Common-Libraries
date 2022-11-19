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
#pragma warning( disable : 4804 )	// unsafe "-" on type bool
#include <tbb\cache_aligned_allocator.h>
#include <Utility/mem.h>
#include <Utility/class_helper.h>
#include <atomic>

#define bit_function __declspec(safebuffers) __forceinline

template<size_t const Width, size_t const Height>  // All components must be divisable by 64 //
struct no_vtable bit_plane
{
#ifdef BIT_VOLUME_ATOMIC
	using bits = std::atomic_size_t;
#else
	using bits = size_t;
#endif

	static constexpr size_t const element_bits = 6;
	static constexpr size_t const element_count = (1 << element_bits);	// block size (64 = (1 << 6))
	static_assert((0 == Width % element_count) && (0 == Height % element_count), "bit_plane dimensions are invalid");

	static constexpr size_t const stride = { (Width * Height ) / element_count };	// number of blocks for each dimension
public:
	static constexpr size_t const total_bit_count = Width * Height;
	static constexpr size_t const Size = stride * sizeof(bits);
	
	static constexpr size_t const width() { return(Width); }
	static constexpr size_t const height() { return(Height); }
	
	STATIC_INLINE_PURE size_t const get_index(size_t const x, size_t const y);

	bit_function bool const   read_bit(size_t const index) const;
	bit_function bool const   read_bit(size_t const x, size_t const y) const;
	bit_function void		  set_bit(size_t const index);
	bit_function void		  set_bit(size_t const x, size_t const y);
	bit_function void		  clear_bit(size_t index);
	bit_function void		  clear_bit(size_t const x, size_t const y);
	bit_function void		  write_bit(size_t const index, bool const state);  // *use set_bit / clear_bit for best performance - avoids branching.
	bit_function void		  write_bit(size_t const x, size_t const y, bool const state);  // *use set_bit / clear_bit for best performance - avoids branching.

	bit_function bits const* const __restrict data() const { return(_bits); }
	bit_function bits* const __restrict		  data() { return(_bits); }

	constexpr size_t const size() const { return(Size); }
	
	void clear();

private:
	alignas(CACHE_LINE_BYTES) bits			_bits[stride] = {};
public:
	// static public methods that should be used for construction/destruction of bit_plane on the heap. These provide alignment support on a cacheline to avoid some false sharing.
	__declspec(safebuffers) static inline bit_plane<Width, Height>* const __restrict create()
	{
		tbb::cache_aligned_allocator< bit_plane<Width, Height> > allocator;
		bit_plane<Width, Height>* const __restrict pReturn(static_cast<bit_plane<Width, Height>*const __restrict>(allocator.allocate(1)));

		pReturn->clear();

		return(pReturn);
	}
	__declspec(safebuffers) static inline void destroy(bit_plane<Width, Height>* plane)
	{
		if (plane) {

			tbb::cache_aligned_allocator< bit_plane<Width, Height> > allocator;
			allocator.deallocate(plane, 0);
			plane = nullptr;
		}
	}
};
template<size_t const Width, size_t const Height>
__inline __declspec(noalias) size_t const bit_plane<Width, Height>::get_index(size_t const x, size_t const y)
{
	// slices ordered by Y: <---- USING Y
	// (y * xMax) + x;
	return((y * Width) + x);
}

template<size_t const Width, size_t const Height>
bit_function bool const bit_plane<Width, Height>::read_bit(size_t const index) const
{
	size_t const block(index >> element_bits);
	size_t const bit(1ull << (index & (element_count - 1))); // remainder, divisor is always power of two (element_count) "(index % 64) == (index & (64-1))"

	return((_bits[block] & bit) == bit);
}
template<size_t const Width, size_t const Height>
bit_function bool const bit_plane<Width, Height>::read_bit(size_t const x, size_t const y) const
{
	return(read_bit(get_index(x,y)));
}

template<size_t const Width, size_t const Height>
bit_function void bit_plane<Width, Height>::set_bit(size_t const index)
{
	size_t const block(index >> element_bits);
	size_t const bit(1ull << (index & (element_count - 1))); // remainder, divisor is always power of two (element_count)

	// set
	_bits[block] |= bit;
}
template<size_t const Width, size_t const Height>
bit_function void bit_plane<Width, Height>::set_bit(size_t const x, size_t const y)
{
	set_bit(get_index(x, y));
}

template<size_t const Width, size_t const Height>
bit_function void bit_plane<Width, Height>::clear_bit(size_t const index)
{
	size_t const block(index >> element_bits);
	size_t const bit(1ull << (index & (element_count - 1))); // remainder, divisor is always power of two (element_count)

	// clear
	_bits[block] &= (~bit);
}
template<size_t const Width, size_t const Height>
bit_function void bit_plane<Width, Height>::clear_bit(size_t const x, size_t const y)
{
	clear_bit(get_index(x, y));
}

template<size_t const Width, size_t const Height>
bit_function void bit_plane<Width, Height>::write_bit(size_t const index, bool const state)
{
	size_t const block(index >> element_bits);
	size_t const bit(1ull << (index & (element_count - 1ull))); // remainder, divisor is always power of two (element_count)

	// Conditionally set or clear bits without branching
	// https://graphics.stanford.edu/~seander/bithacks.html#ConditionalSetOrClearBitsWithoutBranching
	_bits[block] = (_bits[block] & ~bit) | (-state & bit);
}
template<size_t const Width, size_t const Height>
bit_function void bit_plane<Width, Height>::write_bit(size_t const x, size_t const y, bool const state)
{
	write_bit(get_index(x, y), state);
}

template<size_t const Width, size_t const Height>
void bit_plane<Width, Height>::clear()
{
	if constexpr (Size > 4096) {
		___memset_threaded<alignof(bits)>(_bits, 0, size());
	}
	else {
		memset(_bits, 0, size());
	}
}