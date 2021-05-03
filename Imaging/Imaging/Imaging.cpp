#include "stdafx.h"
#include <tbb\tbb.h>
#include "Imaging.h"
#include <set>
#include <Math/superfastmath.h>

static __inline void __vectorcall
bgrx2bgr(uint8_t* __restrict out, uint8_t const* __restrict in, int const xsize)
{
	for (int x = 0; x < xsize; ++x) {
		*out++ = in[2];
		*out++ = in[1];
		*out++ = in[0];

		in += 4;
	}
}

static __inline void __vectorcall
bgr2bgrx(uint8_t* __restrict out, uint8_t const* __restrict in, int const xsize)
{
	for (int x = 0; x < xsize; ++x) {
		*out++ = in[2];
		*out++ = in[1];
		*out++ = in[0];
		*out++ = UINT8_MAX;

		in += 3;
	}
}
static __inline void __vectorcall
gray2bgr(uint8_t* __restrict out, uint8_t const* __restrict in, int const xsize)
{
	for (int x = 0; x < xsize; ++x) {

		uint8_t const uiGrayLevel = *in++;

		*out++ = uiGrayLevel;
		*out++ = uiGrayLevel;
		*out++ = uiGrayLevel;
	}
}
static __inline void __vectorcall
gray2bgrx(uint8_t* __restrict out, uint8_t const* __restrict in, int const xsize)
{
	for (int x = 0; x < xsize; ++x) {

		uint8_t const uiGrayLevel = *in++;

		*out++ = uiGrayLevel;
		*out++ = uiGrayLevel;
		*out++ = uiGrayLevel;
		*out++ = uiGrayLevel;
	}
}

static __inline void __vectorcall
colorbrightnesscontrast_BGR(uint8_t* __restrict out, uint8_t const* __restrict in, int const xsize,
							double const fBrightness, double const fContrast)
{
	static constexpr uint8_t const MIN_BLACKLEVEL = 16;
	__m128 const xmBrightness(_mm_set1_ps(fBrightness));

	__m128 xmIntensity;

	for (int x = 0; x < xsize; ++x) {
		
		uint32_t const B(*in++), G(*in++), R(*in++);
		if ((MIN_BLACKLEVEL < ((B+G+R+((MIN_BLACKLEVEL>>1)+1)) >> 1)) )
		{
			__m128 const xmBGR(_mm_cvtepi32_ps(_mm_setr_epi32(B, G, R, 0)));
			__m128 const xmContrast(_mm_set1_ps(fContrast));

			xmIntensity = _mm_fmadd_ps(xmContrast, xmBGR, xmBrightness);
		}
		else
		{
			__m128 const xmBGR(_mm_cvtepi32_ps(_mm_setr_epi32(B, G, R, 0)));
			__m128 const xmContrast(_mm_set1_ps(1.0f - fContrast));

			xmIntensity = _mm_fmadd_ps(xmContrast, xmBGR, xmBrightness);
		}
		// Truncate to 0 thru 255	
		uvec4_t clamped;
		SFM::saturate_to_u8(xmIntensity, clamped);

		*out++ = clamped.b;
		*out++ = clamped.g;
		*out++ = clamped.r;
	}
}

static __inline void 
XM_CALLCONV graybgrx_reduction(__m256d const xmConversionFactorInv, __m256d const xmConversionFactor,
							   uint8_t* __restrict out, uint8_t const* __restrict in, 
							   uint8_t const (&__restrict rLUT)[256], int const xsize)
{
	//ConversionFactor = 255 / (NumberOfShades - 1)
	// all channels equal in GrayBGRX (Replicated)
	//Gray = Integer((AverageValue / ConversionFactor) + 0.5) * ConversionFactor

	__m256d const xmPoint5(_mm256_set1_pd(0.5));
	for (int x = 0; x < xsize; ++x) {

		__m256d xmLuma = _mm256_cvtepi32_pd(_mm_setr_epi32(*in++, *in++, *in++, 0));

		xmLuma = _mm256_fmadd_pd(xmLuma, xmConversionFactorInv, xmPoint5);

		__m256i const xmGray = SFM::saturate_to_u8(_mm256_mul_pd(_mm256_floor_pd(xmLuma), xmConversionFactor));
		
		uint8_t uiGray = _mm256_cvtsi256_si32(xmGray);
		uiGray = rLUT[uiGray];
		*out++ = uiGray;
		*out++ = uiGray;
		*out++ = uiGray;
		*out++ = UINT8_MAX;
	}
}

