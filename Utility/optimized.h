#pragma once
// #################### GENERAL VERSION ##################### //
#include <intrin.h>
#include <xmmintrin.h>
#include <pmmintrin.h>
#include <immintrin.h>


#pragma intrinsic(memcpy)	// ensure intrinsics are used for un-aligned data
#pragma intrinsic(memset)	// ensure intrinsics are used for un-aligned data
// this is for pointers only, so compiler can optimize accordingly if underlying data is known to be aligned
#pragma intrinsic(__builtin_assume_aligned)
#define __builtin_assume_aligned(type, c, alignment) { const_cast<type* __restrict& __restrict>(c) = static_cast<type* const __restrict>(::__builtin_assume_aligned(c, alignment)); }


// ***********##############  use scalable_aligned_malloc / scalable_aligned_free to guarentee aligment ###################************* //
// note: alignas with usage of new() does not guarentee alignment

#include <stdint.h>

static constexpr size_t const CACHE_LINE_BYTES = 128ULL;

#define __PURE __declspec(noalias)
#define __SAFE_BUF __declspec(safebuffers)

// pre-compiled with optimizations on in debug builds only for optimize.cpp
// current workaround for dog-slow for loops and copying / setting data
// fixes the huge degradation of speed while debuging, release mode is unaffected by this bug

// further specialization for extreme optimization purposes only

#define INTRINSIC_MEM_ALLOC_FUNC extern inline __PURE __SAFE_BUF void* const __restrict __vectorcall
#define INTRINSIC_MEMFUNC extern inline __PURE __SAFE_BUF void __vectorcall
#define INLINE_MEMFUNC static __forceinline __PURE __SAFE_BUF void __vectorcall

// [fences] //
INLINE_MEMFUNC __streaming_store_fence();

// [clears]
INTRINSIC_MEMFUNC __memclr_aligned_32_stream(uint8_t* const __restrict dest, size_t const bytes);
INTRINSIC_MEMFUNC __memclr_aligned_32_store(uint8_t* const __restrict dest, size_t const bytes);
INTRINSIC_MEMFUNC __memclr_aligned_16_stream(uint8_t* const __restrict dest, size_t const bytes);
INTRINSIC_MEMFUNC __memclr_aligned_16_store(uint8_t* const __restrict dest, size_t const bytes);

// [copies]
INTRINSIC_MEMFUNC __memcpy_aligned_32_stream(uint8_t* __restrict dest, uint8_t const* __restrict src, size_t bytes);
INTRINSIC_MEMFUNC __memcpy_aligned_32_store(uint8_t* __restrict dest, uint8_t const* __restrict src, size_t bytes);
INTRINSIC_MEMFUNC __memcpy_aligned_16_stream(uint8_t* __restrict dest, uint8_t const* __restrict src, size_t bytes);
INTRINSIC_MEMFUNC __memcpy_aligned_16_store(uint8_t* __restrict dest, uint8_t const* __restrict src, size_t bytes);


// [large allocations] 
// requiring physical memory and no page faults (never swapped to page file)
// Important Notes:
// SetProcessWorkingSetSize() Windows API function should be called if number of pages in program are to exceed the defaults provided to the process by the OS
// only for large allocations which are always allocated as pages of a minimum size, so there is always large overhead
// should never be done during real-time processing, only at initialization stages
// *** memory is intended for duration of program *** 
// *** does NOT need to be deleted/freed ***
// *** automagically freed by OS when process exits *** ///
INTRINSIC_MEM_ALLOC_FUNC __memalloc_large(size_t const bytes, size_t const alignment = 0ULL);

// templated versions that automatically resolve streaming or storing variant for when at compile time the number of elements is known
static constexpr size_t const MEM_CACHE_LIMIT = (1ULL << 15ULL); // (set for 32KB - size of L1 cache on Intel Haswell processors)

#define PUBLIC_MEMFUNC static __forceinline __declspec(noalias) __SAFE_BUF void __vectorcall

// ########### CLEAR ############ //

