#include "stdafx.h"
#include <tbb\tbb.h>
#include "Imaging.h"
#include "Fast2Gray.h"
#include "RBFilter_AVX2.h"
#include "gif_lib.h"
#include <atomic>
#include <fmt/format.h>
#include <sstream>
#include <Objbase.h>
#include <filesystem>
#include <Utility/stringconv.h>

namespace fs = std::filesystem;

#if INCLUDE_TIF_SUPPORT
#include "tinytiffreader.h"
#include "tinytiffwriter.h"
#endif

#if INCLUDE_JPEG_SUPPORT
#include "jpeglib.h"
#endif

#include <Compressonator.h>
#include <tbb/scalable_allocator.h>
#include <Utility/mio/mmap.hpp>
#include <Utility/mem.h>

#pragma intrinsic(memcpy)
#pragma intrinsic(memset)

#define IMAGING_LIMIT_184K (184320ull)
//#define IMAGING_LIMIT_16K (16384ull)

#ifdef IMAGING_LIMIT_184K
#define IMAGING_LIMIT (IMAGING_LIMIT_184K + 1ull)
#else
#define IMAGING_LIMIT (IMAGING_LIMIT_16K + 1ull)
#endif

/* --------------------------------------------------------------------
* Standard image object.
*/
static constexpr uint64_t const IMAGE_SIZE_THRESHOLD(IMAGING_LIMIT * IMAGING_LIMIT + 1ull);	    // max loaded file image size
static constexpr int32_t const MAX_LUT_DIMENSION_SIZE(65 + 1);						    // maximum lut dimension n x n x n

static ImagingMemoryInstance* const __restrict
ImagingNewPrologueSubtype(eIMAGINGMODE const mode, int const xsize, int const ysize,
	int size)
{
	Imaging im;

	im = (Imaging)scalable_malloc(1 * size);
	if (!im)
		return (Imaging)ImagingError_MemoryError();
	
	/* linesize overflow check, roughly the current largest space req'd */
	if (xsize > (INT_MAX >> 2) - 1) {
		return (Imaging)ImagingError_MemoryError();
	}

	/* Setup image descriptor */
	memset(&(*im), 0, sizeof(ImagingMemoryInstance));

	im->xsize = xsize;
	im->ysize = ysize;
	im->type = IMAGING_TYPE_UINT8;

	switch (mode)
	{
	case MODE_1BIT:
		/* 1-bit images */
		im->bands = im->pixelsize = 1;
		break;
	case MODE_L:
		/* 8-bit greyscale (luminance) images */
		im->bands = im->pixelsize = 1;
		break;
	case MODE_L16:
		/* 16-bit greyscale (luminance) images */
		im->bands = 1;
		im->pixelsize = 2;
		im->type = IMAGING_TYPE_UINT32;
		break;
	case MODE_LA:
		/* 8-bit greyscale (luminance) with alpha */
		im->bands = im->pixelsize = 2;
		break;
	case MODE_LA16:
		/* 16-bit greyscale (luminance) with 16bit alpha */
		im->bands = 2;
		im->pixelsize = 4;
		im->type = IMAGING_TYPE_UINT32;
		break;
	case MODE_U32:
		/* 32-bit integer images */
		im->bands = 1;
		im->pixelsize = 4;
		im->type = IMAGING_TYPE_UINT32;
		break;
	case MODE_RGB:
		/* 24-bit true colour images */
		im->bands = 3;
		im->pixelsize = 3;
		break;
	case MODE_BGRX:
		/* 32-bit true colour images with unused alpha */
		im->bands = 4;
		im->pixelsize = 4;
		break;
	case MODE_BGRA:
		/* 32-bit true colour images with alpha */
		im->bands = im->pixelsize = 4;
		break;
	case MODE_RGB16:
		/* 16-bit bpc */
		im->bands = 3;
		im->pixelsize = 6;
		im->type = IMAGING_TYPE_UINT64;
		break;
	case MODE_BGRX16:
		/* 16-bit bpc */
		im->bands = 4;
		im->pixelsize = 8;
		im->type = IMAGING_TYPE_UINT64;
		break;
	case MODE_BGRA16:
		/* 16-bit bpc */
		im->bands = 4;
		im->pixelsize = 8;
		im->type = IMAGING_TYPE_UINT64;
		break;
	case MODE_F32:
		/* 32-bit float images */
		im->bands = 1;
		im->pixelsize = 4;
		im->type = IMAGING_TYPE_FLOAT32;
		break;
	case MODE_BC7:
	case MODE_BC6A:
		// setup same as bgra
		im->bands = im->pixelsize = 4;
		break;
	default:
		scalable_free(im);
		return (Imaging)ImagingError_ValueError("unrecognized mode");
	}

	/* Setup image descriptor */
	im->linesize = xsize * im->pixelsize;
	im->mode = mode;

	{
		/* Pointer array (allocate at least one line, to avoid MemoryError
		exceptions on platforms where calloc(0, x) returns NULL) */
		im->image = (uint8_t **)scalable_malloc(((ysize > 0) ? ysize : 1) * sizeof(void *));
	}

	if (!im->image) {
		scalable_free(im);
		return (Imaging)ImagingError_MemoryError();
	}

	return(im);
}

static ImagingMemoryInstance* const __restrict
ImagingNewPrologue(eIMAGINGMODE const mode, int const xsize, int const ysize)
{
	return( ImagingNewPrologueSubtype(
		mode, xsize, ysize, sizeof(struct ImagingMemoryInstance)
	) );
}

static ImagingMemoryInstance* const __restrict 
ImagingNewEpilogue(ImagingMemoryInstance* const __restrict im)
{
	/* If the raster data allocator didn't setup a destructor,
	assume that it couldn't allocate the required amount of
	memory. */
	if (!im->destroy)
		return (Imaging)ImagingError_MemoryError();

	/* Initialize alias pointers to pixel data. */
	switch (im->pixelsize) {
	case 1: case 3:
		im->image8 = (uint8_t**)im->image;
		break;
	case 2:
	case 4:
		im->image32 = (uint32_t**)im->image;
		break;
	}

	return(im);
}

void __vectorcall
ImagingClear(ImagingMemoryInstance* __restrict im)
{
	memset(im->block, 0, im->xsize * im->ysize * im->pixelsize);
}

void __vectorcall
ImagingDelete(ImagingMemoryInstance* __restrict im)
{
	if (!im)
		return;

	if (im->destroy)
		im->destroy(im);

	if (im->image) {

		scalable_free(im->image);

	}

	scalable_free(im); im = nullptr;
}
void __vectorcall ImagingDelete(ImagingMemoryInstance const* __restrict im)
{
	ImagingDelete(const_cast<ImagingMemoryInstance*>(im));
}

void __vectorcall
ImagingDelete(ImagingSequence* __restrict im)
{
	if (!im)
		return;

	if (im->destroy)
		im->destroy(im);

	scalable_free(im); im = nullptr;
}
void __vectorcall ImagingDelete(ImagingSequence const* __restrict im)
{
	ImagingDelete(const_cast<ImagingSequence*>(im));
}

void __vectorcall
ImagingDelete(ImagingLUT* __restrict im)
{
	if (!im)
		return;

	if (im->destroy)
		im->destroy(im);

	scalable_free(im); im = nullptr;
}
void __vectorcall ImagingDelete(ImagingLUT const* __restrict im)
{
	ImagingDelete(const_cast<ImagingLUT*>(im));
}

void __vectorcall
ImagingDelete(ImagingHistogram* __restrict im)
{
	if (!im)
		return;

	if (im->destroy)
		im->destroy(im);

	scalable_free(im); im = nullptr;
}
void __vectorcall ImagingDelete(ImagingHistogram const* __restrict im)
{
	ImagingDelete(const_cast<ImagingHistogram*>(im));
}

/* Block Storage Type */
/* ------------------ */
/* Allocate image as a single block. */

static void ImagingDestroyBlock_MemoryInstance(ImagingMemoryInstance* const __restrict im)
{
	if (im) {
		if (im->block) {

			scalable_free(im->block); im->block = nullptr;
		}
		im->destroy = nullptr;
	}
}
static void ImagingDestroyBlock_SequenceInstance(ImagingSequenceInstance* const __restrict im)
{
	if (im) {
		if (im->block) {

			scalable_free(im->block); im->block = nullptr;
		}
		im->destroy = nullptr;
	}
}
static void ImagingDestroyBlock_Sequence(ImagingSequence* const __restrict im)
{
	if (im) {
		if (im->images) {

			for (uint32_t i = 0; i < im->count; ++i) {
				if (im->images[i].destroy) {

					im->images[i].destroy(&im->images[i]);
				}
			}

			scalable_free(im->images); im->images = nullptr;
		}
		im->destroy = nullptr;
	}
}
static void ImagingDestroyBlock_LUT(ImagingLUT* const __restrict im)
{
	if (im) {
		if (im->block) {

			scalable_free(im->block); im->block = nullptr;
		}
		im->destroy = nullptr;
	}
}
static void ImagingDestroyBlock_Histogram(ImagingHistogram* const __restrict im)
{
	if (im) {
		if (im->block) {

			scalable_free(im->block); im->block = nullptr;
		}
		im->destroy = nullptr;
	}
}

static ImagingMemoryInstance* const __restrict __vectorcall
ImagingNewBlock(eIMAGINGMODE const mode, int const xsize, int const ysize)
{
	Imaging im;
	size_t y, i;

	im = ImagingNewPrologue(mode, xsize, ysize);
	if (!im)
		return NULL;

	/* We shouldn't overflow, since the threshold defined
	below says that we're only going to allocate max 4M
	here before going to the array allocator. Check anyway.
	*/
	if (im->linesize &&
		im->ysize > INT64_MAX / im->linesize) {
		/* punt if we're going to overflow */
		return NULL;
	}

	if ((size_t)im->ysize * (size_t)im->linesize <= 0) {
		/* some platforms return NULL for malloc(0); this fix
		prevents MemoryError on zero-sized images on such
		platforms */
		im->block = (uint8_t *)scalable_malloc(1);
	}
	else {
		im->block = (uint8_t *)scalable_malloc((size_t)im->ysize * (size_t)im->linesize);

		if (im->block) {		// alias pointers are only valid for buffersize that is 
			// composed of stritcly ysize * linesize = blocksize
			// ie.) Compressed BC7 & BC6A do not have alias pointers
// map alias lines to block memory
			for (y = i = 0; y < im->ysize; ++y) {
				im->image[y] = im->block + i;
				i += im->linesize;
			}
		}
	}

	if (im->block) {
		im->destroy = static_cast<void(*)(ImagingMemoryInstance* const __restrict)>(&ImagingDestroyBlock_MemoryInstance);
	}

	return( ImagingNewEpilogue(im) );
}

ImagingMemoryInstance* const __restrict __vectorcall
ImagingNew(eIMAGINGMODE const mode, int const xsize, int const ysize)
{
	Imaging im(nullptr);

	if (xsize <= 0 || ysize <= 0) {
		return (Imaging)ImagingError_ValueError("bad image size");
	}

	if ((uint64_t)xsize * (uint64_t)ysize < IMAGE_SIZE_THRESHOLD) {
		im = ImagingNewBlock(mode, xsize, ysize);
		if (im)
			return(im);
		/* assume memory error; try allocating in array mode instead */
		ImagingError_Clear();
	}
	else
		return (Imaging)ImagingError_ValueError("image size too big");

	return(im);
}
ImagingLUT* const __restrict __vectorcall ImagingNew(int const size)
{
	ImagingLUT* lut(nullptr);

	if (size <= 0) {
		return (ImagingLUT*)ImagingError_ValueError("bad image size");
	}

	if (size < MAX_LUT_DIMENSION_SIZE) {
		lut = (ImagingLUT*)scalable_malloc(1 * sizeof(ImagingLUT));
		memset(&(*lut), 0, sizeof(ImagingLUT));

		lut->destroy = static_cast<void(*)(ImagingLUT* const __restrict)>(&ImagingDestroyBlock_LUT);

		lut->size = size;  // size of dimension

		//  X  *   Y  *  (sizeof(uint16_t) * 4)
		lut->slicesize = lut->size * lut->size * lut->pixelsize;

		// Z * slicesize
		uint32_t const cubesize(lut->size * lut->slicesize);
		lut->block = (uint16_t*)scalable_malloc(cubesize);
		memset(&(*lut->block), 0, cubesize);
		
	}
	else
		return (ImagingLUT*)ImagingError_ValueError("image lut size too big");

	return(lut);
}
ImagingMemoryInstance* const __restrict __vectorcall ImagingNewCompressed(eIMAGINGMODE const mode /*should be MODE_BC7 or MODE_BC6A*/, int const xsize, int const ysize, int BufferSize)
{
	Imaging im(nullptr);

	if (xsize <= 0 || ysize <= 0) {
		return (Imaging)ImagingError_ValueError("bad image size");
	}

	if ((uint64_t)xsize * (uint64_t)ysize < IMAGE_SIZE_THRESHOLD) {
		im = ImagingNewBlock(mode, xsize, ysize);
		if (im)
			return(im);
		/* assume memory error; try allocating in array mode instead */
		ImagingError_Clear();
	}
	else
		return (Imaging)ImagingError_ValueError("image size too big");

	return(im);
}

