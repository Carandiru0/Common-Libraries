#pragma once
#pragma warning( disable : 4114 )	// "const" used more than once with XMGLOBALCONST

#include <intrin.h>
#include <emmintrin.h>
#include <smmintrin.h>
#include <immintrin.h>
#ifdef __clang__
#include <Utility/avx_svml_intrin.h>
#endif

#include <DirectXMath.h>
#include <cmath>
#include <corecrt_math.h>
#include <DirectXPackedVector.h> // DirectX::PackedVector XMCOLOR
#include <DirectXColors.h>
#include <type_traits>
#include <numbers>
#include "vec4_t.h"

using namespace DirectX;

#define STATIC_INLINE_PURE static __inline __declspec(noalias)
#define STATIC_INLINE static __inline
#define NO_INLINE __declspec(noinline)

// Column-major to Row-major Matrix Mapping / Conversion
//
//		+----- COLUMN - MAJOR ----+				+--------------- ROW - MAJOR ---------------+
//
//		<1>		<2>		<3>		<4>		   <normal matrix notation>
/*		|-|	    |-|		[-]		[-]
		m11		m21		m31		m41				<1>	=>	[	m11		m12		m13		m14		]
		m12		m22		m32		m42				<2> =>	[	m21		m22		m23		m24		]
		m13		m23		m33		m43				<3> =>	[	m31		m32		m33		m34		]
		m14		m24		m34		m44				<4> =>	[	m41		m42		m43		m44		]
		[-]		[-]		[-]		[-]

//		<0>		<1>		<2>		<3>			<2D index based>
/*		|-|	    |-|		[-]		[-]
		m[0][0]	m[1][0]	m[2][0]	m[3][0]			<0>	=>	[	m[0][0]		m[0][1]		m[0][2]		m[0][3]		]	[Right]
		m[0][1]	m[1][1]	m[2][1]	m[3][1]			<1> =>	[	m[1][0]		m[1][1]		m[1][2]		m[1][3]		]	[Up]
		m[0][2]	m[1][2]	m[2][2]	m[3][2]			<2> =>	[	m[2][0]		m[2][1]		m[2][2]		m[2][3]		]	[Forward]
		m[0][3]	m[1][3]	m[2][3]	m[3][3]			<3> =>	[	m[3][0]		m[3][1]		m[3][2]		m[3][3]		]	[Position]
		[-]		[-]		[-]		[-]
		[Right] [Up]    [Forw]  [Position]

//		<1>		<2>		<3>		<4>			 <1D index based>
/*		|-|	    |-|		[-]		[-]
		0		4		8		12				<1>	=>	[		0		1		2		3		]
		1		5		9		13				<2> =>	[		4		5		6		7		]
		2		6		10		14				<3> =>	[		8		9		10		11		]
		3		7		11		15				<4> =>	[		12		13		14		15		]
		[-]		[-]		[-]		[-]
*/

#ifndef SFM_NO_OVERRIDES	// for remapping, globally, certain common math functions to use the superfastmath version override instead. If conflicts arise, define SFM_NO_OVERRIDES before including this header file.

#ifdef fma
#undef fma
#define fma SFM::__fma
#endif
#ifdef fmaf
#undef fmaf
#define fmaf SFM::__fma
#endif
#ifndef fma
#define fma SFM::__fma
#endif
#ifndef fmaf
#define fmaf SFM::__fma
#endif
#ifndef fms
#define fms SFM::__fms
#endif
#ifndef fmsf
#define fmsf SFM::__fms
#endif

//#ifdef sqrt CONFLICTS WITH STD, <RANDOM>
//#undef sqrt
//#define sqrt SFM::__sqrt
//#endif
#ifdef sqrtf
#undef sqrtf
#define sqrtf SFM::__sqrt
#endif
//#ifndef sqrt CONFLICTS WITH STD, <RANDOM>
//#define sqrt SFM::__sqrt
//#endif
#ifndef sqrtf
#define sqrtf SFM::__sqrt
#endif

#ifdef abs
#undef abs
#endif
#ifdef fabsf
#undef fabsf
#define fabsf SFM::abs
#endif
#ifdef fabs
#undef fabs
#define fabs SFM::abs
#endif
#ifndef fabsf
#define fabsf SFM::abs
#endif
#ifndef fabs
#define fabs SFM::abs
#endif

#ifdef __min
#undef __min
#define __min SFM::min
#endif
#ifdef min
#undef min
#endif
#ifdef __max
#undef __max
#define __max SFM::max
#endif
#ifdef max
#undef max
#endif
#ifndef __min
#define __min SFM::min
#endif
#ifndef __max
#define __max SFM::max
#endif

#ifdef sin
#undef sin
#define sin SFM::__sin
#endif
#ifdef sinf
#undef sinf
#define sinf SFM::__sin
#endif
#ifndef sin
#define sin SFM::__sin
#endif
#ifndef sinf
#define sinf SFM::__sin
#endif

#ifdef cos
#undef cos
#define cos SFM::__cos
#endif
#ifdef cosf
#undef cosf
#define cosf SFM::__cos
#endif
#ifndef cos
#define cos SFM::__cos
#endif
#ifndef cosf
#define cosf SFM::__cos
#endif

#ifdef exp
#undef exp
#define exp SFM::__exp
#endif
#ifdef expf
#undef expf
#define expf SFM::__exp
#endif
//#ifndef log CONFLICTS WITH ANYTHING NAMED SIMPLY LOG
//#define log SFM::__log
//#endif
#ifndef logf
#define logf SFM::__log
#endif
//#ifndef pow CONFLICTS WITH STD, <RANDOM>
//#define pow SFM::__pow
//#endif
#ifndef powf
#define powf SFM::__pow
#endif

#endif // SFM_NO_OVERRIDES

namespace SFM	// (s)uper (f)ast (m)ath
{
	static constexpr double const GOLDEN_RATIO = std::numbers::phi; // don't change, type purposely double
	static constexpr double const GOLDEN_RATIO_ZERO = (GOLDEN_RATIO - 1.0);

#define VCONST extern inline const constexpr __declspec(selectany)
	const struct alignas(16) v_const
	{
		const union alignas(16) { uint32_t u[4]; __m128 v; };
		__forceinline __vectorcall operator __m128() const { return(v); }
	};
	const struct alignas(16) v_const_d
	{
		const union alignas(16) { uint64_t u[2]; __m128d v; };
		__forceinline __vectorcall operator __m128d() const { return(v); }
	};

	// abs, min & max:
	//

	VCONST v_const vsignbits{ 0x80000000, 0x80000000, 0x80000000, 0x80000000 };
	VCONST v_const_d vsignbits_d{ 0x8000000000000000, 0x8000000000000000 };

	STATIC_INLINE_PURE double const __vectorcall abs(double const a) {
		
		return(_mm_cvtsd_f64(_mm_andnot_pd(vsignbits_d, _mm_set_sd(a))));
	}
	STATIC_INLINE_PURE float const __vectorcall abs(float const a) {

		return(_mm_cvtss_f32(_mm_andnot_ps(vsignbits, _mm_set_ss(a))));
	}
	STATIC_INLINE_PURE XMVECTOR const __vectorcall abs(FXMVECTOR const a) {

		return(_mm_andnot_ps(vsignbits, a));
	}
	STATIC_INLINE_PURE __m128i const __vectorcall abs(__m128i const a) {

		return(_mm_abs_epi32(a));
	}
	/* 64 bit version of abs (int64_t/uint64_t) does not exist here due to requirement of avx-512 intrinsics - Use std::abs instead if 64bit numbers are required */
	STATIC_INLINE_PURE int32_t const __vectorcall abs(int32_t const a) {

		return(_mm_cvtsi128_si32(_mm_abs_epi32(_mm_set1_epi32(a))));
	}

	STATIC_INLINE_PURE double const __vectorcall min(double const a, double const b) {

		return(_mm_cvtsd_f64(_mm_min_sd(_mm_set_sd(a), _mm_set_sd(b))));
	}
	STATIC_INLINE_PURE float const __vectorcall min(float const a, float const b) {

		return(_mm_cvtss_f32(_mm_min_ss(_mm_set_ss(a), _mm_set_ss(b))));
	}
	STATIC_INLINE_PURE XMVECTOR const __vectorcall min(FXMVECTOR const a, FXMVECTOR const b) {

		return(_mm_min_ps(a, b));
	}
	STATIC_INLINE_PURE __m128i const __vectorcall min(__m128i const a, __m128i const b) {

		return(_mm_min_epi32(a, b));
	}
	/* 64 bit version of min (int64_t/uint64_t) does not exist here due to requirement of avx-512 intrinsics - Use std::min instead if 64bit numbers are required */
	STATIC_INLINE_PURE int32_t const __vectorcall min(int32_t const a, int32_t const b) {

		return(_mm_cvtsi128_si32(_mm_min_epi32(_mm_set1_epi32(a), _mm_set1_epi32(b))));
	}
	STATIC_INLINE_PURE uint32_t const __vectorcall min(uint32_t const a, uint32_t const b) {

		return((uint32_t)_mm_cvtsi128_si32(_mm_min_epu32(_mm_set1_epi32(a), _mm_set1_epi32(b))));
	}

	STATIC_INLINE_PURE double const __vectorcall max(double const a, double const b) {

		return(_mm_cvtsd_f64(_mm_max_sd(_mm_set_sd(a), _mm_set_sd(b))));
	}
	STATIC_INLINE_PURE float const __vectorcall max(float const a, float const b) {

		return(_mm_cvtss_f32(_mm_max_ss(_mm_set_ss(a), _mm_set_ss(b))));
	}
	STATIC_INLINE_PURE XMVECTOR const __vectorcall max(FXMVECTOR const a, FXMVECTOR const b) {

		return(_mm_max_ps(a, b));
	}
	STATIC_INLINE_PURE __m128i const __vectorcall max(__m128i const a, __m128i const b) {

		return(_mm_max_epi32(a, b));
	}
	/* 64 bit version of max (int64_t/uint64_t) does not exist here due to requirement of avx-512 intrinsics - Use std::max instead if 64bit numbers are required */
	STATIC_INLINE_PURE int32_t const __vectorcall max(int32_t const a, int32_t const b) {

		return(_mm_cvtsi128_si32(_mm_max_epi32(_mm_set1_epi32(a), _mm_set1_epi32(b))));
	}
	STATIC_INLINE_PURE uint32_t const __vectorcall max(uint32_t const a, uint32_t const b) {

		return((uint32_t)_mm_cvtsi128_si32(_mm_max_epu32(_mm_set1_epi32(a), _mm_set1_epi32(b))));
	}
	//-----------------------------------------------------------------------------------------------------------------------------------------------//

	// clamping and saturation with type conversion:
	//

	STATIC_INLINE_PURE __m256 const __vectorcall clamp(__m256 const a, __m256 const min_, __m256 const max_)
	{
		return(_mm256_min_ps(_mm256_max_ps(a, min_), max_));
	}
	STATIC_INLINE_PURE XMVECTOR const __vectorcall clamp(FXMVECTOR a, FXMVECTOR min_, FXMVECTOR max_)
	{
		return(_mm_min_ps(_mm_max_ps(a, min_), max_));
	}
	STATIC_INLINE_PURE float const __vectorcall clamp(float const a, float const min_, float const max_)
	{
		return(_mm_cvtss_f32(_mm_min_ss(_mm_max_ss(_mm_set_ss(a), _mm_set_ss(min_)), _mm_set_ss(max_))));
	}
	STATIC_INLINE_PURE __m256i __vectorcall clamp(__m256i const a, __m256i const min_, __m256i const max_)
	{
		return(_mm256_min_epi32(_mm256_max_epi32(a, min_), max_));
	}
	STATIC_INLINE_PURE __m128i __vectorcall clamp(__m128i const a, __m128i const min_, __m128i const max_)
	{
		return(_mm_min_epi32(_mm_max_epi32(a, min_), max_));
	}
#define clamp_m256i clamp
#define clamp_m128i clamp

