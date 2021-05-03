#pragma once

/*******************************************************************
*   RGB2Y.h
*   RGB2Y
*
*	Author: Kareem Omar
*	kareem.omar@uah.edu
*	https://github.com/komrad36
*
*	Last updated Dec 9, 2016
*******************************************************************/
//
// Fastest CPU (AVX/SSE) implementation of RGB to grayscale.
// Roughly 3x faster than OpenCV's implementation with AVX2, or 2x faster
// than OpenCV's implementation if using SSE only.
//
// Converts an RGB color image to grayscale.
//
// You can use equal weighting by calling the templated
// function with weight set to 'false', or you
// can specify custom weights in RGB2Y.h (only slightly slower).
//
// The default weights match OpenCV's default.
//
// For even more speed see the CUDA version:
// github.com/komrad36/CUDARGB2Y
//
// If you do not have AVX2, uncomment the #define below to route the code
// through only SSE isntructions. NOTE THAT THIS IS ABOUT 50% SLOWER.
// A processor with full AVX2 support is highly recommended.
//
// All functionality is contained in RGB2Y.h.
// 'main.cpp' is a demo and test harness.
//

#pragma once

#include <cstdint>
#include <immintrin.h>

namespace Fast2Gray
{
	static inline struct sDynamicWeighting
	{
		static constexpr uint32_t const COMPONENTS = 3;

		// Set your weights here.
		static constexpr double B_WEIGHT = 0.114;
		static constexpr double G_WEIGHT = 0.587;
		static constexpr double R_WEIGHT = 0.299;

		__declspec(align(32)) double const vSRGB[4];
		__declspec(align(32)) double vWeightsBGR[4];

		sDynamicWeighting()
			: vSRGB{ B_WEIGHT, G_WEIGHT, R_WEIGHT, 0.0 },
			  vWeightsBGR{ 1.0, 1.0, 1.0, 0.0 }
		{}

	} oDynamicWeighting;

	static inline void __vectorcall setWeights(__m128 const xmWeights)
	{
		__m256d const xmNewWeights = _mm256_cvtps_pd(xmWeights);

		_mm256_store_pd(oDynamicWeighting.vWeightsBGR, xmNewWeights);
	}
	static inline void __vectorcall calculateNewWeightingSRGB(uint16_t (& __restrict vNewDynamicWeighting)[sDynamicWeighting::COMPONENTS])
	{
		// B //
		// G //
		// R //

		// ensure SRGB weights add up to 1 (normalized)
		__m128d xmB = _mm_set1_pd(oDynamicWeighting.vWeightsBGR[0] * sDynamicWeighting::B_WEIGHT);
		__m128d xmG = _mm_set1_pd(oDynamicWeighting.vWeightsBGR[1] * sDynamicWeighting::G_WEIGHT);
		__m128d xmR = _mm_set1_pd(oDynamicWeighting.vWeightsBGR[2] * sDynamicWeighting::R_WEIGHT);
		{
			__m128d const xmTotal = _mm_add_pd(_mm_add_pd(xmB, xmG), xmR);

			xmB = _mm_div_pd(xmB, xmTotal);
			xmG = _mm_div_pd(xmG, xmTotal);
			xmR = _mm_div_pd(xmR, xmTotal);
		}

		// RGB => BGR takes place here
		vNewDynamicWeighting[2] = static_cast<uint16_t>(_mm256_cvtsd_f64(_mm256_castpd128_pd256(_mm_fmadd_sd(_mm_set1_pd(64.0), xmB, _mm_set1_pd(0.5)))));
		vNewDynamicWeighting[1] = static_cast<uint16_t>(_mm256_cvtsd_f64(_mm256_castpd128_pd256(_mm_fmadd_sd(_mm_set1_pd(64.0), xmG, _mm_set1_pd(0.5)))));
		vNewDynamicWeighting[0] = static_cast<uint16_t>(_mm256_cvtsd_f64(_mm256_castpd128_pd256(_mm_fmadd_sd(_mm_set1_pd(64.0), xmR, _mm_set1_pd(0.5)))));
	}
	static inline void __vectorcall calculateNewWeightingLinear(uint16_t(&__restrict vNewDynamicWeighting)[sDynamicWeighting::COMPONENTS])
	{
		// B //
		// G //
		// R //

		// ensure Linear weights add up to 1 (normalized)
		__m128d xmB = _mm_set1_pd(oDynamicWeighting.vWeightsBGR[0]);
		__m128d xmG = _mm_set1_pd(oDynamicWeighting.vWeightsBGR[1]);
		__m128d xmR = _mm_set1_pd(oDynamicWeighting.vWeightsBGR[2]);
		{
			__m128d const xmTotal = _mm_add_pd(_mm_add_pd(xmB, xmG), xmR);

			xmB = _mm_div_pd(xmB, xmTotal);
			xmG = _mm_div_pd(xmG, xmTotal);
			xmR = _mm_div_pd(xmR, xmTotal);
		}
		// RGB => BGR takes place here
		vNewDynamicWeighting[2] = static_cast<uint16_t>(_mm256_cvtsd_f64(_mm256_castpd128_pd256(_mm_fmadd_sd(_mm_set1_pd(64.0), xmB, _mm_set1_pd(0.5)))));
		vNewDynamicWeighting[1] = static_cast<uint16_t>(_mm256_cvtsd_f64(_mm256_castpd128_pd256(_mm_fmadd_sd(_mm_set1_pd(64.0), xmG, _mm_set1_pd(0.5)))));
		vNewDynamicWeighting[0] = static_cast<uint16_t>(_mm256_cvtsd_f64(_mm256_castpd128_pd256(_mm_fmadd_sd(_mm_set1_pd(64.0), xmR, _mm_set1_pd(0.5)))));
	}

