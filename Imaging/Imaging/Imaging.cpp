#include "stdafx.h"
#include <tbb/tbb.h>
#include "Imaging.h"
#include <set>
#include <Math/superfastmath.h>

#pragma intrinsic(memcpy)
#pragma intrinsic(memset)

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

// KTX Support //

// including vulkan.hpp is a nightmare for a small library and a huge dependency. 
// Redefined VkFormat - values are constant, but additional VkFormat may be added by Khronos from timne to time, synchronize if those formats are required.
// this is local to this cpp file only for a reason, don't move it

namespace { // purposely anonymous namespace

	typedef enum VkFormat {
		VK_FORMAT_UNDEFINED = 0,
		VK_FORMAT_R4G4_UNORM_PACK8 = 1,
		VK_FORMAT_R4G4B4A4_UNORM_PACK16 = 2,
		VK_FORMAT_B4G4R4A4_UNORM_PACK16 = 3,
		VK_FORMAT_R5G6B5_UNORM_PACK16 = 4,
		VK_FORMAT_B5G6R5_UNORM_PACK16 = 5,
		VK_FORMAT_R5G5B5A1_UNORM_PACK16 = 6,
		VK_FORMAT_B5G5R5A1_UNORM_PACK16 = 7,
		VK_FORMAT_A1R5G5B5_UNORM_PACK16 = 8,
		VK_FORMAT_R8_UNORM = 9,
		VK_FORMAT_R8_SNORM = 10,
		VK_FORMAT_R8_USCALED = 11,
		VK_FORMAT_R8_SSCALED = 12,
		VK_FORMAT_R8_UINT = 13,
		VK_FORMAT_R8_SINT = 14,
		VK_FORMAT_R8_SRGB = 15,
		VK_FORMAT_R8G8_UNORM = 16,
		VK_FORMAT_R8G8_SNORM = 17,
		VK_FORMAT_R8G8_USCALED = 18,
		VK_FORMAT_R8G8_SSCALED = 19,
		VK_FORMAT_R8G8_UINT = 20,
		VK_FORMAT_R8G8_SINT = 21,
		VK_FORMAT_R8G8_SRGB = 22,
		VK_FORMAT_R8G8B8_UNORM = 23,
		VK_FORMAT_R8G8B8_SNORM = 24,
		VK_FORMAT_R8G8B8_USCALED = 25,
		VK_FORMAT_R8G8B8_SSCALED = 26,
		VK_FORMAT_R8G8B8_UINT = 27,
		VK_FORMAT_R8G8B8_SINT = 28,
		VK_FORMAT_R8G8B8_SRGB = 29,
		VK_FORMAT_B8G8R8_UNORM = 30,
		VK_FORMAT_B8G8R8_SNORM = 31,
		VK_FORMAT_B8G8R8_USCALED = 32,
		VK_FORMAT_B8G8R8_SSCALED = 33,
		VK_FORMAT_B8G8R8_UINT = 34,
		VK_FORMAT_B8G8R8_SINT = 35,
		VK_FORMAT_B8G8R8_SRGB = 36,
		VK_FORMAT_R8G8B8A8_UNORM = 37,
		VK_FORMAT_R8G8B8A8_SNORM = 38,
		VK_FORMAT_R8G8B8A8_USCALED = 39,
		VK_FORMAT_R8G8B8A8_SSCALED = 40,
		VK_FORMAT_R8G8B8A8_UINT = 41,
		VK_FORMAT_R8G8B8A8_SINT = 42,
		VK_FORMAT_R8G8B8A8_SRGB = 43,
		VK_FORMAT_B8G8R8A8_UNORM = 44,
		VK_FORMAT_B8G8R8A8_SNORM = 45,
		VK_FORMAT_B8G8R8A8_USCALED = 46,
		VK_FORMAT_B8G8R8A8_SSCALED = 47,
		VK_FORMAT_B8G8R8A8_UINT = 48,
		VK_FORMAT_B8G8R8A8_SINT = 49,
		VK_FORMAT_B8G8R8A8_SRGB = 50,
		VK_FORMAT_A8B8G8R8_UNORM_PACK32 = 51,
		VK_FORMAT_A8B8G8R8_SNORM_PACK32 = 52,
		VK_FORMAT_A8B8G8R8_USCALED_PACK32 = 53,
		VK_FORMAT_A8B8G8R8_SSCALED_PACK32 = 54,
		VK_FORMAT_A8B8G8R8_UINT_PACK32 = 55,
		VK_FORMAT_A8B8G8R8_SINT_PACK32 = 56,
		VK_FORMAT_A8B8G8R8_SRGB_PACK32 = 57,
		VK_FORMAT_A2R10G10B10_UNORM_PACK32 = 58,
		VK_FORMAT_A2R10G10B10_SNORM_PACK32 = 59,
		VK_FORMAT_A2R10G10B10_USCALED_PACK32 = 60,
		VK_FORMAT_A2R10G10B10_SSCALED_PACK32 = 61,
		VK_FORMAT_A2R10G10B10_UINT_PACK32 = 62,
		VK_FORMAT_A2R10G10B10_SINT_PACK32 = 63,
		VK_FORMAT_A2B10G10R10_UNORM_PACK32 = 64,
		VK_FORMAT_A2B10G10R10_SNORM_PACK32 = 65,
		VK_FORMAT_A2B10G10R10_USCALED_PACK32 = 66,
		VK_FORMAT_A2B10G10R10_SSCALED_PACK32 = 67,
		VK_FORMAT_A2B10G10R10_UINT_PACK32 = 68,
		VK_FORMAT_A2B10G10R10_SINT_PACK32 = 69,
		VK_FORMAT_R16_UNORM = 70,
		VK_FORMAT_R16_SNORM = 71,
		VK_FORMAT_R16_USCALED = 72,
		VK_FORMAT_R16_SSCALED = 73,
		VK_FORMAT_R16_UINT = 74,
		VK_FORMAT_R16_SINT = 75,
		VK_FORMAT_R16_SFLOAT = 76,
		VK_FORMAT_R16G16_UNORM = 77,
		VK_FORMAT_R16G16_SNORM = 78,
		VK_FORMAT_R16G16_USCALED = 79,
		VK_FORMAT_R16G16_SSCALED = 80,
		VK_FORMAT_R16G16_UINT = 81,
		VK_FORMAT_R16G16_SINT = 82,
		VK_FORMAT_R16G16_SFLOAT = 83,
		VK_FORMAT_R16G16B16_UNORM = 84,
		VK_FORMAT_R16G16B16_SNORM = 85,
		VK_FORMAT_R16G16B16_USCALED = 86,
		VK_FORMAT_R16G16B16_SSCALED = 87,
		VK_FORMAT_R16G16B16_UINT = 88,
		VK_FORMAT_R16G16B16_SINT = 89,
		VK_FORMAT_R16G16B16_SFLOAT = 90,
		VK_FORMAT_R16G16B16A16_UNORM = 91,
		VK_FORMAT_R16G16B16A16_SNORM = 92,
		VK_FORMAT_R16G16B16A16_USCALED = 93,
		VK_FORMAT_R16G16B16A16_SSCALED = 94,
		VK_FORMAT_R16G16B16A16_UINT = 95,
		VK_FORMAT_R16G16B16A16_SINT = 96,
		VK_FORMAT_R16G16B16A16_SFLOAT = 97,
		VK_FORMAT_R32_UINT = 98,
		VK_FORMAT_R32_SINT = 99,
		VK_FORMAT_R32_SFLOAT = 100,
		VK_FORMAT_R32G32_UINT = 101,
		VK_FORMAT_R32G32_SINT = 102,
		VK_FORMAT_R32G32_SFLOAT = 103,
		VK_FORMAT_R32G32B32_UINT = 104,
		VK_FORMAT_R32G32B32_SINT = 105,
		VK_FORMAT_R32G32B32_SFLOAT = 106,
		VK_FORMAT_R32G32B32A32_UINT = 107,
		VK_FORMAT_R32G32B32A32_SINT = 108,
		VK_FORMAT_R32G32B32A32_SFLOAT = 109,
		VK_FORMAT_R64_UINT = 110,
		VK_FORMAT_R64_SINT = 111,
		VK_FORMAT_R64_SFLOAT = 112,
		VK_FORMAT_R64G64_UINT = 113,
		VK_FORMAT_R64G64_SINT = 114,
		VK_FORMAT_R64G64_SFLOAT = 115,
		VK_FORMAT_R64G64B64_UINT = 116,
		VK_FORMAT_R64G64B64_SINT = 117,
		VK_FORMAT_R64G64B64_SFLOAT = 118,
		VK_FORMAT_R64G64B64A64_UINT = 119,
		VK_FORMAT_R64G64B64A64_SINT = 120,
		VK_FORMAT_R64G64B64A64_SFLOAT = 121,
		VK_FORMAT_B10G11R11_UFLOAT_PACK32 = 122,
		VK_FORMAT_E5B9G9R9_UFLOAT_PACK32 = 123,
		VK_FORMAT_D16_UNORM = 124,
		VK_FORMAT_X8_D24_UNORM_PACK32 = 125,
		VK_FORMAT_D32_SFLOAT = 126,
		VK_FORMAT_S8_UINT = 127,
		VK_FORMAT_D16_UNORM_S8_UINT = 128,
		VK_FORMAT_D24_UNORM_S8_UINT = 129,
		VK_FORMAT_D32_SFLOAT_S8_UINT = 130,
		VK_FORMAT_BC1_RGB_UNORM_BLOCK = 131,
		VK_FORMAT_BC1_RGB_SRGB_BLOCK = 132,
		VK_FORMAT_BC1_RGBA_UNORM_BLOCK = 133,
		VK_FORMAT_BC1_RGBA_SRGB_BLOCK = 134,
		VK_FORMAT_BC2_UNORM_BLOCK = 135,
		VK_FORMAT_BC2_SRGB_BLOCK = 136,
		VK_FORMAT_BC3_UNORM_BLOCK = 137,
		VK_FORMAT_BC3_SRGB_BLOCK = 138,
		VK_FORMAT_BC4_UNORM_BLOCK = 139,
		VK_FORMAT_BC4_SNORM_BLOCK = 140,
		VK_FORMAT_BC5_UNORM_BLOCK = 141,
		VK_FORMAT_BC5_SNORM_BLOCK = 142,
		VK_FORMAT_BC6H_UFLOAT_BLOCK = 143,
		VK_FORMAT_BC6H_SFLOAT_BLOCK = 144,
		VK_FORMAT_BC7_UNORM_BLOCK = 145,
		VK_FORMAT_BC7_SRGB_BLOCK = 146,
		VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK = 147,
		VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK = 148,
		VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK = 149,
		VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK = 150,
		VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK = 151,
		VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK = 152,
		VK_FORMAT_EAC_R11_UNORM_BLOCK = 153,
		VK_FORMAT_EAC_R11_SNORM_BLOCK = 154,
		VK_FORMAT_EAC_R11G11_UNORM_BLOCK = 155,
		VK_FORMAT_EAC_R11G11_SNORM_BLOCK = 156,
		VK_FORMAT_ASTC_4x4_UNORM_BLOCK = 157,
		VK_FORMAT_ASTC_4x4_SRGB_BLOCK = 158,
		VK_FORMAT_ASTC_5x4_UNORM_BLOCK = 159,
		VK_FORMAT_ASTC_5x4_SRGB_BLOCK = 160,
		VK_FORMAT_ASTC_5x5_UNORM_BLOCK = 161,
		VK_FORMAT_ASTC_5x5_SRGB_BLOCK = 162,
		VK_FORMAT_ASTC_6x5_UNORM_BLOCK = 163,
		VK_FORMAT_ASTC_6x5_SRGB_BLOCK = 164,
		VK_FORMAT_ASTC_6x6_UNORM_BLOCK = 165,
		VK_FORMAT_ASTC_6x6_SRGB_BLOCK = 166,
		VK_FORMAT_ASTC_8x5_UNORM_BLOCK = 167,
		VK_FORMAT_ASTC_8x5_SRGB_BLOCK = 168,
		VK_FORMAT_ASTC_8x6_UNORM_BLOCK = 169,
		VK_FORMAT_ASTC_8x6_SRGB_BLOCK = 170,
		VK_FORMAT_ASTC_8x8_UNORM_BLOCK = 171,
		VK_FORMAT_ASTC_8x8_SRGB_BLOCK = 172,
		VK_FORMAT_ASTC_10x5_UNORM_BLOCK = 173,
		VK_FORMAT_ASTC_10x5_SRGB_BLOCK = 174,
		VK_FORMAT_ASTC_10x6_UNORM_BLOCK = 175,
		VK_FORMAT_ASTC_10x6_SRGB_BLOCK = 176,
		VK_FORMAT_ASTC_10x8_UNORM_BLOCK = 177,
		VK_FORMAT_ASTC_10x8_SRGB_BLOCK = 178,
		VK_FORMAT_ASTC_10x10_UNORM_BLOCK = 179,
		VK_FORMAT_ASTC_10x10_SRGB_BLOCK = 180,
		VK_FORMAT_ASTC_12x10_UNORM_BLOCK = 181,
		VK_FORMAT_ASTC_12x10_SRGB_BLOCK = 182,
		VK_FORMAT_ASTC_12x12_UNORM_BLOCK = 183,
		VK_FORMAT_ASTC_12x12_SRGB_BLOCK = 184,
		VK_FORMAT_G8B8G8R8_422_UNORM = 1000156000,
		VK_FORMAT_B8G8R8G8_422_UNORM = 1000156001,
		VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM = 1000156002,
		VK_FORMAT_G8_B8R8_2PLANE_420_UNORM = 1000156003,
		VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM = 1000156004,
		VK_FORMAT_G8_B8R8_2PLANE_422_UNORM = 1000156005,
		VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM = 1000156006,
		VK_FORMAT_R10X6_UNORM_PACK16 = 1000156007,
		VK_FORMAT_R10X6G10X6_UNORM_2PACK16 = 1000156008,
		VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16 = 1000156009,
		VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16 = 1000156010,
		VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16 = 1000156011,
		VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16 = 1000156012,
		VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16 = 1000156013,
		VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16 = 1000156014,
		VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16 = 1000156015,
		VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16 = 1000156016,
		VK_FORMAT_R12X4_UNORM_PACK16 = 1000156017,
		VK_FORMAT_R12X4G12X4_UNORM_2PACK16 = 1000156018,
		VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16 = 1000156019,
		VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16 = 1000156020,
		VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16 = 1000156021,
		VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16 = 1000156022,
		VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16 = 1000156023,
		VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16 = 1000156024,
		VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16 = 1000156025,
		VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16 = 1000156026,
		VK_FORMAT_G16B16G16R16_422_UNORM = 1000156027,
		VK_FORMAT_B16G16R16G16_422_UNORM = 1000156028,
		VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM = 1000156029,
		VK_FORMAT_G16_B16R16_2PLANE_420_UNORM = 1000156030,
		VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM = 1000156031,
		VK_FORMAT_G16_B16R16_2PLANE_422_UNORM = 1000156032,
		VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM = 1000156033,
		VK_FORMAT_G8_B8R8_2PLANE_444_UNORM = 1000330000,
		VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16 = 1000330001,
		VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16 = 1000330002,
		VK_FORMAT_G16_B16R16_2PLANE_444_UNORM = 1000330003,
		VK_FORMAT_A4R4G4B4_UNORM_PACK16 = 1000340000,
		VK_FORMAT_A4B4G4R4_UNORM_PACK16 = 1000340001,
		VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK = 1000066000,
		VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK = 1000066001,
		VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK = 1000066002,
		VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK = 1000066003,
		VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK = 1000066004,
		VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK = 1000066005,
		VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK = 1000066006,
		VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK = 1000066007,
		VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK = 1000066008,
		VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK = 1000066009,
		VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK = 1000066010,
		VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK = 1000066011,
		VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK = 1000066012,
		VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK = 1000066013,
		VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG = 1000054000,
		VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG = 1000054001,
		VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG = 1000054002,
		VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG = 1000054003,
		VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG = 1000054004,
		VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG = 1000054005,
		VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG = 1000054006,
		VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG = 1000054007,
		VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK_EXT = VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK,
		VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK_EXT = VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK,
		VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK_EXT = VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK,
		VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK_EXT = VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK,
		VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK_EXT = VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK,
		VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK_EXT = VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK,
		VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK_EXT = VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK,
		VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK_EXT = VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK,
		VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK_EXT = VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK,
		VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK_EXT = VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK,
		VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK_EXT = VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK,
		VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK_EXT = VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK,
		VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK_EXT = VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK,
		VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK_EXT = VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK,
		VK_FORMAT_G8B8G8R8_422_UNORM_KHR = VK_FORMAT_G8B8G8R8_422_UNORM,
		VK_FORMAT_B8G8R8G8_422_UNORM_KHR = VK_FORMAT_B8G8R8G8_422_UNORM,
		VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM_KHR = VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM,
		VK_FORMAT_G8_B8R8_2PLANE_420_UNORM_KHR = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,
		VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM_KHR = VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM,
		VK_FORMAT_G8_B8R8_2PLANE_422_UNORM_KHR = VK_FORMAT_G8_B8R8_2PLANE_422_UNORM,
		VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM_KHR = VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM,
		VK_FORMAT_R10X6_UNORM_PACK16_KHR = VK_FORMAT_R10X6_UNORM_PACK16,
		VK_FORMAT_R10X6G10X6_UNORM_2PACK16_KHR = VK_FORMAT_R10X6G10X6_UNORM_2PACK16,
		VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16_KHR = VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16,
		VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16_KHR = VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16,
		VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16_KHR = VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16,
		VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16_KHR = VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16,
		VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16_KHR = VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16,
		VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16_KHR = VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16,
		VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16_KHR = VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16,
		VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16_KHR = VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16,
		VK_FORMAT_R12X4_UNORM_PACK16_KHR = VK_FORMAT_R12X4_UNORM_PACK16,
		VK_FORMAT_R12X4G12X4_UNORM_2PACK16_KHR = VK_FORMAT_R12X4G12X4_UNORM_2PACK16,
		VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16_KHR = VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16,
		VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16_KHR = VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16,
		VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16_KHR = VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16,
		VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16_KHR = VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16,
		VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16_KHR = VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16,
		VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16_KHR = VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16,
		VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16_KHR = VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16,
		VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16_KHR = VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16,
		VK_FORMAT_G16B16G16R16_422_UNORM_KHR = VK_FORMAT_G16B16G16R16_422_UNORM,
		VK_FORMAT_B16G16R16G16_422_UNORM_KHR = VK_FORMAT_B16G16R16G16_422_UNORM,
		VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM_KHR = VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM,
		VK_FORMAT_G16_B16R16_2PLANE_420_UNORM_KHR = VK_FORMAT_G16_B16R16_2PLANE_420_UNORM,
		VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM_KHR = VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM,
		VK_FORMAT_G16_B16R16_2PLANE_422_UNORM_KHR = VK_FORMAT_G16_B16R16_2PLANE_422_UNORM,
		VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM_KHR = VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM,
		VK_FORMAT_G8_B8R8_2PLANE_444_UNORM_EXT = VK_FORMAT_G8_B8R8_2PLANE_444_UNORM,
		VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16_EXT = VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16,
		VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16_EXT = VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16,
		VK_FORMAT_G16_B16R16_2PLANE_444_UNORM_EXT = VK_FORMAT_G16_B16R16_2PLANE_444_UNORM,
		VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT = VK_FORMAT_A4R4G4B4_UNORM_PACK16,
		VK_FORMAT_A4B4G4R4_UNORM_PACK16_EXT = VK_FORMAT_A4B4G4R4_UNORM_PACK16,
		VK_FORMAT_MAX_ENUM = 0x7FFFFFFF
	} VkFormat;
} // end ns