	STATIC_INLINE_PURE __m256 const __vectorcall saturate(__m256 const a) // for special case of between 0.0f and 1.0f
	{
		return(_mm256_min_ps(_mm256_max_ps(a, _mm256_setzero_ps()), _mm256_set1_ps(1.0f)));
	}
	STATIC_INLINE_PURE XMVECTOR const __vectorcall saturate(FXMVECTOR const a) // for special case of between 0.0f and 1.0f
	{
		return(_mm_min_ps(_mm_max_ps(a, _mm_setzero_ps()), _mm_set1_ps(1.0f)));
	}
	STATIC_INLINE_PURE float const __vectorcall saturate(float const a) // for special case of between 0.0f and 1.0f
	{
		return(_mm_cvtss_f32(_mm_min_ss(_mm_max_ss(_mm_set_ss(a), _mm_setzero_ps()), _mm_set_ss(1.0f))));
	}

	STATIC_INLINE_PURE uint32_t const __vectorcall saturate_to_u8(float const a) // implicitly rounds to nearest int
	{
		return((uint32_t)_mm_cvtsi128_si32(clamp_m128i(_mm_cvtps_epi32(_mm_set1_ps(a)), _mm_setzero_si128(), _mm_set1_epi32(UINT8_MAX))));
	}
	STATIC_INLINE_PURE __m256i const __vectorcall saturate_to_u8(__m256d const xmVectorColor) // implicitly rounds to nearest int
	{
		return(clamp_m256i(_mm256_castsi128_si256(_mm256_cvtpd_epi32(xmVectorColor)), _mm256_setzero_si256(), _mm256_set1_epi32(UINT8_MAX)));
	}
	STATIC_INLINE_PURE __m256i const __vectorcall saturate_to_u8(__m256 const xmVectorColor) // implicitly rounds to nearest int
	{
		return(clamp_m256i(_mm256_cvtps_epi32(xmVectorColor), _mm256_setzero_si256(), _mm256_set1_epi32(UINT8_MAX)));
	}
	STATIC_INLINE_PURE __m128i const __vectorcall saturate_to_u8(__m128 const xmVectorColor) // implicitly rounds to nearest int
	{
		return(clamp_m128i(_mm_cvtps_epi32(xmVectorColor), _mm_setzero_si128(), _mm_set1_epi32(UINT8_MAX)));
	}
	STATIC_INLINE_PURE __m128i const __vectorcall saturate_to_u16(__m128 const xmVectorColor) // implicitly rounds to nearest int
	{
		return(clamp_m128i(_mm_cvtps_epi32(xmVectorColor), _mm_setzero_si128(), _mm_set1_epi32(UINT16_MAX)));
	}
	STATIC_INLINE_PURE __m128i const __vectorcall saturate_to_u8(__m128i const a)
	{
		return(clamp_m128i(a, _mm_setzero_si128(), _mm_set1_epi32(UINT8_MAX)));
	}
	STATIC_INLINE_PURE uint32_t const __vectorcall saturate_to_u8(int const a)
	{
		return((uint32_t)_mm_cvtsi128_si32(clamp_m128i(_mm_set1_epi32(a), _mm_setzero_si128(), _mm_set1_epi32(UINT8_MAX))));
	}

	STATIC_INLINE_PURE void __vectorcall saturate_to_u8(__m128 const xmVectorColor, uvec4_t& __restrict scalar_values)  // implicitly rounds to nearest
	{
		_mm_store_si128((__m128i*)scalar_values.data, saturate_to_u8(xmVectorColor));
	}
	STATIC_INLINE_PURE void __vectorcall saturate_to_u8(__m128i const xmVectorColor, uvec4_t& __restrict scalar_values)  // implicitly rounds to nearest
	{
		_mm_store_si128((__m128i*)scalar_values.data, saturate_to_u8(xmVectorColor));
	}
	STATIC_INLINE_PURE void __vectorcall saturate_to_u16(__m128 const xmVectorColor, uvec4_t& __restrict scalar_values)  // implicitly rounds to nearest
	{
		_mm_store_si128((__m128i*)scalar_values.data, saturate_to_u16(xmVectorColor));
	}

	STATIC_INLINE_PURE float const __vectorcall u8_to_float(uint8_t const byte) // byte (0-255) to normalized float (0.0f ... 1.0f)
	{
		constexpr float const INV_BYTE = 1.0f / 255.0f;

		return(_mm256_cvtss_f32(saturate(_mm256_mul_ps(_mm256_cvtepi32_ps(_mm256_set1_epi32(byte)), _mm256_set1_ps(INV_BYTE)))));
	}
	STATIC_INLINE_PURE uint32_t const __vectorcall float_to_u8(float const norm) // normalized float (0.0f ... 1.0f) to byte (0-255)
	{
		constexpr float const BYTE = 255.0f;

		return(_mm256_cvtsi256_si32(saturate_to_u8(_mm256_mul_ps(_mm256_set1_ps(norm), _mm256_set1_ps(BYTE)))));
	}

	// for abgr(rgba) (packed color) conversion to float
	STATIC_INLINE_PURE float const uintBitsToFloat(uint32_t const hex)
	{
		// implicit conversion of hex data uint32_t to floatBits
		return(*reinterpret_cast<float const* const __restrict>(&hex)); // in glsl use floatBitsToUint to convert the float back to a uint with all bits intact
	}
	// for float conversion to abgr(rgba) (packed color)
	STATIC_INLINE_PURE uint32_t const floatBitsToUint(float const hex)
	{
		// implicit conversion of hex data floatBits to uint32_t
		return(*reinterpret_cast<uint32_t const* const __restrict>(&hex)); // in glsl use uintBitsToFloat to convert the uint back to a float with all bits intact
	}
	
	// rcp, sqrt, rqsrt:
	//

	STATIC_INLINE_PURE float const __vectorcall rcp(float const A)	// faster reciprocal
	{
		return(_mm_cvtss_f32(_mm256_castps256_ps128(_mm256_rcp_ps(_mm256_set1_ps(A)))));
	}
	STATIC_INLINE_PURE XMVECTOR const __vectorcall rcp(FXMVECTOR const A) // faster reciprocal
	{
		return(_mm256_castps256_ps128(_mm256_rcp_ps(_mm256_castps128_ps256(A))));
	}

	constexpr uint64_t const sqrt_helper(uint64_t const x, uint64_t const lo, uint64_t const hi) // float version doesnot work
	{
#define MID ((lo + hi + 1ULL) >> 1ULL)
		return( lo == hi ? lo : ((x / MID < MID)
			? sqrt_helper(x, lo, MID - 1ULL) : sqrt_helper(x, MID, hi)) );
#undef MID
	}
	// compile time 
	constexpr uint64_t const ct_sqrt(uint64_t const x) // compile time sqrt for integers only (good for initializations of constants)
	{
		return( sqrt_helper(x, 0ULL, (x >> 1ULL) + 1ULL) );
	}

	STATIC_INLINE_PURE float const __vectorcall __sqrt(float const A) // faster square root
	{
		return(_mm_cvtss_f32(_mm256_castps256_ps128(_mm256_sqrt_ps(_mm256_set1_ps(A)))));
	}
	STATIC_INLINE_PURE XMVECTOR const __vectorcall __sqrt(FXMVECTOR const A) // faster square root
	{
		return(_mm256_castps256_ps128(_mm256_sqrt_ps(_mm256_castps128_ps256(A))));
	}

	// #### note this estimate works out to half the precision as compared to using 1.0f / sqrt(), for higher precision use XMVectorReciprocalSqrt
	STATIC_INLINE_PURE float const __vectorcall rsqrt(float const A) 
	{
		return(_mm_cvtss_f32(_mm256_castps256_ps128(_mm256_rsqrt_ps(_mm256_set1_ps(A)))));
	}
	// #### note this estimate works out to half the precision as compared to using 1.0f / sqrt(), for higher precision use XMVectorReciprocalSqrt
	STATIC_INLINE_PURE XMVECTOR const __vectorcall rsqrt(FXMVECTOR const A) 
	{
		return(_mm256_castps256_ps128(_mm256_rsqrt_ps(_mm256_castps128_ps256(A))));
	}

	// fused multiply add/subtract:
	//
	STATIC_INLINE_PURE double const __vectorcall __fma(double const A, double const B, double const C)
	{
		return(_mm_cvtsd_f64(_mm_fmadd_sd(_mm_set_sd(A), _mm_set_sd(B), _mm_set_sd(C))));
	}
	STATIC_INLINE_PURE float const __vectorcall __fma(float const A, float const B, float const C)
	{
		return(_mm_cvtss_f32(_mm_fmadd_ss(_mm_set_ss(A), _mm_set_ss(B), _mm_set_ss(C))));
	}
	STATIC_INLINE_PURE XMVECTOR const __vectorcall __fma(FXMVECTOR const A, FXMVECTOR const B, FXMVECTOR const C)
	{
		return(_mm_fmadd_ps(A, B, C));
	}

	STATIC_INLINE_PURE double const __vectorcall __fms(double const A, double const B, double const C)
	{
		return(_mm_cvtsd_f64(_mm_fmsub_sd(_mm_set_sd(A), _mm_set_sd(B), _mm_set_sd(C))));
	}
	STATIC_INLINE_PURE float const __vectorcall __fms(float const A, float const B, float const C)
	{
		return(_mm_cvtss_f32(_mm_fmsub_ss(_mm_set_ss(A), _mm_set_ss(B), _mm_set_ss(C))));
	}
	STATIC_INLINE_PURE XMVECTOR const __vectorcall __fms(FXMVECTOR const A, FXMVECTOR const B, FXMVECTOR const C)
	{
		return(_mm_fmsub_ps(A, B, C));
	}

	// trig:
	//

	// sin & asin SVML
	STATIC_INLINE_PURE float const __vectorcall __sin(float const A) 
	{
		return(_mm_cvtss_f32(_mm256_castps256_ps128(_mm256_sin_ps(_mm256_set1_ps(A)))));
	}
	STATIC_INLINE_PURE XMVECTOR const __vectorcall __sin(FXMVECTOR const A) 
	{
		return(_mm256_castps256_ps128(_mm256_sin_ps(_mm256_castps128_ps256(A))));
	}
	STATIC_INLINE_PURE float const __vectorcall __asin(float const A) 
	{
		return(_mm_cvtss_f32(_mm256_castps256_ps128(_mm256_asin_ps(_mm256_set1_ps(A)))));
	}
	STATIC_INLINE_PURE XMVECTOR const __vectorcall __asin(FXMVECTOR const A) 
	{
		return(_mm256_castps256_ps128(_mm256_asin_ps(_mm256_castps128_ps256(A))));
	}
	// cos & acos SVML
	STATIC_INLINE_PURE float const __vectorcall __cos(float const A) 
	{
		return(_mm_cvtss_f32(_mm256_castps256_ps128(_mm256_cos_ps(_mm256_set1_ps(A)))));
	}
	STATIC_INLINE_PURE XMVECTOR const __vectorcall __cos(FXMVECTOR const A) 
	{
		return(_mm256_castps256_ps128(_mm256_cos_ps(_mm256_castps128_ps256(A))));
	}
	STATIC_INLINE_PURE float const __vectorcall __acos(float const A) // acos with high precision
	{
		return(_mm_cvtss_f32(_mm256_castps256_ps128(_mm256_acos_ps(_mm256_set1_ps(A)))));
	}
	STATIC_INLINE_PURE XMVECTOR const __vectorcall __acos(FXMVECTOR const A) // acos with high precision
	{
		return(_mm256_castps256_ps128(_mm256_acos_ps(_mm256_castps128_ps256(A))));
	}
	STATIC_INLINE_PURE float const __vectorcall __acos_approx(float const A) // faster acos, with 0.07f degrees maximum error
	{
		constexpr float const _term0 = 8.0f / 3.0f;
		constexpr float const _term1 = 1.0f / 3.0f;

		// 8500ns, 0.06 degree error
		__m128 const two = _mm_set_ss(2.0f);
		__m128 const xx = _mm_set_ss(A);

		__m128 const a = _mm_sqrt_ss(_mm_fmadd_ss(xx, two, two));
		__m128 const b = _mm_mul_ss(_mm_sqrt_ss(_mm_fnmadd_ss(xx, two, two)), _mm_set_ss(_term1));
		__m128 const c = _mm_sqrt_ss(_mm_sub_ss(two, a));
		return (_mm_cvtss_f32(_mm_fmsub_ss(_mm_set_ss(_term0), c, b)));
	}
	STATIC_INLINE_PURE XMVECTOR const __vectorcall __acos_approx(FXMVECTOR const A) // faster acos, with 0.07f degrees maximum error
	{
		constexpr float const _term0 = 8.0f / 3.0f;
		constexpr float const _term1 = 1.0f / 3.0f;

		// 8500ns, 0.06 degree error
		__m128 const two = _mm_set_ps1(2.0f);

		__m128 const a = _mm_sqrt_ps(_mm_fmadd_ps(A, two, two));
		__m128 const b = _mm_mul_ps(_mm_sqrt_ps(_mm_fnmadd_ps(A, two, two)), _mm_set_ps1(_term1));
		__m128 const c = _mm_sqrt_ps(_mm_sub_ps(two, a));
		return (_mm_fmsub_ps(_mm_set_ps1(_term0), c, b));
	}

