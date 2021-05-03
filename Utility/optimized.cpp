#include <stdafx.h>
#include <malloc.h>
#include "optimized.h"
#include <Math/superfastmath.h>

#define INTRINSIC_MEMFUNC_IMPL inline __PURE __SAFE_BUF void __vectorcall

#define stream true
#define store  false

// ########### CLEAR ############ //

namespace internal_mem
{
	template<bool const streaming, typename T>
	static INTRINSIC_MEMFUNC_IMPL __memclr_aligned(T* __restrict dest, size_t bytes)
	{
		__m256i const xmZero(_mm256_setzero_si256());

		if constexpr (std::is_same_v<T, __m256i>) { // size does fit into multiple of 32bytes

			static constexpr size_t const element_size = sizeof(__m256i);
			__builtin_assume_aligned(T, dest, 32ULL);

#ifndef NDEBUG
			assert_print(0 == (bytes % element_size), "__memclr_aligned:  size not a multiple of sizeof(__m256i) - data is not aligned\n");
			static_assert(0 == (sizeof(T) % element_size), "__memclr_aligned:  element not divisable by sizeof(__m256i)\n");
#endif
			
#pragma loop( ivdep ) // 128 bytes/iteration
			while (bytes >= CACHE_LINE_BYTES)
			{
				if constexpr (streaming) {
					_mm256_stream_si256(dest, xmZero);	// vertex buffers are very large, using streaming stores
					_mm256_stream_si256(dest + 1ULL, xmZero);	// vertex buffers are very large, using streaming stores
					_mm256_stream_si256(dest + 2ULL, xmZero);	// vertex buffers are very large, using streaming stores
					_mm256_stream_si256(dest + 3ULL, xmZero);	// vertex buffers are very large, using streaming stores
				}
				else {
					_mm256_store_si256(dest, xmZero);
					_mm256_store_si256(dest + 1ULL, xmZero);
					_mm256_store_si256(dest + 2ULL, xmZero);
					_mm256_store_si256(dest + 3ULL, xmZero);
				}
				dest += 4ULL;
				bytes -= CACHE_LINE_BYTES;
			}

#pragma loop( ivdep ) // 32 bytes/iteration
			while (bytes >= element_size)
			{
				if constexpr (streaming) {
					_mm256_stream_si256(dest, xmZero);	// vertex buffers are very large, using streaming stores
				}
				else {
					_mm256_store_si256(dest, xmZero);
				}
				++dest;
				bytes -= element_size;
			}
		}
		else { // size does not fit into multiple of 32bytes

			static constexpr size_t const element_size = sizeof(__m128i);
			__builtin_assume_aligned(T, dest, 16ULL);

#ifndef NDEBUG
			assert_print(0 == (bytes % element_size), "__memclr_aligned:  size not a multiple of sizeof(__m128i) - data is not aligned\n");
			static_assert(0 == (sizeof(T) % element_size), "__memclr_aligned:  element not divisable by sizeof(__m128i)\n");
#endif

#pragma loop( ivdep ) // 128 bytes/iteration
			while (bytes >= CACHE_LINE_BYTES)
			{
				if constexpr (streaming) {
					_mm_stream_si128(dest, _mm256_castsi256_si128(xmZero));
					_mm_stream_si128(dest + 1ULL, _mm256_castsi256_si128(xmZero));
					_mm_stream_si128(dest + 2ULL, _mm256_castsi256_si128(xmZero));
					_mm_stream_si128(dest + 3ULL, _mm256_castsi256_si128(xmZero)); 
					_mm_stream_si128(dest + 4ULL, _mm256_castsi256_si128(xmZero));
					_mm_stream_si128(dest + 5ULL, _mm256_castsi256_si128(xmZero));
					_mm_stream_si128(dest + 6ULL, _mm256_castsi256_si128(xmZero));
					_mm_stream_si128(dest + 7ULL, _mm256_castsi256_si128(xmZero));
				}
				else {
					_mm_store_si128(dest, _mm256_castsi256_si128(xmZero));
					_mm_store_si128(dest + 1ULL, _mm256_castsi256_si128(xmZero));
					_mm_store_si128(dest + 2ULL, _mm256_castsi256_si128(xmZero));
					_mm_store_si128(dest + 3ULL, _mm256_castsi256_si128(xmZero));
					_mm_store_si128(dest + 4ULL, _mm256_castsi256_si128(xmZero));
					_mm_store_si128(dest + 5ULL, _mm256_castsi256_si128(xmZero));
					_mm_store_si128(dest + 6ULL, _mm256_castsi256_si128(xmZero));
					_mm_store_si128(dest + 7ULL, _mm256_castsi256_si128(xmZero));
				}
				dest += 8ULL;
				bytes -= CACHE_LINE_BYTES;
			}

#pragma loop( ivdep ) // 16bytes / iteration
			while (bytes >= element_size)
			{
				if constexpr (streaming) {
					_mm_stream_si128(dest, _mm256_castsi256_si128(xmZero));	
				}
				else {
					_mm_store_si128(dest, _mm256_castsi256_si128(xmZero));
				}
				++dest;
				bytes -= element_size;
			}
		}
	}
}//end ns

