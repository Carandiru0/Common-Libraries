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
#ifndef POINT2D_H
#define POINT2D_H
#include <stdint.h>
#include "superfastmath.h"

typedef union alignas(16) point2D
{
	struct alignas(16)
	{
		int32_t x,			// low
			    y;			// low
				// (not required) padx, pady;	// high, high
	};

	alignas(16) __m128i v;

	__forceinline __vectorcall point2D() : v(_mm_setzero_si128())
	{}
	__forceinline __vectorcall point2D(point2D const& a) : v(a.v)
	{}
	__forceinline __vectorcall point2D(point2D const&& a) : v(a.v) 
	{}
	__forceinline void __vectorcall operator=(point2D const& a) {
		v = a.v;
	}
	__forceinline void __vectorcall operator=(point2D const&& a) {
		v = a.v;
	}
	__forceinline bool const __vectorcall isZero() const {
		// bugfix: ensure the extra "pad" bytes do not contribute to comparison
		//return(_mm_testz_si128(v,v)); // this tests x,y,z,w which is no good - can't rely on z,w values 
		return(!(x | y)); // must only test x,y and exclude z,w padding
	}
	__forceinline __vectorcall point2D(int32_t const x0y0) : v(_mm_set_epi32(1, 1, x0y0, x0y0)) // *bugfix - must initialize z,w to 1,1 to avoid division by zero in vectorized code
	{}
	__forceinline __vectorcall point2D(uint32_t const x0y0) : v(_mm_set_epi32(1, 1, x0y0, x0y0))
	{}
	__forceinline __vectorcall point2D(int32_t const x0, int32_t const y0) : v(_mm_set_epi32(1, 1, y0, x0))
	{}
	__forceinline __vectorcall point2D(int64_t const x0, int64_t const y0) : v(_mm_set_epi32(1, 1, (int32_t)y0, (int32_t)x0))  // only need 32bits
	{}
	__forceinline __vectorcall point2D(uint32_t const x0, uint32_t const y0) : v(_mm_set_epi32(1, 1, y0, x0))
	{}
	__forceinline __vectorcall point2D(size_t const x0, size_t const y0) : v(_mm_set_epi32(1, 1, (uint32_t)y0, (uint32_t)x0))   // only need 32bits
	{}
	// conversion from float to int defaults to floor, same as v2_to_p2D. If rounding is desired(rarely) use default ctor with v2_to_p2D_rounded instead
	__forceinline __vectorcall point2D(float const x0, float const y0) : v(_mm_cvtps_epi32(_mm_round_ps(_mm_set_ps(1.0f, 1.0f, y0, x0), _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC)))  // floor(v)
	{}
	__forceinline explicit __vectorcall point2D(__m128i const vSrc) : v(vSrc)
	{}

	bool const __vectorcall operator<(point2D const rhs) const
	{
		int64_t const left(int64_t(x) + int64_t(y)),
					  right(int64_t(rhs.x) + int64_t(rhs.y));

		return(left < right);
	}
	bool const __vectorcall operator==(point2D const rhs) const
	{
		// https://stackoverflow.com/questions/26880863/testing-equality-between-two-m128i-variables
		__m128i const neq = _mm_xor_si128(v, rhs.v);
		return(_mm_test_all_zeros(neq, neq)); //v0 == v1 (true) or v0 != v1 (false)
	}

} point2D_t;

