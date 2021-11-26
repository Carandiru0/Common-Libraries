// intrinsic memcpy, memset fastest benchmarks run.
// this is the new memory file

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

static constexpr size_t const CACHE_LINE_BYTES = 64ull;
static constexpr size_t const MINIMUM_PAGE_SIZE = 4096ull; // Windows Only, cannot be changed by user //

#define __PURE __declspec(noalias)
#define __SAFE_BUF __declspec(safebuffers)
#define INLINE_MEMFUNC static __inline __PURE __SAFE_BUF void __vectorcall

// **** note all -streaming- memory functions assume a minimum alignment of 16bytes **** //
// **** alignment must be supplied explicitly for safety ****

// [fences] //
INLINE_MEMFUNC __streaming_store_fence();

// [clears]
/* use native intrinsic memset() for unaligned data, otherwise __memclr_stream & __memset_strean are better */
template<size_t const alignment>
INLINE_MEMFUNC __memclr_stream(void* const __restrict dest, size_t bytes);   // for streaming stores/non-temporal / write combined memory dest
template<size_t const alignment>
INLINE_MEMFUNC __memset_stream(void* const __restrict dest, int const value, size_t bytes);	// for streaming stores/non-temporal / write combined memory dest

// [copies]
/* use the native intrinsic memcpy() for small data - if it's a *large* copy __memcpy_stream is better. */
template<size_t const alignment>
INLINE_MEMFUNC __memcpy_stream(void* const __restrict dest, void const* const __restrict src, size_t bytes);	// for streaming stores/non-temporal larger than L2 cache size copies / write combined memory dest

// [very large copies]
/* use the native intrinsic memcpy() for small data - if it's a *very large* copy __memcpy_threaded is better. */
template<size_t const alignment>
INLINE_MEMFUNC __memcpy_threaded(void* const __restrict dest, void const* const __restrict src, size_t const bytes, size_t const block_size = (MINIMUM_PAGE_SIZE)); // defaults to 4KB block/thread, customize based on total size *must be power of 2*
																																	   // for best results, data should be aligned
																																					   // best alignment is CACHE_LINE_BYTES (64) to mitigate false sharing!
// [virtual memory]
// for prefetching a memory into virtual memory, ensuring it is not paged from disk multiple times if said memory is accessed for example a memory mapped file
// this avoids a continous amount of page faults that cause delays of millions of cycles potentially by ensuring the data pointed to by memory is cached before the first access
INLINE_MEMFUNC  __prefetch_vmem(void const* const __restrict address, size_t const bytes);


// [fences] //
INLINE_MEMFUNC __streaming_store_fence()
{
	_mm_sfence();
}

// [clears]] //

template<> // aligned to 16 bytes specialization
INLINE_MEMFUNC __memclr_stream<16>(void* const __restrict dest, size_t bytes)
{
	static constexpr size_t const element_size = sizeof(__m128i);

	alignas(16) uint8_t* __restrict d((uint8_t * __restrict)std::assume_aligned<16>(dest));

	__m128i const xmZero(_mm_setzero_si128());

	// 32 bytes / iteration
#pragma loop( ivdep )
	for (; bytes >= CACHE_LINE_BYTES; bytes -= (CACHE_LINE_BYTES >> 1)) {
		// expolit both integer and floating point "uop ports"
		_mm_stream_si128((__m128i* __restrict)d, xmZero);
		_mm_stream_ps((float* const __restrict)(((__m128*)d) + 1), _mm_castsi128_ps(xmZero));

		d += (CACHE_LINE_BYTES >> 1);
	}

	// 16 bytes / iteration
#pragma loop( ivdep )
	for (; bytes >= element_size; bytes -= element_size) {

		_mm_stream_si128((__m128i* __restrict)d, xmZero);

		d += element_size;
	}
}
template<size_t const alignment> // any other alignment greater than 16
INLINE_MEMFUNC __memclr_stream(void* const __restrict dest, size_t bytes)
{
	static constexpr size_t const element_size = sizeof(__m256i);

	alignas(alignment) uint8_t* __restrict d((uint8_t * __restrict)std::assume_aligned<alignment>(dest));

	__m256i const xmZero(_mm256_setzero_si256());

	// 64 bytes / iteration
#pragma loop( ivdep )
	for (; bytes >= CACHE_LINE_BYTES; bytes -= CACHE_LINE_BYTES) {
		// expolit both integer and floating point "uop ports"
		_mm256_stream_si256((__m256i* __restrict)d, xmZero);
		_mm256_stream_ps((float* const __restrict)(((__m256*)d) + 1), _mm256_castsi256_ps(xmZero));

		d += CACHE_LINE_BYTES;
	}

	// 32 bytes / iteration
#pragma loop( ivdep )
	for (; bytes >= element_size; bytes -= element_size) {

		_mm256_stream_si256((__m256i* __restrict)d, xmZero);

		d += element_size;
	}

	// 16 bytes / iteration
#pragma loop( ivdep )
	for (; bytes >= (element_size >> 1); bytes -= (element_size >> 1)) {

		_mm_stream_si128((__m128i* __restrict)d, _mm256_castsi256_si128(xmZero));

		d += (element_size >> 1);
	}
}