INTRINSIC_MEMFUNC_IMPL __memclr_aligned_32_stream(uint8_t* const __restrict dest, size_t const bytes)
{
	internal_mem::__memclr_aligned<stream>((__m256i* const __restrict)dest, bytes);
}
INTRINSIC_MEMFUNC_IMPL __memclr_aligned_32_store(uint8_t* const __restrict dest, size_t const bytes)
{
	internal_mem::__memclr_aligned<store>((__m256i* const __restrict)dest, bytes);
}

INTRINSIC_MEMFUNC_IMPL __memclr_aligned_16_stream(uint8_t* const __restrict dest, size_t const bytes)
{
	internal_mem::__memclr_aligned<stream>((__m128i* const __restrict)dest, bytes);
}
INTRINSIC_MEMFUNC_IMPL __memclr_aligned_16_store(uint8_t* const __restrict dest, size_t const bytes)
{
	internal_mem::__memclr_aligned<store>((__m128i* const __restrict)dest, bytes);
}




// ########### COPY ############ //
namespace internal_mem
{
	template<bool const streaming, typename T>
	static INTRINSIC_MEMFUNC_IMPL __memcpy_aligned(T* __restrict dest, T const* __restrict src, size_t bytes)
	{
		if constexpr (std::is_same_v<T, __m256i>) {
			static constexpr size_t const element_size = sizeof(__m256i);
			__builtin_assume_aligned(T, dest, 32ULL);

#ifndef NDEBUG
			assert_print(0 == (bytes % element_size), "__memcpy_aligned:  size not a multiple of sizeof(__m256i) - data is not aligned\n");
			static_assert(0 == (sizeof(T) % element_size), "__memcpy_aligned:  element not divisable by sizeof(__m256i)\n");
#endif

#pragma loop( ivdep ) // 128 bytes / iteration
			while (bytes >= CACHE_LINE_BYTES)
			{
				if constexpr (streaming) {
					_mm256_stream_si256(dest, _mm256_stream_load_si256(src));
					_mm256_stream_si256(dest + 1ULL, _mm256_stream_load_si256(src + 1ULL));
					_mm256_stream_si256(dest + 2ULL, _mm256_stream_load_si256(src + 2ULL));
					_mm256_stream_si256(dest + 3ULL, _mm256_stream_load_si256(src + 3ULL));
				}
				else {
					_mm256_store_si256(dest, _mm256_load_si256(src));
					_mm256_store_si256(dest + 1ULL, _mm256_load_si256(src + 1ULL));
					_mm256_store_si256(dest + 2ULL, _mm256_load_si256(src + 2ULL));
					_mm256_store_si256(dest + 3ULL, _mm256_load_si256(src + 3ULL));
				}
				dest += 4ULL; src += 4ULL;
				bytes -= CACHE_LINE_BYTES;
			}

#pragma loop( ivdep ) // 32 bytes / iteration
			while (bytes >= element_size)
			{
				if constexpr (streaming) {
					_mm256_stream_si256(dest, _mm256_stream_load_si256(src));
				}
				else {
					_mm256_store_si256(dest, _mm256_load_si256(src));
				}
				++dest; ++src;
				bytes -= element_size;
			}
		}
		else {
			static constexpr size_t const element_size = sizeof(__m128i);
			__builtin_assume_aligned(T, dest, 16ULL);

#ifndef NDEBUG
			assert_print(0 == (bytes % element_size), "__memcpy_aligned:  size not a multiple of sizeof(__m128i) - data is not aligned\n");
			static_assert(0 == (sizeof(T) % element_size), "__memcpy_aligned:  element not divisable by sizeof(__m128i)\n");
#endif

#pragma loop( ivdep ) // 128 bytes / iteration
			while (bytes >= CACHE_LINE_BYTES)
			{
				if constexpr (streaming) {
					_mm_stream_si128(dest, _mm_stream_load_si128(src));
					_mm_stream_si128(dest + 1ULL, _mm_stream_load_si128(src + 1ULL));
					_mm_stream_si128(dest + 2ULL, _mm_stream_load_si128(src + 2ULL));
					_mm_stream_si128(dest + 3ULL, _mm_stream_load_si128(src + 3ULL));
					_mm_stream_si128(dest + 4ULL, _mm_stream_load_si128(src + 4ULL));
					_mm_stream_si128(dest + 5ULL, _mm_stream_load_si128(src + 5ULL));
					_mm_stream_si128(dest + 6ULL, _mm_stream_load_si128(src + 6ULL));
					_mm_stream_si128(dest + 7ULL, _mm_stream_load_si128(src + 7ULL));
				}
				else {
					_mm_store_si128(dest, _mm_load_si128(src));
					_mm_store_si128(dest + 1ULL, _mm_load_si128(src + 1ULL));
					_mm_store_si128(dest + 2ULL, _mm_load_si128(src + 2ULL));
					_mm_store_si128(dest + 3ULL, _mm_load_si128(src + 3ULL));
					_mm_store_si128(dest + 4ULL, _mm_load_si128(src + 4ULL));
					_mm_store_si128(dest + 5ULL, _mm_load_si128(src + 5ULL));
					_mm_store_si128(dest + 6ULL, _mm_load_si128(src + 6ULL));
					_mm_store_si128(dest + 7ULL, _mm_load_si128(src + 7ULL));
				}
				dest += 8ULL; src += 8ULL;
				bytes -= CACHE_LINE_BYTES;
			}

#pragma loop( ivdep ) // 16 bytes / iteration
			while (bytes >= element_size)
			{
				if constexpr (streaming) {
					_mm_stream_si128(dest, _mm_stream_load_si128(src));
				}
				else {
					_mm_store_si128(dest, _mm_load_si128(src));
				}
				++dest; ++src;
				bytes -= element_size;
			}
		}
	}
}//end ns