STATIC_INLINE_PURE point2D_t const __vectorcall p2D_min(point2D_t const a, point2D_t const b) {
	return(point2D_t(_mm_min_epi32(a.v, b.v)));
}
STATIC_INLINE_PURE point2D_t const __vectorcall p2D_max(point2D_t const a, point2D_t const b) {
	return(point2D_t(_mm_max_epi32(a.v, b.v)));
}
STATIC_INLINE_PURE point2D_t const __vectorcall p2D_shiftl(point2D_t const a, uint32_t const left) {
	return(point2D_t(_mm_slli_epi32(a.v, left)));
}
STATIC_INLINE_PURE point2D_t const __vectorcall p2D_shiftr(point2D_t const a, uint32_t const right) {
	return(point2D_t(_mm_srli_epi32(a.v, right)));
}
STATIC_INLINE_PURE point2D_t const __vectorcall p2D_double(point2D_t const a) {
	return(point2D_t(_mm_slli_epi32(a.v, 1)));
}
STATIC_INLINE_PURE point2D_t const __vectorcall p2D_half(point2D_t const a) {
	return(point2D_t(_mm_srli_epi32(a.v, 1)));
}
STATIC_INLINE_PURE point2D_t const __vectorcall p2D_add(point2D_t const a, point2D_t const b) {
	return(point2D_t(_mm_add_epi32(a.v, b.v)));
}
STATIC_INLINE_PURE point2D_t const __vectorcall p2D_addh(point2D_t const a, point2D_t const b) {
	return(point2D_t(_mm_srli_epi32(p2D_add(a, b).v, 1)));
}
STATIC_INLINE_PURE point2D_t const __vectorcall p2D_adds(point2D_t const a, int32_t const s) {
	return(point2D_t(_mm_add_epi32(a.v, point2D_t(s).v)));
}

STATIC_INLINE_PURE point2D_t const __vectorcall p2D_sub(point2D_t const a, point2D_t const b) {
	return(point2D_t(_mm_sub_epi32(a.v, b.v)));
}
STATIC_INLINE_PURE point2D_t const __vectorcall p2D_subh(point2D_t const a, point2D_t const b) {
	return(point2D_t(_mm_srli_epi32(p2D_sub(a, b).v, 1)));
}
STATIC_INLINE_PURE point2D_t const __vectorcall p2D_subs(point2D_t const a, int32_t const s) {
	return(point2D_t(_mm_sub_epi32(a.v, point2D_t(s).v)));
}

STATIC_INLINE_PURE point2D_t const __vectorcall p2D_mul(point2D_t const a, point2D_t const b) {
	return(point2D_t(_mm_mullo_epi32(a.v, b.v)));  // bugfix: dont use _mm_mul_epi32, it operates outside bounds of usable(2 int32's) union, 64bit operation really....
}
STATIC_INLINE_PURE point2D_t const __vectorcall p2D_muls(point2D_t const a, int32_t const s) {
	return(point2D_t(_mm_mullo_epi32(a.v, point2D_t(s).v)));  // bugfix: dont use _mm_mul_epi32, it operates outside bounds of usable(2 int32's) union, 64bit operation really....
}
STATIC_INLINE_PURE point2D_t const __vectorcall p2D_div(point2D_t const a, point2D_t const b) {
	return(point2D_t(_mm_div_epi32(a.v, b.v)));  
}
STATIC_INLINE_PURE point2D_t const __vectorcall p2D_divs(point2D_t const a, int32_t const s) {
	return(point2D_t(_mm_div_epi32(a.v, point2D_t(s).v)));  
}