	// 241
	template<const bool last_row_and_col>
	static inline void __vectorcall processDynamicWeights(const uint8_t* __restrict const pt, const int32_t cols_minus_j, uint8_t* const __restrict out,
							   uint16_t const (&__restrict vNewDynamicWeighting)[sDynamicWeighting::COMPONENTS]) {

		// Internal; do NOT modify
		__declspec(align(16)) uint16_t const	B_WT(vNewDynamicWeighting[0]),		// B //
												G_WT(vNewDynamicWeighting[1]),		// G //
												R_WT(vNewDynamicWeighting[2]);		// R //

		__m128i h3;

		__m256i p1a = _mm256_mullo_epi16(_mm256_cvtepu8_epi16(_mm_loadu_si128(reinterpret_cast<const __m128i*>(pt))), _mm256_setr_epi16(B_WT, G_WT, R_WT, B_WT, G_WT, R_WT, B_WT, G_WT, R_WT, B_WT, G_WT, R_WT, B_WT, G_WT, R_WT, B_WT));
		__m256i p1b = _mm256_mullo_epi16(_mm256_cvtepu8_epi16(_mm_loadu_si128(reinterpret_cast<const __m128i*>(pt + 18))), _mm256_setr_epi16(B_WT, G_WT, R_WT, B_WT, G_WT, R_WT, B_WT, G_WT, R_WT, B_WT, G_WT, R_WT, B_WT, G_WT, R_WT, B_WT));
		__m256i p2a = _mm256_mullo_epi16(_mm256_cvtepu8_epi16(_mm_loadu_si128(reinterpret_cast<const __m128i*>(pt + 1))), _mm256_setr_epi16(G_WT, R_WT, B_WT, G_WT, R_WT, B_WT, G_WT, R_WT, B_WT, G_WT, R_WT, B_WT, G_WT, R_WT, B_WT, G_WT));
		__m256i p2b = _mm256_mullo_epi16(_mm256_cvtepu8_epi16(_mm_loadu_si128(reinterpret_cast<const __m128i*>(pt + 19))), _mm256_setr_epi16(G_WT, R_WT, B_WT, G_WT, R_WT, B_WT, G_WT, R_WT, B_WT, G_WT, R_WT, B_WT, G_WT, R_WT, B_WT, G_WT));
		__m256i p3a = _mm256_mullo_epi16(_mm256_cvtepu8_epi16(_mm_loadu_si128(reinterpret_cast<const __m128i*>(pt + 2))), _mm256_setr_epi16(R_WT, B_WT, G_WT, R_WT, B_WT, G_WT, R_WT, B_WT, G_WT, R_WT, B_WT, G_WT, R_WT, B_WT, G_WT, R_WT));
		__m256i p3b = _mm256_mullo_epi16(_mm256_cvtepu8_epi16(_mm_loadu_si128(reinterpret_cast<const __m128i*>(pt + 20))), _mm256_setr_epi16(R_WT, B_WT, G_WT, R_WT, B_WT, G_WT, R_WT, B_WT, G_WT, R_WT, B_WT, G_WT, R_WT, B_WT, G_WT, R_WT));
		__m256i suma = _mm256_adds_epu16(p3a, _mm256_adds_epu16(p1a, p2a));
		__m256i sumb = _mm256_adds_epu16(p3b, _mm256_adds_epu16(p1b, p2b));
		__m256i scla = _mm256_srli_epi16(suma, 6);
		__m256i sclb = _mm256_srli_epi16(sumb, 6);
		__m256i shfta = _mm256_shuffle_epi8(scla, _mm256_setr_epi8(0, 6, 12, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 18, 24, 30, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1));
		__m256i shftb = _mm256_shuffle_epi8(sclb, _mm256_setr_epi8(-1, -1, -1, -1, -1, -1, 0, 6, 12, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 18, 24, 30, -1, -1, -1, -1));
		__m256i accum = _mm256_or_si256(shfta, shftb);
		h3 = _mm_or_si128(_mm256_castsi256_si128(accum), _mm256_extracti128_si256(accum, 1));

		if (last_row_and_col) {
			switch (cols_minus_j) {
			case 12:
				out[11] = static_cast<uint8_t>(_mm_extract_epi8(h3, 11));
			case 11:
				out[10] = static_cast<uint8_t>(_mm_extract_epi8(h3, 10));
			case 10:
				out[9] = static_cast<uint8_t>(_mm_extract_epi8(h3, 9));
			case 9:
				out[8] = static_cast<uint8_t>(_mm_extract_epi8(h3, 8));
			case 8:
				out[7] = static_cast<uint8_t>(_mm_extract_epi8(h3, 7));
			case 7:
				out[6] = static_cast<uint8_t>(_mm_extract_epi8(h3, 6));
			case 6:
				out[5] = static_cast<uint8_t>(_mm_extract_epi8(h3, 5));
			case 5:
				out[4] = static_cast<uint8_t>(_mm_extract_epi8(h3, 4));
			case 4:
				out[3] = static_cast<uint8_t>(_mm_extract_epi8(h3, 3));
			case 3:
				out[2] = static_cast<uint8_t>(_mm_extract_epi8(h3, 2));
			case 2:
				out[1] = static_cast<uint8_t>(_mm_extract_epi8(h3, 1));
			case 1:
				out[0] = static_cast<uint8_t>(_mm_extract_epi8(h3, 0));
			}
		}
		else {
			_mm_storeu_si128(reinterpret_cast<__m128i*>(out), h3);
		}
	}