	// sincos SVML
	STATIC_INLINE_PURE float const __vectorcall sincos(float* const __restrict c, float const A) 
	{
		__m256 c_;
		float const s = _mm_cvtss_f32(_mm256_castps256_ps128(_mm256_sincos_ps(&c_, _mm256_set1_ps(A))));
		*c = _mm_cvtss_f32(_mm256_castps256_ps128(c_));

		return(s);
	}
	STATIC_INLINE_PURE XMVECTOR const __vectorcall sincos(XMVECTOR * const __restrict c, FXMVECTOR const A) 
	{
		__m256 c_;
		__m128 s = _mm256_castps256_ps128(_mm256_sincos_ps(&c_, _mm256_castps128_ps256(A)));
		*c = _mm256_castps256_ps128(c_);

		return(s);
	}
	// exp and log SVML
	STATIC_INLINE_PURE float const __vectorcall __exp(float const A) 
	{
		return(_mm_cvtss_f32(_mm256_castps256_ps128(_mm256_exp_ps(_mm256_set1_ps(A)))));
	}
	STATIC_INLINE_PURE XMVECTOR const __vectorcall __exp(FXMVECTOR const A) 
	{
		return(_mm256_castps256_ps128(_mm256_exp_ps(_mm256_castps128_ps256(A))));
	}
	STATIC_INLINE_PURE float const __vectorcall __exp2(float const A)
	{
		return(_mm_cvtss_f32(_mm256_castps256_ps128(_mm256_exp2_ps(_mm256_set1_ps(A)))));
	}
	STATIC_INLINE_PURE XMVECTOR const __vectorcall __exp2(FXMVECTOR const A)
	{
		return(_mm256_castps256_ps128(_mm256_exp2_ps(_mm256_castps128_ps256(A))));
	}
	STATIC_INLINE_PURE float const __vectorcall __log(float const A) 
	{
		return(_mm_cvtss_f32(_mm256_castps256_ps128(_mm256_log_ps(_mm256_set1_ps(A)))));
	}
	STATIC_INLINE_PURE XMVECTOR const __vectorcall __log(FXMVECTOR const A)
	{
		return(_mm256_castps256_ps128(_mm256_log_ps(_mm256_castps128_ps256(A))));
	}
	// pow SVML
	STATIC_INLINE_PURE float const __vectorcall __pow(float const A, float const power) 
	{
		return(_mm_cvtss_f32(_mm256_castps256_ps128(_mm256_pow_ps(_mm256_set1_ps(A), _mm256_set1_ps(power)))));
	}
	STATIC_INLINE_PURE XMVECTOR const __vectorcall __pow(FXMVECTOR const A, FXMVECTOR const power)
	{
		return(_mm256_castps256_ps128(_mm256_pow_ps(_mm256_castps128_ps256(A), _mm256_castps128_ps256(power))));
	}
#ifdef XMVectorPow	// msft directxmath version of XMVectorPow is abysmal, override
#undef XMVectorPow
#define XMVectorPow __pow
#endif

	// signed fract, fract, mod & sgn:
	//
	STATIC_INLINE_PURE XMVECTOR const __vectorcall sfract(FXMVECTOR const Sn)
	{
		return(_mm_sub_ps(Sn, _mm_round_ps(Sn, _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC)));	// faster signed fractional part
	}
	STATIC_INLINE_PURE float const __vectorcall sfract(float const Sn)
	{
		return(_mm_cvtss_f32(sfract(_mm_set1_ps(Sn))));	// faster signed fractional part
	}

	STATIC_INLINE_PURE float const __vectorcall fract(float const Sn)
	{
		return(SFM::abs(sfract(Sn)));	// faster unsigned fractional part
	}
	STATIC_INLINE_PURE XMVECTOR const __vectorcall fract(FXMVECTOR const Sn)
	{
		return(SFM::abs(sfract(Sn)));	// faster unsigned fractional part
	}

	STATIC_INLINE_PURE float const __vectorcall mod(float const Sn, float& fInteger)  // mod > breaks number into integer and fractional components.
	{
		// returns unsigned fractional part, store integral part in fIntegral

		float const fFractional = SFM::fract(Sn);
		fInteger = Sn - fFractional;
		return(fFractional); // faster than C99 modff
	}

	STATIC_INLINE_PURE float const __vectorcall fmod(float const x, float const y)	// fmod > computes the remainder of x/y (remainder of float is it's fractional component) - more akin to glsl's function named "mod(x,y)"
	{
		// V1 % V2 = V1 - V2 * truncate(V1 / V2)    ( truncate is like floor excepts it rounds to zero instead of neg inf )
		return(_mm_cvtss_f32(XMVectorMod(_mm_set1_ps(x), _mm_set1_ps(y)))); // faster than C99 fmodf
	}

	// Extended sign: returns -1, 0 or 1 based on sign of a
	STATIC_INLINE_PURE XMVECTOR const __vectorcall sgn(FXMVECTOR const a) // branchless!
	{
		XMVECTOR const xmP(XMVectorSelect(_mm_setzero_ps(), _mm_set1_ps(1.0f), XMVectorGreater(a, _mm_setzero_ps())));
		XMVECTOR const xmN(XMVectorSelect(_mm_setzero_ps(), _mm_set1_ps(-1.0f), XMVectorLess(a, _mm_setzero_ps())));

		return(XMVectorAdd(xmP, xmN));
	}
	STATIC_INLINE_PURE float const __vectorcall sgn(float const a) // branchless!
	{
		// -1, 0, or +1
		// Comparisons are not branching
		// https://www.finblackett.com/bithacks
		return( ((float)((a > 0.0f) - (a < 0.0f))) );
	}
	STATIC_INLINE_PURE int32_t const __vectorcall sgn(int32_t const a) // branchless!
	{
		// -1, 0, or +1
		// Comparisons are not branching
		// https://www.finblackett.com/bithacks
		return( (a > 0) - (a < 0) );
	}

	// interpolation & easing:
	//
	STATIC_INLINE_PURE double const __vectorcall lerp(double const A, double const B, double const tNorm)
	{
		//A = __fma(tNorm, B, (1 - tNorm)*A)		// ok
		//A = __fma(tNorm, B, __fma(-tNorm, A, A));	// better
		//A = __fma(B - A, tNorm, A);				// best
		return(__fma(B - A, tNorm, A));
	}
	STATIC_INLINE_PURE float const __vectorcall lerp(float const A, float const B, float const tNorm)
	{
		//A = __fma(tNorm, B, (1 - tNorm)*A)		// ok
		//A = __fma(tNorm, B, __fma(-tNorm, A, A));	// better
		//A = __fma(B - A, tNorm, A);				// best
		return(__fma(B - A, tNorm, A));
	}
	STATIC_INLINE_PURE XMVECTOR const __vectorcall lerp(FXMVECTOR const A, FXMVECTOR const B, float const tNorm)
	{
		return(__fma(_mm_sub_ps(B, A), _mm_set_ps1(tNorm), A));
	}
	STATIC_INLINE_PURE XMVECTOR const __vectorcall lerp(FXMVECTOR const A, FXMVECTOR const B, FXMVECTOR const tNorm)
	{
		return(__fma(_mm_sub_ps(B, A), tNorm, A));
	}

	STATIC_INLINE_PURE float const __vectorcall mix(float const A, float const B, float const weight)
	{
		return(lerp(A, B, weight));
	}

	// triangle wave - for converting a linear [0.0f ... 1.0f] range to a triangular version mapping to [0.0f ... 0.5f] = [0.0f ... 1.0f] then [0.5f ... 0.0f] = [1.0f ... 0.0f]
	STATIC_INLINE_PURE float const __vectorcall triangle_wave(float const A, float const B, float const tNorm)
	{
		float const t = SFM::abs(SFM::fract(tNorm) * 2.0f - 1.0f);

		return(lerp(A, B, t));
	}
	STATIC_INLINE_PURE XMVECTOR const __vectorcall triangle_wave(FXMVECTOR const A, FXMVECTOR const B, float const tNorm)
	{
		float const t = SFM::abs(SFM::fract(tNorm) * 2.0f - 1.0f);

		return(lerp(A, B, t));
	}
	STATIC_INLINE_PURE XMVECTOR const __vectorcall triangle_wave(FXMVECTOR const A, FXMVECTOR const B, FXMVECTOR const tNorm)
	{
		XMVECTOR const t = SFM::abs(XMVectorSubtract(XMVectorScale(SFM::fract(tNorm), 2.0f), _mm_set_ps1(1.0f)));

		return(lerp(A, B, t));
	}

	// https://www.shadertoy.com/view/Xt23zV - Dave Hoskins 
	// Linear Step - give it a range [edge0, edge1] and a fraction between [0...1]
	// returns the normalized [0...1] equivalent of whatever range [edge0, edge1] is, linearly.
	// (like smoothstep, except it's purely linear
	STATIC_INLINE_PURE float const __vectorcall linearstep(float const edge0, float const edge1, float const x)
	{
		return(saturate((x - edge0) / (edge1 - edge0)));
	}
	STATIC_INLINE_PURE XMVECTOR const __vectorcall linearstep(XMVECTOR const edge0, XMVECTOR const edge1, XMVECTOR const x)
	{
		return(SFM::saturate((x - edge0) / (edge1 - edge0)));
	}
	/*
	// linearstep is invertable, derived inverse_linearstep:
	float inverse_linearstep(in const float edge0, in const float edge1, in const float d)
	{
		//       (x - edge0)
		// d = ---------------
		//     (edge1 - edge0)
		//
		// x = d * (edge1 - edge0) + edge0
		//
		// herbie optimized -> fma(d, edge1 - edge0, edge0)
		return( clamp(fma(d, edge1 - edge0, edge0), 0.0f, 1.0f) );
	}
	*/
	STATIC_INLINE_PURE float const __vectorcall inverse_linearstep(float const edge0, float const edge1, float const d)
	{
		return(SFM::saturate(SFM::__fma(d, edge1 - edge0, edge0)));
	}
	STATIC_INLINE_PURE XMVECTOR const __vectorcall inverse_linearstep(XMVECTOR const edge0, XMVECTOR const edge1, XMVECTOR const d)
	{
		return(SFM::saturate(SFM::__fma(d, edge1 - edge0, edge0)));
	}
	
