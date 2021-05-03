#pragma once
#pragma message("WARNING: XMM128.h is for legacy usage only, not for new development - deprecated")

#include <intrin.h>
#include <emmintrin.h>
#include <smmintrin.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h> // DirectX::PackedVector XMCOLOR
#include <DirectXColors.h>

namespace XMConstants
{
	using namespace DirectX;

	XMGLOBALCONST XMVECTORF32 Inv255 = { { { 1.0f / 255.0f, 1.0f / 255.0f, 1.0f / 255.0f, 1.0f / 255.0f } } };
};

namespace XMM128
{
	using namespace DirectX;
	extern bool const m_bSSE41;

	static __inline __m128d XM_CALLCONV FMA_DoublePrecision(__m128d const a, __m128d const b, __m128d const c)
	{
		return(_mm_fmadd_sd(a, b, c));
	}
	static __inline __m128i XM_CALLCONV Clamp_m128i(__m128i const a, __m128i const min, __m128i const max)
	{
		//if (m_bSSE41)
			return(_mm_min_epi32(_mm_max_epi32(a, min), max));
		//else
		//	return(_mm_min_epi16(_mm_max_epi16(a, min), max));
	}
	static __inline __m128i XM_CALLCONV MaxOneMinIsZero_m128i(__m128i const maxVal)
	{
		//if (m_bSSE41)
			return(_mm_max_epi32(_mm_setzero_si128(), maxVal));
		//else
		//	return(_mm_max_epi16(_mm_setzero_si128(), maxVal));
	}
	static __inline __m128i XM_CALLCONV Mul_m128i(__m128i const a, __m128i const b)
	{
		//if (m_bSSE41)
			return(_mm_mullo_epi32(a,b));
		//else
		//	return(_mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(_mm_mul_epu32(a, b)), _mm_castsi128_ps(_mm_mul_epu32(_mm_srli_si128(a, 4), _mm_srli_si128(b, 4))), _MM_SHUFFLE(2, 0, 2, 0))));
	}
	static __inline XMVECTOR XM_CALLCONV clamp(FXMVECTOR a, FXMVECTOR min, FXMVECTOR max)
	{
		return( _mm_min_ps(_mm_max_ps(a, min), max) );
	}
	
	static __inline __m128 XM_CALLCONV LoadOne(float const fVal)
	{ 
		return( _mm_set_ss(fVal) );
	}
	static __inline __m128 XM_CALLCONV LoadOne(float const* const __restrict pfVal)
	{ 
		return( _mm_load_ss(pfVal) );
	}
	static __inline __m128 XM_CALLCONV LoadAll(float const fVal)
	{ 
		return(_mm_set_ps1(fVal) );
	}
	static __inline __m128 XM_CALLCONV LoadAll(float const* const __restrict pfVal)
	{ 
		return( _mm_load1_ps(pfVal) );
	}
	static __inline __m128 XM_CALLCONV Load(float const* const __restrict pfVal)
	{ 
		return( _mm_load_ps(pfVal) );
	}
	static __inline __m128 XM_CALLCONV Load(float const fVal0, float const fVal1, float const fVal2 = 0.0f, float const fVal3 = 0.0f)
	{
		return(_mm_set_ps(fVal0, fVal1, fVal2, fVal3));
	}

	static __inline void XM_CALLCONV Store(float* const __restrict pfVal, __m128 rRegister)
	{ 
		_mm_store_ps(pfVal, rRegister);
	}
	static __inline void XM_CALLCONV StoreOne( float* const __restrict pfVal, __m128 rRegister ) 
	{ 
		_mm_store_ss(pfVal, rRegister);
	}
	

	static __inline __m128i XM_CALLCONV Load( int_fast32_t const iVal ) 
	{ 
		return(_mm_set1_epi32(iVal));
	}
	static __inline __m128i XM_CALLCONV Load(uint_fast32_t const iVal)
	{ 
		return(_mm_set1_epi32(iVal));
	}
	static __inline __m128i XM_CALLCONV Load( long long const iVal ) 
	{ 
		__declspec(align(16)) long long const xi32[4] = { iVal, iVal, iVal, iVal };
		return( _mm_load_si128( (__m128i*)xi32 ) ); 
	}