void __vectorcall Parallel_BGRX2BGR(uint8_t* const* const __restrict& __restrict pDst, uint8_t const* const* const __restrict& __restrict pSrc,
	int const& xSize, int const& ySize)
{
	struct { // avoid lambda heap
		uint8_t* const* const __restrict Dst;
		uint8_t const* const* const __restrict Src;
		int const Width;
	} const p = { pDst, pSrc, xSize };

	tbb::parallel_for(int(0), ySize, [&p](int i)
	{
		bgrx2bgr(p.Dst[i], p.Src[i], p.Width);
	});
}
void __vectorcall Parallel_BGR2BGRX(uint8_t* const* const __restrict& __restrict pDst, uint8_t const* const* const __restrict& __restrict pSrc,
	                   int const& xSize, int const& ySize)
{
	struct { // avoid lambda heap
		uint8_t* const* const __restrict Dst;
		uint8_t const* const* const __restrict Src;
		int const Width;
	} const p = { pDst, pSrc, xSize };

	tbb::parallel_for(int(0), ySize, [&p](int i)
	{
		bgr2bgrx(p.Dst[i], p.Src[i], p.Width);
	});
}

void __vectorcall Parallel_Gray2BGR(uint8_t* const* const __restrict& __restrict pDst, uint8_t const* const* const __restrict& __restrict pSrc,
	                   int const& xSize, int const& ySize)
{
	struct { // avoid lambda heap
		uint8_t* const* const __restrict Dst;
		uint8_t const* const* const __restrict Src;
		int const Width;
	} const p = { pDst, pSrc, xSize };

	tbb::parallel_for(int(0), ySize, [&p](int i)
	{
		gray2bgr(p.Dst[i], p.Src[i], p.Width);
	});
}
void __vectorcall Parallel_Gray2BGRX(uint8_t* const* const __restrict& __restrict pDst, uint8_t const* const* const __restrict& __restrict pSrc,
	int const& xSize, int const& ySize)
{
	struct { // avoid lambda heap
		uint8_t* const* const __restrict Dst;
		uint8_t const* const* const __restrict Src;
		int const Width;
	} const p = { pDst, pSrc, xSize };

	tbb::parallel_for(int(0), ySize, [&p](int i)
	{
		gray2bgrx(p.Dst[i], p.Src[i], p.Width);
	});
}
void __vectorcall Parallel_GrayBGRXReduction(uint8_t* const* const  __restrict& pDst, uint8_t const* const* const __restrict& pSrc, int const& xSize, int const& ySize, double const& dLevels,
							    uint8_t const (& __restrict rLUT)[256])
{
	double const dLevelsReduced = 255.0 / (dLevels - 1.0);
	struct { // avoid lambda heap
		__m256d const xmConversionFactorInv, xmConversionFactor;
		uint8_t* const* const __restrict Dst;
		uint8_t const* const* const __restrict Src;
		uint8_t const(& __restrict LUT)[256];
		int const Stride;
	} const p = { _mm256_set1_pd(1.0 / dLevelsReduced), _mm256_set1_pd(dLevelsReduced), pDst, pSrc, rLUT, xSize };

	tbb::parallel_for(int(0), ySize, [&p](int i)
	{
		graybgrx_reduction(p.xmConversionFactorInv, p.xmConversionFactor, p.Dst[i], p.Src[i], p.LUT, p.Stride);
	});
}