	// tNorm = current time / total duration = [0.0f... 1.0f] //
	STATIC_INLINE_PURE float const __vectorcall smoothstep(float const A, float const B, float const tNorm)
	{
		float const t = tNorm * tNorm * (3.0f - 2.0f * tNorm);

		return(lerp(A, B, t));
	}
	STATIC_INLINE_PURE XMVECTOR const __vectorcall smoothstep(FXMVECTOR const A, FXMVECTOR const B, float const tNorm)
	{
		float const t = tNorm * tNorm * (3.0f - 2.0f * tNorm);

		return(lerp(A, B, t));
	}
	STATIC_INLINE_PURE XMVECTOR const __vectorcall smoothstep(FXMVECTOR const A, FXMVECTOR const B, FXMVECTOR const tNorm)
	{
		XMVECTOR const t = XMVectorMultiply(XMVectorMultiply(tNorm, tNorm), XMVectorSubtract(_mm_set_ps1(3.0f), XMVectorScale(tNorm, 2.0f)));

		return(lerp(A, B, t));
	}
	STATIC_INLINE_PURE float const __vectorcall step(float const edge, float const x)
	{
		// return value, 0.0 is returned if x < edge, and 1.0 is returned otherwise
		return(x < edge ? 0.0f : 1.0f);
	}
	STATIC_INLINE_PURE XMVECTOR const __vectorcall step(FXMVECTOR const edge, FXMVECTOR const x)
	{
		// return value, 0.0 is returned if x < edge, and 1.0 is returned otherwise
		return(XMVectorSelect(_mm_set_ps1(1.0f), _mm_setzero_ps(), XMVectorLess(x, edge)));
	}

	STATIC_INLINE_PURE float const __vectorcall bellcurve(float x) // 0..1 input to 0..1 output *or* -1...1 input to 0...1 output
	{
		// mu is 0.0 (centered) sigma is 0.5
		constexpr float const c(XM_PI / -1.25331414f); // optimized magic value - bellcurve perfect match (to six digits of precision) - removes sqrt
												     // https://www.desmos.com/calculator/xxwdiqa4sk
		x = 2.0f * x;  // converts input range
		
		return(SFM::__exp(x * x * c));
	}
	STATIC_INLINE_PURE XMVECTOR const __vectorcall bellcurve(XMVECTOR x) // 0..1 input to 0..1 output *or* -1...1 input to 0...1 output
	{
		// mu is 0.0 (centered) sigma is 0.5
		constexpr float const c(XM_PI / -1.25331414f); // optimized magic value - bellcurve perfect match (to six digits of precision) - removes sqrt
													   // https://www.desmos.com/calculator/xxwdiqa4sk

		x = XMVectorMultiply(x, XMVectorReplicate(2.0f));  // converts input range

		return(SFM::__exp(XMVectorMultiply(XMVectorMultiply(x, x), XMVectorReplicate(c)))); // range after is [0.0f...1.0f]
	}

	STATIC_INLINE_PURE float const __vectorcall smoothfract(float const Sn)
	{
		// smoothstep(.001, .999 - step(.4, SFM::abs(x - .5)), x - step(.9, x))
		float const s = fract(Sn);

		return(smoothstep(0.001f, 0.999f - step(0.4f, SFM::abs(s - 0.5f)), s - step(0.9f, s)));
	}
	STATIC_INLINE_PURE XMVECTOR const __vectorcall smoothfract(FXMVECTOR const Sn)
	{
		XMVECTOR const s = fract(Sn);

		return(smoothstep(_mm_set1_ps(0.001f), XMVectorSubtract(_mm_set1_ps(0.999f),
			step(_mm_set1_ps(0.4f), SFM::abs(XMVectorSubtract(s, _mm_set1_ps(0.5f))))),
			XMVectorSubtract(s, step(_mm_set1_ps(0.9f), s))));
	}

	// So note slerp is computationally very expensive
	// in *most* cases slerp is not required (ie. animation)
	// You should use nlerp instead which is simply: normalize(lerp(a,b,t))
	// see: 
	// https://www.desmos.com/calculator/pk6bnorbtq
	//
	//=== Slerp =================================================== - superior interpolation for normalized vectors (directions)
	// original glsl @ https://www.shadertoy.com/view/4sV3zt
	// which was adapted from source at:
	// https://keithmaggio.wordpress.com/2011/02/15/math-magician-lerp-slerp-and-nlerp/
	STATIC_INLINE_PURE XMVECTOR const __vectorcall clamp(FXMVECTOR a, FXMVECTOR min_, FXMVECTOR max_); // forward required declaration

	// A & B should be normalized directions/vectors not points
	STATIC_INLINE_PURE XMVECTOR const __vectorcall slerp3D(FXMVECTOR const A, FXMVECTOR const B, float const tNorm) // 3D xyz vectors version
	{

		// Dot product - the cosine of the angle between 2 vectors.
		XMVECTOR xmDot = XMVector3Dot(A, B);
		// Clamp it to be in the range of Acos()
		// This may be unnecessary, but floating point
		// precision can be a fickle mistress.
		xmDot = clamp(xmDot, g_XMNegativeOne, g_XMOne);
		// Acos(dot) returns the angle between start and end,
		// And multiplying that by percent returns the angle between
		// start and the final result.
		XMVECTOR xmSin, xmCos;
		{
			XMVECTOR const xmTheta = XMVectorScale(__acos(xmDot), tNorm);
			xmSin = sincos(&xmCos, xmTheta);
		}
		XMVECTOR const xmRelative = XMVector3Normalize(XMVectorSubtract(B, XMVectorMultiply(A, xmDot))); // Orthonormal basis
		// The final result.
		return (__fma(A, xmCos, XMVectorMultiply(xmRelative, xmSin)));
	}

	// A & B should be normalized directions/vectors not points
	STATIC_INLINE_PURE XMVECTOR const __vectorcall slerp2D(FXMVECTOR const A, FXMVECTOR const B, float const tNorm) // 2D xy vectors version
	{
		// Dot product - the cosine of the angle between 2 vectors.
		XMVECTOR xmDot = XMVector2Dot(A, B);
		// Clamp it to be in the range of Acos()
		// This may be unnecessary, but floating point
		// precision can be a fickle mistress.
		xmDot = clamp(xmDot, g_XMNegativeOne, g_XMOne);
		// Acos(dot) returns the angle between start and end,
		// And multiplying that by percent returns the angle between
		// start and the final result.
		XMVECTOR xmSin, xmCos;
		{
			XMVECTOR const xmTheta = XMVectorScale(__acos(xmDot), tNorm);
			xmSin = sincos(&xmCos, xmTheta);
		}
		XMVECTOR const xmRelative = XMVector2Normalize(XMVectorSubtract(B, XMVectorMultiply(A, xmDot))); // Orthonormal basis
		// The final result.
		return (__fma(A, xmCos, XMVectorMultiply(xmRelative, xmSin)));
	}

	// spherical coordinate generation by golden ratio
	// https://www.shadertoy.com/view/wtByz1 - sphere plotted with point generated from golden ratio.
	// this is a modified function derived from above.
	// takes an input of 1D values, the current iteration value, and the maximum value or number of values used
	// returns 3D point on surface of sphere equidistance from other points
	namespace constants
	{	// g value is scaled to work with same number of bits to shift, see: https://www.desmos.com/calculator/deqrvwazm9
		XMGLOBALCONST inline float const _golden{ XM_PI * (1.0f + SFM::__sqrt(5.0f)) };
	} // end ns
	STATIC_INLINE_PURE XMVECTOR const __vectorcall golden_sphere_coord(float const iteration, float const max_iterations)
	{
		float const phi = SFM::__acos_approx(1.0f - 2.0f * (iteration + 0.5f) / max_iterations);

		float const theta = constants::_golden * iteration;

		float cphi, sphi;
		sphi = SFM::sincos(&cphi, phi);;

		float c, s;
		s = SFM::sincos(&c, theta);

		// this is the most natural function ever
		return(XMVectorSet(sphi * c, sphi * s, cphi, 0.0f));
	}



	// circular interpolation
	// recommend ease_in on one component(x|y)
	// and ease_inout on the other component (x,y)
	// tNorm = current time / total duration = [0.0f... 1.0f] //
	// accelerate from A
	STATIC_INLINE_PURE float const __vectorcall ease_in_circular(float const A, float const B, float const tNorm)
	{
#define Delta (B-A)

		return(-Delta * (SFM::__sqrt(1.0f - tNorm * tNorm) - 1.0f) + A);
#undef Delta
	}
	// decelerate to B
	STATIC_INLINE_PURE float const __vectorcall ease_out_circular(float const A, float const B, float tNorm)
	{
#define Delta (B-A)
		--tNorm;
		return(Delta * SFM::__sqrt(1.0f - tNorm * tNorm) + A);
#undef Delta
	}
	// accel to 0.5 , decel from 0.5 (of A to B)
	STATIC_INLINE_PURE float const __vectorcall ease_inout_circular(float const A, float const B, float tNorm)
	{
#define Delta ((B-A)*0.5f)
		tNorm *= 2.0f;
		if (tNorm < 1.0f)
			return(-Delta * (SFM::__sqrt(1.0f - tNorm * tNorm) - 1.0f) + A);
		tNorm -= 2.0f;
		return(Delta * (SFM::__sqrt(1.0f - tNorm * tNorm) + 1.0f) + A);
#undef Delta
	}

	// quadratic interpolation
	// recommend ease_in on one component(x|y)
	// and ease_inout on the other component (x,y)
	// tNorm = current time / total duration = [0.0f... 1.0f] //
	// accelerate from A
	STATIC_INLINE_PURE float const __vectorcall ease_in_quadratic(float const A, float const B, float const tNorm)
	{
#define Delta (B-A)

		return(Delta * tNorm * tNorm + A);
#undef Delta
	}
	STATIC_INLINE_PURE XMVECTOR const __vectorcall ease_in_quadratic(FXMVECTOR const A, FXMVECTOR const B, float const tNorm)
	{
		XMVECTOR const xmNorm(_mm_set1_ps(tNorm));

#define Delta (XMVectorSubtract(B, A))

		return( XMVectorMultiply(Delta, SFM::__fma(xmNorm, xmNorm, A)) );
#undef Delta
	}

	// decelerate to B
	STATIC_INLINE_PURE float const __vectorcall ease_out_quadratic(float const A, float const B, float const tNorm)
	{
#define Delta (B-A)

		return(-Delta * tNorm * (tNorm - 2.0f) + A);
#undef Delta
	}
	STATIC_INLINE_PURE XMVECTOR const __vectorcall ease_out_quadratic(FXMVECTOR const A, FXMVECTOR const B, float const tNorm)
	{
		XMVECTOR const xmNorm(_mm_set1_ps(tNorm));

#define Delta (XMVectorSubtract(B, A))

		return(XMVectorMultiply(XMVectorNegate(Delta), SFM::__fma(xmNorm, XMVectorSubtract(xmNorm, _mm_set1_ps(2.0f)), A)));
#undef Delta
	}

	// accel to 0.5 , decel from 0.5 (of A to B)
	STATIC_INLINE_PURE float const __vectorcall ease_inout_quadratic(float const A, float const B, float tNorm)
	{
#define Delta ((B-A)*0.5f)
		tNorm *= 2.0f;
		if (tNorm < 1.0f)
			return(Delta * tNorm * tNorm + A);
		--tNorm;
		return(-Delta * (tNorm * (tNorm - 2.0f) - 1.0f) + A);
#undef Delta
	}
	STATIC_INLINE_PURE XMVECTOR const __vectorcall ease_inout_quadratic(FXMVECTOR const A, FXMVECTOR const B, float tNorm)
	{
#define Delta (XMVectorScale(XMVectorSubtract(B, A), 0.5f))

		tNorm *= 2.0f;
		if (tNorm < 1.0f) {
			XMVECTOR const xmNorm(_mm_set1_ps(tNorm));
			return(XMVectorMultiply(Delta, SFM::__fma(xmNorm, xmNorm, A)));
		}
		--tNorm;
		XMVECTOR const xmNorm(_mm_set1_ps(tNorm));
		return(SFM::__fma(XMVectorNegate(Delta), SFM::__fms(xmNorm, XMVectorSubtract(xmNorm, _mm_set1_ps(2.0f)), _mm_set1_ps(1.0f)), A));
#undef Delta
	}