ImagingHistogram* const __restrict __vectorcall ImagingNewHistogram(ImagingMemoryInstance const* const __restrict im)
{
	ImagingHistogram* histo(nullptr);

	if (!im) {
		return(nullptr);
	}

	histo = (ImagingHistogram*)scalable_malloc(1 * sizeof(ImagingHistogram));
	memset(&(*histo), 0, sizeof(ImagingHistogram));

	histo->destroy = static_cast<void(*)(ImagingHistogram* const __restrict)>(&ImagingDestroyBlock_Histogram);

	bool bHDR(false);

	// unsupported formats:
	// MODE_U32
	// MODE_F32

	switch (im->mode)
	{
	case MODE_L16:
	case MODE_LA16:
	case MODE_BGRA16:
	case MODE_BGRX16:
	case MODE_RGB16:
		histo->count = UINT16_MAX + 1;    // # of histogram bins for 16bpc image
		bHDR = true;
		break;
	default:
		histo->count = UINT8_MAX + 1;     // # of histogram bins for 8bpc image
		break;
	}

	histo->block = (uint32_t*)scalable_malloc(histo->count * sizeof(uint32_t));
	memset(&(*histo->block), 0, histo->count * sizeof(uint32_t));

	// build histogram
	int const width(im->xsize), height(im->ysize);

	uint32_t* const __restrict counts = (uint32_t * const __restrict)histo->block;
	uint32_t count(width * height);

	if (bHDR) {
		uint16_t const* pIn16 = (uint16_t const*)im->block;

		while (--count != 0) { // for every pixel in the image

			++counts[*pIn16];
			++pIn16;
		}
	}
	else {
		uint8_t const* pIn8 = (uint8_t const*)im->block;

		while (--count != 0) { // for every pixel in the image

			++counts[*pIn8];
			++pIn8;	
		}
	}

	return(histo);
}
static ImagingMemoryInstance* const __restrict __vectorcall
_copy(ImagingMemoryInstance const * const __restrict imIn)
{
    if (!imIn)
		return (Imaging)ImagingError_ValueError(NULL);

	ImagingMemoryInstance* __restrict imOut = ImagingNew(imIn->mode, imIn->xsize, imIn->ysize);
    if (!imOut)
        return(nullptr);

	{
		if (nullptr != imIn->block && nullptr != imOut->block)
			memcpy(imOut->block, imIn->block, imIn->ysize * imIn->linesize);
	}

    return(imOut);
}

ImagingMemoryInstance* const __restrict __vectorcall
ImagingCopy(ImagingMemoryInstance const* const __restrict imIn)
{
    return(_copy(imIn));
}

static ImagingLUT* const __restrict __vectorcall
_copy(ImagingLUT const* const __restrict lutIn)
{
	if (!lutIn)
		return (ImagingLUT*)ImagingError_ValueError(NULL);

	ImagingLUT* __restrict lutOut = ImagingNew(lutIn->size);

	if (!lutOut)
		return(nullptr);

	if (nullptr != lutIn->block && nullptr != lutOut->block)
		memcpy(lutOut->block, lutIn->block, lutIn->size * lutIn->slicesize);

	return(lutOut);
}

ImagingLUT* const __restrict __vectorcall
ImagingCopy(ImagingLUT const* const __restrict lutIn)
{
	return(_copy(lutIn));
}


void __vectorcall ImagingChromaKey(ImagingMemoryInstance* const __restrict im)	// (INPLACE) key[ 0x00b140 ] - for best results this function should be used earliest in the pipeline before any image alteration are made like dithering, etc.
{																				//                           - this function specifies the exact color key to use, but compensates for +-1 in differences.
	static constexpr uint32_t const color_key(0x0040b100);	// abgr

	uint32_t* __restrict block(reinterpret_cast<uint32_t*>(im->block));
	uvec4_t rgba{};
	uvec4_v const color_key_comp(0x00, 0xb1, 0x40); // color key per-component

	uint32_t count(im->xsize * im->ysize);
	while (--count != 0) { // for every pixel in the image

		// remove color & alpha component  (remove green screen color & set what ever alpha it was before, possibly opaque, to transparent)
		
		//                         //									  a r g b        a b g r
		//if (*block == 0x00b140)  // a, b, g, r // r g b a backwards  [0x0000b140 --> 0x0040b100]
		uint32_t const color_dont_care_alpha(*block & 0x00ffffff);

		if (color_dont_care_alpha == color_key) // color key takes care of the swizzle mentioned above
		{
			*block = 0; // remove color & alpha component  (remove green screen color & set what ever alpha it was before, possibly opaque, to transparent)
		}
		else {

			SFM::unpack_rgba(color_dont_care_alpha, rgba.r, rgba.g, rgba.b, rgba.a);
			
			// for colors close enough to color key (off by +-1 in any component)
			if (uvec4_v::all<3>(color_key_comp == uvec4_v(rgba.r - 1, rgba.g, rgba.b, rgba.a)) || //	 uvec4_v::all(color_key_comp == uvec4_v(rgba.r + 1, rgba.g, rgba.b, rgba.a))  // exception r is zero for color key
				uvec4_v::all<3>(color_key_comp == uvec4_v(rgba.r, rgba.g - 1, rgba.b, rgba.a)) ||
				uvec4_v::all<3>(color_key_comp == uvec4_v(rgba.r, rgba.g + 1, rgba.b, rgba.a)) ||
				uvec4_v::all<3>(color_key_comp == uvec4_v(rgba.r, rgba.g, rgba.b - 1, rgba.a)) ||
				uvec4_v::all<3>(color_key_comp == uvec4_v(rgba.r, rgba.g, rgba.b + 1, rgba.a)))
			{
				*block = 0;
			}
		}

		++block;
	}
}

namespace dithering {
	XMGLOBALCONST inline uint32_t const _n255by15{ (255U / 15U) };
	XMGLOBALCONST inline uint32_t const _table[64]{ 
	3U,129U,34U,160U,10U,136U,42U,168U,192U,66U,223U,97U,200U,73U,231U,105U,50U,
	176U,18U,144U,58U,184U,26U,152U,239U,113U,207U,81U,247U,121U,215U,89U,14U,
	140U,46U,180U,7U,133U,38U,164U,203U,77U,235U,109U,196U,70U,227U,101U,62U,188U,
	30U,156U,54U,180U,22U,148U,251U,125U,219U,93U,243U,117U,211U,85U 
	};
} // end ns
void __vectorcall ImagingDither(ImagingMemoryInstance* const __restrict im)
{
	uint32_t const width(im->xsize);
	uint32_t const height(im->ysize);

	uint32_t* const __restrict block(reinterpret_cast<uint32_t* const>(im->block));

	uvec4_t rgba{};

	__m128i const 
		nF0{ _mm_set1_epi32(0xF0) },
		n0F{ _mm_set1_epi32(0x0F) };

	for (uint32_t y = 0; y < height; ++y) {

		for (uint32_t x = 0; x < width; ++x) {

			uint32_t const pixel(y * width + x);

			SFM::unpack_rgba(block[pixel], rgba.r, rgba.g, rgba.b, rgba.a);

			// lookup
			uint32_t const d = dithering::_table[(((y & 7U) << 3U) + 7U) - (x & 7U)];

			// saturating addition
			//ucolor = (ucolor & nF0) + ((d - (ucolor & n0F) * n255by15) >> 4U & 16U);
			uvec4_v ucolor(rgba);
			ucolor.v = _mm_add_epi32(_mm_and_si128(ucolor.v, nF0),
				_mm_and_si128(
					_mm_srli_epi32(_mm_sub_epi32(_mm_set1_epi32(d),
						           _mm_mullo_epi32(_mm_and_si128(ucolor.v, n0F), _mm_set1_epi32(dithering::_n255by15))), 4), // SSE4.2 req for _mm_mullo_epi32
				_mm_set1_epi32(16))
			);
			//ucolor = clamp(ucolor, 0U, 255U);
			ucolor.v = SFM::saturate_to_u8(ucolor.v);

			// combine high and low bytes
			//ucolor = (ucolor & nF0) | ((ucolor >> 4) & n0F);
			ucolor.v = _mm_or_si128(_mm_and_si128(ucolor.v, nF0), _mm_and_si128(_mm_srli_epi32(ucolor.v, 4), n0F));

			ucolor.rgba(rgba);

			block[pixel] = SFM::pack_rgba(rgba.r, rgba.g, rgba.b, rgba.a);
		}
	}
}

// overwrites input
void __vectorcall ImagingLerpL16(ImagingMemoryInstance* const __restrict A, ImagingMemoryInstance const* const __restrict B, float const tT)
{
	uint32_t const height(B->ysize);
	uint32_t const width(B->xsize);

	uint16_t* const __restrict blockA(reinterpret_cast<uint16_t* const>(A->block));
	uint16_t const* const __restrict blockB(reinterpret_cast<uint16_t const* const>(B->block));

	static constexpr float const NORMALIZE = 1.0f / float(UINT16_MAX);
	static constexpr float const DENORMALIZE = float(UINT16_MAX);

	for (uint32_t y = 0; y < height; ++y) {

		for (uint32_t x = 0; x < width; ++x) {

			uint32_t const pixel(y * width + x);

			uint32_t const bA(blockA[pixel]),
				           bB(blockB[pixel]);

			float const fA(((float)bA) * NORMALIZE),
				        fB(((float)bB) * NORMALIZE);

			// floating point precision is best in [0.0f ... 1.0f] range, so normalizing before lerp then denormalization after is a huge benefit
			blockA[pixel] = SFM::saturate_to_u16(DENORMALIZE * SFM::lerp(fA, fB, tT));
		}
	}
}

// overwrites input
void __vectorcall ImagingLerp(ImagingMemoryInstance* const __restrict A, ImagingMemoryInstance const* const __restrict B, float const tT)
{
	uint32_t const height(B->ysize);
	uint32_t const width(B->xsize);

	uint32_t* const __restrict blockA(reinterpret_cast<uint32_t* const>(A->block));
	uint32_t const* const __restrict blockB(reinterpret_cast<uint32_t const* const>(B->block));

	static constexpr float const NORMALIZE = 1.0f / float(UINT8_MAX);
	static constexpr float const DENORMALIZE = float(UINT8_MAX);
	XMVECTOR const xmNorm(_mm_set1_ps(NORMALIZE));
	XMVECTOR const xmDeNorm(_mm_set1_ps(DENORMALIZE));

	uvec4_t rgba_dst{}, rgba_src{};

	for (uint32_t y = 0; y < height; ++y) {

		for (uint32_t x = 0; x < width; ++x) {

			uint32_t const pixel(y * width + x);

			SFM::unpack_rgba(blockA[pixel], rgba_dst.r, rgba_dst.g, rgba_dst.b, rgba_dst.a);
			SFM::unpack_rgba(blockB[pixel], rgba_src.r, rgba_src.g, rgba_src.b, rgba_src.a);

			// floating point precision is best in [0.0f ... 1.0f] range, so normalizing before lerp then denormalization after is a huge benefit
			SFM::saturate_to_u8(XMVectorMultiply(xmDeNorm, SFM::lerp(XMVectorMultiply(xmNorm, uvec4_v(rgba_dst).v4f()), XMVectorMultiply(xmNorm, uvec4_v(rgba_src).v4f()), tT)), rgba_dst);

			blockA[pixel] = SFM::pack_rgba(rgba_dst.r, rgba_dst.g, rgba_dst.b, rgba_dst.a);
		}
	}
}

void __vectorcall ImagingLerp(ImagingMemoryInstance* const __restrict out, ImagingMemoryInstance const* const __restrict A, ImagingMemoryInstance const* const __restrict B, float const tT)
{
	uint32_t const height(B->ysize);
	uint32_t const width(B->xsize);

	uint32_t* const __restrict blockOut(reinterpret_cast<uint32_t* const>(out->block));
	uint32_t const* const __restrict blockA(reinterpret_cast<uint32_t const* const>(A->block));
	uint32_t const* const __restrict blockB(reinterpret_cast<uint32_t const* const>(B->block));

	static constexpr float const NORMALIZE = 1.0f / float(UINT8_MAX);
	static constexpr float const DENORMALIZE = float(UINT8_MAX);
	XMVECTOR const xmNorm(_mm_set1_ps(NORMALIZE));
	XMVECTOR const xmDeNorm(_mm_set1_ps(DENORMALIZE));

	for (uint32_t y = 0; y < height; ++y) {

		for (uint32_t x = 0; x < width; ++x) {

			uint32_t const pixel(y * width + x);

			uvec4_t rgba_srcA, rgba_srcB;
			SFM::unpack_rgba(blockA[pixel], rgba_srcA.r, rgba_srcA.g, rgba_srcA.b, rgba_srcA.a);
			SFM::unpack_rgba(blockB[pixel], rgba_srcB.r, rgba_srcB.g, rgba_srcB.b, rgba_srcB.a);

			// floating point precision is best in [0.0f ... 1.0f] range, so normalizing before lerp then denormalization after is a huge benefit
			uvec4_t rgba_dst;
			SFM::saturate_to_u8(XMVectorMultiply(xmDeNorm, SFM::lerp(XMVectorMultiply(xmNorm, uvec4_v(rgba_srcA).v4f()), XMVectorMultiply(xmNorm, uvec4_v(rgba_srcB).v4f()), tT)), rgba_dst);

			blockOut[pixel] = SFM::pack_rgba(rgba_dst.r, rgba_dst.g, rgba_dst.b, rgba_dst.a);
		}
	}
}