	static __inline __m128i XM_CALLCONV Load(int_fast32_t const riVal0, int_fast32_t const riVal1, int_fast32_t const riVal2 = 0, int_fast32_t const riVal3 = 0)
	{
		return(_mm_setr_epi32(riVal0, riVal1, riVal2, riVal3));
	}
	static __inline __m128 XM_CALLCONV LoadConv(int_fast32_t const riVal0, int_fast32_t const riVal1, int_fast32_t const riVal2 = 0, int_fast32_t const riVal3 = 0)
	{
		return(_mm_cvtepi32_ps(_mm_setr_epi32(riVal0, riVal1, riVal2, riVal3)));
	}
	static __inline __m128i XM_CALLCONV Load(uint_fast32_t const riVal0, uint_fast32_t const riVal1, uint_fast32_t const riVal2 = 0, uint_fast32_t const riVal3 = 0)
	{
		return(_mm_setr_epi32(riVal0, riVal1, riVal2, riVal3));
	}
	static __inline __m128 XM_CALLCONV LoadConv(uint_fast32_t const riVal0, uint_fast32_t const riVal1, uint_fast32_t const riVal2 = 0, uint_fast32_t const riVal3 = 0)
	{
		return(_mm_cvtepi32_ps(_mm_setr_epi32(riVal0, riVal1, riVal2, riVal3)));
	}
	static __inline __m128i XM_CALLCONV Load( unsigned char const riVal0, unsigned char const riVal1, unsigned char const riVal2 = 0x0, unsigned char const riVal3 = 0x0 )
	{
		return(_mm_setr_epi32(riVal0, riVal1, riVal2, riVal3));
	}
	static __inline __m128 XM_CALLCONV LoadConv(unsigned char const riVal0, unsigned char const riVal1, unsigned char const riVal2 = 0x0, unsigned char const riVal3 = 0x0)
	{
		return(_mm_cvtepi32_ps(_mm_setr_epi32(riVal0, riVal1, riVal2, riVal3)));
	}

	static __inline XMVECTOR XM_CALLCONV XMVector2Dot(FXMVECTOR xmV1, FXMVECTOR xmV2)
	{
		//if (m_bSSE41)
			return(_mm_dp_ps(xmV1, xmV2, 0x3f));
		//else
		//	return(DirectX::XMVector2Dot(xmV1, xmV2));
	}
	static __inline XMVECTOR XM_CALLCONV XMVector3Dot(FXMVECTOR xmV1, FXMVECTOR xmV2)
	{
		//if (m_bSSE41)
			return(_mm_dp_ps(xmV1, xmV2, 0x7f));
		//else
		//	return(DirectX::XMVector3Dot(xmV1, xmV2));
	}
	static __inline XMVECTOR XM_CALLCONV XMVector4Dot(FXMVECTOR xmV1, FXMVECTOR xmV2)
	{
		//if (m_bSSE41)
			return(_mm_dp_ps(xmV1, xmV2, 0xff));
		//else
		//	return(DirectX::XMVector4Dot(xmV1, xmV2));
	}
	static __inline XMVECTOR XM_CALLCONV XMVectorRound(FXMVECTOR xmV)
	{
		//if (m_bSSE41)
			return(_mm_round_ps(xmV, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC));
		//else
		//	return(DirectX::XMVectorRound(xmV));
	}
	static __inline XMVECTOR XM_CALLCONV XMVectorTruncate(FXMVECTOR xmV)
	{
		//if (m_bSSE41)
			return(_mm_round_ps(xmV, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC));
		//else
		//	return(DirectX::XMVectorTruncate(xmV));
	}
	static __inline XMVECTOR XM_CALLCONV XMVectorFloor(FXMVECTOR xmV)
	{
		//if (m_bSSE41)
			return(_mm_floor_ps(xmV));
		//else
		//	return(DirectX::XMVectorFloor(xmV));
	}
	static __inline XMVECTOR XM_CALLCONV XMVectorCeiling(FXMVECTOR xmV)
	{
		//if (m_bSSE41)
			return(_mm_ceil_ps(xmV));
		//else
		//	return(DirectX::XMVectorCeiling(xmV));
	}