	STATIC_INLINE_PURE XMVECTOR const __vectorcall catmull(FXMVECTOR const Scalars, float const tNorm) // catmull spline interpolation for a set of scalars (eg. heights)
	{
		float const t2 = tNorm * tNorm;
		float const t3 = tNorm * t2;

		XMVECTOR Packed = _mm_set_ps(
			(-t3 + 2.0f * t2 - tNorm) * 0.5f,
			(3.0f * t3 - 5.0f * t2 + 2.0f) * 0.5f,
			(-3.0f * t3 + 4.0f * t2 + tNorm) * 0.5f,
			(t3 - t2) * 0.5f
		);

		Packed = _mm_mul_ps(Packed, Scalars);
		Packed = _mm_hadd_ps(Packed, Packed); // X & Y = X+Y, Z & W = Z+W
		Packed = _mm_hadd_ps(Packed, Packed); // "" ""
		// all components have same value now, which is the result scalar (eg. interpolated height)
		return(Packed);
	}

	// floor, ceil, round:
	//

	/// #### ceiling
	STATIC_INLINE_PURE double const __vectorcall ceil(double const a) //sse4 required
	{
		return(_mm_cvtsd_f64(_mm_round_pd(_mm_set1_pd(a), _MM_FROUND_TO_POS_INF | _MM_FROUND_NO_EXC)));
	}
	STATIC_INLINE_PURE float const __vectorcall ceil(float const a) //sse4 required
	{
		return(_mm_cvtss_f32(_mm_round_ps(_mm_set1_ps(a), _MM_FROUND_TO_POS_INF | _MM_FROUND_NO_EXC)));
	}
	STATIC_INLINE_PURE XMVECTOR const __vectorcall ceil(FXMVECTOR const a) //sse4 required
	{
		return(_mm_round_ps(a, _MM_FROUND_TO_POS_INF | _MM_FROUND_NO_EXC));
	}
	STATIC_INLINE_PURE int64_t const __vectorcall ceil_to_i64(double const a) //sse4 required
	{
		return(_mm_cvtsd_si64(_mm_round_pd(_mm_set1_pd(a), _MM_FROUND_TO_POS_INF | _MM_FROUND_NO_EXC)));
	}
	STATIC_INLINE_PURE int32_t const __vectorcall ceil_to_i32(float const a) //sse4 required
	{
		return(_mm_cvtss_si32(_mm_round_ps(_mm_set1_ps(a), _MM_FROUND_TO_POS_INF | _MM_FROUND_NO_EXC)));
	}
	STATIC_INLINE_PURE ivec4_v const __vectorcall ceil_to_i32(FXMVECTOR const a) //sse4 required
	{
		return(ivec4_v(_mm_cvtps_epi32(ceil(a))));
	}
	STATIC_INLINE_PURE uint64_t const __vectorcall ceil_to_u64(double const a) //sse4 required
	{
		return((uint64_t)ceil_to_i64(a));
	}
	STATIC_INLINE_PURE uint32_t const __vectorcall ceil_to_u32(float const a) //sse4 required
	{
		return((uint32_t)ceil_to_i32(a));
	}
	STATIC_INLINE_PURE uvec4_v const __vectorcall ceil_to_u32(FXMVECTOR const a) //sse4 required
	{
		return(uvec4_v(_mm_cvtps_epi32(ceil(a))));
	}
	template<class T>
	struct ceiling			/// #### So this is HOW todo function dispatch based on type at compile time! ##### ////
	{
		// if "condition (true/false)" of equal type, enable tempolate instantion of function
		template< bool cond, typename U >
		using resolvedType = typename std::enable_if< cond, U >::type;

		template< typename U = T >
		constexpr STATIC_INLINE_PURE resolvedType< std::is_same<U, double>::value, U > const __vectorcall ceil(double const a)
		{
			return(SFM::ceil(a));
		}
		template< typename U = T >
		constexpr STATIC_INLINE_PURE resolvedType< std::is_same<U, float>::value, U > const __vectorcall ceil(float const a)
		{
			return(SFM::ceil(a));
		}
		template< typename U = T >
		constexpr STATIC_INLINE_PURE resolvedType< std::is_same<U, int32_t>::value, U > const __vectorcall ceil(float const a)
		{
			return(ceil_to_i32(a));
		}
		template< typename U = T >
		constexpr STATIC_INLINE_PURE resolvedType< std::is_same<U, uint32_t>::value, U > const __vectorcall ceil(float const a)
		{
			return(ceil_to_u32(a));
		}
	};
	template <typename T>
	constexpr STATIC_INLINE_PURE T const __vectorcall ceil(double const a) //sse4 required
	{ // default double returned function must be defined b4 this func and rounding struct 
		return(ceiling<T>::ceil(a));
	}
	template <typename T>
	constexpr STATIC_INLINE_PURE T const __vectorcall ceil(float const a) //sse4 required
	{ // default float returned function must be defined b4 this func and rounding struct 
		return(ceiling<T>::ceil(a));
	}

	/// #### flooring
	STATIC_INLINE_PURE double const __vectorcall floor(double const a) //sse4 required
	{
		return(_mm_cvtsd_f64(_mm_round_pd(_mm_set1_pd(a), _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC)));
	}
	STATIC_INLINE_PURE float const __vectorcall floor(float const a) //sse4 required
	{
		return(_mm_cvtss_f32(_mm_round_ps(_mm_set1_ps(a), _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC)));
	}
	STATIC_INLINE_PURE XMVECTOR const __vectorcall floor(FXMVECTOR const a) //sse4 required
	{
		return(_mm_round_ps(a, _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC));
	}
	STATIC_INLINE_PURE int64_t const __vectorcall floor_to_i64(double const a) //sse4 required
	{
		return(_mm_cvtsd_si64(_mm_round_pd(_mm_set1_pd(a), _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC)));
	}
	STATIC_INLINE_PURE int32_t const __vectorcall floor_to_i32(float const a) //sse4 required
	{
		return(_mm_cvtss_si32(_mm_round_ps(_mm_set1_ps(a), _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC)));
	}
	STATIC_INLINE_PURE ivec4_v const __vectorcall floor_to_i32(FXMVECTOR const a) //sse4 required
	{
		return(ivec4_v(_mm_cvtps_epi32(floor(a))));
	}
	STATIC_INLINE_PURE uint64_t const __vectorcall floor_to_u64(double const a) //sse4 required
	{
		return((uint64_t)floor_to_i64(a));
	}
	STATIC_INLINE_PURE uint32_t const __vectorcall floor_to_u32(float const a) //sse4 required
	{
		return((uint32_t)floor_to_i32(a));
	}
	STATIC_INLINE_PURE uvec4_v const __vectorcall floor_to_u32(FXMVECTOR const a) //sse4 required
	{
		return(uvec4_v(_mm_cvtps_epi32(floor(a))));
	}
	template<class T>
	struct flooring			/// #### So this is HOW todo function dispatch based on type at compile time! ##### ////
	{
		// if "condition (true/false)" of equal type, enable tempolate instantion of function
		template< bool cond, typename U >
		using resolvedType = typename std::enable_if< cond, U >::type;

		template< typename U = T >
		constexpr STATIC_INLINE_PURE resolvedType< std::is_same<U, double>::value, U > const __vectorcall floor(double const a)
		{
			return(SFM::floor(a));
		}
		template< typename U = T >
		constexpr STATIC_INLINE_PURE resolvedType< std::is_same<U, float>::value, U > const __vectorcall floor(float const a)
		{
			return(SFM::floor(a));
		}
		template< typename U = T >
		constexpr STATIC_INLINE_PURE resolvedType< std::is_same<U, int32_t>::value, U > const __vectorcall floor(float const a)
		{
			return(floor_to_i32(a));
		}
		template< typename U = T >
		constexpr STATIC_INLINE_PURE resolvedType< std::is_same<U, uint32_t>::value, U > const __vectorcall floor(float const a)
		{
			return(floor_to_u32(a));
		}
	};
	template <typename T>
	constexpr STATIC_INLINE_PURE T const __vectorcall floor(double const a) //sse4 required
	{ // default double returned function must be defined b4 this func and rounding struct 
		return(flooring<T>::floor(a));
	}
	template <typename T>
	constexpr STATIC_INLINE_PURE T const __vectorcall floor(float const a) //sse4 required
	{ // default float returned function must be defined b4 this func and rounding struct 
		return(flooring<T>::floor(a));
	}

	/// ######### rounding
	STATIC_INLINE_PURE double const __vectorcall round(double const a) //sse4 required
	{
		return(_mm_cvtsd_f64(_mm_round_pd(_mm_set1_pd(a), _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC)));
	}
	STATIC_INLINE_PURE float const __vectorcall round(float const a) //sse4 required
	{
		return(_mm_cvtss_f32(_mm_round_ps(_mm_set1_ps(a), _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC)));
	}
	STATIC_INLINE_PURE XMVECTOR const __vectorcall round(FXMVECTOR const a) //sse4 required
	{
		return(_mm_round_ps(a, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC));
	}
	STATIC_INLINE_PURE int64_t const __vectorcall round_to_i64(double const a) //sse4 required
	{
		return(_mm_cvtsd_si64(_mm_round_pd(_mm_set1_pd(a), _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC)));
	}
	STATIC_INLINE_PURE int32_t const __vectorcall round_to_i32(float const a) //sse4 required
	{
		return(_mm_cvtss_si32(_mm_round_ps(_mm_set1_ps(a), _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC)));
	}
	STATIC_INLINE_PURE ivec4_v const __vectorcall round_to_i32(FXMVECTOR const a) //sse4 required
	{
		return(ivec4_v(_mm_cvtps_epi32(round(a))));
	}
	STATIC_INLINE_PURE uint64_t const __vectorcall round_to_u64(double const a) //sse4 required
	{
		return((uint64_t)round_to_i64(a));
	}
	STATIC_INLINE_PURE uint32_t const __vectorcall round_to_u32(float const a) //sse4 required
	{
		return((uint32_t)round_to_i32(a));
	}
	STATIC_INLINE_PURE uvec4_v const __vectorcall round_to_u32(FXMVECTOR const a) //sse4 required
	{
		return(uvec4_v(_mm_cvtps_epi32(round(a))));
	}
	template<class T>
	struct rounding			/// #### So this is HOW todo function dispatch based on type at compile time! ##### ////
	{
		// if "condition (true/false)" of equal type, enable tempolate instantion of function
		template< bool cond, typename U >
		using resolvedType = typename std::enable_if< cond, U >::type;

		template< typename U = T >
		constexpr STATIC_INLINE_PURE resolvedType< std::is_same<U, double>::value, U > const __vectorcall round(double const a)
		{
			return(SFM::round(a));
		}
		template< typename U = T >
		constexpr STATIC_INLINE_PURE resolvedType< std::is_same<U, float>::value, U > const __vectorcall round(float const a)
		{
			return(SFM::round(a));
		}
		template< typename U = T >
		constexpr STATIC_INLINE_PURE resolvedType< std::is_same<U, int32_t>::value, U > const __vectorcall round(float const a)
		{
			return(round_to_i32(a));
		}
		template< typename U = T >
		constexpr STATIC_INLINE_PURE resolvedType< std::is_same<U, uint32_t>::value, U > const __vectorcall round(float const a)
		{
			return(round_to_u32(a));
		}
	};
	template <typename T>
	constexpr STATIC_INLINE_PURE T const __vectorcall round(double const a) //sse4 required
	{ // default double returned function must be defined b4 this func and rounding struct 
		return(rounding<T>::round(a));
	}
	template <typename T>
	constexpr STATIC_INLINE_PURE T const __vectorcall round(float const a) //sse4 required
	{ // default float returned function must be defined b4 this func and rounding struct 
		return(rounding<T>::round(a));
	}


