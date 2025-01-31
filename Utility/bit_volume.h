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

#pragma intrinsic(memset)

#ifndef bit_function
#define bit_function __declspec(safebuffers) __forceinline
#endif

template<size_t const Width, size_t const Height, size_t const Depth, typename bits = size_t>  // must be divisable by 64 and either  size_t  -or-  std::atomic_size_t //
struct no_vtable bit_volume
{
	static constexpr size_t const element_bits = 6;
	static constexpr size_t const element_count = (1 << element_bits);	// block size (64 = (1 << 6))
	static_assert((0 == Width % element_count) && (0 == Height % element_count) && (0 == Depth % element_count), "bit_volume dimensions are invalid");

	static constexpr size_t const Stride = { (Width * Height * Depth) / element_count };	// number of blocks for each dimension
public:
	static constexpr size_t const total_bit_count = Width * Height * Depth;
	static constexpr size_t const Size = Stride * sizeof(bits);
	
	static constexpr size_t const width() { return(Width); }
	static constexpr size_t const height() { return(Height); }
	static constexpr size_t const depth() { return(Depth); }
	static constexpr size_t const stride() { return(Stride); }

	STATIC_INLINE_PURE size_t const get_index(size_t const x, size_t const y, size_t const z);

	bit_function bool const   read_bit(size_t const index) const;
	bit_function bool const   read_bit(size_t const x, size_t const y, size_t const z) const;
	bit_function void		  set_bit(size_t const index);
	bit_function void		  set_bit(size_t const x, size_t const y, size_t const z);
	bit_function void		  clear_bit(size_t index);
	bit_function void		  clear_bit(size_t const x, size_t const y, size_t const z);
	bit_function void		  write_bit(size_t const index, bool const state);  // *use set_bit / clear_bit for best performance - avoids branching.
	bit_function void		  write_bit(size_t const x, size_t const y, size_t const z, bool const state);  // *use set_bit / clear_bit for best performance - avoids branching.

	bit_function bits const* const __restrict data() const { return(_bits); }
	bit_function bits* const __restrict		  data() { return(_bits); }

	constexpr size_t const size() const { return(Size); }
	
	void clear();

private:
	alignas(CACHE_LINE_BYTES) bits			_bits[Stride] = {};
public:
	// static public methods that should be used for construction/destruction of bit_volume on the heap. These provide alignment support on a cacheline to avoid some false sharing.
	__declspec(safebuffers) static inline bit_volume<Width, Height, Depth, bits>* const __restrict create()
	{
		tbb::cache_aligned_allocator< bit_volume<Width, Height, Depth, bits> > allocator;
		bit_volume<Width, Height, Depth, bits>* const __restrict pReturn(static_cast<bit_volume<Width, Height, Depth, bits>*const __restrict>(allocator.allocate(1)));

		pReturn->clear();

		return(pReturn);
	}
	__declspec(safebuffers) static inline void destroy(bit_volume<Width, Height, Depth, bits>* volume)
	{
		if (volume) {

			tbb::cache_aligned_allocator< bit_volume<Width, Height, Depth, bits> > allocator;
			allocator.deallocate(volume, 0);
			volume = nullptr;
		}
	}
};
template<size_t const Width, size_t const Height, size_t const Depth, typename bits>
__inline __declspec(noalias) size_t const bit_volume<Width, Height, Depth, bits>::get_index(size_t const x, size_t const y, size_t const z)
{
	// slices ordered by Y: <---- USING Y
	// (y * xMax * zMax) + (z * xMax) + x;
	return((y * Width * Depth) + (z * Width) + x);
}

template<size_t const Width, size_t const Height, size_t const Depth, typename bits>
bit_function bool const bit_volume<Width, Height, Depth, bits>::read_bit(size_t const index) const
{
	size_t const block(index >> element_bits);
	size_t const bit(1ull << (index & (element_count - 1))); // remainder, divisor is always power of two (element_count) "(index % 64) == (index & (64-1))"

	return((_bits[block] & bit) == bit);
}
template<size_t const Width, size_t const Height, size_t const Depth, typename bits>
bit_function bool const bit_volume<Width, Height, Depth, bits>::read_bit(size_t const x, size_t const y, size_t const z) const
{
	return(read_bit(get_index(x,y,z)));
}

template<size_t const Width, size_t const Height, size_t const Depth, typename bits>
bit_function void bit_volume<Width, Height, Depth, bits>::set_bit(size_t const index)
{
	size_t const block(index >> element_bits);
	size_t const bit(1ull << (index & (element_count - 1))); // remainder, divisor is always power of two (element_count)

	// set
	_bits[block] |= bit;
}
template<size_t const Width, size_t const Height, size_t const Depth, typename bits>
bit_function void bit_volume<Width, Height, Depth, bits>::set_bit(size_t const x, size_t const y, size_t const z)
{
	set_bit(get_index(x, y, z));
}

template<size_t const Width, size_t const Height, size_t const Depth, typename bits>
bit_function void bit_volume<Width, Height, Depth, bits>::clear_bit(size_t const index)
{
	size_t const block(index >> element_bits);
	size_t const bit(1ull << (index & (element_count - 1))); // remainder, divisor is always power of two (element_count)

	// clear
	_bits[block] &= (~bit);
}
template<size_t const Width, size_t const Height, size_t const Depth, typename bits>
bit_function void bit_volume<Width, Height, Depth, bits>::clear_bit(size_t const x, size_t const y, size_t const z)
{
	clear_bit(get_index(x, y, z));
}

template<size_t const Width, size_t const Height, size_t const Depth, typename bits>
bit_function void bit_volume<Width, Height, Depth, bits>::write_bit(size_t const index, bool const state)
{
	size_t const block(index >> element_bits);
	size_t const bit(1ull << (index & (element_count - 1ull))); // remainder, divisor is always power of two (element_count)

	// Conditionally set or clear bits without branching
	// https://graphics.stanford.edu/~seander/bithacks.html#ConditionalSetOrClearBitsWithoutBranching
	_bits[block] = (_bits[block] & ~bit) | (-state & bit);
}
template<size_t const Width, size_t const Height, size_t const Depth, typename bits>
bit_function void bit_volume<Width, Height, Depth, bits>::write_bit(size_t const x, size_t const y, size_t const z, bool const state)
{
	write_bit(get_index(x, y, z), state);
}

template<size_t const Width, size_t const Height, size_t const Depth, typename bits>
void bit_volume<Width, Height, Depth, bits>::clear()
{
	if constexpr (Size > 4096) {
		___memset_threaded<CACHE_LINE_BYTES>(_bits, 0, size());
	}
	else {
		memset(_bits, 0, size());
	}
}


// atomic version
template<size_t const Width, size_t const Height, size_t const Depth>
using bit_volume_atomic = bit_volume<Width, Height, Depth, std::atomic_size_t>;





