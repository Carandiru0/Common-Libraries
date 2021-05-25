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


	//##############################
	__m128i v;// main vector register
	//##############################

	__forceinline explicit __vectorcall vec4_v(__m128i const v_) // loads the intrinsic register (__m128i) directly
		: v(v_)
	{}

	__forceinline explicit __vectorcall vec4_v(vec4_t<T, 16> const& __restrict scalar_values) // loads value stored in vec4_t
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
};
using ivec4_v = vec4_v<int32_t>;		// vector type like a XMVECTOR
using uvec4_v = vec4_v<uint32_t>;