template<size_t const numelements, bool const stream = false, typename T>
PUBLIC_MEMFUNC __memclr_aligned_32(T* const __restrict dest)					// nunmelements known at compile time allows check for stream at compile time
{
	constexpr size_t const size(sizeof(T) * numelements);

	if constexpr (stream || size > (MEM_CACHE_LIMIT))
	{
		__memclr_aligned_32_stream((uint8_t* const __restrict)dest, size);
	}
	else {

		__memclr_aligned_32_store((uint8_t* const __restrict)dest, size);
	}
}
template<bool const stream, typename T>
PUBLIC_MEMFUNC __memclr_aligned_32(T* const __restrict dest, size_t const size)	// must explicitly state stream / store
{
	if constexpr (stream)
	{
		__memclr_aligned_32_stream((uint8_t* const __restrict)dest, size);
	}
	else {

		__memclr_aligned_32_store((uint8_t* const __restrict)dest, size);
	}
}

template<size_t const numelements, bool const stream = false, typename T>
PUBLIC_MEMFUNC __memclr_aligned_16(T* const __restrict dest)					// nunmelements known at compile time allows check for stream at compile time
{
	constexpr size_t const size(sizeof(T) * numelements);

	if constexpr (stream || size > (MEM_CACHE_LIMIT))
	{
		__memclr_aligned_16_stream((uint8_t* const __restrict)dest, size);
	}
	else {

		__memclr_aligned_16_store((uint8_t* const __restrict)dest, size);
	}
}
template<bool const stream, typename T>
PUBLIC_MEMFUNC __memclr_aligned_16(T* const __restrict dest, size_t const size)	// must explicitly state stream / store
{
	if constexpr (stream)
	{
		__memclr_aligned_16_stream((uint8_t* const __restrict)dest, size);
	}
	else {

		__memclr_aligned_16_store((uint8_t* const __restrict)dest, size);
	}
}

// ########### COPY ############ //

template<size_t const numelements, bool const stream = false, typename T>	// nunmelements known at compile time allows check for stream at compile time
PUBLIC_MEMFUNC __memcpy_aligned_32(T* const __restrict dest, T const* const __restrict src)
{
	constexpr size_t const size(sizeof(T) * numelements);

	if constexpr (stream || size > (MEM_CACHE_LIMIT)) {

		__memcpy_aligned_32_stream((uint8_t* const __restrict)dest, (uint8_t const* const __restrict)src, size);
	}
	else {

		__memcpy_aligned_32_store((uint8_t* const __restrict)dest, (uint8_t const* const __restrict)src, size);
	}
}

template<bool const stream, typename T>	// must explicitly state stream / store
PUBLIC_MEMFUNC __memcpy_aligned_32(T* const __restrict dest, T const* const __restrict src, size_t const size) 
{
	if constexpr (stream) {
	
		__memcpy_aligned_32_stream((uint8_t* const __restrict)dest, (uint8_t const* const __restrict)src, size);
	}
	else {

		__memcpy_aligned_32_store((uint8_t* const __restrict)dest, (uint8_t const * const __restrict)src, size);
	}
}

template<size_t const numelements, bool const stream = false, typename T>	// nunmelements known at compile time allows check for stream at compile time
PUBLIC_MEMFUNC __memcpy_aligned_16(T* const __restrict dest, T const* const __restrict src)
{
	constexpr size_t const size(sizeof(T) * numelements);

	if constexpr (stream || size > (MEM_CACHE_LIMIT)) {

		__memcpy_aligned_16_stream((uint8_t* const __restrict)dest, (uint8_t const* const __restrict)src, size);
	}
	else {

		__memcpy_aligned_16_store((uint8_t* const __restrict)dest, (uint8_t const* const __restrict)src, size);
	}
}

template<bool const stream, typename T>	// must explicitly state stream / store
PUBLIC_MEMFUNC __memcpy_aligned_16(T* const __restrict dest, T const* const __restrict src, size_t const size)
{
	if constexpr (stream) {

		__memcpy_aligned_16_stream((uint8_t* const __restrict)dest, (uint8_t const* const __restrict)src, size);
	}
	else {

		__memcpy_aligned_16_store((uint8_t* const __restrict)dest, (uint8_t const* const __restrict)src, size);
	}
}

INLINE_MEMFUNC __streaming_store_fence()
{
	_mm_sfence();
}