// overwrites input
void __vectorcall ImagingLUTLerp(ImagingLUT* const __restrict A, ImagingLUT const* const __restrict B, float const tT)
{
	if (A->size != B->size)
		return;

	uint32_t const lut_size(B->size);

	uint16_t* const __restrict blockA(A->block);
	uint16_t const* const __restrict blockB(B->block);

	static constexpr float const NORMALIZE = 1.0f / float(UINT16_MAX);
	static constexpr float const DENORMALIZE = float(UINT16_MAX);
	XMVECTOR const xmNorm(_mm_set1_ps(NORMALIZE));
	XMVECTOR const xmDeNorm(_mm_set1_ps(DENORMALIZE));

	for (uint32_t b = 0; b < lut_size; ++b) {

		for (uint32_t g = 0; g < lut_size; ++g) {

			for (uint32_t r = 0; r < lut_size; ++r) {

				uint32_t const index(((b * lut_size * lut_size) + (g * lut_size) + r) << 2);
				uint16_t* const lut_colorA(blockA + index);
				uint16_t const* const lut_colorB(blockB + index);

				uvec4_t rgba_dst{};

				// floating point precision is best in [0.0f ... 1.0f] range, so normalizing before lerp then denormalization after is a huge benefit
				SFM::saturate_to_u16(XMVectorMultiply(xmDeNorm, SFM::lerp(XMVectorMultiply(xmNorm, uvec4_v(lut_colorA[0], lut_colorA[1], lut_colorA[2]).v4f()),
																		  XMVectorMultiply(xmNorm, uvec4_v(lut_colorB[0], lut_colorB[1], lut_colorB[2]).v4f()), tT)), rgba_dst);
				// overwrite input A
				lut_colorA[0] = rgba_dst.r;
				lut_colorA[1] = rgba_dst.g;
				lut_colorA[2] = rgba_dst.b;
			}
		}
	}
}

void __vectorcall ImagingBlend(ImagingMemoryInstance* const __restrict A, ImagingMemoryInstance const* const __restrict B)
{
	uint32_t const height(B->ysize);
	uint32_t const width(B->xsize);

	uint32_t* const __restrict blockA(reinterpret_cast<uint32_t* const>(A->block));
	uint32_t const* const __restrict blockB(reinterpret_cast<uint32_t const* const>(B->block));

	uvec4_t rgba_dst{}, rgba_src{};

	for (uint32_t y = 0; y < height; ++y) {

		for (uint32_t x = 0; x < width; ++x) {

			uint32_t const pixel(y * width + x);

			SFM::unpack_rgba(blockA[pixel], rgba_dst.r, rgba_dst.g, rgba_dst.b, rgba_dst.a);
			SFM::unpack_rgba(blockB[pixel], rgba_src.r, rgba_src.g, rgba_src.b, rgba_src.a);

			SFM::blend(uvec4_v(rgba_dst), uvec4_v(rgba_src)).rgba(rgba_dst);

			blockA[pixel] = SFM::pack_rgba(rgba_dst.r, rgba_dst.g, rgba_dst.b, rgba_dst.a);
		}
	}
}

void __vectorcall ImagingVerticalFlip(ImagingMemoryInstance* const __restrict im) // flip Y / invert Y axis / vertical flip (INPLACE)
{
	size_t const stride = (size_t const)im->linesize;
	size_t const height = (size_t const)im->ysize;
	size_t const half_height = height >> 1;

	uint8_t* __restrict pTarget = im->block + (height * stride) - stride;
	uint8_t* __restrict pSrc = im->block;

	for (size_t scanline = height; half_height != scanline; --scanline) {

		uint8_t* __restrict pTargetByte = pTarget;
		for (size_t byte = stride; 0ULL != byte; --byte) {

			std::swap(*pTargetByte, *pSrc);

			++pTargetByte;
			++pSrc;
		}
		pTarget -= stride;
	}
}

STATIC_INLINE_PURE uint32_t const wrap(int const n, int const dimension)
{
	return(SFM::abs(n % dimension));
}

template<typename T>
STATIC_INLINE_PURE ImagingMemoryInstance* const __restrict __vectorcall ImagingOffset(ImagingMemoryInstance const* const __restrict im, int const xoffset, int const yoffset)
{
	Imaging const img_offset(ImagingCopy(im));

	int32_t const width(im->xsize);
	int32_t const height(im->ysize);

	T const* const __restrict blockIn(reinterpret_cast<T const* const __restrict>(im->block));
	T* const __restrict blockOut(reinterpret_cast<T* const __restrict>(img_offset->block));

	for (int32_t y = 0; y < height; ++y) {

		uint32_t const out_y(wrap(y + yoffset, height));

		for (int32_t x = 0; x < width; ++x) {

			uint32_t const out_x(wrap(x + xoffset, width));

			blockOut[out_y * width + out_x] = blockIn[y * width + x];
		}
	}

	return(img_offset);
}

ImagingMemoryInstance* const __restrict __vectorcall ImagingOffset(ImagingMemoryInstance const* const __restrict im, int const xoffset, int const yoffset) // translate image while wrapping around (*NOT INPLACE)
{
	eIMAGINGMODE const mode(im->mode);

	switch (mode)
	{
	case MODE_L:
		return(ImagingOffset<uint8_t>(im, xoffset, yoffset));

	case MODE_LA:
	case MODE_L16:
		return(ImagingOffset<uint16_t>(im, xoffset, yoffset));

	case MODE_BGRX16:
	case MODE_BGRA16:
		return(ImagingOffset<uint64_t>(im, xoffset, yoffset));

	case MODE_LA16:
	case MODE_BGRX:
	case MODE_BGRA:
	default:
		return(ImagingOffset<uint32_t>(im, xoffset, yoffset));
	}

	return(nullptr);
}

// image[y][x] // assuming this is the original orientation 
// image[x][original_width - y] // rotated 90 degrees ccw
// image[original_height - x][y] // 90 degrees cw 
// image[original_height - y][original_width - x] // 180 degrees

template<typename T>
STATIC_INLINE_PURE void ImagingRotateCW(ImagingMemoryInstance* const __restrict img_returned, ImagingMemoryInstance const* const __restrict im,
	                                    uint32_t const width, uint32_t const height)
{
	T* const __restrict blockOut(reinterpret_cast<T* const __restrict>(img_returned->block));
	T const* const __restrict blockIn(reinterpret_cast<T const* const __restrict>(im->block));

	for (uint32_t y = 0; y < height; ++y) {

		for (uint32_t x = 0; x < width; ++x) {

			uint32_t const out_x((height - 1) - y);
			uint32_t const out_y(x);
			blockOut[out_y * height + out_x] = blockIn[y * width + x]; // image[x][original_width - y] // rotated 90 degrees ccw
		}
	}
}

ImagingMemoryInstance* const __restrict __vectorcall ImagingRotateCW(ImagingMemoryInstance const* const __restrict im)
{
	uint32_t const width(im->xsize);
	uint32_t const height(im->ysize);

	eIMAGINGMODE const mode(im->mode);
	Imaging img_returned(ImagingNew(mode, height, width));

	switch (mode)
	{
	case MODE_L:
		ImagingRotateCW<uint8_t>(img_returned, im, width, height);
		break;

	case MODE_LA:
	case MODE_L16:
		ImagingRotateCW<uint16_t>(img_returned, im, width, height);
		break;
	
	case MODE_BGRX16:
	case MODE_BGRA16:
		ImagingRotateCW<uint64_t>(img_returned, im, width, height);
		break;

	case MODE_LA16:
	case MODE_BGRX:
	case MODE_BGRA:
	default:
		ImagingRotateCW<uint32_t>(img_returned, im, width, height);
		break;
	}

	return(img_returned);
}

template<typename T>
STATIC_INLINE_PURE void ImagingRotateCCW(ImagingMemoryInstance* const __restrict img_returned, ImagingMemoryInstance const* const __restrict im,
	                                     uint32_t const width, uint32_t const height)
{
	T* const __restrict blockOut(reinterpret_cast<T* const __restrict>(img_returned->block));
	T const* const __restrict blockIn(reinterpret_cast<T const* const __restrict>(im->block));

	for (uint32_t y = 0; y < height; ++y) {

		for (uint32_t x = 0; x < width; ++x) {

			uint32_t const out_x(y);
			uint32_t const out_y((width - 1) - x);
			blockOut[out_y * height + out_x] = blockIn[y * width + x]; // image[x][original_width - y] // rotated 90 degrees ccw
		}
	}
}

ImagingMemoryInstance* const __restrict __vectorcall ImagingRotateCCW(ImagingMemoryInstance const* const __restrict im)
{
	uint32_t const width(im->xsize);
	uint32_t const height(im->ysize);

	eIMAGINGMODE const mode(im->mode);
	Imaging img_returned(ImagingNew(mode, height, width));

	switch (mode)
	{
	case MODE_L:
		ImagingRotateCCW<uint8_t>(img_returned, im, width, height);
		break;

	case MODE_LA:
	case MODE_L16:
		ImagingRotateCCW<uint16_t>(img_returned, im, width, height);
		break;

	case MODE_BGRX16:
	case MODE_BGRA16:
		ImagingRotateCCW<uint64_t>(img_returned, im, width, height);
		break;

	case MODE_LA16:
	case MODE_BGRX:
	case MODE_BGRA:
	default:
		ImagingRotateCCW<uint32_t>(img_returned, im, width, height);
		break;
	}

	return(img_returned);
}

// (NOT INPLACE) - new image returned of LA16 type, requires normal map of type BGRA16 input.
ImagingMemoryInstance* const __restrict __vectorcall ImagingTangentSpaceNormalMapToDerivativeMapBGRA16(ImagingMemoryInstance* const __restrict im)
{
	uint32_t const width(im->xsize);
	uint32_t const height(im->ysize);
	 
	Imaging img_returned(ImagingNew(MODE_LA16, width, height));

	uint32_t* const __restrict blockOut(reinterpret_cast<uint32_t* const>(img_returned->block));
	uint64_t const* const __restrict blockIn(reinterpret_cast<uint64_t const* const>(im->block));

	static constexpr float const NORMALIZE = 1.0f / float(UINT16_MAX);
	static constexpr float const DENORMALIZE = float(UINT16_MAX);
	XMVECTOR const xmNorm(_mm_set1_ps(NORMALIZE));
	XMVECTOR const xmDeNorm(_mm_set1_ps(DENORMALIZE));
	
	for (uint32_t y = 0; y < height; ++y) {

		for (uint32_t x = 0; x < width; ++x) {

			uint64_t const uNormal(blockIn[y * width + x]);

			XMVECTOR xmTangentSpaceNormal = XMVectorMultiply(xmNorm, uvec4_v((uNormal & 0xffff), ((uNormal >> 16) & 0xffff), ((uNormal >> 32) & 0xffff)).v4f());
			xmTangentSpaceNormal = SFM::__fms(xmTangentSpaceNormal, XMVectorReplicate(2.0f), XMVectorReplicate(1.0f));
			xmTangentSpaceNormal = XMVector3Normalize(xmTangentSpaceNormal);

			// Mikkelsen2020Bump.pdf - https://mmikkelsen3d.blogspot.com/2011/07/derivative-maps.html
			
			// This is setup expecting the input normal map in standard Tangent Space Coordinates
			// red X+ (right), green Y+ (down), blue Z+ (near) [set as default coordinate system in ShaderMap (TS)]
		 
			constexpr float const scale = 1.0f / 128.0f; // 89.55 degrees

			XMFLOAT3A vM, vMa;
			XMStoreFloat3A(&vM, xmTangentSpaceNormal);
			XMStoreFloat3A(&vMa, SFM::abs(xmTangentSpaceNormal));
			                           // avoid division by zero on absolute value
			float const z_ma = SFM::max(0.0001f, SFM::max(vMa.z, scale * SFM::max(vMa.x, vMa.y)));
			                                                                                                                                              // in vulkan texture origin is upper-left, however TS (tangent space) uses -1 as up aswell. So the *y component is already negated* So it passes thru untouched, do not negate it again!
			XMVECTOR xmDerivative = XMVectorDivide(XMVectorNegate(XMVectorSet(vM.x, vM.y, 0.0f, 0.0f)), XMVectorReplicate(z_ma));                         // *bugfix - -float2(vM.x, (-1.0f)*vM.y)/z_ma is supposed to be the output of the derivative map (signed here then packed unsigned)
			// back to 0 ... 1, and then finally uint16_t range                                                                    
			xmDerivative = SFM::saturate(SFM::__fma(xmDerivative, XMVectorReplicate(0.5f), XMVectorReplicate(0.5f)));
			xmDerivative = XMVectorMultiply(xmDeNorm, xmDerivative);                                                               

			uvec4_t la16{};

			SFM::saturate_to_u16(xmDerivative, la16);
			//                                      ABGR              GR              AL   <--- in memory 
			// store the 2 16bit components         RGBA (backwards)  RG (backwards)  LA
			blockOut[y * width + x] = la16.r | ((la16.g << 16) & 0xffff0000); 
		}
	}

	return(img_returned);
}

static ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadRaw(eIMAGINGMODE const mode, std::wstring_view const filenamepath, int const width, int const height)
{
	Imaging returnL(nullptr);

	FILE* fIn;

	if (0 == _wfopen_s(&fIn, filenamepath.data(), L"rbS"))
	{
		returnL = ImagingNew(mode, width, height);

		uint8_t*  __restrict pOut = returnL->block;
		uint32_t const stride = returnL->linesize;

		uint32_t scanline(height);
		while (0 != scanline) {

			_fread_nolock_s(pOut, stride, 1, stride, fIn);
			pOut += stride;

			--scanline;
		}

		_fclose_nolock(fIn); fIn = nullptr;
	}
	return(returnL);
}

ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadRawBGRA16(std::wstring_view const filenamepath, int const width, int const height)
{
	return(ImagingLoadRaw(MODE_BGRA16, filenamepath, width, height));
}
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadRawRGB16(std::wstring_view const filenamepath, int const width, int const height)
{
	return(ImagingLoadRaw(MODE_RGB16, filenamepath, width, height));
}
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadRawBGRA(std::wstring_view const filenamepath, int const width, int const height)
{
	return(ImagingLoadRaw(MODE_BGRA, filenamepath, width, height));
}
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadRawRGB(std::wstring_view const filenamepath, int const width, int const height)
{
	return(ImagingLoadRaw(MODE_RGB, filenamepath, width, height));
}
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadRawLA16(std::wstring_view const filenamepath, int const width, int const height)
{
	return(ImagingLoadRaw(MODE_LA16, filenamepath, width, height));
}
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadRawL16(std::wstring_view const filenamepath, int const width, int const height)
{
	return(ImagingLoadRaw(MODE_L16, filenamepath, width, height));
}
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadRawLA(std::wstring_view const filenamepath, int const width, int const height)
{
	return(ImagingLoadRaw(MODE_LA, filenamepath, width, height));
}
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadRawL(std::wstring_view const filenamepath, int const width, int const height)
{
	return(ImagingLoadRaw(MODE_L, filenamepath, width, height));
}
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadRawF32(std::wstring_view const filenamepath, int const width, int const height)
{
	return(ImagingLoadRaw(MODE_F32, filenamepath, width, height));
}

STATIC_INLINE_PURE ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadFromMemory(Imaging&& __restrict returnL, uint8_t const* __restrict pMemLoad)
{
	uint8_t* __restrict pOut = returnL->block;
	uint32_t const stride = returnL->linesize;

	uint32_t scanline(returnL->ysize);
	while (0 != scanline) {

		memcpy(pOut, pMemLoad, stride);
		pMemLoad += stride;
		pOut += stride;

		--scanline;
	}

	return(returnL);
}
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadFromMemoryBGRA16(uint8_t const* __restrict pMemLoad, int const width, int const height)
{
	return(ImagingLoadFromMemory(std::forward<Imaging&& __restrict>(ImagingNew(MODE_BGRA16, width, height)), pMemLoad));
}
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadFromMemoryRGB16(uint8_t const* __restrict pMemLoad, int const width, int const height)
{
	return(ImagingLoadFromMemory(std::forward<Imaging&& __restrict>(ImagingNew(MODE_RGB16, width, height)), pMemLoad));
}
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadFromMemoryBGRA(uint8_t const* __restrict pMemLoad, int const width, int const height)
{
	return(ImagingLoadFromMemory(std::forward<Imaging&& __restrict>(ImagingNew(MODE_BGRA, width, height)), pMemLoad));
}
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadFromMemoryRGB(uint8_t const* __restrict pMemLoad, int const width, int const height)
{
	return(ImagingLoadFromMemory(std::forward<Imaging&& __restrict>(ImagingNew(MODE_RGB, width, height)), pMemLoad));
}
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadFromMemoryLA16(uint8_t const* __restrict pMemLoad, int const width, int const height)
{
	return(ImagingLoadFromMemory(std::forward<Imaging&& __restrict>(ImagingNew(MODE_LA16, width, height)), pMemLoad));
}
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadFromMemoryLA(uint8_t const* __restrict pMemLoad, int const width, int const height)
{
	return(ImagingLoadFromMemory(std::forward<Imaging&& __restrict>(ImagingNew(MODE_LA, width, height)), pMemLoad));
}
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadFromMemoryL16(uint8_t const* __restrict pMemLoad, int const width, int const height)
{
	return(ImagingLoadFromMemory(std::forward<Imaging&& __restrict>(ImagingNew(MODE_L16, width, height)), pMemLoad));
}
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadFromMemoryL(uint8_t const* __restrict pMemLoad, int const width, int const height)
{
	return(ImagingLoadFromMemory(std::forward<Imaging&& __restrict>(ImagingNew(MODE_L, width, height)), pMemLoad));
}
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadFromMemoryF32(uint8_t const* __restrict pMemLoad, int const width, int const height)
{
	return(ImagingLoadFromMemory(std::forward<Imaging&& __restrict>(ImagingNew(MODE_F32, width, height)), pMemLoad));
}

#if INCLUDE_TIF_SUPPORT
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadTif(std::wstring_view const filenamepath)
{
	Imaging returnL(nullptr);

	TinyTIFFReaderFile* tif(nullptr);
	tif = TinyTIFFReader_open(stringconv::ws2s(filenamepath).c_str());

	if (tif) {

		uint32_t const bits(TinyTIFFReader_getBitsPerSample(tif, 0));          // bit depth of component
		uint32_t const samples(TinyTIFFReader_getSamplesPerPixel(tif));        // # of components

		// determine imaging mode
		uint32_t mode(0);

		switch (samples)
		{
		case 1:
			if (bits > 8) {
				mode = MODE_L16;
			}
			else {
				mode = MODE_L;
			}
			break;
		case 2:
			if (bits > 8) {
				mode = MODE_LA16;
			}
			else {
				mode = MODE_LA;
			}
			break;
		case 3:
			if (bits > 8) {
				mode = MODE_RGB16;
			}
			else {
				mode = MODE_RGB;
			}
			break;
		case 4:
		default:
			if (bits > 8) {
				mode = MODE_BGRA16;
			}
			else {
				mode = MODE_BGRA;
			}
			break;
		}

		uint32_t const width(TinyTIFFReader_getWidth(tif));
		uint32_t const height(TinyTIFFReader_getHeight(tif));

		returnL = ImagingNew((eIMAGINGMODE)mode, width, height);
		ImagingClear(returnL); // ensure memory is zeroed

		// read entire image data (only supporting tif with interleaved/chunky image data - single frame)
		TinyTIFFWriter_readImage(tif, returnL->block, size_t(returnL->ysize) * size_t(returnL->linesize));

		TinyTIFFReader_close(tif); tif = nullptr;

		// auto convert to BGRX
		if (MODE_RGB == mode) {
			Imaging const promoted_image(ImagingRGBToBGRX(returnL));
			ImagingDelete(returnL); returnL = promoted_image;
		}
		else if (MODE_RGB16 == mode) {
			Imaging const promoted_image(ImagingRGB16ToBGRX16(returnL));
			ImagingDelete(returnL); returnL = promoted_image;
		}
	}
	
	return(returnL);
}
#endif

/*
ImagingSequence* const __restrict __vectorcall
ImagingResample(ImagingSequence const* const __restrict imIn, int const xsize, int const ysize, int const filter)
{
	ImagingSequence* imOut(nullptr);

	imOut = (ImagingSequence*)scalable_malloc(1 * sizeof(ImagingSequence));
	memset(&(*imOut), 0, sizeof(ImagingSequence));

	// Setup ouput image descriptor //
	uint32_t const count(imIn->count);
	imOut->count = count;
	imOut->xsize = xsize;
	imOut->ysize = ysize;

	uint32_t const linesize(xsize * imIn->pixelsize),
					 imagesize(ysize * linesize);

	imOut->linesize = linesize;

	imOut->destroy = static_cast<void(*)(ImagingSequence* const __restrict)>(&ImagingDestroyBlock_Sequence);

	imOut->images = (ImagingSequenceInstance*)scalable_malloc(count * sizeof(ImagingSequenceInstance));
	memset(imOut->images, 0, count * sizeof(ImagingSequenceInstance));

	// setup translation to be compatible (ImagingSequenceInstance->ImagingMemoryInstance)
	ImagingMemoryInstance translationIn{};

	translationIn.mode = MODE_BGRX;
	translationIn.type = IMAGING_TYPE_UINT8;
	translationIn.bands = 3;
	translationIn.xsize = imIn->xsize;
	translationIn.ysize = imIn->ysize;
	translationIn.pixelsize = imIn->pixelsize;
	translationIn.linesize = imIn->linesize;

	// temporary allocation for aliasing pointers, which ImagingResample requires
	translationIn.image = (uint8_t**)scalable_malloc(translationIn.ysize * sizeof(void*));

	for (uint32_t i = 0; i < count; ++i) {

		// setup out image
		imOut->images[i].block = (uint8_t*)scalable_malloc(imagesize);
		imOut->images[i].destroy = static_cast<void(*)(ImagingSequenceInstance* const __restrict)>(&ImagingDestroyBlock_SequenceInstance);

		imOut->images[i].xsize = xsize;
		imOut->images[i].ysize = ysize;
		imOut->images[i].linesize = linesize;

		imOut->images[i].delay = imIn->images[i].delay;

		// setup in image (translation)
		translationIn.block = imIn->images[i].block;

		// map alias lines to block memory
		size_t y, o;
		for (y = o = 0; y < translationIn.ysize; ++y) {
			translationIn.image[y] = translationIn.block + o;
			o += translationIn.linesize;
		}
		// Initialize alias pointers to pixel data. 
		translationIn.image32 = (int32_t**)translationIn.image; // always a pixelsize of 4 for ImagingSequence BGRX

		// resample the current frame
		Imaging resampled = ImagingResample(&translationIn, xsize, ysize, filter);

		{ // copy resampled image to output image sequence
			memcpy(imOut->images[i].block, resampled->block, imagesize);
		}

		// free temp resampled image
		ImagingDelete(resampled);

	} // for count images

	// free alias pointers
	scalable_free(translationIn.image); translationIn.image = nullptr;

	return(imOut);
}
*/

enum GIFConstants {
	    //Graphics control extension has most of animation info
	     GCE_Code = GRAPHICS_EXT_FUNC_CODE,
	     GCE_Size = 4,
	    //Fields of the above
	     GCE_Flags = 0,
	     GCE_Delay = 1,
	     GCE_TransColor = 3,
	    //Contents of mask
	     GCE_DisposalUnspecified = DISPOSAL_UNSPECIFIED,		/* No disposal specified. */
	     GCE_DisposalDoNot = DISPOSE_DO_NOT,					/* Leave image in place */
	     GCE_DisposalBackground = DISPOSE_BACKGROUND,			/* Set area to background color */
	     GCE_DisposalPrevious = DISPOSE_PREVIOUS,				/* Restore to previous content */
};

typedef struct FrameHistory
{
	ImagingMemoryInstance const*			 image = nullptr;

} FrameHistory;

typedef struct Metadata
{
	uint32_t mode = DISPOSAL_UNSPECIFIED;
	uint32_t delay = 100;
	int32_t  trans_key = -1;  // -1 denotes an index that has no color key for transparency
} Metadata;

STATIC_INLINE_PURE uint32_t const frame_delay(uint8_t const* const __restrict loc) {
	return (10 * (loc[0] | (((uint32_t)loc[1]) << 8)));
}