STATIC_INLINE_PURE point2D_t const __vectorcall p2D_negate(point2D_t const a) {
	return(point2D_t(_mm_sub_epi32(point2D_t{}.v, a.v)));
}
STATIC_INLINE_PURE point2D_t const __vectorcall p2D_clamp(point2D_t const a, int32_t const min_, int32_t const max_) {
	return(point2D_t(SFM::clamp(a.v, _mm_set1_epi32(min_), _mm_set1_epi32(max_))));
}
STATIC_INLINE_PURE point2D_t const __vectorcall p2D_clamp(point2D_t const a, point2D_t const min_, point2D_t const max_) {
	return(point2D_t(SFM::clamp(a.v, min_.v, max_.v)));
}
STATIC_INLINE_PURE point2D_t const __vectorcall p2D_abs(point2D_t const a) {
	return(point2D_t(_mm_abs_epi32(a.v)));
}
STATIC_INLINE_PURE point2D_t const __vectorcall p2D_sgn(point2D_t const a) {
	return(point2D_t(SFM::sgn(a.x), SFM::sgn(a.y)));
}
STATIC_INLINE_PURE point2D_t const __vectorcall p2D_swap(point2D_t const a) {
	return(point2D_t(a.y, a.x));
}
STATIC_INLINE_PURE point2D_t const __vectorcall p2D_wrap(point2D_t const a, int32_t const max_) { // **only works if max_ is a power of two**
	return(point2D_t(a.x & (max_ - 1), a.y & (max_ - 1)));
}
STATIC_INLINE_PURE point2D_t const __vectorcall p2D_wrapany(point2D_t const a, int32_t const max_) {
	return(point2D_t(a.x % max_, a.y % max_));
}
STATIC_INLINE_PURE point2D_t const XM_CALLCONV v2_to_p2D_rounded(FXMVECTOR const v) {	// assumes XMFLOAT2A was loaded
	return(point2D_t(_mm_cvtps_epi32(SFM::round(v))));
}
STATIC_INLINE_PURE point2D_t const XM_CALLCONV v2_to_p2D(FXMVECTOR const v) {	// assumes XMFLOAT2A was loaded
	return(point2D_t(_mm_cvtps_epi32(SFM::floor(v))));
}
STATIC_INLINE_PURE XMVECTOR const XM_CALLCONV p2D_to_v2(point2D_t const v) {
	return(_mm_cvtepi32_ps(v.v));
}

STATIC_INLINE_PURE point2D_t const __vectorcall p2D_CartToIso(point2D_t const pt2DSpace)
{
	/*
		_x = (_col * tile_width * .5) + (_row * tile_width * .5);
		_y = (_row * tile_hieght * .5) - ( _col * tile_hieght * .5);
	*/
	return(point2D_t(pt2DSpace.x - pt2DSpace.y, (pt2DSpace.x + pt2DSpace.y) >> 1));
}

STATIC_INLINE_PURE point2D_t const __vectorcall p2D_IsoToCart(point2D_t const ptIsoSpace)
{
	/*
		_x = (_col * tile_width * .5) + (_row * tile_width * .5);
		_y = (_row * tile_hieght * .5) - ( _col * tile_hieght * .5);
	*/
	return(point2D_t(((ptIsoSpace.y << 1) - ptIsoSpace.x) >> 1, ((ptIsoSpace.y << 1) + ptIsoSpace.x) >> 1));
}

STATIC_INLINE_PURE int32_t const __vectorcall p2D_distanceSquared(point2D_t const vStart, point2D_t const vEnd)
{
	point2D_t deltaSeperation = p2D_sub(vEnd, vStart);
	deltaSeperation = p2D_mul(deltaSeperation, deltaSeperation);

	return(deltaSeperation.x + deltaSeperation.y);
}