	static __inline XMVECTOR XM_CALLCONV XMVector3LengthSq(FXMVECTOR V)
	{
		//if (m_bSSE41)
			return(XMVector3Dot(V, V));
		//else
		//	return(DirectX::XMVector3LengthSq(V));
	}
	static __inline XMVECTOR XM_CALLCONV XMVector3LengthEst(FXMVECTOR V)
	{
		//if (m_bSSE41)
		//{
			XMVECTOR vTemp = _mm_dp_ps(V, V, 0x7f);
			return(_mm_sqrt_ps(vTemp));
		//}
		//else
		//	return(DirectX::XMVector3LengthEst(V));
	}
	static __inline XMVECTOR XM_CALLCONV XMVector3Length(FXMVECTOR V)
	{
		//if (m_bSSE41)
		//{
			XMVECTOR vTemp = _mm_dp_ps(V, V, 0x7f);
			return(_mm_sqrt_ps(vTemp));
		//}
		//else
		//	return(DirectX::XMVector3Length(V));
	}
	static __inline XMVECTOR XM_CALLCONV XMVector3Normalize(FXMVECTOR V)
	{
		//if (m_bSSE41)
		//{
			XMVECTOR vLengthSq = _mm_dp_ps(V, V, 0x7f);
			// Prepare for the division
			XMVECTOR vResult = _mm_sqrt_ps(vLengthSq);
			// Create zero with a single instruction
			XMVECTOR vZeroMask = _mm_setzero_ps();
			// Test for a divide by zero (Must be FP to detect -0.0)
			vZeroMask = _mm_cmpneq_ps(vZeroMask, vResult);
			// Failsafe on zero (Or epsilon) length planes
			// If the length is infinity, set the elements to zero
			vLengthSq = _mm_cmpneq_ps(vLengthSq, g_XMInfinity);
			// Divide to perform the normalization
			vResult = _mm_div_ps(V, vResult);
			// Any that are infinity, set to zero
			vResult = _mm_and_ps(vResult, vZeroMask);
			// Select qnan or result based on infinite length
			XMVECTOR vTemp1 = _mm_andnot_ps(vLengthSq, g_XMQNaN);
			XMVECTOR vTemp2 = _mm_and_ps(vResult, vLengthSq);
			vResult = _mm_or_ps(vTemp1, vTemp2);
			return(vResult);
		//}
		//else
		//	return(DirectX::XMVector3Normalize(V));
	}

	static __inline XMVECTOR XM_CALLCONV XMVector4LengthEst(FXMVECTOR V)
	{
		//if (m_bSSE41)
		//{
			XMVECTOR vTemp = _mm_dp_ps(V, V, 0xff);
			return(_mm_sqrt_ps(vTemp));
		//}
		//else
		//	return(DirectX::XMVector4LengthEst(V));
	}

	static __inline XMVECTOR XM_CALLCONV XMVector4Length(FXMVECTOR V)
	{
		//if (m_bSSE41)
		//{
			XMVECTOR vTemp = _mm_dp_ps(V, V, 0xff);
			return(_mm_sqrt_ps(vTemp));
		//}
		//else
		//	return(DirectX::XMVector4Length(V));
	}

	static __inline XMVECTOR XM_CALLCONV XMVector4Normalize(FXMVECTOR V)
	{
		//if (m_bSSE41)
		//{
			XMVECTOR vLengthSq = _mm_dp_ps(V, V, 0xff);
			// Prepare for the division
			XMVECTOR vResult = _mm_sqrt_ps(vLengthSq);
			// Create zero with a single instruction
			XMVECTOR vZeroMask = _mm_setzero_ps();
			// Test for a divide by zero (Must be FP to detect -0.0)
			vZeroMask = _mm_cmpneq_ps(vZeroMask, vResult);
			// Failsafe on zero (Or epsilon) length planes
			// If the length is infinity, set the elements to zero
			vLengthSq = _mm_cmpneq_ps(vLengthSq, g_XMInfinity);
			// Divide to perform the normalization
			vResult = _mm_div_ps(V, vResult);
			// Any that are infinity, set to zero
			vResult = _mm_and_ps(vResult, vZeroMask);
			// Select qnan or result based on infinite length
			XMVECTOR vTemp1 = _mm_andnot_ps(vLengthSq, g_XMQNaN);
			XMVECTOR vTemp2 = _mm_and_ps(vResult, vLengthSq);
			vResult = _mm_or_ps(vTemp1, vTemp2);
			return(vResult);
		//}
		//else
		//	return(DirectX::XMVector4Normalize(V));
	}