template<bool const replace_all_pixels = false>
STATIC_INLINE_PURE void __vectorcall ImagingGIFSetPixels(
											GifColorType const* const& __restrict colors,
											ImagingMemoryInstance* const __restrict image_dst,
											SavedImage const& __restrict image_src, 
											uint32_t const exclude_index = 0)
{
	uint32_t* const __restrict block(reinterpret_cast<uint32_t* const __restrict>(image_dst->block));
	uint32_t const dst_width(image_dst->xsize);
	
	for (int32_t y = 0; y < image_src.ImageDesc.Height; ++y) {

		for (int32_t x = 0; x < image_src.ImageDesc.Width; ++x) {

			uint32_t const src(y * image_src.ImageDesc.Width + x);
			uint32_t const map = uint32_t((uint8_t const)image_src.RasterBits[src]);

			uint32_t const dst((y + image_src.ImageDesc.Top) * dst_width + (x + image_src.ImageDesc.Left));

			uint32_t alpha;

			if constexpr (replace_all_pixels) {		
				alpha = 0xFF; // opaque
			}
			else if (exclude_index != map) {
				// only draw pixels that "exist" in current frame
				alpha = 0xFF;
			}
			else {
				alpha = 0x01; // bugfix - allows frame to decay so there isn't a persistant image in blend, after many blends
			}
			// GIF is almost 30 years old
			block[dst] = SFM::pack_rgba(colors[map].Red, colors[map].Green, colors[map].Blue, alpha);
		}
	}
}
STATIC_INLINE_PURE void __vectorcall ImagingGIFDisposePixels(
	ImagingMemoryInstance* const __restrict image_dst,
	SavedImage const& __restrict image_src,
	uint32_t const background_rgba)
{
	uint32_t* const __restrict block(reinterpret_cast<uint32_t* const __restrict>(image_dst->block));
	uint32_t const dst_width(image_dst->xsize);

	for (int32_t y = 0; y < image_src.ImageDesc.Height; ++y) {

		uint32_t const dst((y + image_src.ImageDesc.Top) * dst_width + (image_src.ImageDesc.Left));

		memset(&block[dst], background_rgba, image_src.ImageDesc.Width << 2u);
	}
}

static ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadGIFFrame(uint32_t const frame,
																				FrameHistory* const __restrict history,
																				SavedImage const& __restrict image,
																				ImagingSequence const* const __restrict im, 
																				ColorMapObject const* const common_colormap,
																				int32_t const background_color_index)
{
	ImagingSequenceInstance& __restrict meta_data_image(im->images[frame]);
	ExtensionBlock* ext(nullptr);
	for (auto i = 0; i < image.ExtensionBlockCount; ++i) {
		if (GRAPHICS_EXT_FUNC_CODE == image.ExtensionBlocks[i].Function && GCE_Size == image.ExtensionBlocks[i].ByteCount) {
			ext = &(image.ExtensionBlocks[i]);
			break;
		}
	}
	Metadata meta;
	if (ext) {

		if (ext->Bytes[GCE_Flags] & 1) {
			meta.trans_key = ((uint8_t)ext->Bytes[GCE_TransColor]);
		}
		meta.mode = ((ext->Bytes[GCE_Flags] >> 2) & 7);

		meta.delay = frame_delay((uint8_t const* const)&ext->Bytes[GCE_Delay]);
		if (0 == meta.delay)
			meta.delay = 1;
	}
	meta_data_image.delay = meta.delay; // for the output images, only the delay is needed, everything else is "pre-processed"

	ColorMapObject const* const colormap(image.ImageDesc.ColorMap ? image.ImageDesc.ColorMap : common_colormap);
	// GIF is almost 30 years old
	uint32_t const background_rgba(SFM::pack_rgba(colormap->Colors[background_color_index].Red, colormap->Colors[background_color_index].Green, colormap->Colors[background_color_index].Blue, 0x00));
	
	Imaging returnL(ImagingNew(MODE_BGRA, im->xsize, im->ysize));

	// always clear to transparent
	memset(returnL->block, 0 == frame ? (background_rgba | 0xFF000000) : 0, im->ysize * im->linesize);

	bool bBlend(false);

	switch (meta.mode)
	{
/*3*/case GCE_DisposalPrevious: //- don't do anything and the last frame will just repeat (ImageBlend - dst all transparent, src will replace)
		bBlend = (0 != frame);
		break;
/*0*/case GCE_DisposalUnspecified: // -all pixels are replaced regardless of transparency
		ImagingGIFSetPixels<true>(colormap->Colors, returnL, image);
		break;
/*2*/case GCE_DisposalBackground: //-fill image rect defined for this frame with background color (note the opaque flag is set here, so ImageBlend composition doesn't replace these pixels)
		ImagingGIFDisposePixels(returnL, image, background_rgba | 0xFF000000);	
/*1*/case GCE_DisposalDoNot: // -new pixels are drawn with transparency ImageBlend then does the hard work of vomposition of dst and src
		//fall-through//
	default:
		if (-1 != meta.trans_key) {
			ImagingGIFSetPixels(colormap->Colors, returnL, image, meta.trans_key);
		}
		else {
			ImagingGIFSetPixels<true>(colormap->Colors, returnL, image);
		}
		bBlend = (0 != frame);
		break;
	}

	// apply chroma key filter 1st
	ImagingChromaKey(returnL); // must be applied here, to the raw'ist of data before it becomes blended, dithered, etc.

	if (bBlend) {

		// blend previous frame with current frame based on transparency
		ImagingBlend(returnL, history[frame - 1].image);

	}
		
	history[frame].image = returnL; // released later

	return(returnL); // do not release
}

typedef struct _task_frame_data { // avoid lambda heap
	FrameHistory* const history;
	SavedImage const* const __restrict gif_images;
	ImagingMemoryInstance** const __restrict images;
	ImagingSequence const* const __restrict im;
	ColorMapObject const* const __restrict common_colormap;
	int32_t const background_color_index;
	uint32_t const width, height;
} const task_frame_data; 

static void task_frame_process(uint32_t const i, task_frame_data p)
{
	ImagingMemoryInstance* const frame = ImagingLoadGIFFrame(i, p.history, p.gif_images[i], p.im, p.common_colormap, p.background_color_index);

	if (i & 1) // only dither odd frames so static dither pattern doesn't persist between frames
	{
		static constexpr float const GOLDEN_RATIO = 0.61803398874989484820f; // slightly bias lerp to non dithered original version

		ImagingMemoryInstance const* const original = ImagingCopy(frame);

		ImagingDither(frame); // enhance by dithering, (any inter-frame blending also benefits!)

		ImagingLerp(frame, original, GOLDEN_RATIO);

		ImagingDelete(original);
	}

	p.images[i] = ImagingResample(frame, p.width, p.height, IMAGING_TRANSFORM_BICUBIC);

	// do not delete frame_dithered - it is history for blending (released later) //
}

// this function is purposely single threaded, so when used asynchnously in a background thread it uses minimal cpu resources
ImagingSequence* const __restrict __vectorcall ImagingLoadGIFSequence(std::wstring_view const giffilenamepath, uint32_t width, uint32_t height)
{
	ImagingSequence* im(nullptr);
	int	ErrorCode(0);

	GifFileType* GifFileIn(DGifOpenFileName(giffilenamepath.data(), &ErrorCode));

	if (nullptr != GifFileIn) {

		if (GIF_ERROR != DGifSlurp(GifFileIn)) {

			im = (ImagingSequence*)scalable_malloc(1 * sizeof(ImagingSequence));
			memset(&(*im), 0, sizeof(ImagingSequence));

			/* Setup image descriptor */
			uint32_t const count(uint32_t(GifFileIn->ImageCount));
			im->count = count;
			// the remaining values here are temporary, will be changed to reflect final palettized image dimensions 
			im->xsize = GifFileIn->SWidth;
			im->ysize = GifFileIn->SHeight;
			im->linesize = im->xsize * 4; // for now BGRX is used

			// if zero is passed in for either width or height, default to the native corresponding dimension
			if ( 0 == width )
				width = im->xsize;
			if ( 0 == height )
				height = im->ysize;

			im->destroy = static_cast<void(*)(ImagingSequence* const __restrict)>(&ImagingDestroyBlock_Sequence);

			// ########## Load all gif frames, while resampling to desired size
			ImagingMemoryInstance** __restrict images;
			// non-contigous memory is used during this process for each sequence instance (frame)
			images = (ImagingMemoryInstance**)scalable_malloc(count * sizeof(ImagingMemoryInstance*));
			// move to contigous memory in already allocated im->images[] that also already contains updated meta deta (frame delay)
			// is done afterwards goto line: 864

			// allocate the final frames so we can fill with metadata (ExtensionBlock data)
			im->images = (ImagingSequenceInstance*)scalable_malloc(count * sizeof(ImagingSequenceInstance));
			memset(im->images, 0, count * sizeof(ImagingSequenceInstance));

			FrameHistory* history = (FrameHistory*)scalable_malloc(count * sizeof(FrameHistory));
			memset(history, 0, count * sizeof(FrameHistory));

			{
				task_frame_data p{ history, GifFileIn->SavedImages, images, im, GifFileIn->SColorMap, GifFileIn->SBackGroundColor, width, height };

				// must always process frame 0 first, so potentisl dependency chain does not dead-lock

				// process the remaining frames
				for (uint32_t i = 0; i < count; ++i) {
					task_frame_process(i, p);
				}

				// release original frames (must be done after loading all frames has completed (above)
				for (uint32_t i = 0; i < count; ++i) {
					ImagingDelete(history[i].image);
				}
				scalable_free(history); history = nullptr;
			}

			// no longer need gif ....
			DGifCloseFile(GifFileIn, &ErrorCode); GifFileIn = nullptr;

			// update image sequence dimensions to the resampled width/height, and linesize now reflects the correct size for sequence image
			im->xsize = width;
			im->ysize = height;

			uint32_t const linesize(width * im->pixelsize),
						   imagesize(height * linesize);

			im->linesize = linesize;

			// ########## MOVE	
			for (uint32_t i = 0; i < count; ++i) {

				im->images[i].block = (uint8_t*)scalable_malloc(imagesize);
				im->images[i].destroy = static_cast<void(*)(ImagingSequenceInstance* const __restrict)>(&ImagingDestroyBlock_SequenceInstance);

				im->images[i].xsize = width;
				im->images[i].ysize = height;
				im->images[i].linesize = linesize;

				{ // moving to contigous memory im->images[], can't use memcpy as the meta data (frame delay) would be clobbered
					// however for data in ImagingMemoryInstance can be memcpy or moved
					memcpy(im->images[i].block, images[i]->block, imagesize);
				}
				ImagingDelete(images[i]);
			} // for count images

			scalable_free(images); images = nullptr;
		}
	}

	if (nullptr != GifFileIn) { // error handling, ensure released
		DGifCloseFile(GifFileIn, &ErrorCode);
	}

	return(im);
}


ImagingLUT* const __restrict __vectorcall ImagingLoadLUT(std::wstring_view const cubefilenamepath)
{
	ImagingLUT* lut(nullptr);

	FILE* fIn;
	if (0 == _wfopen_s(&fIn, cubefilenamepath.data(), L"r"))
	{
		static constexpr uint32_t const MAX_LINE_SZ = 250;  // .cube spec states maximum line size
		char buffer[MAX_LINE_SZ];

		// .cube lut specification
		// https://wwwimages2.adobe.com/content/dam/acom/en/products/speedgrade/cc/pdfs/cube-lut-specification-1.0.pdf
		//
		while (0 != fgets(buffer, MAX_LINE_SZ, fIn)) {

			std::istringstream line(buffer);
			std::string keyword;

			line >> keyword;

			// only care about this keyword
			if ("LUT_3D_SIZE" == keyword) {
				
				// allocate memory for lut and 3d lut data
				if (nullptr == lut) {

					int32_t size(0);
					line >> size;  // size of dimension

					lut = ImagingNew(size);
				}
				// end find keywords, start a new loop for 3D table data
			}
			else if ("+" < keyword && keyword < ":") {
				//lines of table data come after keywords
				break;
			}
		} // end while

		if (nullptr != lut && nullptr != lut->block) {

			static constexpr float const DENORMALIZE = float(UINT16_MAX);

			XMVECTOR const xmDeNorm(_mm_set1_ps(DENORMALIZE));

			uint32_t const lut_size(lut->size);
			uint16_t* const __restrict lut_block = lut->block;

			for (uint32_t b = 0; b < lut_size; ++b) {
				for (uint32_t g = 0; g < lut_size; ++g) {
					for (uint32_t r = 0; r < lut_size; ++r) {			

						std::istringstream line(buffer);
						line.precision(8);

						XMFLOAT4A vColor{};

						line >> vColor.x;
						line >> vColor.y;
						line >> vColor.z;

						XMVECTOR const xmIn = XMVectorMultiply(XMLoadFloat4A(&vColor), xmDeNorm);

						uvec4_t rgba;
						SFM::saturate_to_u16(xmIn, rgba);
						
						// slices ordered by r
						// (b * rMax * gMax) + (g * rMax) + r;
						uint16_t* const lut_color(lut_block + (((b * lut_size * lut_size) + (g * lut_size) + r) << 2));

						lut_color[0] = rgba.r;
						lut_color[1] = rgba.g;
						lut_color[2] = rgba.b;

						//next line
						[[unlikely]]
						if (0 == fgets(buffer, MAX_LINE_SZ, fIn)) {
							// error!
							if ((lut_size - 1) != (b & g & r)) {
								return(nullptr);
							}
							// else eof matching end of cube (ok.)
						}
					}
				}
			}
		}

		fclose(fIn); fIn = nullptr;
	}

	return(lut);
}