template<> // aligned to 16 bytes specialization
INLINE_MEMFUNC __memset_stream<16>(void* const __restrict dest, int const value, size_t bytes)
{
	static constexpr size_t const element_size = sizeof(__m128i);

	alignas(16) uint8_t* __restrict d((uint8_t * __restrict)std::assume_aligned<16>(dest));

	__m128i const xmValue(_mm_set1_epi32(value));

	// 32 bytes / iteration
#pragma loop( ivdep )
	for (; bytes >= CACHE_LINE_BYTES; bytes -= (CACHE_LINE_BYTES >> 1)) {
		// expolit both integer and floating point "uop ports"
		_mm_stream_si128((__m128i* __restrict)d, xmValue);
		_mm_stream_ps((float* const __restrict)(((__m128*)d) + 1), _mm_castsi128_ps(xmValue));

		d += (CACHE_LINE_BYTES >> 1);
	}

	// 16 bytes / iteration
#pragma loop( ivdep )
	for (; bytes >= element_size; bytes -= element_size) {

		_mm_stream_si128((__m128i* __restrict)d, xmValue);

		d += element_size;
	}
}
template<size_t const alignment> // any other alignment greater than 16
INLINE_MEMFUNC __memset_stream(void* const __restrict dest, int const value, size_t bytes)
{
	static constexpr size_t const element_size = sizeof(__m256i);

	alignas(alignment) uint8_t* __restrict d((uint8_t* __restrict)std::assume_aligned<alignment>(dest));

	__m256i const xmValue(_mm256_set1_epi32(value));

	// 64 bytes / iteration
#pragma loop( ivdep )
	for (; bytes >= CACHE_LINE_BYTES; bytes -= CACHE_LINE_BYTES) {
		// expolit both integer and floating point "uop ports"
		_mm256_stream_si256((__m256i*)d, xmValue);
		_mm256_stream_ps((float* const __restrict)(((__m256*)d) + 1), _mm256_castsi256_ps(xmValue));

		d += CACHE_LINE_BYTES;
	}

	// 32 bytes / iteration
#pragma loop( ivdep )
	for (; bytes >= element_size; bytes -= element_size) {

		_mm256_stream_si256((__m256i*)d, xmValue);

		d += element_size;
	}

	// 16 bytes / iteration
#pragma loop( ivdep )
	for (; bytes >= (element_size >> 1); bytes -= (element_size >> 1)) {

		_mm_stream_si128((__m128i*)d, _mm256_castsi256_si128(xmValue));

		d += (element_size >> 1);
	}
}

namespace internal_mem
{
	static constexpr size_t const _cache_size = MINIMUM_PAGE_SIZE;

	constinit extern __declspec(selectany) inline thread_local alignas(32) uint8_t _streaming_cache[_cache_size]{};	// 4KB reserved/thread

	// *internal only to be used with _streaming_cache as dest.
	template<size_t const alignment>
	INLINE_MEMFUNC __memcpy_stream_load(uint8_t* const __restrict dest, uint8_t const* const __restrict src, size_t bytes)
	{
		static constexpr size_t const element_size = sizeof(__m256i);

		alignas(CACHE_LINE_BYTES) uint8_t* __restrict d((uint8_t * __restrict)std::assume_aligned<CACHE_LINE_BYTES>(dest));
		alignas(alignment) uint8_t const* __restrict s((uint8_t const* __restrict)std::assume_aligned<alignment>(src));

		// 64 bytes / iteration
#pragma loop( ivdep )
		for (; bytes >= CACHE_LINE_BYTES; bytes -= CACHE_LINE_BYTES) {

			_mm_prefetch((const char*)s + CACHE_LINE_BYTES, _MM_HINT_NTA); // fetch next iteration NTA

			// expolit both integer and floating point "uop ports"
			_mm256_store_si256((__m256i*)d, _mm256_stream_load_si256((__m256i*)s));
			_mm256_store_ps((float* const __restrict)(((__m256*)d) + 1), _mm256_castsi256_ps(_mm256_stream_load_si256((((__m256i*)s) + 1))));

			d += CACHE_LINE_BYTES;
			s += CACHE_LINE_BYTES;
		}

		// 32 bytes / iteration
#pragma loop( ivdep )
		for (; bytes >= element_size; bytes -= element_size) {

			_mm256_store_si256((__m256i*)d, _mm256_stream_load_si256((__m256i*)s));

			d += element_size;
			s += element_size;
		}

		// 16 bytes / iteration
#pragma loop( ivdep )
		for (; bytes >= (element_size >> 1); bytes -= (element_size >> 1)) {

			_mm_store_si128((__m128i*)d, _mm_stream_load_si128((__m128i*)s));

			d += (element_size >> 1);
			s += (element_size >> 1);
		}
	}