void __vectorcall Parallel_BrightnessContrast(uint8_t* const* const __restrict& __restrict pDst, uint8_t const* const* const __restrict& __restrict pSrc,
								 int const& xSize, int const& ySize,
								 double const& BrightFactor, double const& ContrastFactor)
{
	struct { // avoid lambda heap
		uint8_t* const* const __restrict Dst;
		uint8_t const* const* const __restrict Src;
		int const Width;
		double const Brightness,
			         Contrast;
	} const p = { pDst, pSrc, xSize, BrightFactor, ContrastFactor };

	tbb::parallel_for(int(0), ySize, [&p](int i)
	{
		colorbrightnesscontrast_BGR(p.Dst[i], p.Src[i], p.Width, p.Brightness, p.Contrast);
	});
}


/* palette general */

namespace palette_Konstants {
	XMGLOBALCONST inline XMVECTORF32 const _luma{ { { 0.2126f, 0.7152f, 0.0722f, 0.f } } }; // #define LUMA vec3(0.2126f, 0.7152f, 0.0722f)
} // end ns

// **** xmRGB should be converted to linear space b4 call to this function
uint32_t const __vectorcall ImagingPaletteIndexClosestColor(uvec4_v const xmRGB, ImagingMemoryInstance const* const __restrict palette)
{
	constexpr uint32_t const stride_default(256U);
	constexpr int32_t const grey_variance_max(1);  // empiraclly found value ***do NOT change***

	uint32_t const size(palette->xsize);

	ivec4_t rgba;
	ivec4_v(xmRGB.v).xyzw(rgba);

	if (0 == (rgba.r | rgba.g | rgba.b)) { // zero (black)
		return(0U); // index 0 is always black
	}

	uint32_t begin(0), end(size);  // defaults to complete range (including greyscale range)

	if (rgba.r == rgba.g == rgba.b) { // grey ?
		begin = size - stride_default;  // skip to greyscale range
		end = size;
	}
	else {
		// +(R - G), +(R - B)
		// -(G - R), +(G - B)
		// -(B - R), -(B - G)
		ivec4_v abs_diff(rgba.r - rgba.g, rgba.r - rgba.b, rgba.g - rgba.b, 0);
		
		abs_diff.v = SFM::abs(abs_diff.v);

		__m128i const xmTmp = _mm_cmpgt_epi32(abs_diff.v, _mm_set1_epi32(grey_variance_max));

		if ((_mm_movemask_ps(_mm_castsi128_ps(xmTmp)) & 7) == 7) { // all (3) components greater than
			// decidely not gray
			end = size - stride_default;   // exclude greyscale range improving accuracy
		}
	}

	struct { // ordered by sequential access order
		ivec4_v const xmColor;
		uint32_t const* const __restrict paletteBlock;

		uint32_t
			minimumDistance,
			paletteIndex;

		XMVECTOR const xmfColor;
		float dotProduct;

	} p = { ivec4_v(xmRGB.v), reinterpret_cast<uint32_t const* const __restrict>(palette->block),
		    UINT32_MAX - 1U, 0U, 
			XMVector3Normalize(XMVectorScale(p.xmColor.v4f(), 1.0f / 255.0f)), 0.0f };

	for (uint32_t index = begin; index != end; ++index)
	{
		uvec4_t rgba;

		SFM::unpack_rgba(p.paletteBlock[index] & 0x00FFFFFFU, rgba);

		ivec4_v const xmPalette(uvec4_v(rgba).v);

		uvec4_v xmDiff;
		xmDiff.v = _mm_abs_epi32(_mm_sub_epi32(p.xmColor.v, xmPalette.v));

		xmDiff.xyzw(rgba);

		// values are in a range 0...255 - so this will never overflow
		uint32_t const distance_sq = (rgba.r * rgba.r) + (rgba.g * rgba.g) + (rgba.b * rgba.b);

		if (0U == distance_sq) { // exact match
			return(index);
		}
		// minimum distance check
		if (distance_sq <= p.minimumDistance) {

			XMVECTOR const xmfPaletteColor(XMVector3Normalize(XMVectorScale(xmPalette.v4f(), 1.0f / 255.0f)));

			// ensure direction is the closest to a dot product equal to one
			float const dotProduct = XMVectorGetX(SFM::abs(XMVector3Dot(p.xmfColor, xmfPaletteColor)));

			if (dotProduct >= p.dotProduct) {

				p.minimumDistance = distance_sq;
				p.paletteIndex = index;
				p.dotProduct = dotProduct;
			}
		}

	}

	return(p.paletteIndex);
}