void __vectorcall ImagingCopyRaw(void* const pDstMemory, ImagingMemoryInstance const* const __restrict pSrcImage)
{
	uint8_t* __restrict pOut(reinterpret_cast<uint8_t* const __restrict>(pDstMemory));
	uint8_t const * __restrict pIn(pSrcImage->block);
	uint32_t const stride = pSrcImage->linesize;

	int scanline(pSrcImage->ysize);
	while (0 != scanline) {

		memcpy(pOut, pIn, stride);
		pOut += stride; pIn += stride;

		--scanline;
	}
}

void __vectorcall ImagingSwapRB(ImagingMemoryInstance* const __restrict im)
{
	uint32_t const width(im->xsize);
	uint32_t const height(im->ysize);

	uint32_t* const __restrict block(reinterpret_cast<uint32_t* const>(im->block));

	uvec4_t rgba_dst{};

	for (uint32_t y = 0; y < height; ++y) {

		for (uint32_t x = 0; x < width; ++x) {

			uint32_t const pixel(y * width + x);

			SFM::unpack_rgba(block[pixel], rgba_dst.r, rgba_dst.g, rgba_dst.b, rgba_dst.a);
			block[pixel] = SFM::pack_rgba(rgba_dst.b, rgba_dst.g, rgba_dst.r, rgba_dst.a);
		}
	}
}

// dMin, dMax are optional - by default the range is automatically determined by the images data min/max
ImagingMemoryInstance* const __restrict __vectorcall  ImagingF32ToL16(ImagingMemoryInstance const* const __restrict pSrcImageF, double dMin, double dMax)
{
	ImagingMemoryInstance* const __restrict imageL = ImagingNew(eIMAGINGMODE::MODE_L16, pSrcImageF->xsize, pSrcImageF->ysize);

	struct { // avoid lambda heap
		float const* const* const __restrict image_in;
		uint16_t* const* const __restrict    image_out;
		int const size;

	} const p = { (float**)pSrcImageF->image32, (uint16_t**)imageL->image32, imageL->xsize };

	// get the minmax of the float image
	uint32_t const width(pSrcImageF->xsize),
	               height(pSrcImageF->ysize);
	
	for (uint32_t y = 0; y < height; ++y) {

		float const* __restrict pIn(p.image_in[y]);

		for (uint32_t x = 0; x < width; ++x) {

			float const value(*pIn);

			dMin = SFM::min(dMin, (double)value);
			dMax = SFM::max(dMax, (double)value);

			++pIn;
		}
	}

	// perform conversion in double precision
	double const dMini(dMin), dMaxi(dMax);

	tbb::parallel_for(int(0), imageL->ysize, [&p, dMini, dMaxi](int const y) {

		static constexpr double const DENORMALIZE_L16 = double(UINT16_MAX);

		int x = p.size - 1;
		float const* __restrict pIn(p.image_in[y]);
		uint16_t* __restrict pOut(p.image_out[y]);
		do {

			double value = (double)(*pIn);                // perform conversion in double precision

			value = SFM::linearstep(dMini, dMaxi, value); // linearize range to 0.0 ... 1.0

			value = value * DENORMALIZE_L16;              //  0.0 ... 65535.0
			*pOut = SFM::saturate_to_u16(value);          //    0 ... 65535 

			++pOut;
			++pIn;

		} while (--x >= 0);

	});

	return(imageL);
}

ImagingMemoryInstance* const __restrict __vectorcall ImagingLLToLA(ImagingMemoryInstance const* const __restrict pSrcImageL, ImagingMemoryInstance const* const __restrict pSrcImageA)
{
	int const width(pSrcImageL->xsize), height(pSrcImageL->ysize);

	if (width != pSrcImageA->xsize
		|| height != pSrcImageA->ysize) {

		return(nullptr); // assume both L images passed in are the same dimensions.
	}

	Imaging returnLA(ImagingNew(MODE_LA, width, height));

	uint8_t* __restrict pOut = returnLA->block;
	uint8_t const* __restrict pInL = pSrcImageL->block;  // L
	uint8_t const* __restrict pInA = pSrcImageA->block;  // A

	uint32_t const stride = pSrcImageL->linesize; // using checked src dimensions, want iterations in bytes.

	uint32_t scanline(height);
	while (0 != scanline) {

		uint32_t pixels(stride);
		while (0 != pixels) {
		
			*pOut++ = *pInL++;
			*pOut++ = *pInA++;

			pixels -= 2;
		}

		--scanline;
	}

	return(returnLA);
}

ImagingMemoryInstance* const __restrict __vectorcall ImagingL16L16ToLA16(ImagingMemoryInstance const* const __restrict pSrcImageL, ImagingMemoryInstance const* const __restrict pSrcImageA)
{
	int const width(pSrcImageL->xsize), height(pSrcImageL->ysize);

	if (width != pSrcImageA->xsize
		|| height != pSrcImageA->ysize) {

		return(nullptr); // assume both L images passed in are the same dimensions.
	}

	Imaging returnLA16(ImagingNew(MODE_LA16, width, height));

	uint16_t* __restrict pOut = (uint16_t * __restrict)returnLA16->block;
	uint16_t const* __restrict pInL16 = (uint16_t const* __restrict)pSrcImageL->block;  // L
	uint16_t const* __restrict pInA16 = (uint16_t const* __restrict)pSrcImageA->block;  // A

	uint32_t const stride = pSrcImageL->linesize; // using checked src dimensions, want iterations in bytes.

	uint32_t scanline(height);
	while (0 != scanline) {

		uint32_t pixels(stride);
		while (0 != pixels) {

			*pOut++ = *pInL16++;
			*pOut++ = *pInA16++;

			pixels -= 2;
		}

		--scanline;
	}

	return(returnLA16);
}
// residual pixels
STATIC_INLINE_PURE void convertRGBToBGRX(uint8_t* const __restrict dst, uint8_t const* const __restrict src, uint32_t const pixel_count)
{
	// compiler will optimize this simple loop //
	for (uint32_t i = 0; i < pixel_count; ++i) {
		dst[4 * i] = src[3 * i];
		dst[4 * i + 1] = src[3 * i + 1];
		dst[4 * i + 2] = src[3 * i + 2];
		dst[4 * i + 3] = 0xff; // opaque
	}
}
STATIC_INLINE_PURE void convertRGB16ToBGRX16(uint16_t* const __restrict dst, uint16_t const* const __restrict src, uint32_t const pixel_count)
{
	// compiler will optimize this simple loop //
	for (uint32_t i = 0; i < pixel_count; ++i) {
		dst[4 * i] = src[3 * i];
		dst[4 * i + 1] = src[3 * i + 1];
		dst[4 * i + 2] = src[3 * i + 2];
		dst[4 * i + 3] = 0xffff; // opaque
	}
}

void ImagingFastRGBTOBGRX(uint8_t* const __restrict blockOut, uint8_t const* const __restrict blockIn, uint32_t const width, uint32_t const height) // exposed for usage with other buffers, or direct access to two existing Imaging instances' raw data blocks
{
	// each line is done in parallel
	tbb::parallel_for(uint32_t(0), height, [blockOut, blockIn, width](uint32_t const y) {

		uint32_t const row(y * width);
		convertRGBToBGRX(blockOut + row * 4, blockIn + row * 3, width);

	});
}
void ImagingFastRGB16TOBGRX16(uint16_t* const __restrict blockOut, uint16_t const* const __restrict blockIn, uint32_t const width, uint32_t const height)
{
	// each line is done in parallel
	tbb::parallel_for(uint32_t(0), height, [blockOut, blockIn, width](uint32_t const y) {

		uint32_t const row(y * width);
	    convertRGB16ToBGRX16(blockOut + row * 4, blockIn + row * 3, width);

	});
}

ImagingMemoryInstance* const __restrict __vectorcall ImagingRGBToBGRX(ImagingMemoryInstance const* const __restrict pSrcImageRGB)
{
	int const width(pSrcImageRGB->xsize), height(pSrcImageRGB->ysize);

	Imaging returnBGRX(ImagingNew(MODE_BGRX, width, height));

	ImagingFastRGBTOBGRX(returnBGRX->block, pSrcImageRGB->block, width, height);

	return(returnBGRX);
}



ImagingMemoryInstance* const __restrict __vectorcall ImagingRGB16ToBGRX16(ImagingMemoryInstance const* const __restrict pSrcImageRGB16)
{
	int const width(pSrcImageRGB16->xsize), height(pSrcImageRGB16->ysize);

	Imaging returnBGRX16(ImagingNew(MODE_BGRX16, width, height));

	ImagingFastRGB16TOBGRX16(reinterpret_cast<uint16_t* const __restrict>(returnBGRX16->block), reinterpret_cast<uint16_t* const __restrict>(pSrcImageRGB16->block), width, height);

	return(returnBGRX16);
}

bool const __vectorcall ImagingSaveLUT(ImagingLUT const* const __restrict lut, std::string_view const title, std::wstring_view const cubefilenamepath)
{
	bool bReturn(false);

	FILE* fOut;

	if (0 == _wfopen_s(&fOut, cubefilenamepath.data(), L"w"))
	{
		std::string buffer("");

		buffer = fmt::format("TITLE \"{:s}\"\n", title);

		fwrite(buffer.data(), 1, buffer.length(), fOut);

		buffer = fmt::format("# Created by Imaging - supersinfulsilicon software\n");

		fwrite(buffer.data(), 1, buffer.length(), fOut);

		uint32_t const lut_size(lut->size);
		buffer = fmt::format("LUT_3D_SIZE {:d}\n\n", lut_size);

		fwrite(buffer.data(), 1, buffer.length(), fOut);

		static constexpr float const NORMALIZE = 1.0f / float(UINT16_MAX);

		XMVECTOR const xmNorm(_mm_set1_ps(NORMALIZE));

		uint16_t const* const __restrict lut_block = lut->block;

		for (uint32_t b = 0; b < lut_size; ++b) {
			for (uint32_t g = 0; g < lut_size; ++g) {
				for (uint32_t r = 0; r < lut_size; ++r) {

					// slices ordered by r
					// (b * rMax * gMax) + (g * rMax) + r;
					uint16_t const* const lut_color(lut_block + (((b * lut_size * lut_size) + (g * lut_size) + r) << 2));
					
					uvec4_v const rgba(lut_color[0], lut_color[1], lut_color[2], 0);

					XMVECTOR const xmOut = SFM::saturate( XMVectorMultiply(rgba.v4f(), xmNorm) );

					XMFLOAT4A vOut;
					XMStoreFloat4A(&vOut, xmOut);

					buffer = fmt::format("{:.8f} {:.8f} {:.8f}\n", vOut.x, vOut.y, vOut.z);

					fwrite(buffer.data(), 1, buffer.length(), fOut);
				}
			}
		}
		fclose(fOut); fOut = nullptr;
		bReturn = true;
	}
	return(bReturn);
}

bool const __vectorcall ImagingSaveRaw(ImagingMemoryInstance const* const __restrict pSrcImage, std::wstring_view const filenamepath)
{
	bool bReturn(false);

	FILE* fOut;

	if (0 == _wfopen_s(&fOut, filenamepath.data(), L"wbS"))
	{
		uint8_t const*  __restrict pOut = pSrcImage->block;
		uint32_t const stride = pSrcImage->linesize;

		int scanline(pSrcImage->ysize);
		while (0 != scanline) {

			fwrite(pOut, 1, stride, fOut);
			pOut += stride;

			--scanline;
		}

		fclose(fOut); fOut = nullptr;
		bReturn = true;
	}
	return(bReturn);
}