	// bit hacks and tricks:
	//
	STATIC_INLINE_PURE uint8_t const __vectorcall rbit(uint8_t const bits)  // reverse bit order in a single byte https://graphics.stanford.edu/~seander/bithacks.html#ReverseByteWith64Bits
	{
		return((uint8_t const)(((bits * 0x80200802ULL) & 0x0884422110ULL) * 0x0101010101ULL >> 32));		// requires 64bit compiler and processor
	}

	// ***even multiples only, signed only*** 
	template<bool const bRoundUp>
	STATIC_INLINE_PURE int64_t const roundToMultipleOf(int64_t const n, int64_t const multiple) 
	{
		if constexpr(bRoundUp) {
			return((n + (multiple - 1)) & (-multiple));		// add seven then round down to the closest multiple of 8
		}
		else {
			return(n & (-multiple));						// just the negative and trick for rounding down
		}
	}
	// ***even multiples only, signed only*** 
	template<bool const bRoundUp>
	STATIC_INLINE_PURE int32_t const roundToMultipleOf(int32_t const n, int32_t const multiple)
	{
		if constexpr (bRoundUp) {
			return((n + (multiple - 1)) & (-multiple));		// add seven then round down to the closest multiple of 8
		}
		else {
			return(n & (-multiple));						// just the negative and trick for rounding down
		}
	}
	// ***odd/even multiples, unsigned or signed*** 
	template<typename T>
	STATIC_INLINE_PURE T const __vectorcall roundToNearestMultipleOf(T const n, T const multiple)  // unsigned or signed allowed
	{
		T const half(multiple >> 1);
		return(n + half - (n + half) % multiple);
	}

	STATIC_INLINE_PURE constexpr bool const __vectorcall isPow2(size_t const x) {
		return(0 == (x & (x - 1)));
	}

	STATIC_INLINE_PURE constexpr size_t const __vectorcall nextPow2(size_t x) {
		--x;
		x |= x >> 1;
		x |= x >> 2;
		x |= x >> 4;
		x |= x >> 8;
		x |= x >> 16;
		return(++x);
	}
	
	// color and rgba:
	//

	STATIC_INLINE_PURE uvec4_v const __vectorcall blend(uvec4_v const A, uvec4_v const B)
	{
		__m128i const alpha_A(_mm_set1_epi32(A.a()));
		__m128i const alpha_B(_mm_set1_epi32(255U - A.a()));
		
		return(uvec4_v(_mm_srli_epi32( _mm_add_epi32(_mm_mullo_epi32(A.v, alpha_A), _mm_mullo_epi32(B.v, alpha_B)), 8)));		// SSE4.2 req for _mm_mullo_epi32  *bugfix removed horrible integer division in favor of simple right shift. note that this divides the result by 256 rather than 255 so there is 1 unit of precision loss. The speedup is required as this is frequently used during .GIF animations between frames and per pixel (Order n^3) * division cost. This was slowing it down greatly.
	}

	STATIC_INLINE_PURE uvec4_v const __vectorcall modulate(uvec4_v const A, uvec4_v const B)
	{
		return(uvec4_v(_mm_srli_epi32(_mm_mullo_epi32(A.v, B.v), 8)));		// SSE4.2 req for _mm_mullo_epi32  *bugfix removed horrible integer division in favor of simple right shift. note that this divides the result by 256 rather than 255 so there is 1 unit of precision loss. The speedup is required as this is frequently used during .GIF animations between frames and per pixel (Order n^3) * division cost. This was slowing it down greatly.
	}
	//STATIC_INLINE_PURE uint32_t const __vectorcall blend(uint8_t const A, uint8_t const B, uint8_t const weight)
	//{
	//	return((A + (weight * ((B - A) + 127) / 255)));
	//}

	/*STATIC_INLINE_PURE uvec4_v const __vectorcall blend(uint32_t const dst, uint32_t const src)
	{
		static constexpr float const Inv255 = 1.0f / 255.0f;

		__m128 const xmInv255(_mm_set1_ps(Inv255));

		XMVECTOR const xmAlphaDst = XMVectorMultiply(_mm_cvtepi32_ps(_mm_set1_epi32(dst >> 24)), xmInv255);
		
		XMVECTOR const xmColorDst = XMVectorMultiply(xmAlphaDst, XMVectorMultiply(_mm_cvtepi32_ps(_mm_setr_epi32(255, (dst >> 16) & 0xFF, (dst >> 8) & 0xFF, dst & 0xFF)), xmInv255));
		
		XMVECTOR const xmAlphaSrc = XMVectorMultiply(_mm_cvtepi32_ps(_mm_set1_epi32(src >> 24)), xmInv255);

		XMVECTOR const xmColorSrc = XMVectorMultiply(xmAlphaSrc, XMVectorMultiply(_mm_cvtepi32_ps(_mm_setr_epi32(255, (src >> 16) & 0xFF, (src >> 8) & 0xFF, src & 0xFF)), xmInv255));
		
		return(uvec4_v(SFM::saturate_to_u8(XMVectorScale(XMVectorAdd(xmColorDst, xmColorSrc), 255.0f))));
	}*/

	// bitdepth conversion
	namespace convert565888
	{
		XMGLOBALCONST inline uvec4_t const _c0{ { { 527u, 259u, 527u, 1u } } };
		XMGLOBALCONST inline uvec4_t const _c1{ { { 23u, 33u, 23u, 0u } } };
	} // end ns

	STATIC_INLINE_PURE uvec4_v const __vectorcall rgb565_to_888(uvec4_v const rgba) { // ignores alpha 
		return(uvec4_v(_mm_srli_epi32(_mm_add_epi32(_mm_mullo_epi32(rgba.v, uvec4_v(convert565888::_c0).v), uvec4_v(convert565888::_c1).v), 6)));
	}

	namespace convert888565
	{	// g value is scaled to work with same number of bits to shift, see: https://www.desmos.com/calculator/deqrvwazm9
		XMGLOBALCONST inline uvec4_t const _c0{ { { 249u, 509u, 249u, 1u } } };
		XMGLOBALCONST inline uvec4_t const _c1{ { { 1014u, 253u, 1014u, 0u } } };
	} // end ns

	STATIC_INLINE_PURE uvec4_v const __vectorcall rgb888_to_565(uvec4_v const rgba) { // ignores alpha
		return(uvec4_v(_mm_srli_epi32(_mm_add_epi32(_mm_mullo_epi32(rgba.v, uvec4_v(convert888565::_c0).v), uvec4_v(convert888565::_c1).v), 11)));
	}

	// packing
	STATIC_INLINE_PURE uint32_t const __vectorcall pack_rgba(uint32_t const r, uint32_t const g, uint32_t const b, uint32_t const a) {

		return(((a << 24U) | (b << 16U) | (g << 8U) | r));  // ABGR = RGBA mem order
	}
	STATIC_INLINE_PURE uint32_t const __vectorcall pack_rgba(uint32_t const luma) {

		return(((luma << 24U) | (luma << 16U) | (luma << 8U) | luma));  // ABGR = RGBA mem order
	}
	STATIC_INLINE_PURE uint32_t const __vectorcall pack_rgba(uvec4_t const& __restrict rgba) {

		return(((rgba.a << 24U) | (rgba.b << 16U) | (rgba.g << 8U) | rgba.r));  // ABGR = RGBA mem order
	}
	STATIC_INLINE_PURE void __vectorcall unpack_rgba(uint32_t const packed, uint32_t& __restrict r, uint32_t& __restrict g, uint32_t& __restrict b, uint32_t& __restrict a) {

		r = (packed & 0xFFU);
		g = ((packed >> 8U) & 0xFFU);
		b = ((packed >> 16U) & 0xFFU);
		a = ((packed >> 24U) & 0xFFU);
	}
	STATIC_INLINE_PURE void __vectorcall unpack_rgba(uint32_t const packed, uvec4_t& __restrict rgba) {

		rgba.r = (packed & 0xFFU);
		rgba.g = ((packed >> 8U) & 0xFFU);
		rgba.b = ((packed >> 16U) & 0xFFU);
		rgba.a = ((packed >> 24U) & 0xFFU);
	}
	STATIC_INLINE_PURE bool const __vectorcall rgb_le(uvec4_v const packed, uint32_t const lower_bound) { // returns true if all rgb components are less than or equal to lower_bound
											 //all
		// only testing rgb, don't care about alpha
		__m128i const xmResult = _mm_cmpgt_epi32(packed.v, uvec4_v(lower_bound, 0xFFu).v);
		return( _mm_test_all_zeros(xmResult,xmResult) );
	}

	STATIC_INLINE_PURE uvec4_v const __vectorcall lerp(uvec4_v const A, uvec4_v const B, float const t) { // vectors of color

		static constexpr float const NORMALIZE = 1.0f / float(UINT8_MAX);
		static constexpr float const DENORMALIZE = float(UINT8_MAX);
		XMVECTOR const xmNorm(_mm_set1_ps(NORMALIZE));
		XMVECTOR const xmDeNorm(_mm_set1_ps(DENORMALIZE));

		// floating point precision is best in [0.0f ... 1.0f] range, so normalizing before lerp then denormalization after is a huge benefit
		return( uvec4_v(SFM::saturate_to_u8(XMVectorMultiply(xmDeNorm, SFM::lerp(XMVectorMultiply(xmNorm, A.v4f()), XMVectorMultiply(xmNorm, B.v4f()), t)))) );
	}

	STATIC_INLINE_PURE uint32_t const __vectorcall lerp(uint32_t const A, uint32_t const B, float const t) { // packed color

		uvec4_t rgba_A{}, rgba_B{};

		SFM::unpack_rgba(A, rgba_A);
		SFM::unpack_rgba(B, rgba_B);

		lerp(uvec4_v(rgba_A), uvec4_v(rgba_B), t).rgba(rgba_A);

		return(SFM::pack_rgba(rgba_A));
	}

	//------------------------------------------------------------------------------
	// degamma (srgb->linear) function, leverages faster pow function 
	// vector in must be in 0.0f -> 1.0f range
	namespace toLinear_Konstants {
		XMGLOBALCONST inline XMVECTORF32 const _Cutoff{ { { 0.04045f, 0.04045f, 0.04045f, 1.f } } };
		XMGLOBALCONST inline XMVECTORF32 const _ILinear{ { { 1.f / 12.92f, 1.f / 12.92f, 1.f / 12.92f, 1.f } } };
		XMGLOBALCONST inline XMVECTORF32 const _Scale{ { { 1.f / 1.055f, 1.f / 1.055f, 1.f / 1.055f, 1.f } } };
		XMGLOBALCONST inline XMVECTORF32 const _Bias{ { { 0.055f, 0.055f, 0.055f, 0.f } } };
		XMGLOBALCONST inline XMVECTORF32 const _Gamma{ { { 2.4f, 2.4f, 2.4f, 1.f } } };
	} // end ns
	STATIC_INLINE_PURE XMVECTOR __vectorcall XMColorSRGBToRGB(FXMVECTOR srgb)
	{
		XMVECTOR V = XMVectorSaturate(srgb);
		XMVECTOR V0 = XMVectorMultiply(V, toLinear_Konstants::_ILinear);
		XMVECTOR V1 = SFM::__pow(XMVectorMultiply(XMVectorAdd(V, toLinear_Konstants::_Bias), toLinear_Konstants::_Scale), toLinear_Konstants::_Gamma);
		XMVECTOR select = XMVectorGreater(V, toLinear_Konstants::_Cutoff);
		V = XMVectorSelect(V0, V1, select);
		return(XMVectorSelect(srgb, V, g_XMSelect1110));
	}
#define SRGB_to_FLOAT SFM::XMColorSRGBToRGB