	template<const bool last_row_and_col>
	static inline void __vectorcall processStaticLinearWeights(const uint8_t* __restrict const pt, const int32_t cols_minus_j, uint8_t* const __restrict out) {
		__m128i h3;

		__m256i p1a = _mm256_cvtepu8_epi16(_mm_loadu_si128(reinterpret_cast<const __m128i*>(pt)));
		__m256i p1b = _mm256_cvtepu8_epi16(_mm_loadu_si128(reinterpret_cast<const __m128i*>(pt + 18)));
		__m256i p2a = _mm256_cvtepu8_epi16(_mm_loadu_si128(reinterpret_cast<const __m128i*>(pt + 1)));
		__m256i p2b = _mm256_cvtepu8_epi16(_mm_loadu_si128(reinterpret_cast<const __m128i*>(pt + 19)));
		__m256i p3a = _mm256_cvtepu8_epi16(_mm_loadu_si128(reinterpret_cast<const __m128i*>(pt + 2)));
		__m256i p3b = _mm256_cvtepu8_epi16(_mm_loadu_si128(reinterpret_cast<const __m128i*>(pt + 20)));
		__m256i suma = _mm256_adds_epu16(p3a, _mm256_adds_epu16(p1a, p2a));
		__m256i sumb = _mm256_adds_epu16(p3b, _mm256_adds_epu16(p1b, p2b));
		__m256i scla = _mm256_mulhi_epu16(suma, _mm256_set1_epi16(21846));
		__m256i sclb = _mm256_mulhi_epu16(sumb, _mm256_set1_epi16(21846));
		__m256i shfta = _mm256_shuffle_epi8(scla, _mm256_setr_epi8(0, 6, 12, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 18, 24, 30, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1));
		__m256i shftb = _mm256_shuffle_epi8(sclb, _mm256_setr_epi8(-1, -1, -1, -1, -1, -1, 0, 6, 12, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 18, 24, 30, -1, -1, -1, -1));
		__m256i accum = _mm256_or_si256(shfta, shftb);
		h3 = _mm_or_si128(_mm256_castsi256_si128(accum), _mm256_extracti128_si256(accum, 1));

		if (last_row_and_col) {
			switch (cols_minus_j) {
			case 12:
				out[11] = static_cast<uint8_t>(_mm_extract_epi8(h3, 11));
			case 11:
				out[10] = static_cast<uint8_t>(_mm_extract_epi8(h3, 10));
			case 10:
				out[9] = static_cast<uint8_t>(_mm_extract_epi8(h3, 9));
			case 9:
				out[8] = static_cast<uint8_t>(_mm_extract_epi8(h3, 8));
			case 8:
				out[7] = static_cast<uint8_t>(_mm_extract_epi8(h3, 7));
			case 7:
				out[6] = static_cast<uint8_t>(_mm_extract_epi8(h3, 6));
			case 6:
				out[5] = static_cast<uint8_t>(_mm_extract_epi8(h3, 5));
			case 5:
				out[4] = static_cast<uint8_t>(_mm_extract_epi8(h3, 4));
			case 4:
				out[3] = static_cast<uint8_t>(_mm_extract_epi8(h3, 3));
			case 3:
				out[2] = static_cast<uint8_t>(_mm_extract_epi8(h3, 2));
			case 2:
				out[1] = static_cast<uint8_t>(_mm_extract_epi8(h3, 1));
			case 1:
				out[0] = static_cast<uint8_t>(_mm_extract_epi8(h3, 0));
			}
		}
		else {
			_mm_storeu_si128(reinterpret_cast<__m128i*>(out), h3);
		}
	}