struct PaletteColor
{
	uint32_t color,
		     position;

	bool const operator<(PaletteColor const& rhs) const {	// default - sorted by unique color (for set)
		return(color < rhs.color);
	}

	static bool const sortByInsertion(PaletteColor const& lhs, PaletteColor const& rhs) {
		return(lhs.position < rhs.position);
	}
};

// NOT pure black, pure white, grey
#define ValidateColor(rgba) !( (0U == (rgba.r | rgba.g | rgba.b)) || (255U == (rgba.r & rgba.g & rgba.b)) || (rgba.r == rgba.g == rgba.b) )



// super palette //

// **** Palette stores all color in linear space
//palette_basic - optional parameter - can be a 256 color palette to insert into superpalette. greyscale elements are ignored, ONLY color elements are inserted in superpalette.
ImagingMemoryInstance* const __restrict __vectorcall ImagingGenerateSuperPalette1D(ImagingSuperPaletteInfo& __restrict out_info, ImagingMemoryInstance const* const __restrict palette_basic)
{
	static constexpr uint32_t const stride_default(256U);
	constexpr float inv_stride(1.0f / float(stride_default - 1U));

	using PaletteSet = std::set<PaletteColor>; // (by color order)
	using PaletteSetOrdered = std::set<PaletteColor, bool const(* const)(PaletteColor const&, PaletteColor const&)>;  // (by insertion order)

	PaletteSet colors;  // (by color order)
	uvec4_t rgba;

	uint32_t position(0);

	out_info.option[0] = out_info.option[1] = 0;
	if (nullptr != palette_basic) { // optional palette to insert at beginning

		uint32_t const* __restrict pIn = reinterpret_cast<uint32_t const* __restrict>(palette_basic->block);

		for (position = 0; position < stride_default; ++position) {

			SFM::unpack_rgba(*pIn & 0x00FFFFFFU, rgba);

			// only accept values that do not evalute true to any of these conditions
			if (ValidateColor(rgba)) {

				colors.insert(PaletteColor{ SFM::pack_rgba(rgba.r, rgba.g, rgba.b, 0xFF), position });
			}

			++pIn;
		}
		out_info.option[1] = uint32_t(colors.size() - 1);
	}

	// fill in some standard palettes at beginning //
	out_info.skinny[0] = uint32_t(colors.size());
	for (float x = 1.0f; x >= 0.0f; x -= inv_stride) {

		XMVECTOR const xmRGB = SFM::skinny(x);

		SFM::saturate_to_u8(XMVectorScale(xmRGB, 255.0f), rgba);

		// only accept values that do not evalute true to any of these conditions
		if (ValidateColor(rgba)) {

			colors.insert(PaletteColor{ SFM::pack_rgba(rgba.r, rgba.g, rgba.b, 0xFF), position++ });
		}
	}
	out_info.skinny[1] = uint32_t(colors.size() - 1);

	out_info.inferno[0] = uint32_t(colors.size());
	for (float x = 1.0f; x >= 0.0f; x -= inv_stride) {

		XMVECTOR const xmRGB = SFM::inferno(1.0f - x);

		SFM::saturate_to_u8(XMVectorScale(xmRGB, 255.0f), rgba);

		// only accept values that do not evalute true to any of these conditions
		if (ValidateColor(rgba)) {

			colors.insert(PaletteColor{ SFM::pack_rgba(rgba.r, rgba.g, rgba.b, 0xFF), position++ });
		}
	}
	out_info.inferno[1] = uint32_t(colors.size() - 1);

	out_info.viridis[0] = uint32_t(colors.size());
	for (float x = 1.0f; x >= 0.0f; x -= inv_stride) {

		XMVECTOR const xmRGB = SFM::viridis(x);

		SFM::saturate_to_u8(XMVectorScale(xmRGB, 255.0f), rgba);

		// only accept values that do not evalute true to any of these conditions
		if (ValidateColor(rgba)) {

			colors.insert(PaletteColor{ SFM::pack_rgba(rgba.r, rgba.g, rgba.b, 0xFF), position++ });
		}
	}
	out_info.viridis[1] = uint32_t(colors.size() - 1);

	// next 768 elements (256*R,G,B) is hsv rainbow
	constexpr uint32_t const hsv_stride(stride_default * 3U);
	constexpr float const inv_hsv_stride(1.0f / float(hsv_stride - 1U));

	out_info.hsv[0] = uint32_t(colors.size());
	for (float x = 1.0f; x >= 0.0f; x -= inv_hsv_stride) {

		XMVECTOR xmHSV;

		// dark to bright blending nice with next stride (greys)
		float const xx = 1.0f - x;
		if (xx < 0.5f) {
			xmHSV = _mm_setr_ps(xx * 2.0f, 1.0f, xx * 2.0f, 0.0f);
		}
		else {
			xmHSV = _mm_setr_ps(1.0f, 2.0f - xx * 2.0f, xx * 2.0f, 0.0f);
		}

		XMVECTOR xmRGB = SFM::XMColorHSVToSRGB(xmHSV);

		SFM::saturate_to_u8(XMVectorScale(xmRGB, 255.0f), rgba);

		// only accept values that do not evalute true to any of these conditions
		if (ValidateColor(rgba)) {

			colors.insert(PaletteColor{ SFM::pack_rgba(rgba.r, rgba.g, rgba.b, 0xFF), position++ });
		}
	}
	
	out_info.hsv[1] = uint32_t(colors.size() - 1);

	out_info.width = uint32_t(colors.size()) + stride_default;	// actual size (unique colors + 256 shades of grey)
	
	// unique values compiled, reorder the set by the order that we inserted each value
	bool const(* const fn_pt)(PaletteColor const&, PaletteColor const&) = PaletteColor::sortByInsertion;
	PaletteSetOrdered colors_ordered(fn_pt);  // (by insertion order)
	colors_ordered.merge(colors);

	// pull out of ordered set container to new 1D image
	Imaging returnL(ImagingNew(MODE_BGRX, out_info.width, 1));
	ImagingClear(returnL); // ensure cleared memory

	uint32_t* __restrict pOut = reinterpret_cast<uint32_t * __restrict>(returnL->block);

	for (auto i = colors_ordered.cbegin(); i != colors_ordered.cend(); ++i) {
		*pOut++ = i->color;
	}

	// last stride is grayscale
	//greys - bright to dark to blend with last stride (hsv)
	out_info.grey[0] = uint32_t(colors_ordered.size());

	for(int32_t i = 255; i >= 0; --i) {

		*pOut++ = SFM::pack_rgba(i, i, i, 0xFF);
	}
	out_info.grey[1] = out_info.width - 1;

	// override first element to always be pure black
	pOut = reinterpret_cast<uint32_t * __restrict>(returnL->block);
	*pOut = 0;

	return(returnL);
}

// INDIVDUAL PALETTES //

/* exception state */

void *
ImagingError_IOError(void)
{
	fprintf(stderr, "*** exception: file access error\n");
	return NULL;
}

void *
ImagingError_MemoryError(void)
{
	fprintf(stderr, "*** exception: out of memory\n");
	return NULL;
}

void *
ImagingError_ValueError(const char *message)
{
	if (!message)
		message = "exception: bad argument to function";
	fprintf(stderr, "*** %s\n", message);
	return NULL;
}

void *
ImagingError_ModeError(void)
{
	return ImagingError_ValueError("bad image mode");
	return NULL;
}
void *
ImagingError_Mismatch(void)
{
	return ImagingError_ValueError("images don't match");
	return NULL;
}

void
ImagingError_Clear(void)
{
	/* nop */;
}