typedef union alignas(16) rect2D
{
	struct alignas(16)
	{
		int32_t left,		
				top,		
				right,			
				bottom;		
	};

	alignas(16) __m128i v;

	__forceinline __vectorcall rect2D() : v(_mm_setzero_si128())
	{}
	__forceinline __vectorcall rect2D(rect2D const& a) : v(a.v)
	{}
	__forceinline __vectorcall rect2D(rect2D const&& a) : v(a.v)
	{}
	__forceinline void __vectorcall operator=(rect2D const& a) {
		v = a.v;
	}
	__forceinline void __vectorcall operator=(rect2D const&& a) {
		v = a.v;
	}
	__forceinline bool const __vectorcall isZero() const {
		return(_mm_testz_si128(v, v));
	}
	__forceinline explicit __vectorcall rect2D(int32_t const left_, int32_t const top_,
						                       int32_t const right_, int32_t const bottom_) : v(_mm_set_epi32(bottom_, right_, top_, left_))
	{}
	__forceinline explicit __vectorcall rect2D(point2D_t const left_top_, point2D_t const right_bottom_) : v(_mm_set_epi32(right_bottom_.y, right_bottom_.x, left_top_.y, left_top_.x))
	{}
	__forceinline explicit __vectorcall rect2D(__m128i const vSrc) : v(vSrc)
	{}

	// top-left corner .x = left, .y = top
	__forceinline point2D_t const __vectorcall left_top() const { return(point2D_t(left, top)); }
	__forceinline point2D_t const __vectorcall right_top() const { return(point2D_t(right, top)); }

	// bottom-right corner .x = right, .y = bottom
	__forceinline point2D_t const __vectorcall right_bottom() const { return(point2D_t(right, bottom)); }
	__forceinline point2D_t const __vectorcall left_bottom() const { return(point2D_t(left, bottom)); }

	// width, height corrected result is always positive number
	__forceinline int32_t const __vectorcall width() const { return(SFM::abs(right - left)); }
	__forceinline int32_t const __vectorcall height() const { return(SFM::abs(bottom - top)); }

	__forceinline point2D_t const __vectorcall width_height() const { return(point2D_t(width(), height())); }

	__forceinline point2D_t const __vectorcall center() const {
		return( p2D_add(left_top(), p2D_half(width_height())) );	// tl(-1,-1) corner add half of width/height - center
	}
} rect2D_t;

STATIC_INLINE_PURE rect2D_t const __vectorcall r2D_add(rect2D_t const rect, point2D_t const origin) {
	return (rect2D_t(p2D_add(rect.left_top(), origin), p2D_add(rect.right_bottom(), origin)));
}
STATIC_INLINE_PURE rect2D_t const __vectorcall r2D_sub(rect2D_t const a, rect2D_t const b) {
	return (rect2D_t(p2D_sub(a.left_top(), b.left_top()), p2D_sub(a.right_bottom(), b.right_bottom())));
}
STATIC_INLINE_PURE rect2D_t const __vectorcall r2D_sub(rect2D_t const rect, point2D_t const origin) {
	return (rect2D_t(p2D_sub(rect.left_top(), origin), p2D_sub(rect.right_bottom(), origin)));
}
STATIC_INLINE_PURE rect2D_t const __vectorcall r2D_min(rect2D_t const a, rect2D_t const b) {
	return(rect2D_t(_mm_min_epi32(a.v, b.v)));
}
STATIC_INLINE_PURE rect2D_t const __vectorcall r2D_min(rect2D_t const a, int32_t const b) {
	return(rect2D_t(_mm_min_epi32(a.v, _mm_set1_epi32(b))));
}
STATIC_INLINE_PURE rect2D_t const __vectorcall r2D_max(rect2D_t const a, rect2D_t const b) {
	return(rect2D_t(_mm_max_epi32(a.v, b.v)));
}
STATIC_INLINE_PURE rect2D_t const __vectorcall r2D_max(rect2D_t const a, int32_t const b) {
	return(rect2D_t(_mm_max_epi32(a.v, _mm_set1_epi32(b))));
}
STATIC_INLINE_PURE rect2D_t const __vectorcall r2D_clamp(rect2D_t const a, int32_t const min_, int32_t const max_) {

	__m128i const xmMin(_mm_set1_epi32(min_)), xmMax(_mm_set1_epi32(max_));

	return(rect2D_t(point2D_t(SFM::clamp(a.left_top().v, xmMin, xmMax)),
		point2D_t(SFM::clamp(a.right_bottom().v, xmMin, xmMax))));
}