uint32_t const GLtoVKFormat(uint32_t const glFormat) {
	switch (glFormat) {
	case 0x8229: // GL_R8
	case 0x1903: 
	case 0x8FBD: 
		return VK_FORMAT_R8_UNORM;

	case 0x822B: // GL_RG8
	case 0x8227: 
	case 0x8FBE:
		return VK_FORMAT_R8G8_UNORM;

	case 0x822A: 
		return VK_FORMAT_R16_UNORM; // GL_R16
	case 0x8F98: 
		return VK_FORMAT_R16_SNORM; // GL_R16_SNORM

	case 0x822C: 
		return VK_FORMAT_R16G16_UNORM; // GL_RG16
	case 0x8F99: 
		return VK_FORMAT_R16G16_SNORM; // GL_RG16_SNORM

	case 0x80E0: // GL_BGR
	case 0x8051: // GL_RGB
	case 0x1907: // GL_RGB
		return VK_FORMAT_R8G8B8_UNORM;
	case 0x8C41: 
		return VK_FORMAT_R8G8B8_SRGB;  // VK_FORMAT_B8G8R8_SRGB

	case 0x80E1: // GL_RGBA
	case 0x8058:
	case 0x1908: 
		return VK_FORMAT_R8G8B8A8_UNORM; 
	case 0x8C43: 
		return VK_FORMAT_R8G8B8A8_SRGB;  // VK_FORMAT_B8G8R8A8_SRGB

	case 0x8054: 
		return VK_FORMAT_R16G16B16_UNORM;    // GL_RGB16
	case 0x8F9A: 
		return VK_FORMAT_R16G16B16_SNORM;    // GL_RGB16_SNORM	

	case 0x805B: 
		return VK_FORMAT_R16G16B16A16_UNORM; // GL_RGBA16
	case 0x8F9B: 
		return VK_FORMAT_R16G16B16A16_SNORM; // GL_RGBA16_SNORM	

	case 0x83F0: 
		return VK_FORMAT_BC1_RGB_UNORM_BLOCK; // GL_COMPRESSED_RGB_S3TC_DXT1_EXT
	case 0x83F1: 
		return VK_FORMAT_BC1_RGBA_UNORM_BLOCK; // GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
	case 0x83F2: 
		return VK_FORMAT_BC3_UNORM_BLOCK; // GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
	case 0x83F3: 
		return VK_FORMAT_BC5_UNORM_BLOCK; // GL_COMPRESSED_RGBA_S3TC_DXT5_EXT

	case 0x8E8E: 
		return VK_FORMAT_BC6H_SFLOAT_BLOCK;	// COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB
	case 0x8E8F: 
		return VK_FORMAT_BC6H_UFLOAT_BLOCK;	// COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB         
	case 0x8E8C: 
		return VK_FORMAT_BC7_UNORM_BLOCK;	// GL_COMPRESSED_RGBA_BPTC_UNORM_ARB
	case 0x8E8D: 
		return VK_FORMAT_BC7_SRGB_BLOCK;
	}

	return VK_FORMAT_UNDEFINED;
}

