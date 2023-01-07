#pragma once
#include <intrin.h>
#include <emmintrin.h>
#include <smmintrin.h>
#include <immintrin.h>


class cImageCreator;

// eg typedef  int (Fred::*FredMemFn)(char x, float y);
typedef void (__vectorcall cImageCreator::* const pFN_BuildLUT)(__m256d const /*xmConversionFactorInv*/, __m256d const /*xmConversionFactor*/);