bool const ImagingSaveJPEG(eIMAGINGMODE const outClrSpace, ImagingMemoryInstance const* const __restrict pSrcImage, std::wstring_view const filenamepath)
{
#if INCLUDE_JPEG_SUPPORT
	static constexpr uint32_t const JPEG_QUALITY = 98;
	bool bReleaseSrcImageCopy(false);

	FILE* fOut;

	if (0 == _wfopen_s(&fOut, filenamepath.data(), L"wbS"))
	{
		struct jpeg_compress_struct cinfo;
		struct jpeg_error_mgr jerr;

		// Create JPEG Object
		cinfo.err = jpeg_std_error(&jerr);
		jpeg_create_compress(&cinfo);
		jpeg_stdio_dest(&cinfo, fOut);	// set to output to file
		cinfo.image_width = pSrcImage->xsize;    /* image width and height, in pixels */
		cinfo.image_height = pSrcImage->ysize;

		if (MODE_L == outClrSpace) {
			// use fast 2 gray conversion (linear)
			Imaging rgbCopy(nullptr);
			if (MODE_BGRX == pSrcImage->mode) {
				rgbCopy = ImagingNew(MODE_RGB, cinfo.image_width, cinfo.image_height);
				Parallel_BGRX2BGR(rgbCopy->image, pSrcImage->image, cinfo.image_width, cinfo.image_height);
			}
			else if (MODE_RGB == pSrcImage->mode) {
				rgbCopy = ImagingCopy(pSrcImage);
			}

			if (nullptr != rgbCopy) {
				Imaging grayImage = ImagingNew(MODE_L, cinfo.image_width, cinfo.image_height);
				Fast2Gray::rgb2gray_ToLinear(rgbCopy->block, cinfo.image_width, cinfo.image_height,
											 grayImage->block);
				// override
				const_cast<ImagingMemoryInstance const* __restrict&>(pSrcImage) = grayImage;
				bReleaseSrcImageCopy = true;

				ImagingDelete(rgbCopy);
			}
			else
				return(false); // unsupported
		}
										// Configure JPEG Compression
		
		cinfo.input_components = pSrcImage->pixelsize;				/* # of color components per pixel */
		/* colorspace of input image */
		switch (pSrcImage->mode)
		{
		case MODE_1BIT:
		case MODE_L:
			cinfo.in_color_space = JCS_GRAYSCALE;
			break;
		case MODE_RGB:
			cinfo.in_color_space = JCS_RGB;
			break;
		case MODE_BGRX:
			cinfo.in_color_space = JCS_EXT_BGRX;
			break;
		case MODE_U32:
		case MODE_BGRA:
			cinfo.in_color_space = JCS_EXT_BGRA;
			break;
		}
		 
		jpeg_set_defaults(&cinfo); // dependent on colorspace first being set

		jpeg_set_quality(&cinfo, JPEG_QUALITY, TRUE /* limit to baseline-JPEG values */);

		cinfo.write_Adobe_marker = FALSE;
		cinfo.arith_code = FALSE;
		cinfo.dct_method = JDCT_FLOAT;
		cinfo.progressive_mode = FALSE;
		cinfo.optimize_coding = TRUE;
		cinfo.smoothing_factor = 0;
		
		/* colorspace of output image */
		switch (outClrSpace)
		{
		case MODE_1BIT:
		case MODE_L:
			cinfo.jpeg_color_space = JCS_GRAYSCALE;
			cinfo.num_components = 1;
			break;
		case MODE_RGB:
			cinfo.jpeg_color_space = JCS_RGB;
			cinfo.num_components = 3;
			break;
		case MODE_BGRX:
			cinfo.jpeg_color_space = JCS_EXT_BGRX;
			cinfo.num_components = 4;
			break;
		case MODE_U32:
		case MODE_BGRA:
			cinfo.jpeg_color_space = JCS_EXT_BGRA;
			cinfo.num_components = 4;
			break;
		}

		// Start Compression //
		jpeg_start_compress(&cinfo, TRUE);

		//JSAMPROW row_pointer(pSrcImage->block);      /* pointer to JSAMPLE row[s] */
		//int const row_stride(cinfo.image_width * cinfo.input_components);  /* physical row width in image buffer */
									  /* JSAMPLEs per row in image_buffer */
		//cinfo.next_scanline = 0;
		//while (cinfo.next_scanline < cinfo.image_height) {
			/* jpeg_write_scanlines expects an array of pointers to scanlines.
			* Here the array is only one element long, but you could pass
			* more than one scanline at a time if that's more convenient.
			*/
		//	(void)jpeg_write_scanlines(&cinfo, &row_pointer, 1);
		//	row_pointer += row_stride;
		//}

		// feed the compressor scanlines
		uint8_t* const* __restrict ppOut = pSrcImage->image;

		/* jpeg_write_scanlines expects an array of pointers to scanlines.
		* Here the array is only one element long, but you could pass
		* more than one scanline at a time if that's more convenient.
		*/
		(void)jpeg_write_scanlines(&cinfo, (JSAMPARRAY)ppOut, cinfo.image_height);

		// Finish compression
		jpeg_finish_compress(&cinfo);
		fflush(fOut);

		fclose(fOut); fOut = nullptr;

		// Release memory
		jpeg_destroy_compress(&cinfo);

		if (bReleaseSrcImageCopy) {
			ImagingDelete(const_cast<ImagingMemoryInstance* __restrict&>(pSrcImage)); // lazy but works
		}
		return(true);
	}

#endif

	return(false);
}

void ImagingSaveJPEG(tbb::task_group& __restrict tG, eIMAGINGMODE const outClrSpace, ImagingMemoryInstance const* const __restrict pSrcImage, std::wstring_view const filenamepath)
{
#if INCLUDE_JPEG_SUPPORT
	wchar_t* threadLocalPathFile = new wchar_t[filenamepath.length() + 1];
	
	filenamepath.copy(threadLocalPathFile, filenamepath.length());

	// make a quick copy just to bullet proof (a little)
	Imaging cpyImage = ImagingCopy(pSrcImage);

	// async non blocking file export
	tG.run([threadLocalPathFile, cpyImage, outClrSpace]
	{
		ImagingSaveJPEG(outClrSpace, cpyImage, threadLocalPathFile);

		ImagingDelete(cpyImage);

		if (threadLocalPathFile) {
			delete[] threadLocalPathFile;
		}
	});
#endif
}

#if INCLUDE_TIF_SUPPORT
bool const __vectorcall ImagingSaveToTif(ImagingMemoryInstance const* const __restrict pSrcImage, std::wstring_view const filenamepath)						// single image //
{
	bool bReturn(false);
	if (((MODE_L | MODE_LA | MODE_L16 | MODE_LA16 | MODE_BGRX | MODE_BGRA | MODE_BGRX16 | MODE_BGRA16) & pSrcImage->mode))
	{
		uint32_t samples(0),    // # of components
			     bits(0),       // bitdepth of a component
			     interpret(0);  // TinyTIFFWriter_Greyscale, etc

		samples = pSrcImage->bands; // samples / bands / components

		switch (pSrcImage->mode)
		{
		case MODE_L:
			interpret = TinyTIFFWriter_Greyscale;
			bits = 8;
			break;
		case MODE_LA:
			interpret = TinyTIFFWriter_GreyscaleAndAlpha;
			bits = 8;
			break;
		case MODE_L16:
			interpret = TinyTIFFWriter_Greyscale;
			bits = 16;
			break;
		case MODE_LA16:
			interpret = TinyTIFFWriter_GreyscaleAndAlpha;
			bits = 16;
			break;
		case MODE_BGRX16:
		case MODE_BGRA16:
			interpret = TinyTIFFWriter_RGBA;
			bits = 16;
			break;
		case MODE_BGRX:
		case MODE_BGRA:
		default:
			interpret = TinyTIFFWriter_RGBA;
			bits = 8;
			break;
		}

		// TINYTIFF_EXPORT TinyTIFFWriterFile* TinyTIFFWriter_open(const char* filename, uint16_t bitsPerSample, enum TinyTIFFWriterSampleFormat sampleFormat, uint16_t samples, uint32_t width, uint32_t height, enum TinyTIFFWriterSampleInterpretation sampleInterpretation);

		TinyTIFFWriterFile* tif = TinyTIFFWriter_open(stringconv::ws2s(filenamepath).c_str(), bits, TinyTIFFWriter_UInt, samples, pSrcImage->xsize, pSrcImage->ysize, (TinyTIFFWriterSampleInterpretation)interpret);
		if (tif) {

			TinyTIFFWriter_writeImage(tif, pSrcImage->block);

			TinyTIFFWriter_close(tif); tif = nullptr;

			bReturn = true;
		}
	}

	return(bReturn);
}
#endif

struct KtxHeader {
	uint8_t identifier[12];
	uint32_t endianness;
	uint32_t glType;
	uint32_t glTypeSize;
	uint32_t glFormat;
	uint32_t glInternalFormat;
	uint32_t glBaseInternalFormat;
	uint32_t pixelWidth;
	uint32_t pixelHeight;
	uint32_t pixelDepth;
	uint32_t numberOfArrayElements;
	uint32_t numberOfFaces;
	uint32_t numberOfMipmapLevels;
	uint32_t bytesOfKeyValueData;
};

bool const __vectorcall ImagingSaveToKTX(ImagingMemoryInstance const* const __restrict pSrcImage, std::wstring_view const filenamepath) // RGB images should be converted to BGRX first
{
	bool bReturn(false);

	if (MODE_BC7 == pSrcImage->mode) {
		return(ImagingSaveCompressedBC7ToKTX(pSrcImage, filenamepath));
	}

	// supporting L8, LA8 and RGBA8

	//internal format
	static constexpr uint32_t const KTX_R8 = 0x8229;
	static constexpr uint32_t const KTX_RG8 = 0x822B;
	static constexpr uint32_t const KTX_R16 = 0x822A;
	static constexpr uint32_t const KTX_RG16 = 0x822C;
	static constexpr uint32_t const KTX_RGBA8 = 0x8058;
	static constexpr uint32_t const KTX_RGBA16 = 0x805B;

	//base internal format
	static constexpr uint32_t const KTX__R = 0x1903;
	static constexpr uint32_t const KTX__RG = 0x8227;
	static constexpr uint32_t const KTX__RGBA = 0x1908;
	
	static constexpr uint32_t const KTX_UNSIGNED_SHORT = 0x1403;
	static constexpr uint32_t const KTX_UNSIGNED_BYTE = 0x1401;
	static constexpr uint32_t const KTX_ENDIAN_REF(0x04030201);

	static constexpr const uint8_t ktx_magic_id[12] = {
		0xAB, 0x4B, 0x54, 0x58,
		0x20, 0x31, 0x31, 0xBB,
		0x0D, 0x0A, 0x1A, 0x0A
	};

	FILE* fOut;

	if (((MODE_L|MODE_LA|MODE_L16|MODE_LA16|MODE_BGRX|MODE_BGRA|MODE_BGRX16|MODE_BGRA16) & pSrcImage->mode) && 0 == _wfopen_s(&fOut, filenamepath.data(), L"wbS"))
	{
		KtxHeader header = {};
		memcpy(header.identifier, ktx_magic_id, 12);

		header.endianness = KTX_ENDIAN_REF;

		if ((MODE_L16 | MODE_LA16 | MODE_BGRX16 | MODE_BGRA16) & pSrcImage->mode)
			header.glType = KTX_UNSIGNED_SHORT; //  For compressed formats, type=0.
		else
			header.glType = KTX_UNSIGNED_BYTE; //  For compressed formats, type=0.

		header.glTypeSize = pSrcImage->pixelsize;

		switch (pSrcImage->mode)
		{
		case MODE_L:
			header.glFormat = header.glBaseInternalFormat = KTX__R;
			header.glInternalFormat = KTX_R8;
			break;
		case MODE_LA:
			header.glFormat = header.glBaseInternalFormat = KTX__RG;
			header.glInternalFormat = KTX_RG8;
			break;
		case MODE_L16:
			header.glFormat = header.glBaseInternalFormat = KTX__R;
			header.glInternalFormat = KTX_R16;
			break;
		case MODE_LA16:
			header.glFormat = header.glBaseInternalFormat = KTX__RG;
			header.glInternalFormat = KTX_RG16;
			break;
		case MODE_BGRA16:
		case MODE_BGRX16:
			header.glFormat = header.glBaseInternalFormat = KTX__RGBA;
			header.glInternalFormat = KTX_RGBA16;
			break;
		case MODE_BGRA:
		case MODE_BGRX:
		default:
			header.glFormat = header.glBaseInternalFormat = KTX__RGBA;
			header.glInternalFormat = KTX_RGBA8;
			break;
		}

		header.pixelWidth = pSrcImage->xsize;
		header.pixelHeight = pSrcImage->ysize;
		header.pixelDepth = 0; // must be 0 for 2D/cubemap textures
		// KTX spec says this field must be 0 for non-array textures
		header.numberOfArrayElements = 0;
		header.numberOfFaces = 1;
		header.numberOfMipmapLevels = 1;
		header.bytesOfKeyValueData = 0;

		// write header
		fwrite(&header, sizeof(KtxHeader), 1, fOut);

		// write size of data
		uint32_t const dataSize(pSrcImage->xsize * pSrcImage->ysize * pSrcImage->pixelsize);
		fwrite(&dataSize, sizeof(uint32_t), 1, fOut);

		// write data
		fwrite(&(*pSrcImage->block), dataSize, 1, fOut);

		fflush(fOut);
		fclose(fOut); fOut = nullptr;
		bReturn = true;
	}
	return(bReturn);
}

