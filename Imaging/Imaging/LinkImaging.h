#pragma once

class cImageCreator;

// eg typedef  int (Fred::*FredMemFn)(char x, float y);
typedef void (XM_CALLCONV cImageCreator::* const pFN_BuildLUT)(__m256d const /*xmConversionFactorInv*/, __m256d const /*xmConversionFactor*/);