STATIC_INLINE_PURE rect2D_t const __vectorcall r2D_set_by_width_height(point2D_t const x0_y0, point2D_t const width_height) { // default is expansion from top left
	point2D_t const x1_y1 = p2D_add(x0_y0, width_height);
	return ( rect2D_t(x0_y0, x1_y1) );
}
STATIC_INLINE_PURE rect2D_t const __vectorcall r2D_set_by_width_height(float const x0, float const y0, float const width, float const height) {
	point2D_t const x0_y0(point2D_t(SFM::floor_to_i32(x0), SFM::floor_to_i32(y0)));
	point2D_t const x1_y1 = p2D_add(x0_y0, point2D_t(SFM::floor_to_i32(width), SFM::floor_to_i32(height)));
	return (rect2D_t(x0_y0, x1_y1));
}
STATIC_INLINE_PURE rect2D_t const __vectorcall r2D_set_by_width_height(int32_t const x0, int32_t const y0, int32_t const width, int32_t const height) {
	point2D_t const x0_y0(point2D_t(x0, y0));
	point2D_t const x1_y1 = p2D_add(x0_y0, point2D_t(width,height));
	return (rect2D_t(x0_y0, x1_y1));
}

STATIC_INLINE_PURE rect2D_t const XM_CALLCONV v4_to_r2D(FXMVECTOR const v) {	// assumes XMFLOAT2A was loaded
	return(rect2D_t(_mm_cvtps_epi32(XMVectorFloor(v))));
}
STATIC_INLINE_PURE XMVECTOR const XM_CALLCONV r2D_to_v4(rect2D_t const v) {
	return(_mm_cvtepi32_ps(v.v));
}
STATIC_INLINE_PURE XMVECTOR const XM_CALLCONV r2D_to_nk_rect(rect2D_t const v) {	
	// nk_rect is x,y,width,height vs rect2D_t x0,y0,x1,y1
	return(uvec4_v(v.left, v.top, v.width(), v.height()).v4f());
}

template<bool const grow_left_top = true, bool const grow_right_bottom = true>
STATIC_INLINE_PURE rect2D_t const __vectorcall r2D_grow(rect2D_t const a, point2D_t const grow) {

	if constexpr (grow_left_top && grow_right_bottom) {
		return(rect2D_t(p2D_sub(a.left_top(), grow), p2D_add(a.right_bottom(), grow)));
	}
	else if constexpr (grow_left_top) {
		return(rect2D_t(p2D_sub(a.left_top(), grow), a.right_bottom()));
	}
	else if constexpr (grow_right_bottom) {
		return(rect2D_t(a.left_top(), p2D_add(a.right_bottom(), grow)));
	}
}
template<bool const shrink_left_top = true, bool const shrink_right_bottom = true>
STATIC_INLINE_PURE rect2D_t const __vectorcall r2D_shrink(rect2D_t const a, point2D_t const shrink) {

	if constexpr (shrink_left_top && shrink_right_bottom) {
		return(rect2D_t(p2D_add(a.left_top(), shrink), p2D_sub(a.right_bottom(), shrink)));
	}
	else if constexpr (shrink_left_top) {
		return(rect2D_t(p2D_add(a.left_top(), shrink), a.right_bottom()));
	}
	else if constexpr (shrink_right_bottom) {
		return(rect2D_t(a.left_top(), p2D_sub(a.right_bottom(), shrink)));
	}
}

STATIC_INLINE_PURE bool const __vectorcall r2D_contains(rect2D_t const a, rect2D_t const b) {

	// for integers there are no intrinsic functions for >= or <= let the compiler deal with this one.

	// min = left top
	// max = right bottom

	// Check bounds
	return ((b.left >= a.left) &&
			(b.top >= a.top) &&
			(b.right <= a.right) &&
			(b.bottom <= a.bottom));
}
STATIC_INLINE_PURE bool const __vectorcall r2D_contains(rect2D_t const a, point2D_t const b) {

	// for integers there are no intrinsic functions for >= or <= let the compiler deal with this one.
	
	// min = left top
	// max = right bottom
	
	// Check bounds
	return ((b.x >= a.left)  &&
		    (b.y >= a.top)   &&
		    (b.x <= a.right) &&
		    (b.y <= a.bottom));
}
#endif


