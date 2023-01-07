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
#pragma warning( disable : 4166 )   // __vectorcall on ctor's

#include <stdint.h>
#include <intrin.h>
#include <emmintrin.h>
#include <smmintrin.h>
#include <immintrin.h>

template<typename T, size_t const alignment>
struct alignas(alignment) vec4_t  // SCALAR ACCESS
{
	union alignas(alignment)
	{
		struct alignas(alignment) {
			T x, y, z, w;
		};
		struct alignas(alignment) {
			T r, g, b, a;
		};
		alignas(alignment) T data[4];
	};
};
using ivec4_t = vec4_t<int32_t, 16>;			// scalar type like a XMFLOAT4A
using uvec4_t = vec4_t<uint32_t, 16>;

template<typename T>
struct alignas(16) vec4_v  // VECTOR ACCESS
{
	// for those times when you need to acess only a single component:
	__inline T const __vectorcall x() const {
		return((T)_mm_cvtsi128_si32(v));
	}
	__inline T const __vectorcall r() const {
		return((T)_mm_cvtsi128_si32(v));
	}

	__inline T const __vectorcall y() const {
		return((T)_mm_cvtsi128_si32(_mm_shuffle_epi32(v, _MM_SHUFFLE(1, 1, 1, 1))));
	}
	__inline T const __vectorcall g() const {
		return((T)_mm_cvtsi128_si32(_mm_shuffle_epi32(v, _MM_SHUFFLE(1, 1, 1, 1))));
	}

	__inline T const __vectorcall z() const {
		return((T)_mm_cvtsi128_si32(_mm_shuffle_epi32(v, _MM_SHUFFLE(2, 2, 2, 2))));
	}
	__inline T const __vectorcall b() const {
		return((T)_mm_cvtsi128_si32(_mm_shuffle_epi32(v, _MM_SHUFFLE(2, 2, 2, 2))));
	}

	__inline T const __vectorcall w() const {
		return((T)_mm_cvtsi128_si32(_mm_shuffle_epi32(v, _MM_SHUFFLE(3, 3, 3, 3))));
	}
	__inline T const __vectorcall a() const {
		return((T)_mm_cvtsi128_si32(_mm_shuffle_epi32(v, _MM_SHUFFLE(3, 3, 3, 3))));
	}

	// otherwise its faster to:
	__inline void __vectorcall xyzw(vec4_t<T, 16>& __restrict scalar_values) const {	// for when more than one component needs to be accessed, just pull all 4 out of vector - values must be aligned
		_mm_store_si128((__m128i*)scalar_values.data, v);
	}
	__inline void __vectorcall rgba(vec4_t<T, 16>& __restrict scalar_values) const {	// for when more than one component needs to be accessed, just pull all 4 out of vector - values must be aligned
		_mm_store_si128((__m128i*)scalar_values.data, v);
	}

	__inline void __vectorcall operator=(vec4_t<T, 16> const& __restrict scalar_values) { // for assigning new values for all components
		v = _mm_load_si128((__m128i*)scalar_values.data);
	}

	__inline __m128 const __vectorcall v4f() const { // converts directly to floating point 
		return(_mm_cvtepi32_ps(v));
	}

private:
	// internal private
	template<uint32_t const num_components>
	static __inline __declspec(noalias) consteval uint32_t const num_components_to_mask() // strictly compile time only evaluation (consteval)
	{
		if constexpr (1 == num_components) {
			return(0x1);
		}
		else if constexpr (2 == num_components) {
			return(0x3);
		}
		else if constexpr (3 == num_components) {
			return(0x7);
		}

		return(0xf);
	}
public:
	// comparison // use with overloaded comparison operators to reduce vector result contained in __m128i to a scalar result (boolean). Must specify template parameter for the number of components you care about.
	template<uint32_t const num_components>
	static __inline __declspec(noalias) bool const __vectorcall all(__m128i const test) {
		constexpr uint32_t const mask(num_components_to_mask<num_components>());

		return(((_mm_movemask_ps(_mm_castsi128_ps(test)) & mask) == mask) != 0);
	}
	template<uint32_t const num_components>
	static __inline __declspec(noalias) bool const __vectorcall any(__m128i const test) {
		constexpr uint32_t const mask(num_components_to_mask<num_components>());

		return((_mm_movemask_ps(_mm_castsi128_ps(test)) & mask) != 0); 
	}
	template<uint32_t const num_components>
	static __inline __declspec(noalias) uint32_t const __vectorcall result(__m128i const test) {
		constexpr uint32_t const mask(num_components_to_mask<num_components>());

		return((_mm_movemask_ps(_mm_castsi128_ps(test)) & mask)); // then test the return value bits.  x = (1 << 0) , y = (1 << 1) , z = (1 << 2) , w = (1 << 3)
	}

	// comparison operators // use with static functions [any, all] for boolean result. These return a __m128i containing the "vector" result of the comparison.
	__inline __m128i const __vectorcall operator==(vec4_v const& __restrict rhs) const {
		return(_mm_cmpeq_epi32(v, rhs.v));
	}
	__inline __m128i const __vectorcall operator!=(vec4_v const& __restrict rhs) const {
		// there is no "neq" intrinsic, workaround by complementing the "eq" intrinsic result - overhead of extra intrinsics is neglible, _mm_and_not_si128 is very fast.
		return(_mm_andnot_si128(_mm_cmpeq_epi32(v, rhs.v), _mm_set1_epi32(0xFFFFFFFF)));   // !(a == b) & 1
	}
	__inline __m128i const __vectorcall operator>(vec4_v const& __restrict rhs) const {
		return(_mm_cmpgt_epi32(v, rhs.v));
	}
	__inline __m128i const __vectorcall operator<(vec4_v const& __restrict rhs) const {
		return(_mm_cmplt_epi32(v, rhs.v));
	}
	// ** greater than equal and less than equal not implemented, again missing intrinsics. todo: add these functions if required in future
	 
	 
	 
	//##############################
	__m128i v;// main vector register
	//##############################

	// ctors //
	__forceinline explicit __vectorcall vec4_v(__m128i const v_) // loads the intrinsic register (__m128i) directly
		: v(v_)
	{}

	__forceinline explicit __vectorcall vec4_v(__m128 const v_) // converts the intrinsic register (__m128->__m128i) directly
		: v(_mm_cvtps_epi32(v_))
	{}

	__forceinline __vectorcall vec4_v(vec4_t<T, 16> const& __restrict scalar_values) // loads value stored in vec4_t
		: v(_mm_load_si128((__m128i*)scalar_values.data))
	{}
	
	__forceinline explicit __vectorcall vec4_v(T const val0, T const val1, T const val2, T const val3 = T(0))  // ** do not change as there are other uses for uvec4_v other than color!!! compatible with XMVECTOR.
		: v(_mm_setr_epi32(val0, val1, val2, val3))
	{}
	
	__forceinline explicit __vectorcall vec4_v(T const val012, T const val3)	// replicates val012 into first 3 components, then val3 is last component
		: v(_mm_setr_epi32(val012, val012, val012, val3))
	{}
	
	__forceinline explicit __vectorcall vec4_v(T const replicated_to_all_components = T(0))
		: v(_mm_set1_epi32(replicated_to_all_components))
	{}

	__forceinline operator __m128i const() const {
		return(v);
	}
};
using ivec4_v = vec4_v<int32_t>;		// vector type like a XMVECTOR
using uvec4_v = vec4_v<uint32_t>;