	static __inline XMVECTOR XM_CALLCONV XMPlaneNormalize(FXMVECTOR P)
	{
		//if (m_bSSE41)
		//{
			XMVECTOR vLengthSq = _mm_dp_ps(P, P, 0x7f);
			// Prepare for the division
			XMVECTOR vResult = _mm_sqrt_ps(vLengthSq);
			// Failsafe on zero (Or epsilon) length planes
			// If the length is infinity, set the elements to zero
			vLengthSq = _mm_cmpneq_ps(vLengthSq, g_XMInfinity);
			// Reciprocal mul to perform the normalization
			vResult = _mm_div_ps(P, vResult);
			// Any that are infinity, set to zero
			vResult = _mm_and_ps(vResult, vLengthSq);
			return(vResult);
		//}
		//else
		//	return(DirectX::XMPlaneNormalize(P));
	}
	//
	// SSECoordinateTransform
	//
	// This function transforms the vector, pV (x, y, z, 1), by the matrix, pM,
	// projecting the result back into w=1
	//
	static __inline void XM_CALLCONV SSEVec3TransformCoord(XMFLOAT3A * __restrict ResultVector, const XMFLOAT3A * __restrict InputVector, const XMFLOAT4X4A * __restrict TransformMatrix)
	{
		__m128 xmm0, xmm1, xmm2, xmm4, xmm5, xmm6, xmm7;

		xmm0 = LoadAll(&InputVector->x);
		xmm1 = LoadAll(&InputVector->y);
		xmm2 = LoadAll(&InputVector->z);

		xmm4 = _mm_load_ps((float *)TransformMatrix);
		xmm5 = _mm_load_ps((float *)TransformMatrix + 4);
		xmm6 = _mm_load_ps((float *)TransformMatrix + 8);
		xmm7 = _mm_load_ps((float *)TransformMatrix + 12);

		xmm0 = _mm_mul_ps(xmm4, xmm0);
		xmm1 = _mm_mul_ps(xmm5, xmm1);
		xmm2 = _mm_mul_ps(xmm6, xmm2);

		xmm1 = _mm_add_ps(xmm0, xmm1);
		xmm1 = _mm_add_ps(xmm1, xmm2);
		xmm1 = _mm_add_ps(xmm1, xmm7);

		xmm2 = _mm_unpackhi_ps(xmm1, xmm1);
		xmm2 = _mm_movehl_ps(xmm2, xmm2);

		xmm0 = _mm_div_ps(xmm1, xmm2);

		XMStoreFloat3A(ResultVector, xmm0);
	}
	static __inline void XM_CALLCONV SSEVec3TransformCoordArray(XMFLOAT3A * __restrict ResultVector, const XMFLOAT3A * __restrict InputVector, const XMFLOAT4X4A * __restrict TransformMatrix, size_t const& uiElements )
	{
		__m128 xmm0, xmm1, xmm2, xmm4, xmm5, xmm6, xmm7;

		xmm4 = _mm_load_ps((float *)TransformMatrix);
		xmm5 = _mm_load_ps((float *)TransformMatrix + 4);
		xmm6 = _mm_load_ps((float *)TransformMatrix + 8);
		xmm7 = _mm_load_ps((float *)TransformMatrix + 12);

		for ( size_t iDx = 0 ; iDx < uiElements ; ++iDx )
		{
			xmm0 = LoadAll(&InputVector[iDx].x);
			xmm1 = LoadAll(&InputVector[iDx].y);
			xmm2 = LoadAll(&InputVector[iDx].z);

			xmm0 = _mm_mul_ps(xmm4, xmm0);
			xmm1 = _mm_mul_ps(xmm5, xmm1);
			xmm2 = _mm_mul_ps(xmm6, xmm2);

			xmm1 = _mm_add_ps(xmm0, xmm1);
			xmm1 = _mm_add_ps(xmm1, xmm2);
			xmm1 = _mm_add_ps(xmm1, xmm7);

			xmm2 = _mm_unpackhi_ps(xmm1, xmm1);
			xmm2 = _mm_movehl_ps(xmm2, xmm2);

			xmm0 = _mm_div_ps(xmm1, xmm2);

			XMStoreFloat3A(&ResultVector[iDx], xmm0);
		}		
	}
}

namespace XMM256
{
#include <immintrin.h>
	extern bool const m_bAVX2;

	static __inline __m256d XM_CALLCONV FMA3_DoublePrecision(__m256d const a, __m256d const b, __m256d const c)
	{
		return(_mm256_fmadd_pd(a, b, c));
	}
	static __inline __m256i XM_CALLCONV Clamp_m256i(__m256i const a, __m256i const min, __m256i const max)
	{
		return(_mm256_min_epi32(_mm256_max_epi32(a, min), max));
	}
	static __inline __m256i XM_CALLCONV MaxOneMinIsZero_m256i(__m256i const maxVal)
	{
		return(_mm256_max_epi32(_mm256_setzero_si256(), maxVal));
	}
	static __inline __m256i XM_CALLCONV Mul_m256i(__m256i const a, __m256i const b)
	{
		return(_mm256_mullo_epi32(a, b));
	}
	static __inline __m256d XM_CALLCONV Load_m256d_DoublePrecision(unsigned char const riVal0, unsigned char const riVal1, unsigned char const riVal2 = 0x0, unsigned char const riVal3 = 0x0)
	{
		return(_mm256_cvtepi32_pd(XMM128::Load(riVal0, riVal1, riVal2, riVal3)));
	}