eIMAGINGMODE const VKtoImagingMode(uint32_t const vkFormat) {
	switch (vkFormat) {
	case VK_FORMAT_R8_UNORM: 
		return MODE_L; // VK_FORMAT_R8_UNORM 

	case VK_FORMAT_R8G8_UNORM: 
		return MODE_LA; // VK_FORMAT_R8G8_UNORM 

	case VK_FORMAT_R16_UNORM: 
		return MODE_L16;   // VK_FORMAT_R16_UNORM 

	case VK_FORMAT_R16G16_UNORM: 
		return MODE_LA16;  // VK_FORMAT_R16G16_UNORM 

	case VK_FORMAT_R8G8B8_UNORM: // VK_FORMAT_R8G8B8_UNORM 
	case VK_FORMAT_R8G8B8_SRGB:  // VK_FORMAT_R8G8B8_SRGB 
	case VK_FORMAT_B8G8R8_UNORM: // VK_FORMAT_B8G8R8_UNORM 
	case VK_FORMAT_B8G8R8_SRGB:  // VK_FORMAT_B8G8R8_SRGB 
		return MODE_RGB;

	case VK_FORMAT_R8G8B8A8_UNORM: // VK_FORMAT_R8G8B8A8_UNORM 
	case VK_FORMAT_R8G8B8A8_SRGB:  // VK_FORMAT_R8G8B8A8_SRGB 
	case VK_FORMAT_B8G8R8A8_UNORM: // VK_FORMAT_B8G8R8A8_UNORM 
	case VK_FORMAT_B8G8R8A8_SRGB:  // VK_FORMAT_B8G8R8A8_SRGB 
		return MODE_BGRA;

	case VK_FORMAT_R16G16B16_UNORM: // VK_FORMAT_R16G16B16_UNORM 
	case VK_FORMAT_R16G16B16_SNORM: // VK_FORMAT_R16G16B16_SNORM 
		return MODE_RGB16;

	case VK_FORMAT_R16G16B16A16_UNORM: // VK_FORMAT_R16G16B16A16_UNORM 
	case VK_FORMAT_R16G16B16A16_SNORM: // VK_FORMAT_R16G16B16A16_SNORM 
		return MODE_BGRA16;

	case VK_FORMAT_BC6H_SFLOAT_BLOCK:  // compressed
	case VK_FORMAT_BC6H_UFLOAT_BLOCK:
		return MODE_BC6A;

	case VK_FORMAT_BC7_UNORM_BLOCK:    // compressed
	case VK_FORMAT_BC7_SRGB_BLOCK:
		return MODE_BC7;
	}

	return MODE_ERROR;
}

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