	STATIC_INLINE_PURE uint32_t const srgb_to_linear_rgba(uint32_t const packed) // there is a faster function that uses a lookup table in Imaging.
	{
		uvec4_t rgba;
		SFM::unpack_rgba(packed, rgba);

		static constexpr float const NORMALIZE = 1.0f / float(UINT8_MAX);
		static constexpr float const DENORMALIZE = float(UINT8_MAX);

		XMVECTOR xmColor = XMVectorMultiply(_mm_set1_ps(NORMALIZE), uvec4_v(rgba).v4f());
		xmColor = SFM::XMColorSRGBToRGB(xmColor);
		SFM::saturate_to_u8(XMVectorMultiply(_mm_set1_ps(DENORMALIZE), xmColor), rgba);

		return( SFM::pack_rgba(rgba) );
	}

	//------------------------------------------------------------------------------
	// gamma (linear->srgb) function, leverages faster pow function
	// vector in must be in 0.0f -> 1.0f range
	namespace toSRGB_Konstants {
		XMGLOBALCONST inline XMVECTORF32 const _Cutoff{ { { 0.0031308f, 0.0031308f, 0.0031308f, 1.f } } };
		XMGLOBALCONST inline XMVECTORF32 const _Linear{ { { 12.92f, 12.92f, 12.92f, 1.f } } };
		XMGLOBALCONST inline XMVECTORF32 const _Scale{ { { 1.055f, 1.055f, 1.055f, 1.f } } };
		XMGLOBALCONST inline XMVECTORF32 const _Bias{ { { 0.055f, 0.055f, 0.055f, 0.f } } };
		XMGLOBALCONST inline XMVECTORF32 const _InvGamma{ { { 1.0f / 2.4f, 1.0f / 2.4f, 1.0f / 2.4f, 1.f } } };
	} // end ns
	STATIC_INLINE_PURE XMVECTOR __vectorcall XMColorRGBToSRGB(FXMVECTOR rgb)
	{
		XMVECTOR V = XMVectorSaturate(rgb);
		XMVECTOR V0 = XMVectorMultiply(V, toSRGB_Konstants::_Linear);
		XMVECTOR V1 = XMVectorSubtract(XMVectorMultiply(toSRGB_Konstants::_Scale, SFM::__pow(V, toSRGB_Konstants::_InvGamma)), toSRGB_Konstants::_Bias);
		XMVECTOR select = XMVectorLess(V, toSRGB_Konstants::_Cutoff);
		V = XMVectorSelect(V1, V0, select);
		return(XMVectorSelect(rgb, V, g_XMSelect1110));
	}
	#define FLOAT_to_SRGB SFM::XMColorRGBToSRGB

	// Y = (0.2126 * sRGBtoLin(vR) + 0.7152 * sRGBtoLin(vG) + 0.0722 * sRGBtoLin(vB))
	XMGLOBALCONST inline XMVECTORF32 const _Luminance{ 0.2126f, 0.7152f, 0.0722f };

	STATIC_INLINE_PURE float const __vectorcall XMColorSRGBToLuminance(FXMVECTOR srgb) // returns color's luminance, correctly transforms to linear space before luminance is calculated
	{
		XMVECTOR xmLinear = XMColorSRGBToRGB(srgb);

		xmLinear = XMVector3Dot(_Luminance, xmLinear);

		return(XMVectorGetX(xmLinear));
	}
	STATIC_INLINE_PURE float const __vectorcall XMColorRGBToLuminance(FXMVECTOR linear) // returns color's luminance, for linear color space only!
	{
		return(XMVectorGetX(XMVector3Dot(_Luminance, linear)));
	}
	STATIC_INLINE_PURE float const __vectorcall luminance(uvec4_v const packed) // assumes color is in linear space
	{
		static constexpr float const NORMALIZE = 1.0f / float(UINT8_MAX);
		return(XMColorRGBToLuminance(XMVectorScale(packed.v4f(), NORMALIZE)));
	}
	
	// OKLAB colorspace
	/* https://www.shadertoy.com/view/ttcyRS w/updated values
	vec3 oklab_mix( vec3 colA, vec3 colB, float h )
	{
		// https://bottosson.github.io/posts/oklab
		const mat3 kCONEtoLMS = mat3(                
			 0.4122214708f,  0.2119034982f,  0.0883024619f,
			 0.5363325363f,  0.6806995451f,  0.2817188376f,
			 0.0514459929f,  0.1073969566f,  0.6299787005f);
		const mat3 kLMStoCONE = mat3(
			 4.0767416621f, -1.2684380046f, -0.0041960863f,
			-3.3077115913f,  2.6097574011f, -0.7034186147f,
			 0.2309699292f, -0.3413193965f,  1.7076147010f);
                    
		// rgb to cone (arg of pow can't be negative)
		vec3 lmsA = pow( kCONEtoLMS*colA, vec3(1.0f/3.0f) );
		vec3 lmsB = pow( kCONEtoLMS*colB, vec3(1.0f/3.0f) );
		// lerp
		vec3 lms = mix( lmsA, lmsB, h );
		// cone to rgb
		return kLMStoCONE*(lms*lms*lms);
	}
	*/
	namespace oklab_Konstants {
		
		XMGLOBALCONST inline XMFLOAT4X4A const _c0{ 0.4122214708f, 0.2119034982f, 0.0883024619f, 0.0f,
												    0.5363325363f, 0.6806995451f, 0.2817188376f, 0.0f,
												    0.0514459929f, 0.1073969566f, 0.6299787005f, 0.0f,
													0.0f,		   0.0f,		  0.0f,			 1.0f
												  };
		XMGLOBALCONST inline XMFLOAT4X4A const _c1{ 4.0767416621f, -1.2684380046f, -0.0041960863f, 0.0f,
												   -3.3077115913f,  2.6097574011f, -0.7034186147f, 0.0f,
												    0.2309699292f, -0.3413193965f,  1.7076147010f, 0.0f,
													0.0f,			0.0f,			0.0f,		   1.0f
												  };

		static constexpr float const _r0(1.0f / 3.0f);

	} // end ns
	STATIC_INLINE_PURE XMVECTOR const __vectorcall XMColorRGBToOKLAB(FXMVECTOR rgb)
	{
		return(SFM::__pow(XMVector3TransformNormal(rgb, XMLoadFloat4x4A(&oklab_Konstants::_c0)), _mm_set1_ps(oklab_Konstants::_r0)));
	}
	STATIC_INLINE_PURE XMVECTOR const __vectorcall XMColorOKLABToRGB(FXMVECTOR oklab)
	{
		return(XMVector3TransformNormal(XMVectorMultiply(oklab, XMVectorMultiply(oklab, oklab)), XMLoadFloat4x4A(&oklab_Konstants::_c1)));
	}

	// http://lolengine.net/blog/2013/07/27/rgb-to-hsv-in-glsl
	namespace rgbToHSV_Konstants {
		XMGLOBALCONST inline XMVECTORF32 const _c0{ { { 0.0f, -1.0f / 3.0f, 2.0f / 3.0f, -1.0f } } };
		//XMGLOBALCONST inline XMVECTORF32 const _c1{ { { 1.0f, 1.0f, 1.0f, 0.0f } } };
	} // end ns
	STATIC_INLINE_PURE XMVECTOR const __vectorcall XMColorRGBToHSV(FXMVECTOR rgb)
	{
		XMFLOAT4A a0, a1;

#define c a0
#define K a1
#define p a1
#define q a1
#define r x
#define g y
#define b z

		/*
		vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
		vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
		vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));

		float d = q.x - min(q.w, q.y);
		float e = 1.0e-10;
		return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
		*/

		XMStoreFloat4A(&c, rgb);
		XMStoreFloat4A(&K, rgbToHSV_Konstants::_c0);

		XMStoreFloat4A(&p, SFM::lerp(XMVectorSet(c.b, c.g, K.w, K.z), XMVectorSet(c.g, c.b, K.x, K.y), SFM::step(c.b, c.g)));
		XMStoreFloat4A(&q, SFM::lerp(XMVectorSet(p.x, p.y, p.w, c.r), XMVectorSet(c.r, p.y, p.z, p.x), SFM::step(p.x, c.r)));

		float const d = q.x - SFM::min(q.w, q.y);
		constexpr float const e = 1.0e-10f;

		return(XMVectorSet(SFM::abs(q.z + (q.w - q.y) / (6.0f * d + e)), d / (q.x + e), q.x, 0.0f));
#undef c
#undef K
#undef p
#undef q
#undef r
#undef g
#undef b
	}

	// https://www.shadertoy.com/view/4t3czs
	// vec3 HSVToSRGB(float h, float s, float v) {
	//	return vec3(v * (1. - s + s * clamp(abs(fract(h + 1.0) * 6. - 3.) - 1., 0., 1.)),
	//		        v * (1. - s + s * clamp(abs(fract(h + 2. / 3.) * 6. - 3.) - 1., 0., 1.)),
	//		        v * (1. - s + s * clamp(abs(fract(h + 1. / 3.) * 6. - 3.) - 1., 0., 1.)));
	//}
	namespace hsvToSRGB_Konstants {
		XMGLOBALCONST inline XMVECTORF32 const _c0{ { { 1.0f, 2.0f / 3.0f, 1.0f / 3.0f, 0.0f } } };
		XMGLOBALCONST inline XMVECTORF32 const _c1{ { { 1.0f, 1.0f, 1.0f, 0.0f } } };
	} // end ns
	STATIC_INLINE_PURE XMVECTOR const __vectorcall XMColorHSVToSRGB(FXMVECTOR hsv) // better than DirectX::XMColorHSVToRGB
	{
		XMVECTOR h = XMVectorSplatX(hsv);			// xxx = hue
		h = XMVectorAdd(h, hsvToSRGB_Konstants::_c0);
		h = SFM::fract(h);
		h = SFM::__fms(h, _mm_set1_ps(6.0f), _mm_set1_ps(3.0f));
		h = SFM::abs(h);
		h = XMVectorSubtract(h, hsvToSRGB_Konstants::_c1);
		h = SFM::saturate(h);

		XMVECTOR const s = XMVectorSplatY(hsv);		// yyy = saturation
		h = SFM::__fma(s, h, XMVectorSubtract(hsvToSRGB_Konstants::_c1, s));

		return(XMVectorSetW(XMVectorMultiply(XMVectorSplatZ(hsv), h), 1.0f));	// zzz = value
	}
	