	template<bool last_row>
	static inline void __vectorcall processRow_Dynamic(const uint8_t* __restrict pt, const int32_t cols, uint8_t* const __restrict out,
						    uint16_t const(& __restrict vNewDynamicWeighting)[sDynamicWeighting::COMPONENTS]) {
		int j = 0;
		for (; j < cols - 12; j += 12, pt += 36) {
			processDynamicWeights<false>(pt, cols - j, out + j, vNewDynamicWeighting);
		}
		processDynamicWeights<last_row>(pt, cols - j, out + j, vNewDynamicWeighting);
	}
	template<bool last_row>
	static inline void __vectorcall processRow_StaticLinear(const uint8_t* __restrict pt, const int32_t cols, uint8_t* const __restrict out) {
		int j = 0;
		for (; j < cols - 12; j += 12, pt += 36) {
			processStaticLinearWeights<false>(pt, cols - j, out + j);
		}
		processStaticLinearWeights<last_row>(pt, cols - j, out + j);
	}

	static inline void __vectorcall _RGB2Y_Dynamic(const uint8_t* __restrict const data, const int32_t cols, const int32_t start_row, const int32_t rows, const int32_t stride, uint8_t* const __restrict out,
							  uint16_t const(& __restrict vNewDynamicWeighting)[sDynamicWeighting::COMPONENTS] ) {
		int i = start_row;
		for (; i < start_row + rows - 2; ++i) {
			processRow_Dynamic<false>(data + 3 * i * stride, cols, out + i * cols, vNewDynamicWeighting);
		}
		processRow_Dynamic<true>(data + 3 * i * stride, cols, out + i * cols, vNewDynamicWeighting);
	}
	static inline void __vectorcall _RGB2Y_StaticLinear(const uint8_t* __restrict const data, const int32_t cols, const int32_t start_row, const int32_t rows, const int32_t stride, uint8_t* const __restrict out) {
		int i = start_row;
		for (; i < start_row + rows - 2; ++i) {
			processRow_StaticLinear<false>(data + 3 * i * stride, cols, out + i * cols);
		}
		processRow_StaticLinear<true>(data + 3 * i * stride, cols, out + i * cols);
	}

	static inline void __vectorcall rgb2gray_ToSRGB(const uint8_t* const __restrict image, const int width, const int height, uint8_t* const __restrict out)
	{
		__declspec(align(16)) uint16_t vCurrentWeighting[sDynamicWeighting::COMPONENTS];

		calculateNewWeightingSRGB(vCurrentWeighting);
		_RGB2Y_Dynamic(image, width, 0, height, width, out, vCurrentWeighting);
	}

	static inline void __vectorcall rgb2gray_ToLinear(const uint8_t* const __restrict image, const int width, const int height, uint8_t* const __restrict out)
	{
		__declspec(align(16)) uint16_t vCurrentWeighting[sDynamicWeighting::COMPONENTS];

		calculateNewWeightingLinear(vCurrentWeighting);
		_RGB2Y_Dynamic(image, width, 0, height, width, out, vCurrentWeighting);
	}

	static inline void __vectorcall rgb2gray_ToStaticLinear(const uint8_t* const __restrict image, const int width, const int height, uint8_t* const __restrict out)
	{
		_RGB2Y_StaticLinear(image, width, 0, height, width, out);
	}

} // end namespace
