#pragma once
#include <stdint.h>
#include "Imaging.h"
void __vectorcall
ImagingResampleVerticalConvolution8u(uint32_t * const __restrict lineOut, ImagingMemoryInstance const * const __restrict imIn,
	int const xmin, int const xmax, int16_t const * const __restrict k, int const coefs_precision);

void __vectorcall
ImagingResampleHorizontalConvolution8u(uint32_t * const __restrict lineOut, uint32_t const* const __restrict lineIn,
	int const xsize, int const * const __restrict xbounds, int16_t const * const __restrict kk, int const kmax, int const coefs_precision);

void __vectorcall
ImagingResampleHorizontalConvolution8u4x(
	uint32_t * const __restrict lineOut0, uint32_t * const __restrict lineOut1, uint32_t * const __restrict lineOut2, uint32_t * const __restrict lineOut3,
	uint32_t const * const __restrict lineIn0, uint32_t const * const __restrict lineIn1, uint32_t const * const __restrict lineIn2, uint32_t  const * const __restrict lineIn3,
	int const xsize, int const * const __restrict xbounds, int16_t const * const __restrict kk, int const kmax, int const coefs_precision);