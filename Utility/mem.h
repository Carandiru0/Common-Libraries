/* Copyright (C) 2022 Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * 
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

// this is the new memory file ( ___memcpy_threaded & ___memset_threaded )

// *** important note about using memcpy and memset, and these functions, when suitable, are better ***
// https://msrc-blog.microsoft.com/2021/01/11/building-faster-amd64-memset-routines/
// Uncached memory Performance
// "A partner noticed that memset performance when operating on uncached memory was severely degraded for very large sizes. 
// On the CPU’s we tested, SSE stores are 16x faster than using enhanced “rep stosb”. 
// This indicates that on uncached memory, “rep stosb” stores one byte at a time. It is very fast with cached or write back memory only."
// "As there is no way for memset to determine what kind of memory it is writing to, we decided to leave the code as - is. Drivers that need to zero large regions of uncached memory quickly can either write their own SSE loop....."

// *************************************************************************************************************************************************************************
// only large clears and copies ( > 4KB )
// *************************************************************************************************************************************************************************

// I have measured the performance of ___memcpy_threaded & ___memset_threaded
// intrinsic memcpy and intrinsic memset are fastest on a single thread. 
// only see a benefit when multithreaded.
// ___memcpy_threaded & ___memset_threaded are far faster and maximize bandwidth usage. the interal functions that these methods use are faster than threading intrinsic memcpy and intrinsic memset. However the internal functions are slower if they are not multithreaded.
// Very large copies, where the LATENCY to START ___memcpy_threaded or ___memset_threaded is LESS than the time it takes to do an equivalent single threaded intrinsic memcpy or intrinsic memset, a LARGE speed up is obtained.
// eg.) I measured:
//					intrinsic memcpy (single threaded) at 22 GB/s
//				    intrinsic memcpy (multithreaded) at 29 GB/s
//					___memcpy_threaded at 33.3 GB/s (memory bandwidth is saturated)
// 
// ___memset_threaded has the same 33.3 GB/s, but in this case it's all write bandwidth (wow) vs. ___memcpy_threaded which is a combined [read 10 GB/s + write 23.3 GB/s] (33.3 GB/s total)
// to see the actual upper limit for memory bandwidth, I need a faster computer.
// *all measured results obtained with pcm (performance ccounter monitor) utility in real-time, very accurate and no cpu power throttling was observed.

#pragma once
#include "..\Math\superfastmath.h"

// ***********##############  use scalable_aligned_malloc / scalable_aligned_free to guarentee aligment ###################************* //
// note: alignas with usage of new() does not guarentee alignment
#pragma intrinsic(_mm_mfence)
#pragma intrinsic(__faststorefence) // https://docs.microsoft.com/en-us/cpp/intrinsics/faststorefence?view=msvc-160 -- The effect is comparable to but faster than the _mm_mfence intrinsic on all x64 platforms. Not faster than _mm_sfence.
#pragma intrinsic(_mm_sfence)
#pragma intrinsic(_mm_prefetch)
#pragma intrinsic(memcpy)	// ensure intrinsics are used for un-aligned data
#pragma intrinsic(memset)	// ensure intrinsics are used for un-aligned data
#pragma intrinsic(__movsb)  

static constexpr size_t const CACHE_LINE_BYTES = 64ull;
static constexpr size_t const MINIMUM_PAGE_SIZE = 4096ull; // Windows Only, cannot be changed by user //

#define __PURE __declspec(noalias)
#define __SAFE_BUF __declspec(safebuffers)
#define INLINE_MEMFUNC static __inline __PURE __SAFE_BUF void __vectorcall

// **** note all -streaming- memory functions assume a minimum alignment of 16bytes **** //
// **** alignment should be supplied explicitly for safety ****

// [fences] //
INLINE_MEMFUNC ___streaming_store_fence() { _mm_sfence(); }

// only large clears and copies ( > 4KB )
// IF IN DOUBT, USE THE INTRINSIC MEMSET AND MEMCPY routines
// *** specific usage only ***

// [very large copies]
template<size_t const alignment>
INLINE_MEMFUNC ___memcpy_threaded(void* const __restrict dest, void const* const __restrict src, size_t const bytes, size_t const block_size = (MINIMUM_PAGE_SIZE));        // defaults to 4KB block/thread, customize based on total size *must be power of 2*
template<size_t const alignment>                                                                                                                                            // for best results, data should be aligned (minimum 16)
INLINE_MEMFUNC ___memcpy_threaded_stream(void* const __restrict dest, void const* const __restrict src, size_t const bytes, size_t const block_size = (MINIMUM_PAGE_SIZE)); // best alignment is CACHE_LINE_BYTES (64) to mitigate false sharing!
																																									 
// [very large clears]
template<size_t const alignment>
INLINE_MEMFUNC ___memset_threaded(void* const __restrict dest, int const value, size_t const bytes, size_t const block_size = (MINIMUM_PAGE_SIZE));
template<size_t const alignment>
INLINE_MEMFUNC ___memset_threaded_stream(void* const __restrict dest, int const value, size_t const bytes, size_t const block_size = (MINIMUM_PAGE_SIZE));

// [virtual memory]
// for prefetching a memory into virtual memory, ensuring it is not paged from disk multiple times if said memory is accessed for example a memory mapped file
// this avoids a continous amount of page faults that cause delays of millions of cycles potentially by ensuring the data pointed to by memory is cached before the first access
INLINE_MEMFUNC ___prefetch_vmem(void const* const __restrict address, size_t const bytes);


// [internal (private) functionality begins]
namespace internal_mem
{
	template<size_t const alignment>
	INLINE_MEMFUNC memset_stream(uint8_t* __restrict d, __m256i const xmValue, size_t bytes)
	{
		static constexpr size_t const element_size = sizeof(__m256i);

		{ // 128 bytes / iteration
			static constexpr size_t const chunk_size(element_size << 2);
#pragma loop( ivdep )
			for (; bytes >= chunk_size; bytes -= chunk_size) {

				// exploit both integer and floating point "uop ports"

				_mm256_stream_si256((__m256i * __restrict)d, xmValue);
				_mm256_stream_si256(((__m256i * __restrict)d) + 1, xmValue);
				_mm256_stream_ps((float* const __restrict)(((__m256 * __restrict)d) + 2), _mm256_castsi256_ps(xmValue));
				_mm256_stream_ps((float* const __restrict)(((__m256 * __restrict)d) + 3), _mm256_castsi256_ps(xmValue));

				d += chunk_size;
			}
		}

		{ // 64 bytes / iteration
			static constexpr size_t const chunk_size(element_size << 1);
#pragma loop( ivdep )
			for (; bytes >= chunk_size; bytes -= chunk_size) {

				// exploit both integer and floating point "uop ports"

				_mm256_stream_si256((__m256i * __restrict)d, xmValue);
				_mm256_stream_ps((float* const __restrict)(((__m256 * __restrict)d) + 1), _mm256_castsi256_ps(xmValue));

				d += chunk_size;
			}
		}

		{ // 32 bytes / iteration
			static constexpr size_t const chunk_size(element_size);
#pragma loop( ivdep )
			for (; bytes >= chunk_size; bytes -= chunk_size) {

				_mm256_stream_si256((__m256i * __restrict)d, xmValue);

				d += chunk_size;
			}
		}

		{ // 16 bytes / iteration
			static constexpr size_t const chunk_size(element_size >> 1);
#pragma loop( ivdep )
			for (; bytes >= chunk_size; bytes -= chunk_size) {

				_mm_stream_si128((__m128i * __restrict)d, _mm256_castsi256_si128(xmValue));

				d += chunk_size;
			}
		}
	}

	template<size_t const alignment>
	INLINE_MEMFUNC memcpy_stream_load(uint8_t* __restrict d, uint8_t const* __restrict s, size_t bytes)
	{
		static constexpr size_t const element_size = sizeof(__m256i);

		{ // 128 bytes / iteration
			static constexpr size_t const chunk_size(element_size << 2);
#pragma loop( ivdep )
			for (; bytes >= chunk_size; bytes -= chunk_size) {

				__m256i const
					r0(_mm256_stream_load_si256((__m256i * __restrict)s)),
					r1(_mm256_stream_load_si256(((__m256i * __restrict)s) + 1));
				__m256 const
					r2(_mm256_castsi256_ps(_mm256_stream_load_si256(((__m256i * __restrict)s) + 2))),
					r3(_mm256_castsi256_ps(_mm256_stream_load_si256(((__m256i * __restrict)s) + 3)));

				_mm_prefetch((const char*)(s + chunk_size), _MM_HINT_NTA); // prefetch next

				// exploit both integer and floating point "uop ports"

				_mm256_store_si256((__m256i * __restrict)d, r0);
				_mm256_store_si256(((__m256i * __restrict)d) + 1, r1);
				_mm256_store_ps((float* const __restrict)(((__m256 * __restrict)d) + 2), r2);
				_mm256_store_ps((float* const __restrict)(((__m256 * __restrict)d) + 3), r3);

				d += chunk_size;
				s += chunk_size;
			}
		}

		{ // 64 bytes / iteration
			static constexpr size_t const chunk_size(element_size << 1);
#pragma loop( ivdep )
			for (; bytes >= chunk_size; bytes -= chunk_size) {

				__m256i const
					r0(_mm256_stream_load_si256((__m256i * __restrict)s));
				__m256 const
					r1(_mm256_castsi256_ps(_mm256_stream_load_si256(((__m256i * __restrict)s) + 1)));

				// exploit both integer and floating point "uop ports"

				_mm256_store_si256((__m256i * __restrict)d, r0);
				_mm256_store_ps((float* const __restrict)(((__m256 * __restrict)d) + 1), r1);

				d += chunk_size;
				s += chunk_size;
			}
		}

		{ // 32 bytes / iteration
			static constexpr size_t const chunk_size(element_size);
#pragma loop( ivdep )
			for (; bytes >= chunk_size; bytes -= chunk_size) {

				_mm256_store_si256((__m256i * __restrict)d, _mm256_stream_load_si256((__m256i * __restrict)s));

				d += chunk_size;
				s += chunk_size;
			}
		}

		{ // 16 bytes / iteration
			static constexpr size_t const chunk_size(element_size >> 1);
#pragma loop( ivdep )
			for (; bytes >= chunk_size; bytes -= chunk_size) {

				_mm_store_si128((__m128i * __restrict)d, _mm_stream_load_si128((__m128i * __restrict)s));

				d += chunk_size;
				s += chunk_size;
			}
		}
	}

	template<size_t const alignment>
	INLINE_MEMFUNC memcpy_stream_store(uint8_t* __restrict d, uint8_t const* __restrict s, size_t bytes)
	{
		static constexpr size_t const element_size = sizeof(__m256i);

		{ // 128 bytes / iteration
			static constexpr size_t const chunk_size(element_size << 2);
#pragma loop( ivdep )
			for (; bytes >= chunk_size; bytes -= chunk_size) {

				__m256i const
					r0(_mm256_load_si256((__m256i * __restrict)s)),
					r1(_mm256_load_si256(((__m256i * __restrict)s) + 1));
				__m256 const
					r2(_mm256_castsi256_ps(_mm256_load_si256(((__m256i * __restrict)s) + 2))),
					r3(_mm256_castsi256_ps(_mm256_load_si256(((__m256i * __restrict)s) + 3)));

				_mm_prefetch((const char*)(s + chunk_size), _MM_HINT_T0); // prefetch next

				// exploit both integer and floating point "uop ports"

				_mm256_stream_si256(((__m256i * __restrict)d), r0);
				_mm256_stream_si256(((__m256i * __restrict)d) + 1, r1);
				_mm256_stream_ps((float* __restrict)(((__m256 * __restrict)d) + 2), r2);
				_mm256_stream_ps((float* __restrict)(((__m256 * __restrict)d) + 3), r3);

				d += chunk_size;
				s += chunk_size;
			}
		}

		{ // 64 bytes / iteration
			static constexpr size_t const chunk_size(element_size << 1);
#pragma loop( ivdep )
			for (; bytes >= chunk_size; bytes -= chunk_size) {

				__m256i const
					r0(_mm256_load_si256((__m256i * __restrict)s));
				__m256 const
					r1(_mm256_castsi256_ps(_mm256_load_si256(((__m256i * __restrict)s) + 1)));

				// exploit both integer and floating point "uop ports"

				_mm256_stream_si256((__m256i * __restrict)d, r0);
				_mm256_stream_ps((float* __restrict)(((__m256* __restrict)d) + 1), r1);

				d += chunk_size;
				s += chunk_size;
			}
		}

		{ // 32 bytes / iteration
			static constexpr size_t const chunk_size(element_size);
#pragma loop( ivdep )
			for (; bytes >= chunk_size; bytes -= chunk_size) {

				_mm256_stream_si256((__m256i * __restrict)d, _mm256_load_si256((__m256i * __restrict)s));

				d += chunk_size;
				s += chunk_size;
			}
		}

		{ // 16 bytes / iteration
			static constexpr size_t const chunk_size(element_size >> 1);
#pragma loop( ivdep )
			for (; bytes >= chunk_size; bytes -= chunk_size) {

				_mm_stream_si128((__m128i * __restrict)d, _mm_load_si128((__m128i * __restrict)s));

				d += chunk_size;
				s += chunk_size;
			}
		}
	}

	static constexpr size_t const _cache_size = MINIMUM_PAGE_SIZE;
	// *bugfix - do not move this alignas()
	alignas(CACHE_LINE_BYTES) constinit extern __declspec(selectany) inline thread_local uint8_t _streaming_cache[_cache_size]{};	// 4KB reserved/thread

	template<size_t const alignment>
	INLINE_MEMFUNC memcpy_stream(uint8_t* __restrict d, uint8_t const* __restrict s, size_t bytes)
	{
		static constexpr size_t const element_size = sizeof(__m256i);
		static constexpr size_t const block_size = internal_mem::_cache_size;

		// 4096 bytes / iteration
		for (; bytes >= block_size; bytes -= block_size) {

			_mm_mfence(); // isolates streaming loads from streaming stores

			_mm_prefetch((const char*)s, _MM_HINT_NTA);
			memcpy_stream_load<alignment>(internal_mem::_streaming_cache, s, block_size);

			_mm_mfence(); // isolates streaming loads from streaming stores

			_mm_prefetch((const char*)internal_mem::_streaming_cache, _MM_HINT_T0);
			memcpy_stream_store<alignment>(d, internal_mem::_streaming_cache, block_size);

			d += block_size; s += block_size;
		}

		// residual block bytes, < 4096 bytes
		if (bytes) {

			_mm_mfence(); // isolates streaming loads from streaming stores

			_mm_prefetch((const char*)s, _MM_HINT_NTA);
			memcpy_stream_load<alignment>(internal_mem::_streaming_cache, s, bytes);

			_mm_mfence(); // isolates streaming loads from streaming stores

			_mm_prefetch((const char*)internal_mem::_streaming_cache, _MM_HINT_T0);
			memcpy_stream_store<alignment>(d, internal_mem::_streaming_cache, bytes);
		}

	}
} // end ns
// [internal (private) functionality ends]

// [implementation (public) functionality begins]

// [very large copies] *for all other uses besides write combined memory*
template<size_t const alignment>
INLINE_MEMFUNC ___memcpy_threaded(void* const __restrict dest, void const* const __restrict src, size_t const bytes, size_t const block_size)
{
	tbb::parallel_for(
		tbb::blocked_range<__m256i const* __restrict>((__m256i* const __restrict)src, (__m256i* const __restrict)(((uint8_t const* const __restrict)src) + bytes), SFM::roundToMultipleOf<false>((int64_t)block_size, CACHE_LINE_BYTES)), // rounding down blocksize to prevent overlapping cachelines (false sharing)
		[&](tbb::blocked_range<__m256i const* __restrict> block) {

			ptrdiff_t const offset(((uint8_t const* const __restrict)block.begin()) - ((uint8_t const* const __restrict)src));
	        ptrdiff_t const current_block_size(((uint8_t const* const __restrict)block.end()) - ((uint8_t const* const __restrict)block.begin())); // required since last block can be partial 

	        memcpy((((uint8_t* const __restrict)dest) + offset), (uint8_t const* const __restrict)block.begin(), (size_t const)current_block_size); // intrinsic
		}
	);
}

// [very large copies] *to write combined memory*
template<size_t const alignment>
INLINE_MEMFUNC ___memcpy_threaded_stream(void* const __restrict dest, void const* const __restrict src, size_t const bytes, size_t const block_size)
{
	tbb::parallel_for(
		tbb::blocked_range<__m256i const* __restrict>((__m256i* const __restrict)src, (__m256i* const __restrict)(((uint8_t const* const __restrict)src) + bytes), SFM::roundToMultipleOf<false>((int64_t)block_size, CACHE_LINE_BYTES)), // rounding down blocksize to prevent overlapping cachelines (false sharing)
		[&](tbb::blocked_range<__m256i const* __restrict> block) {

			ptrdiff_t const offset(((uint8_t const* const __restrict)block.begin()) - ((uint8_t const* const __restrict)src));
			ptrdiff_t const current_block_size(((uint8_t const* const __restrict)block.end()) - ((uint8_t const* const __restrict)block.begin())); // required since last block can be partial 

			internal_mem::memcpy_stream<alignment>((((uint8_t* const __restrict)dest) + offset), (uint8_t const* const __restrict)block.begin(), (size_t const)current_block_size); // streaming custom memcpy achieves upto 16x improvement when used with write combined memory destination
		}
	);
}

// [very large clears] *for all other uses besides write combined memory*
template<size_t const alignment>
INLINE_MEMFUNC ___memset_threaded(void* const __restrict dest, int const value, size_t const bytes, size_t const block_size)
{
	tbb::parallel_for(
		tbb::blocked_range<__m256i const* __restrict>((__m256i* const __restrict)dest, (__m256i* const __restrict)(((uint8_t const* const __restrict)dest) + bytes), SFM::roundToMultipleOf<false>((int64_t)block_size, CACHE_LINE_BYTES)), // rounding down blocksize to prevent overlapping cachelines (false sharing)
		[&](tbb::blocked_range<__m256i const* __restrict> block) {

			ptrdiff_t const offset(((uint8_t const* const __restrict)block.begin()) - ((uint8_t const* const __restrict)dest));
			ptrdiff_t const current_block_size(((uint8_t const* const __restrict)block.end()) - ((uint8_t const* const __restrict)block.begin())); // required since last block can be partial 

			memset((((uint8_t* const __restrict)dest) + offset), value, (size_t const)current_block_size); // intrinsic
		}
	);
}

// [very large clears] *to write combined memory*
template<size_t const alignment>
INLINE_MEMFUNC ___memset_threaded_stream(void* const __restrict dest, int const value, size_t const bytes, size_t const block_size)
{
	tbb::parallel_for(
		tbb::blocked_range<__m256i const* __restrict>((__m256i* const __restrict)dest, (__m256i* const __restrict)(((uint8_t const* const __restrict)dest) + bytes), SFM::roundToMultipleOf<false>((int64_t)block_size, CACHE_LINE_BYTES)), // rounding down blocksize to prevent overlapping cachelines (false sharing)
		[&](tbb::blocked_range<__m256i const* __restrict> block) {

			ptrdiff_t const offset(((uint8_t const* const __restrict)block.begin()) - ((uint8_t const* const __restrict)dest));
	        ptrdiff_t const current_block_size(((uint8_t const* const __restrict)block.end()) - ((uint8_t const* const __restrict)block.begin())); // required since last block can be partial 

	        internal_mem::memset_stream<alignment>((((uint8_t* const __restrict)dest) + offset), _mm256_set1_epi32(value), (size_t const)current_block_size); // streaming custom memset achieves upto 16x improvement when used with write combined memory destination
		}
	);
}

INLINE_MEMFUNC  ___prefetch_vmem(void const* const __restrict address, size_t const bytes)
{
	WIN32_MEMORY_RANGE_ENTRY entry{ const_cast<PVOID>(address), bytes };

	PrefetchVirtualMemory(GetCurrentProcess(), 1, &entry, 0);
}