constinit static inline uint8_t const srgb_to_linear_lut[256] = { // single channel
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	2,
	2,
	2,
	2,
	2,
	2,
	2,
	2,
	3,
	3,
	3,
	3,
	3,
	3,
	4,
	4,
	4,
	4,
	4,
	5,
	5,
	5,
	5,
	6,
	6,
	6,
	6,
	7,
	7,
	7,
	8,
	8,
	8,
	8,
	9,
	9,
	9,
	10,
	10,
	10,
	11,
	11,
	12,
	12,
	12,
	13,
	13,
	13,
	14,
	14,
	15,
	15,
	16,
	16,
	17,
	17,
	17,
	18,
	18,
	19,
	19,
	20,
	20,
	21,
	22,
	22,
	23,
	23,
	24,
	24,
	25,
	25,
	26,
	27,
	27,
	28,
	29,
	29,
	30,
	30,
	31,
	32,
	32,
	33,
	34,
	35,
	35,
	36,
	37,
	37,
	38,
	39,
	40,
	41,
	41,
	42,
	43,
	44,
	45,
	45,
	46,
	47,
	48,
	49,
	50,
	51,
	51,
	52,
	53,
	54,
	55,
	56,
	57,
	58,
	59,
	60,
	61,
	62,
	63,
	64,
	65,
	66,
	67,
	68,
	69,
	70,
	71,
	72,
	73,
	74,
	76,
	77,
	78,
	79,
	80,
	81,
	82,
	84,
	85,
	86,
	87,
	88,
	90,
	91,
	92,
	93,
	95,
	96,
	97,
	99,
	100,
	101,
	103,
	104,
	105,
	107,
	108,
	109,
	111,
	112,
	114,
	115,
	116,
	118,
	119,
	121,
	122,
	124,
	125,
	127,
	128,
	130,
	131,
	133,
	134,
	136,
	138,
	139,
	141,
	142,
	144,
	146,
	147,
	149,
	151,
	152,
	154,
	156,
	157,
	159,
	161,
	163,
	164,
	166,
	168,
	170,
	171,
	173,
	175,
	177,
	179,
	181,
	183,
	184,
	186,
	188,
	190,
	192,
	194,
	196,
	198,
	200,
	202,
	204,
	206,
	208,
	210,
	212,
	214,
	216,
	218,
	220,
	222,
	224,
	226,
	229,
	231,
	233,
	235,
	237,
	239,
	242,
	244,
	246,
	248,
	250,
	253,
	255
};

uvec4_v const ImagingSRGBtoLinearVector(uint32_t const packed_srgb)
{
	uvec4_t srgb;
	SFM::unpack_rgba(packed_srgb, srgb);

	uvec4_t linear;
	linear.r = srgb_to_linear_lut[srgb.r];
	linear.g = srgb_to_linear_lut[srgb.g];
	linear.b = srgb_to_linear_lut[srgb.b];
	linear.a = srgb.a;
	
	return{ linear };
}
uint32_t const ImagingSRGBtoLinear(uint32_t const packed_srgb)
{
	uvec4_t srgb;
	SFM::unpack_rgba(packed_srgb, srgb);

	uvec4_t linear;
	linear.r = srgb_to_linear_lut[srgb.r];
	linear.g = srgb_to_linear_lut[srgb.g];
	linear.b = srgb_to_linear_lut[srgb.b];
	linear.a = srgb.a;

	return(SFM::pack_rgba(linear));
}