bool const __vectorcall ImagingSaveLayersToKTX(ImagingMemoryInstance const* const* const __restrict pSrcImages, uint32_t const numLayers, std::wstring_view const filenamepath) // RGB images should be converted to BGRX first
{
	bool bReturn(false);

	if (numLayers <= 1) {
		return(false);
	}

	// supporting L8, LA8 and RGBA8, all images for layers must be equal in type and dimensions

	//internal format
	static constexpr uint32_t const KTX_R8 = 0x8229;
	static constexpr uint32_t const KTX_RG8 = 0x822B;
	static constexpr uint32_t const KTX_R16 = 0x822A;
	static constexpr uint32_t const KTX_RG16 = 0x822C;
	static constexpr uint32_t const KTX_RGBA8 = 0x8058;
	static constexpr uint32_t const KTX_RGBA16 = 0x805B;

	//base internal format
	static constexpr uint32_t const KTX__R = 0x1903;
	static constexpr uint32_t const KTX__RG = 0x8227;
	static constexpr uint32_t const KTX__RGBA = 0x1908;

	static constexpr uint32_t const KTX_UNSIGNED_SHORT = 0x1403;
	static constexpr uint32_t const KTX_UNSIGNED_BYTE = 0x1401;
	static constexpr uint32_t const KTX_ENDIAN_REF(0x04030201);

	static constexpr const uint8_t ktx_magic_id[12] = {
		0xAB, 0x4B, 0x54, 0x58,
		0x20, 0x31, 0x31, 0xBB,
		0x0D, 0x0A, 0x1A, 0x0A
	};

	FILE* fOut;
	KtxHeader header = {};
	memcpy(header.identifier, ktx_magic_id, 12);

	if (((MODE_L | MODE_LA | MODE_L16 | MODE_LA16 | MODE_BGRX | MODE_BGRA | MODE_BGRX16 | MODE_BGRA16) & pSrcImages[0]->mode))
	{
		header.endianness = KTX_ENDIAN_REF;

		if ((MODE_L16 | MODE_LA16 | MODE_BGRX16 | MODE_BGRA16) & pSrcImages[0]->mode)
			header.glType = KTX_UNSIGNED_SHORT; //  For compressed formats, type=0.
		else
			header.glType = KTX_UNSIGNED_BYTE; //  For compressed formats, type=0.

		header.glTypeSize = pSrcImages[0]->pixelsize;

		switch (pSrcImages[0]->mode)
		{
		case MODE_L:
			header.glFormat = header.glBaseInternalFormat = KTX__R;
			header.glInternalFormat = KTX_R8;
			break;
		case MODE_LA:
			header.glFormat = header.glBaseInternalFormat = KTX__RG;
			header.glInternalFormat = KTX_RG8;
			break;
		case MODE_L16:
			header.glFormat = header.glBaseInternalFormat = KTX__R;
			header.glInternalFormat = KTX_R16;
			break;
		case MODE_LA16:
			header.glFormat = header.glBaseInternalFormat = KTX__RG;
			header.glInternalFormat = KTX_RG16;
			break;
		case MODE_BGRA16:
		case MODE_BGRX16:
			header.glFormat = header.glBaseInternalFormat = KTX__RGBA;
			header.glInternalFormat = KTX_RGBA16;
			break;
		case MODE_BGRA:
		case MODE_BGRX:
		default:
			header.glFormat = header.glBaseInternalFormat = KTX__RGBA;
			header.glInternalFormat = KTX_RGBA8;
			break;
		}

		header.pixelWidth = pSrcImages[0]->xsize;
		header.pixelHeight = pSrcImages[0]->ysize;
		header.pixelDepth = 0; // must be 0 for 2D/cubemap textures
		// KTX spec says this field must be 0 for non-array textures
		header.numberOfArrayElements = numLayers;
		header.numberOfFaces = 1;
		header.numberOfMipmapLevels = 1;
		header.bytesOfKeyValueData = 0;
	}
	else if (MODE_BC7 == pSrcImages[0]->mode)
	{
		return(false); // todo when needed

	}
	else
		return(false);

	if (0 == _wfopen_s(&fOut, filenamepath.data(), L"wbS"))
	{
		// write header
		fwrite(&header, sizeof(KtxHeader), 1, fOut);

		// write size of data
		uint32_t const layerDataSize(pSrcImages[0]->xsize * pSrcImages[0]->ysize * pSrcImages[0]->pixelsize);
		fwrite(&layerDataSize, sizeof(uint32_t), 1, fOut);

		// write data
		for (uint32_t layer = 0; layer < numLayers; ++layer) {
			fwrite(&(*pSrcImages[layer]->block), layerDataSize, 1, fOut);
		}

		fflush(fOut);
		fclose(fOut); fOut = nullptr;
		bReturn = true;
	}
	return(bReturn);
}

static constexpr uint32_t const
BLOCK_WIDTH = 4,
BLOCK_HEIGHT = BLOCK_WIDTH,
BLOCK_DEPTH = 1;

bool const __vectorcall ImagingSaveCompressedBC7ToKTX(ImagingMemoryInstance const* const __restrict pSrcImage, std::wstring_view const filenamepath)
{
	bool bReturn(false);

	static constexpr uint32_t const KTX_COMPRESSED_RGBA_BPTC_UNORM_ARB = 0x8E8C;
	static constexpr uint32_t const KTX__RGBA = 0x1908;
	static constexpr uint32_t const KTX_UNSIGNED_BYTE = 0x1401;
	static constexpr uint32_t const KTX_ENDIAN_REF(0x04030201);
	
	static constexpr const uint8_t ktx_magic_id[12] = {
		0xAB, 0x4B, 0x54, 0x58,
		0x20, 0x31, 0x31, 0xBB,
		0x0D, 0x0A, 0x1A, 0x0A
	};

	FILE* fOut;

	if (MODE_BC7 == pSrcImage->mode && 0 == _wfopen_s(&fOut, filenamepath.data(), L"wbS"))
	{
		KtxHeader header = {};
		memcpy(header.identifier, ktx_magic_id, 12);

		header.endianness = KTX_ENDIAN_REF;
		header.glType = 0; //  For compressed formats, type=0.
		header.glTypeSize = 1; // or endianness conversion. for compressed types, size=1
		header.glFormat = 0; // For compressed formats, format=0
		header.glInternalFormat = KTX_COMPRESSED_RGBA_BPTC_UNORM_ARB;
		header.glBaseInternalFormat = KTX__RGBA;
		header.pixelWidth = pSrcImage->xsize;
		header.pixelHeight = pSrcImage->ysize;
		header.pixelDepth = 0; // must be 0 for 2D/cubemap textures
		// KTX spec says this field must be 0 for non-array textures
		header.numberOfArrayElements = 0;
		header.numberOfFaces = 1;
		header.numberOfMipmapLevels = 1;
		header.bytesOfKeyValueData = 0;

		// write header
		fwrite(&header, sizeof(KtxHeader), 1, fOut);

		// write size of compressed data
		uint32_t dataCompressedSize(0);
		{
			uint32_t const num_blocks = (header.pixelWidth / BLOCK_WIDTH) * (header.pixelHeight / BLOCK_HEIGHT);  // BLOCK_DEPTH == 1
			dataCompressedSize = num_blocks * BC_BLOCK_BYTES;

			fwrite(&dataCompressedSize, sizeof(uint32_t), 1, fOut);
		}

		fwrite(&(*pSrcImage->block), dataCompressedSize, 1, fOut);
		fflush(fOut);
		fclose(fOut); fOut = nullptr;
		bReturn = true;
	}
	return(bReturn);
}

// Isolated Dynamic DLL Compressonator.dll //
// Loaded dynamically at run time, to isolate and shutdown thread usage //
typedef BC_ERROR (CMP_API * const PROC_CMP_InitializeBCLibrary)(void);

typedef CMP_DWORD (CMP_API * const PROC_CMP_CalculateBufferSize)(const CMP_Texture* pTexture);

typedef BC_ERROR (CMP_API * const PROC_CMP_ShutdownBCLibrary)(void);

typedef CMP_ERROR (CMP_API * const PROC_CMP_ConvertTexture)(CMP_Texture* pSourceTexture, CMP_Texture* pDestTexture, const CMP_CompressOptions* pOptions,
	CMP_Feedback_Proc pFeedbackProc, CMP_DWORD_PTR pUser1, CMP_DWORD_PTR pUser2);

ImagingMemoryInstance* const __restrict __vectorcall ImagingCompressBGRAToBC7(ImagingMemoryInstance const* const __restrict pSrcImage, float const normalizedQuality)
{
	ImagingMemoryInstance* __restrict imgReturn(nullptr);

	HINSTANCE hinstLib(LoadLibrary(L"Compressonator.dll"));

	if (nullptr != hinstLib) {

		PROC_CMP_InitializeBCLibrary	InitializeBCLibrary( (PROC_CMP_InitializeBCLibrary)GetProcAddress(hinstLib, "CMP_InitializeBCLibrary") );
		PROC_CMP_CalculateBufferSize	CalculateBufferSize((PROC_CMP_CalculateBufferSize)GetProcAddress(hinstLib, "CMP_CalculateBufferSize"));
		PROC_CMP_ShutdownBCLibrary		ShutdownBCLibrary((PROC_CMP_ShutdownBCLibrary)GetProcAddress(hinstLib, "CMP_ShutdownBCLibrary"));
		PROC_CMP_ConvertTexture			ConvertTexture((PROC_CMP_ConvertTexture)GetProcAddress(hinstLib, "CMP_ConvertTexture"));

		if (nullptr != InitializeBCLibrary && nullptr != CalculateBufferSize && nullptr != ShutdownBCLibrary && nullptr != ConvertTexture) {

			InitializeBCLibrary();

			CMP_Texture srcTexture;

			srcTexture.dwSize = sizeof(CMP_Texture);
			srcTexture.dwWidth = pSrcImage->xsize;
			srcTexture.dwHeight = pSrcImage->ysize;
			srcTexture.dwPitch = pSrcImage->linesize;

			srcTexture.dwDataSize = pSrcImage->pixelsize * pSrcImage->xsize * pSrcImage->ysize;
			srcTexture.pData = pSrcImage->block;
			srcTexture.format = CMP_FORMAT_BGRA_8888;

			//===================================
			// Initialize Compressed Destination
			//===================================
			CMP_Texture destTexture;
			destTexture.dwSize = sizeof(destTexture);

			destTexture.dwWidth = srcTexture.dwWidth;
			destTexture.dwHeight = srcTexture.dwHeight;
			destTexture.dwPitch = 0;
			destTexture.format = CMP_FORMAT_BC7;
			destTexture.dwDataSize = CalculateBufferSize(&destTexture);

			imgReturn = ImagingNewCompressed(MODE_BC7, destTexture.dwWidth, destTexture.dwHeight, destTexture.dwDataSize);
			
			if (nullptr != imgReturn) {

				destTexture.pData = (CMP_BYTE*)imgReturn->block;
				destTexture.nBlockWidth = BLOCK_WIDTH;
				destTexture.nBlockHeight = BLOCK_HEIGHT;
				destTexture.nBlockDepth = BLOCK_DEPTH;

				//==========================
				// Set Compression Options
				//==========================
				CMP_CompressOptions options = { 0 };
				options.dwSize = sizeof(options);
				options.fquality = normalizedQuality;
				options.dwnumThreads = 1;
				options.bDisableMultiThreading = true;  // compressonator libray spawns multiple threads that stick around eatring resourcfes of the main application

				//==========================
				// Compress Texture
				//==========================
				CMP_ERROR cmp_status = ConvertTexture(&srcTexture, &destTexture, &options, NULL, NULL, NULL);
				if (CMP_OK != cmp_status)
				{
					ImagingDelete(imgReturn); imgReturn = nullptr;
				}
			}
			ShutdownBCLibrary();
		}
		FreeLibrary(hinstLib);
		CoFreeUnusedLibrariesEx(0, 0); // actually unloads the dll from memory immediately if ShutdownBCLibrary has been called first or InitializeBCLibrary was never called
	}

	return(imgReturn);
}

ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadKTX(std::wstring_view const filenamepath)
{
	static constexpr wchar_t const* const EXTENSION_KTX1 = L".ktx";
	static constexpr wchar_t const* const EXTENSION_KTX2 = L".ktx2";

	fs::path const filename(filenamepath);

	uint32_t version(0);

	if (fs::exists(filename)) {

		if (filename.extension() == EXTENSION_KTX1) {
			version = KTX_VERSION::KTX1;
		}
		else if (filename.extension() == EXTENSION_KTX2) {
			version = KTX_VERSION::KTX2;
		}
	}
	else {
		return(nullptr);
	}

	if (0 != version) {
		std::error_code error{};

		mio::mmap_source mmap = mio::make_mmap_source(filenamepath, false, error);
		if (!error) {

			if (mmap.is_open() && mmap.is_mapped()) {
				___prefetch_vmem(mmap.data(), mmap.size());

				uint8_t const* const pReadPointer((uint8_t*)mmap.data());
				
				if (KTX_VERSION::KTX1 == version) {
					KTXFileLayout<KTX_VERSION::KTX1> const ktxFile(pReadPointer, pReadPointer + mmap.length());

					if (ktxFile.ok()) {

						uint32_t const baseOffset(ktxFile.offset(0, 0, 0));
						return(ktxFile.upload(pReadPointer + baseOffset));
					}
				}
				else {
					KTXFileLayout<KTX_VERSION::KTX2> const ktx2File(pReadPointer, pReadPointer + mmap.length());

					if (ktx2File.ok()) {

						uint32_t const baseOffset(ktx2File.offset(0, 0, 0));
						return(ktx2File.upload(pReadPointer + baseOffset));
					}
				}
			}

		}
	}

	return(nullptr);
}