	// *internal only to be used with _streaming_cache as src.
	template<size_t const alignment>
	INLINE_MEMFUNC __memcpy_stream_store(uint8_t* const __restrict dest, uint8_t const* const __restrict src, size_t bytes)
	{
		static constexpr size_t const element_size = sizeof(__m256i);

		alignas(alignment) uint8_t* __restrict d((uint8_t * __restrict)std::assume_aligned<alignment>(dest));
		alignas(CACHE_LINE_BYTES) uint8_t const* __restrict s((uint8_t const* __restrict)std::assume_aligned<CACHE_LINE_BYTES>(src));

		// 64 bytes / iteration
#pragma loop( ivdep )
		for (; bytes >= CACHE_LINE_BYTES; bytes -= CACHE_LINE_BYTES) {

			_mm_prefetch((const char*)s + CACHE_LINE_BYTES, _MM_HINT_T0); // fetch next iteration T0

			// expolit both integer and floating point "uop ports"
			_mm256_stream_si256((__m256i*)d, _mm256_load_si256((__m256i*)s));
			_mm256_stream_ps((float* const __restrict)(((__m256*)d) + 1), _mm256_castsi256_ps(_mm256_load_si256((((__m256i*)s) + 1))));

			d += CACHE_LINE_BYTES;
			s += CACHE_LINE_BYTES;
		}

		// 32 bytes / iteration
#pragma loop( ivdep )
		for (; bytes >= element_size; bytes -= element_size) {

			_mm256_stream_si256((__m256i*)d, _mm256_load_si256((__m256i*)s));

			d += element_size;
			s += element_size;
		}

		// 16 bytes / iteration
#pragma loop( ivdep )
		for (; bytes >= (element_size >> 1); bytes -= (element_size >> 1)) {

			_mm_stream_si128((__m128i*)d, _mm_load_si128((__m128i*)s));

			d += (element_size >> 1);
			s += (element_size >> 1);
		}
	}
} // end ns

// [copies] //
template<size_t const alignment>
INLINE_MEMFUNC __memcpy_stream(void* const __restrict dest, void const* const __restrict src, size_t bytes)
{
	static constexpr size_t const element_size = sizeof(__m256i);
	static constexpr size_t const block_size = internal_mem::_cache_size;
	
	alignas(alignment) uint8_t* __restrict d((uint8_t* __restrict)std::assume_aligned<alignment>(dest));
	alignas(alignment) uint8_t const * __restrict s((uint8_t const* __restrict)std::assume_aligned<alignment>(src));

	alignas(CACHE_LINE_BYTES) uint8_t* const __restrict cache(std::assume_aligned<CACHE_LINE_BYTES>(internal_mem::_streaming_cache));

	// 4096 bytes / iteration
	for (; bytes >= block_size; bytes -= block_size) {

		_mm_mfence(); // isolates streaming loads from streaming stores

		_mm_prefetch((const char*)s, _MM_HINT_NTA);
		internal_mem::__memcpy_stream_load<alignment>(cache, s, block_size);

		_mm_mfence(); // isolates streaming loads from streaming stores

		_mm_prefetch((const char*)cache, _MM_HINT_T0);
		internal_mem::__memcpy_stream_store<alignment>(d, cache, block_size);

		d += block_size; s += block_size;
	}

	// residual block bytes, < 4096 bytes
	if (bytes) {

		_mm_mfence(); // isolates streaming loads from streaming stores

		_mm_prefetch((const char*)s, _MM_HINT_NTA);
		internal_mem::__memcpy_stream_load<alignment>(cache, s, bytes);

		_mm_mfence(); // isolates streaming loads from streaming stores

		_mm_prefetch((const char*)cache, _MM_HINT_T0);
		internal_mem::__memcpy_stream_store<alignment>(d, cache, bytes);
	}

}

// [very large copies]
template<size_t const alignment>
INLINE_MEMFUNC __memcpy_threaded(void* const __restrict dest, void const* const __restrict src, size_t const bytes, size_t const block_size)
{
	tbb::parallel_for(
		tbb::blocked_range<__m256i const* __restrict>((__m256i* const __restrict)src, (__m256i* const __restrict)(((uint8_t const* const __restrict)src) + bytes), block_size),
		[&](tbb::blocked_range<__m256i const* __restrict> block) {

			ptrdiff_t const offset(((uint8_t const* const __restrict)block.begin()) - ((uint8_t const* const __restrict)src));
			ptrdiff_t const current_block_size(((uint8_t const* const __restrict)block.end()) - ((uint8_t const* const __restrict)block.begin())); // required since last block can be partial 

			__memcpy_stream<alignment>((__m256i* const __restrict)(((uint8_t const* const __restrict)dest) + offset), block.begin(), (size_t const)current_block_size);
		}
	);
}

INLINE_MEMFUNC  __prefetch_vmem(void const* const __restrict address, size_t const bytes)
{
	WIN32_MEMORY_RANGE_ENTRY entry{ const_cast<PVOID>(address), bytes };

	PrefetchVirtualMemory(GetCurrentProcess(), 1, &entry, 0);
}