	static __inline __m256 XM_CALLCONV LoadScalar(float const fVal)
	{
		return(_mm256_set1_ps(fVal));
	}
	static __inline __m256d XM_CALLCONV LoadScalar(double const fVal)
	{
		return(_mm256_set1_pd(fVal));
	}
	static __inline __m256 XM_CALLCONV Load(float const* const __restrict pfVal)
	{
		return(_mm256_load_ps(pfVal));
	}
	static __inline __m256d XM_CALLCONV Load(double const* const __restrict pfVal)
	{
		return(_mm256_load_pd(pfVal));
	}
	static __inline void XM_CALLCONV Store(float* const __restrict pfVal, __m256 rRegister)
	{
		_mm256_store_ps(pfVal, rRegister);
	}
	static __inline void XM_CALLCONV Store(double* const __restrict pfVal, __m256d rRegister)
	{
		_mm256_store_pd(pfVal, rRegister);
	}
	static __inline __m256 XM_CALLCONV Ceil(float const a)
	{
		return(_mm256_ceil_ps(_mm256_set1_ps(a)));
	}
	static __inline __m256d XM_CALLCONV Ceil(double const a)
	{
		return(_mm256_ceil_pd(_mm256_set1_pd(a)));
	}
	static __inline __m256 XM_CALLCONV Ceil(__m256 xmV)
	{
		return(_mm256_ceil_ps(xmV));
	}
	static __inline __m256d XM_CALLCONV Ceil(__m256d xmV)
	{
		return(_mm256_ceil_pd(xmV));
	}
	static __inline __m256 XM_CALLCONV Floor(float const a)
	{
		return(_mm256_floor_ps(_mm256_set1_ps(a)));
	}
	static __inline __m256d XM_CALLCONV Floor(double const a)
	{
		return(_mm256_floor_pd(_mm256_set1_pd(a)));
	}
	static __inline __m256 XM_CALLCONV Floor(__m256 xmV)
	{
		return(_mm256_floor_ps(xmV));
	}
	static __inline __m256d XM_CALLCONV Floor(__m256d xmV)
	{
		return(_mm256_floor_pd(xmV));
	}
	static __inline __m256 XM_CALLCONV Round(float const a)
	{
		return(_mm256_round_ps(_mm256_set1_ps(a), _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC));
	}
	static __inline __m256d XM_CALLCONV Round(double const a)
	{
		return(_mm256_round_pd(_mm256_set1_pd(a), _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC));
	}
	static __inline __m256 XM_CALLCONV Round(__m256 xmV)
	{
		return(_mm256_round_ps(xmV, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC));
	}
	static __inline __m256d XM_CALLCONV Round(__m256d xmV)
	{
		return(_mm256_round_pd(xmV, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC));
	}
	static __inline int const XM_CALLCONV clamp(int const a, int const min, int const max)
	{
		__m256i xmClamped = Clamp_m256i(_mm256_set1_epi32(a), _mm256_set1_epi32(min), _mm256_set1_epi32(max));
		
		return(xmClamped.m256i_i32[0]);
	}

	static __inline double const XM_CALLCONV clamp(double const a, double const min, double const max)
	{
		__declspec(align(16)) double dReturn;

		__m256d xmClamped = _mm256_min_pd(_mm256_max_pd(_mm256_set1_pd(a), _mm256_set1_pd(min)), _mm256_set1_pd(max));

		_mm_store_sd(&dReturn, _mm256_castpd256_pd128(xmClamped));

		return(dReturn);
	}