INTRINSIC_MEMFUNC_IMPL __memcpy_aligned_32_stream(uint8_t* const __restrict dest, uint8_t const* const __restrict src, size_t const bytes)
{
	internal_mem::__memcpy_aligned<stream>((__m256i* const __restrict)dest, (__m256i* const __restrict)src, bytes);
}
INTRINSIC_MEMFUNC_IMPL __memcpy_aligned_32_store(uint8_t* const __restrict dest, uint8_t const* const __restrict src, size_t const bytes)
{
	internal_mem::__memcpy_aligned<store>((__m256i* const __restrict)dest, (__m256i* const __restrict)src, bytes);
}
INTRINSIC_MEMFUNC_IMPL __memcpy_aligned_16_stream(uint8_t* const __restrict dest, uint8_t const* const __restrict src, size_t const bytes)
{
	internal_mem::__memcpy_aligned<stream>((__m128i* const __restrict)dest, (__m128i* const __restrict)src, bytes);
}
INTRINSIC_MEMFUNC_IMPL __memcpy_aligned_16_store(uint8_t* const __restrict dest, uint8_t const* const __restrict src, size_t const bytes)
{
	internal_mem::__memcpy_aligned<store>((__m128i* const __restrict)dest, (__m128i* const __restrict)src, bytes);
}


// large allocations //
INTRINSIC_MEM_ALLOC_FUNC __memalloc_large(size_t const bytes, size_t const alignment)
{
	static size_t pageSize(0);

	if (0 == pageSize) {
		SYSTEM_INFO sysInfo{};
		::GetSystemInfo(&sysInfo);
		pageSize = (size_t)sysInfo.dwPageSize;
	}

	if (bytes >= pageSize) { // guard so only large allocations actually use VirtualAlloc2

		size_t const pageBytes((size_t)SFM::roundToMultipleOf<true>((int64_t)bytes, (int64_t)pageSize));

		/*
		MEM_ADDRESS_REQUIREMENTS addressReqs{};
		MEM_EXTENDED_PARAMETER param{};

		MEM_EXTENDED_PARAMETER* lpParam(nullptr);
		ULONG nParams(0);

		if (0 != alignment) { // not working ?? dunno why tried all cases of alignment, pages too
			addressReqs.Alignment = alignment;
			//addressReqs.HighestEndingAddress = (PVOID)(ULONG_PTR)(0x7fffffff - pageSize + 1); // must be aligned

			param.Type = MemExtendedParameterAddressRequirements;
			param.Pointer = &addressReqs;

			lpParam = &param;
			++nParams;
		}
		*/
		void* const __restrict vmem = ::VirtualAlloc2(nullptr, nullptr, pageBytes,
			MEM_COMMIT | MEM_RESERVE,
			PAGE_READWRITE,
			/*lpParam*/nullptr, /*nParams*/0);

		if (nullptr != vmem) { 
			
			::VirtualLock(vmem, pageBytes); // physical memory only, no swappage crap!!!!

			return(vmem);
		}

	}

	// still succeed with an allocation
	if (0 == alignment) {
		return(::malloc(bytes));
	}
	return(::_aligned_malloc(bytes, alignment));
}