	// ####### Palette Polynomials, input range 0.0f....1.0f //
	namespace viridis_Konstants {
		XMGLOBALCONST inline XMVECTORF32 const _c0{ { { 0.2777273272234177, 0.005407344544966578, 0.3340998053353061, 1.f } } };
		XMGLOBALCONST inline XMVECTORF32 const _c1{ { { 0.1050930431085774, 1.404613529898575, 1.384590162594685, 1.f } } };
		XMGLOBALCONST inline XMVECTORF32 const _c2{ { { -0.3308618287255563, 0.214847559468213, 0.09509516302823659, 1.f } } };
		XMGLOBALCONST inline XMVECTORF32 const _c3{ { { -4.634230498983486, -5.799100973351585, -19.33244095627987, 1.f } } };
		XMGLOBALCONST inline XMVECTORF32 const _c4{ { { 6.228269936347081, 14.17993336680509, 56.69055260068105, 1.f } } };
		XMGLOBALCONST inline XMVECTORF32 const _c5{ { { 4.776384997670288, -13.74514537774601, -65.35303263337234, 1.f } } };
		XMGLOBALCONST inline XMVECTORF32 const _c6{ { { -5.435455855934631, 4.645852612178535, 26.3124352495832, 1.f } } };
	} // end ns
	STATIC_INLINE_PURE XMVECTOR const __vectorcall viridis(float const t) {

		XMVECTOR const xmT(_mm_set1_ps(t));

		XMVECTOR x;

		// c0 + t * (c1 + t * (c2 + t * (c3 + t * (c4 + t * (c5 + t * c6)))))
		x = SFM::__fma(xmT, viridis_Konstants::_c6, viridis_Konstants::_c5);
		x = SFM::__fma(x, xmT, viridis_Konstants::_c4);
		x = SFM::__fma(x, xmT, viridis_Konstants::_c3);
		x = SFM::__fma(x, xmT, viridis_Konstants::_c2);
		x = SFM::__fma(x, xmT, viridis_Konstants::_c1);
		x = SFM::__fma(x, xmT, viridis_Konstants::_c0);

		return(x);
	}
	namespace inferno_Konstants {
		XMGLOBALCONST inline XMVECTORF32 const _c0{ { { 0.0002189403691192265, 0.001651004631001012, -0.01948089843709184, 1.f } } };
		XMGLOBALCONST inline XMVECTORF32 const _c1{ { { 0.1065134194856116, 0.5639564367884091, 3.932712388889277, 1.f } } };
		XMGLOBALCONST inline XMVECTORF32 const _c2{ { { 11.60249308247187, -3.972853965665698, -15.9423941062914, 1.f } } };
		XMGLOBALCONST inline XMVECTORF32 const _c3{ { { -41.70399613139459, 17.43639888205313, 44.35414519872813, 1.f } } };
		XMGLOBALCONST inline XMVECTORF32 const _c4{ { { 77.162935699427, -33.40235894210092, -81.80730925738993, 1.f } } };
		XMGLOBALCONST inline XMVECTORF32 const _c5{ { { -71.31942824499214, 32.62606426397723, 73.20951985803202, 1.f } } };
		XMGLOBALCONST inline XMVECTORF32 const _c6{ { { 25.13112622477341, -12.24266895238567, -23.07032500287172, 1.f } } };
	} // end ns
	STATIC_INLINE_PURE XMVECTOR const __vectorcall inferno(float const t) {

		XMVECTOR const xmT(_mm_set1_ps(t));

		XMVECTOR x;

		// c0 + t * (c1 + t * (c2 + t * (c3 + t * (c4 + t * (c5 + t * c6)))))
		x = SFM::__fma(xmT, inferno_Konstants::_c6, inferno_Konstants::_c5);
		x = SFM::__fma(x, xmT, inferno_Konstants::_c4);
		x = SFM::__fma(x, xmT, inferno_Konstants::_c3);
		x = SFM::__fma(x, xmT, inferno_Konstants::_c2);
		x = SFM::__fma(x, xmT, inferno_Konstants::_c1);
		x = SFM::__fma(x, xmT, inferno_Konstants::_c0);

		return(x);
	}
	namespace skinny_Konstants {
		//[0, 58, 50] lower bound  HSV
		//[30, 255, 255] upper bound  HSV
		XMGLOBALCONST inline XMVECTORF32 const _c0{ { { 0.0, 58.0/255.0, 50/255.0, 1.f } } };
		XMGLOBALCONST inline XMVECTORF32 const _c1{ { { 30.0/255.0, 1.0, 1.0, 1.f } } };
	} // end ns
	STATIC_INLINE_PURE XMVECTOR const __vectorcall skinny(float const t) {

		return(SFM::XMColorHSVToSRGB(SFM::lerp(skinny_Konstants::_c0, skinny_Konstants::_c1, t)));
	}
	
	// Advanced matrix disection and other good matrix helpers:
	//

	// #### View Matrix Helper Functions #### //
	STATIC_INLINE_PURE XMVECTOR const __vectorcall getRightVector( FXMMATRIX const mat )
	{
		/*
		[	1	0	0	Sx	]	X-Axis basis vector (row-major), not including last component Sx
			0	1	0	Sy
			0	0	1	Sz
			Tx	Ty	Tz	1
		*/

		return(XMVectorSelect( g_XMZero, mat.r[0], g_XMSelect1110 ));
	}
	STATIC_INLINE_PURE XMVECTOR const __vectorcall getUpVector( FXMMATRIX const mat )
	{
		/*
			1	0	0	Sx
		[	0	1	0	Sy	]	Y-Axis basis vector (row-major), not including last component Sy
			0	0	1	Sz
			Tx	Ty	Tz	1
		*/

		return(XMVectorSelect( g_XMZero, mat.r[1], g_XMSelect1110 ));
	}
	STATIC_INLINE_PURE XMVECTOR const __vectorcall getForwardVector( FXMMATRIX const mat )
	{
		/*
			1	0	0	Sx
			0	1	0	Sy
		[	0	0	1	Sz	]	Z-Axis basis vector (row-major), not including last component Sz
			Tx	Ty	Tz	1
		*/

		return(XMVectorSelect( g_XMZero, mat.r[2], g_XMSelect1110 ));
	}
	STATIC_INLINE_PURE XMVECTOR const __vectorcall getPositionVector( FXMMATRIX const mat )
	{
		/*
			1	0	0	Sx
			0	1	0	Sy
			0	0	1	Sz
		[	Tx	Ty	Tz	1	] Position(Translation) basis vector (row-major), not including last component 1
		*/

		return(XMVectorSelect( g_XMZero, mat.r[3], g_XMSelect1110 ));
	}

	STATIC_INLINE_PURE XMVECTOR const __vectorcall WorldToScreen(FXMVECTOR const worldPt, FXMVECTOR const frameBufferSize, FXMMATRIX const viewProj)
	{
		XMVECTOR const screenPt(XMVector3TransformCoord(worldPt, viewProj));
		XMVECTOR const halfFrameBufferSize(XMVectorScale(frameBufferSize, 0.5f));
		
		return(SFM::__fma(screenPt, halfFrameBufferSize, halfFrameBufferSize));
	}
	/* this is strange should work but doesn't check w/stackoverflow.com!
	// fastest way to replace a column in XMMATRIX (which is row major)
	#define REPLACE_COLUMN(m, v, s) \
		m.r[0] = XMVectorSelect( m.r[0], XMVectorSplatX( v ), s ); \
		m.r[1] = XMVectorSelect( m.r[1], XMVectorSplatY( v ), s ); \
		m.r[2] = XMVectorSelect( m.r[2], XMVectorSplatZ( v ), s ); \
		m.r[3] = XMVectorSelect( m.r[3], XMVectorSplatW( v ), s ); \

	template <uint32_t const columnIndex>
	STATIC_INLINE_PURE XMMATRIX const __vectorcall XMReplaceColumn( FXMMATRIX xmMatIn, XMVECTOR const xmNew )
	{
		XMMATRIX xmMat;
		if constexpr ( 0 == columnIndex )
		{
			inline XMVECTORF32 const xmSelectColumn{ XM_SELECT_1, XM_SELECT_0, XM_SELECT_0, XM_SELECT_0 };
			REPLACE_COLUMN(xmMat, xmNew, xmSelectColumn);
		}
		else if constexpr (1 == columnIndex)
		{
			inline XMVECTORF32 const xmSelectColumn{ XM_SELECT_0, XM_SELECT_1, XM_SELECT_0, XM_SELECT_0 };
			REPLACE_COLUMN( xmMat, xmNew, xmSelectColumn );
		}
		else if constexpr (2 == columnIndex)
		{
			inline XMVECTORF32 const xmSelectColumn{ XM_SELECT_1, XM_SELECT_1, XM_SELECT_0, XM_SELECT_1 };
			REPLACE_COLUMN( xmMat, xmNew, xmSelectColumn );
		}
		else if constexpr (3 == columnIndex)
		{
			inline XMVECTORF32 const xmSelectColumn{ XM_SELECT_0, XM_SELECT_0, XM_SELECT_0, XM_SELECT_1 };
			REPLACE_COLUMN( xmMat, xmNew, xmSelectColumn );
		}

		return(xmMat);
	}
	*/

	template <uint32_t const columnIndex>
	STATIC_INLINE_PURE XMMATRIX const __vectorcall XMReplaceColumn( FXMMATRIX xmMat, XMVECTOR const xmNew )
	{
		XMFLOAT4X4A matMod;
		XMFLOAT4A vNew; 

		XMStoreFloat4x4A(&matMod, xmMat);
		XMStoreFloat4A(&vNew, xmNew);

		matMod.m[0][columnIndex] = vNew.x;
		matMod.m[1][columnIndex] = vNew.y;
		matMod.m[2][columnIndex] = vNew.z;
		matMod.m[3][columnIndex] = vNew.w;

		return( XMLoadFloat4x4A(&matMod) );
	}
	// Given position/normal of the plane, calculates plane in camera space.
	STATIC_INLINE_PURE XMVECTOR const __vectorcall CameraSpaceReflectionPlane( FXMMATRIX const matViewReflect, CXMVECTOR __restrict pos, CXMVECTOR __restrict normal, float const sideSign, float const clipPlaneOffset )
	{
		XMVECTOR const offsetPos = XMVectorAdd( pos, XMVectorScale(normal, clipPlaneOffset ));
		XMVECTOR const cpos = XMVector3TransformCoord( offsetPos, matViewReflect );
		XMVECTOR const cnormal = XMVectorScale( XMVector3Normalize( XMVector3TransformNormal( normal, matViewReflect ) ), sideSign );

		return( XMVectorSelect( XMVectorNegate( XMVector3Dot( cpos, cnormal ) ), cnormal, g_XMSelect1110 ) );
	}

	// taken from http://www.terathon.com/code/oblique.html, modified for DirectX coord system, and row-major XMMATRIX
	STATIC_INLINE_PURE XMMATRIX const __vectorcall MakeProjectionMatrixOblique( XMMATRIX xmProj, CXMVECTOR __restrict clipPlane )
	{
		// Calculate the clip-space corner point opposite the clipping plane
		// as (sgn(clipPlane.x), sgn(clipPlane.y), 1, 1) and
		// transform it into camera space by multiplying it
		// by the inverse of the projection matrix
		XMVECTOR xmQ = XMVectorSet( (sgn( XMVectorGetX( clipPlane ) ) ),
									(sgn( XMVectorGetY( clipPlane ) ) ),
									1.0f,
									1.0f
								  );
		
		xmQ = XMVector4Transform(xmQ, XMMatrixInverse( nullptr, xmProj ) );

		// Calculate the scaled plane vector
		// // Vector4 c = clipPlane * (1.0f / Vector3.Dot( clipPlane, q )); // // 
		
		xmQ = XMVectorMultiply( clipPlane, XMVectorReciprocal( XMVector4Dot( clipPlane, xmQ ) ) );

		// Replace the third row of the projection matrix
		// ## that was in reference to a column major matrix
		// XMMATRIX is natively Row-major!!!
		// supposed to be indices	2,		6,		10,		14
		// which maps to			m13,	m23,	m33,	m43
		xmProj = XMReplaceColumn<2>(xmProj, xmQ);


		/* keep for checking
		XMFLOAT4X4A matProj;
		XMStoreFloat4x4A(&matProj, xmProj);
		
		XMFLOAT4A vColumn;
		XMStoreFloat4A(&vColumn, xmQ);

		if ( vColumn.x != matProj._13
			|| vColumn.y != matProj._23
			|| vColumn.z != matProj._33
			|| vColumn.w != matProj._43 )
		{
			throw("ERROR Matrix does not match vector column");
		}
		*/

		// out
		return(xmProj);
	}

	// Isometric conversion:
	//
	STATIC_INLINE_PURE XMVECTOR const __vectorcall v2_CartToIso(FXMVECTOR const pt2DSpace)
	{
		XMFLOAT2A v;
		XMStoreFloat2A(&v, pt2DSpace);

		return(XMVectorSet(v.x - v.y, (v.x + v.y) * 0.5f, 0.0f, 0.0f));
	}

	STATIC_INLINE_PURE XMVECTOR const __vectorcall v2_IsoToCart(FXMVECTOR const ptIsoSpace)
	{
		XMFLOAT2A v;
		XMStoreFloat2A(&v, ptIsoSpace);

		return(XMVectorScale(XMVectorSet( __fms(v.y , 2.0f, v.x), __fma(v.y, 2.0f, v.x), 0.0f, 0.0f), 0.5f));
	}

} // end namespace SFM