	static __inline float const XM_CALLCONV clampf(float const a, float const min, float const max)
	{
		__declspec(align(16)) float fReturn;

		__m256 xmClamped = _mm256_min_ps(_mm256_max_ps(_mm256_set1_ps(a), _mm256_set1_ps(min)), _mm256_set1_ps(max));

		_mm_store_ss(&fReturn, _mm256_castps256_ps128(xmClamped));

		return(fReturn);
	}
	static __inline uint16_t const XM_CALLCONV saturate_to_u16(__m256 xmS)
	{
		__m256i xmClamped = Clamp_m256i(_mm256_cvtps_epi32(xmS), _mm256_setzero_si256(), _mm256_set1_epi32(UINT16_MAX));

		return(xmClamped.m256i_u16[0]);
	}
	static __inline uint16_t const XM_CALLCONV saturate_to_u16(__m256d xmDouble)
	{
		__m256i xmClamped = Clamp_m256i(_mm256_castsi128_si256(_mm256_cvtpd_epi32(xmDouble)), _mm256_setzero_si256(), _mm256_set1_epi32(UINT16_MAX));

		return(xmClamped.m256i_u16[0]);
	}
	static __inline __m256i const XM_CALLCONV saturate_to_u16(__m256i& xmClamped, __m256d xmDouble)
	{
		xmClamped = Clamp_m256i(_mm256_castsi128_si256(_mm256_cvtpd_epi32(xmDouble)), _mm256_setzero_si256(), _mm256_set1_epi32(UINT16_MAX));

		return(xmClamped);
	}
	static __inline __m256i const XM_CALLCONV saturate_to_u8(__m256i& xmClamped, __m256d xmDouble)
	{
		xmClamped = Clamp_m256i(_mm256_castsi128_si256(_mm256_cvtpd_epi32(xmDouble)), _mm256_setzero_si256(), _mm256_set1_epi32(UINT8_MAX));

		return(xmClamped);
	}
	static __inline uint8_t const XM_CALLCONV saturate_to_u8(__m256 xmS)
	{
		__m256i xmClamped = Clamp_m256i((_mm256_cvtps_epi32(xmS)), _mm256_setzero_si256(), _mm256_set1_epi32(UINT8_MAX));

		return(xmClamped.m256i_u8[0]);
	}
	static __inline uint32_t const XM_CALLCONV saturate_to(__m256 xmS, uint32_t const maximumValue)
	{
		__m256i xmClamped = Clamp_m256i((_mm256_cvtps_epi32(xmS)), _mm256_setzero_si256(), _mm256_set1_epi32(maximumValue));

		return(xmClamped.m256i_u32[0]);
	}
	static __inline void XM_CALLCONV to_i32(__m256d const xmDouble, int32_t (&values)[4])
	{
		#pragma loop( ivdep )
		for ( uint32_t i = 0 ; i < 4 ; ++i )
			values[i] = _mm256_cvtpd_epi32(xmDouble).m128i_i32[i];
	}
	static __inline void XM_CALLCONV to_i32(__m256 const xmS, int32_t(&values)[8])
	{
		#pragma loop( ivdep )
		for (uint32_t i = 0; i < 4; ++i)
			values[i] = _mm256_cvtps_epi32(xmS).m256i_i32[i];
	}
	static __inline uint8_t const XM_CALLCONV saturate_to_u8(__m256d xmDouble)
	{
		__m256i xmClamped = Clamp_m256i(_mm256_castsi128_si256(_mm256_cvtpd_epi32(xmDouble)), _mm256_setzero_si256(), _mm256_set1_epi32(UINT8_MAX));

		return(xmClamped.m256i_u8[0]);
	}
	static __inline uint8_t const XM_CALLCONV saturate_to_u8(int const a)
	{
		__m256i xmClamped = Clamp_m256i(_mm256_set1_epi32(a), _mm256_setzero_si256(), _mm256_set1_epi32(UINT8_MAX));

		return(xmClamped.m256i_u8[0]);
	}

	static __inline double const XM_CALLCONV saturate(double const a)
	{
		__declspec(align(16)) double dReturn;

		_mm_store_sd(&dReturn, _mm256_castpd256_pd128(_mm256_min_pd(_mm256_max_pd(_mm256_set1_pd(a), _mm256_setzero_pd()), _mm256_set1_pd(1.0))));

		return(dReturn);
	}

	static __inline __m256 const XM_CALLCONV saturatef(__m256 const a)
	{
		return(_mm256_min_ps(_mm256_max_ps(a, _mm256_setzero_ps()), _mm256_set1_ps(1.0f)) );
	}

	static __inline float const XM_CALLCONV saturatef(float const a)
	{
		__declspec(align(16)) float fReturn;

		_mm_store_ss(&fReturn, _mm256_castps256_ps128(_mm256_min_ps(_mm256_max_ps(_mm256_set1_ps(a), _mm256_setzero_ps()), _mm256_set1_ps(1.0f)) ));

		return(fReturn);
	}
}

