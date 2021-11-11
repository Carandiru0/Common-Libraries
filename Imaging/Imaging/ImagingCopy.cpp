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

#if INCLUDE_PNG_SUPPORT
#include "lodepng.h"
#endif

#if INCLUDE_JPEG_SUPPORT
#include "jpeglib.h"
#endif
#include <Compressonator.h>
#include <tbb/scalable_allocator.h>
#include <Utility/mio/mmap.hpp>
#include <Utility/mem.h>

/* --------------------------------------------------------------------
* Standard image object.
*/
static constexpr int64_t const IMAGE_SIZE_THRESHOLD(16384ull * 16384ull + 1ull);	// max loaded file image size, resampling can only goto a maximum of 16384x16384
static constexpr int32_t const MAX_LUT_DIMENSION_SIZE(65 + 1);						// maximum lut dimension n x n x n

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
	case MODE_LA:
		/* 8-bit greyscale (luminance) with alpha */
		im->bands = im->pixelsize = 2;
		break;
	case MODE_I:
		/* 32-bit integer images */
		im->bands = 1;
		im->pixelsize = 4;
		im->type = IMAGING_TYPE_INT32;
		break;
	case MODE_F:
		/* 32-bit floating point images */
		im->bands = 1;
		im->pixelsize = 4;
		im->type = IMAGING_TYPE_FLOAT32;
		break;
	case MODE_RGB:
		/* 24-bit true colour images */
		im->bands = 3;
		im->pixelsize = 3;
		break;
	case MODE_BGRX:
		/* 32-bit true colour images with padding */
		im->bands = 3;
		im->pixelsize = 4;
		break;
	case MODE_BGRA:
		/* 32-bit true colour images with alpha */
		im->bands = im->pixelsize = 4;
		break;
	case MODE_CMYK:
		/* 32-bit colour separation */
		im->bands = im->pixelsize = 4;
		break;
	case MODE_YCbCr:
		/* 24-bit video format */
		im->bands = 3;
		im->pixelsize = 4;
		break;
	case MODE_LAB:
		/* 24-bit color, luminance, + 2 color channels */
		/* L is uint8, a,b are int8 */
		im->bands = 3;
		im->pixelsize = 4;
		break;
	case MODE_HSV:
		/* 24-bit color, luminance, + 2 color channels */
		/* L is uint8, a,b are int8 */
		im->bands = 3;
		im->pixelsize = 4;
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
	case 1: case 2: case 3:
		im->image8 = (uint8_t **)im->image;
		break;
	case 4:
		im->image32 = (int32_t **)im->image;
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

static ImagingMemoryInstance* const __restrict __vectorcall
ImagingNewBlock(eIMAGINGMODE const mode, int const xsize, int const ysize, int const BufferSize = 0)
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
		im->ysize > INT_MAX / im->linesize) {
		/* punt if we're going to overflow */
		return NULL;
	}

	if (im->ysize * im->linesize <= 0) {
		/* some platforms return NULL for malloc(0); this fix
		prevents MemoryError on zero-sized images on such
		platforms */
		im->block = (uint8_t *)scalable_malloc(1);
	}
	else {
		/* malloc check ok, overflow check above */
		if (0 != BufferSize) {
			im->block = (uint8_t *)scalable_malloc(BufferSize);

			// alias pointers are only valid for buffersize that is 
			// composed of stritcly ysize * linesize = blocksize
			// ie.) Compressed BC7 & BC6A do not have alias pointers
			scalable_free(im->image);

			im->image = nullptr;
			im->image8 = nullptr;
			im->image32 = nullptr;
		}
		else {
			im->block = (uint8_t *)scalable_malloc(im->ysize * im->linesize);

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

	if ((int64_t)xsize * (int64_t)ysize < IMAGE_SIZE_THRESHOLD) {
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

	if ((int64_t)xsize * (int64_t)ysize < IMAGE_SIZE_THRESHOLD) {
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

static ImagingMemoryInstance* const __restrict __vectorcall
_copy(ImagingMemoryInstance const * const __restrict imIn)
{
    if (!imIn)
	return (Imaging) ImagingError_ValueError(NULL);

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
	uint32_t const height(im->ysize);
	uint32_t const width(im->xsize);

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

void __vectorcall ImagingVerticalFlip(ImagingMemoryInstance* const __restrict im) // flip Y / invert Y axis / vertical flip (INPLACE
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
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadRawBGRA(std::wstring_view const filenamepath, int const width, int const height)
{
	return(ImagingLoadRaw(MODE_BGRA, filenamepath, width, height));
}
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadRawL(std::wstring_view const filenamepath, int const width, int const height)
{
	return(ImagingLoadRaw(MODE_L, filenamepath, width, height));
}
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadRawLA(std::wstring_view const filenamepath, int const width, int const height)
{
	return(ImagingLoadRaw(MODE_LA, filenamepath, width, height));
}
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadFromMemoryBGRA(uint8_t const* __restrict pMemLoad, int const width, int const height)
{
	Imaging returnL(ImagingNew(MODE_BGRA, width, height));

	uint8_t*  __restrict pOut = returnL->block;
	uint32_t const stride = returnL->linesize;

	uint32_t scanline(height);
	while ( 0 != scanline ) {

		memcpy(pOut, pMemLoad, stride);
		pMemLoad += stride;
		pOut += stride;

		--scanline;
	}

	return(returnL);
}

ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadFromMemoryLA(uint8_t const* __restrict pMemLoad, int const width, int const height)
{
	Imaging returnL(ImagingNew(MODE_LA, width, height));

	uint8_t* __restrict pOut = returnL->block;
	uint32_t const stride = returnL->linesize;

	uint32_t scanline(height);
	while (0 != scanline) {

		memcpy(pOut, pMemLoad, stride);
		pMemLoad += stride;
		pOut += stride;

		--scanline;
	}

	return(returnL);
}

ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadFromMemoryL(uint8_t const* __restrict pMemLoad, int const width, int const height)
{
	Imaging returnL(ImagingNew(MODE_L, width, height));

	uint8_t* __restrict pOut = returnL->block;
	uint32_t const stride = returnL->linesize;

	uint32_t scanline(height);
	while (0 != scanline) {

		memcpy(pOut, pMemLoad, stride);
		pMemLoad += stride;
		pOut += stride;

		--scanline;
	}

	return(returnL);
}

#if INCLUDE_PNG_SUPPORT
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadPNG(std::wstring_view const filenamepath)
{
	Imaging returnL(nullptr);

	FILE* fIn;

	if (0 == _wfopen_s(&fIn, filenamepath.data(), L"rbS"))
	{
		if (0 == _fseek_nolock(fIn, 0, SEEK_END) != 0)
		{
			long const size = _ftell_nolock(fIn);

			if (size > 0 && size != LONG_MAX) {
				if (0 == _fseek_nolock(fIn, 0, 0) != 0) { // back to start

					size_t const buffer_size((size_t)size);
					uint8_t* buffer(nullptr);
					buffer = (uint8_t*)scalable_malloc((size_t)buffer_size);

					size_t const read_size = _fread_nolock_s(buffer, buffer_size, 1, buffer_size, fIn);
					
					if (read_size == buffer_size) {

						uint8_t* out(nullptr);
						uint32_t w(0), h(0);
						if (0 == lodepng_decode_memory(&out, &w, &h, buffer, buffer_size, LCT_RGBA, 8)) {

							returnL = ImagingNew(MODE_BGRA, w, h);
							if (nullptr != returnL) {
								memcpy(returnL->block, out, size_t(w) * size_t(h) * sizeof(uint32_t));
							}
						}

						if (out) {
							lodepng_free(out); out = nullptr;
						}
					}

					if (buffer) {
						scalable_free(buffer); buffer = nullptr;
					}
				}
			}
		}

		_fclose_nolock(fIn); fIn = nullptr;
	}
	return(returnL);
}
#endif

ImagingSequence* const __restrict __vectorcall
ImagingResample(ImagingSequence const* const __restrict imIn, int const xsize, int const ysize, int const filter)
{
	ImagingSequence* imOut(nullptr);

	imOut = (ImagingSequence*)scalable_malloc(1 * sizeof(ImagingSequence));
	memset(&(*imOut), 0, sizeof(ImagingSequence));

	/* Setup ouput image descriptor */
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
	Imaging			 image = nullptr;

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
		if (meta.delay == 0)
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
		bBlend = true;
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
		if (0 != frame) {
			bBlend = true;
		}
		break;
	
	}

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
				width = im->xsize - 1;
			if ( 0 == height )
				height = im->ysize - 1;

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
							 imagesize(height*linesize);

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
	}

	return(lut);
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

	if (0 == _wfopen_s(&fOut, filenamepath.data(), L"wb"))
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

	if (0 == _wfopen_s(&fOut, filenamepath.data(), L"wb"))
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
		case MODE_I:
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
		case MODE_I:
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

#if INCLUDE_PNG_SUPPORT
bool const __vectorcall ImagingSaveToPNG(ImagingMemoryInstance const* const __restrict pSrcImage, std::wstring_view const filenamepath)						// single image //
{
	bool bReturn(false);

	if (((MODE_L | MODE_LA | MODE_BGRX | MODE_BGRA) & pSrcImage->mode))
	{
		LodePNGColorType colortype;

		switch (pSrcImage->mode)
		{
		case MODE_L:
			colortype = LCT_GREY;
				break;
		case MODE_LA:
			colortype = LCT_GREY_ALPHA;
				break;
		case MODE_BGRX:
		case MODE_BGRA:
		default:
			colortype = LCT_RGBA;
			break;
		}

		unsigned char* buffer(nullptr);
		size_t buffersize(0);
		if (0 == lodepng_encode_memory(&buffer, &buffersize, pSrcImage->block, pSrcImage->xsize, pSrcImage->ysize, colortype, 8) && 0 != buffersize) {

			FILE* fOut;

			if (0 == _wfopen_s(&fOut, filenamepath.data(), L"wb"))
			{
				// write data
				fwrite(&(*buffer), buffersize, 1, fOut);

				fflush(fOut);
				fclose(fOut); fOut = nullptr;
				bReturn = true;
			}
		}
		if (buffer) {
			lodepng_free(buffer); buffer = nullptr;
		}
	}

	return(bReturn);
}

bool const __vectorcall ImagingSaveToPNG(ImagingMemoryInstance const* __restrict const* const __restrict pSrcImage, uint32_t const image_count, std::wstring_view const filenamepath)		// sequence/array images //
{
	std::atomic_bool bReturn(true);

	std::wstring const filenamepath_noext(filenamepath.substr(0, filenamepath.find_last_of('.')));

	tbb::parallel_for(uint32_t(0), image_count, [&](uint32_t const image) {

		if (!bReturn) // skip parallel task if one task has already failed
			return;

		std::wstring szFilename(L"");

		if (image < 10) {
			szFilename = fmt::format(L"{:s}00{:d}.png", filenamepath_noext, image);
		}
		else if (image < 100) {
			szFilename = fmt::format(L"{:s}0{:d}.png", filenamepath_noext, image);
		}
		else {
			szFilename = fmt::format(L"{:s}{:d}.png", filenamepath_noext, image);
		}

		bReturn = ImagingSaveToPNG(pSrcImage[image], szFilename);
	});

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

bool const __vectorcall ImagingSaveToKTX(ImagingMemoryInstance const* const __restrict pSrcImage, std::wstring_view const filenamepath)
{
	bool bReturn(false);

	if (MODE_BC7 == pSrcImage->mode) {
		return(ImagingSaveCompressedBC7ToKTX(pSrcImage, filenamepath));
	}

	// supporting L8, LA8 and RGBA8

	//internal format
	static constexpr uint32_t const KTX_R8 = 0x8229;
	static constexpr uint32_t const KTX_RG8 = 0x822B;
	static constexpr uint32_t const KTX_RGBA8 = 0x8058;

	//base internal format
	static constexpr uint32_t const KTX__R = 0x1903;
	static constexpr uint32_t const KTX__RG = 0x8227;
	static constexpr uint32_t const KTX__RGBA = 0x1908;
	
	static constexpr uint32_t const KTX_UNSIGNED_BYTE = 0x1401;
	static constexpr uint32_t const KTX_ENDIAN_REF(0x04030201);

	static constexpr const uint8_t ktx_magic_id[12] = {
		0xAB, 0x4B, 0x54, 0x58,
		0x20, 0x31, 0x31, 0xBB,
		0x0D, 0x0A, 0x1A, 0x0A
	};

	FILE* fOut;

	if (((MODE_L|MODE_LA|MODE_BGRX|MODE_BGRA) & pSrcImage->mode) && 0 == _wfopen_s(&fOut, filenamepath.data(), L"wb"))
	{
		KtxHeader header = {};
		memcpy(header.identifier, ktx_magic_id, 12);

		header.endianness = KTX_ENDIAN_REF;
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

bool const __vectorcall ImagingSaveLayersToKTX(ImagingMemoryInstance const* const* const __restrict pSrcImages, uint32_t const numLayers, std::wstring_view const filenamepath)
{
	bool bReturn(false);

	if (numLayers <= 1) {
		return(false);
	}

	// supporting L8, LA8 and RGBA8, all images for layers must be equal in type and dimensions

	//internal format
	static constexpr uint32_t const KTX_R8 = 0x8229;
	static constexpr uint32_t const KTX_RG8 = 0x822B;
	static constexpr uint32_t const KTX_RGBA8 = 0x8058;

	//base internal format
	static constexpr uint32_t const KTX__R = 0x1903;
	static constexpr uint32_t const KTX__RG = 0x8227;
	static constexpr uint32_t const KTX__RGBA = 0x1908;

	static constexpr uint32_t const KTX_UNSIGNED_BYTE = 0x1401;
	static constexpr uint32_t const KTX_ENDIAN_REF(0x04030201);

	static constexpr const uint8_t ktx_magic_id[12] = {
		0xAB, 0x4B, 0x54, 0x58,
		0x20, 0x31, 0x31, 0xBB,
		0x0D, 0x0A, 0x1A, 0x0A
	};

	FILE* fOut;

	if (((MODE_L | MODE_LA | MODE_BGRX | MODE_BGRA) & pSrcImages[0]->mode) && 0 == _wfopen_s(&fOut, filenamepath.data(), L"wb"))
	{
		KtxHeader header = {};
		memcpy(header.identifier, ktx_magic_id, 12);

		header.endianness = KTX_ENDIAN_REF;
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

		// write header
		fwrite(&header, sizeof(KtxHeader), 1, fOut);

		// write size of data
		uint32_t const layerDataSize(pSrcImages[0]->xsize * pSrcImages[0]->ysize * pSrcImages[0]->pixelsize);
		//uint32_t const dataSize(layerDataSize * numLayers);
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

	if (MODE_BC7 == pSrcImage->mode && 0 == _wfopen_s(&fOut, filenamepath.data(), L"wb"))
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

namespace
{
	/// Scale a value by mip level, but do not reduce to zero.
	inline uint32_t mipScale(uint32_t value, uint32_t mipLevel) {
		return std::max(value >> mipLevel, (uint32_t)1);
	}
	/// KTX files use OpenGL format values. This converts some common ones to Vulkan equivalents.
	static constexpr uint32_t const KTX_ENDIAN_REF(0x04030201);
	inline eIMAGINGMODE const GLtoImagingMode(uint32_t const glFormat) {
		switch (glFormat) {
		case 0x8229: return MODE_L;
		case 0x1903: return MODE_L;

		case 0x822B: return MODE_LA;
		case 0x8227: return MODE_LA;

		case 0x1907: return MODE_RGB; // GL_RGB

		case 0x8058: return MODE_BGRX; // GL_RGBA
		case 0x1908: return MODE_BGRX; // GL_RGBA

		case 0x83F0: return MODE_ERROR; // GL_COMPRESSED_RGB_S3TC_DXT1_EXT
		case 0x83F1: return MODE_ERROR; // GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
		case 0x83F2: return MODE_ERROR; // GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
		case 0x83F3: return MODE_ERROR; // GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
		case 0x8E8C: return MODE_ERROR;	// GL_COMPRESSED_RGBA_BPTC_UNORM_ARB
		case 0x8E8D: return MODE_ERROR;
		}
		return MODE_ERROR;
	}
	/// Layout of a KTX file in a buffer.
	template<bool const WorkaroundLayerSizeDoubledInFileBug = false>
	class KTXFileLayout {
	public:
		KTXFileLayout() {
		}

		KTXFileLayout(uint8_t const* const __restrict begin, uint8_t const* const __restrict end) {
			uint8_t const* p = begin;
			if (p + sizeof(Header) > end) return;
			header = *(Header*)p;
			static constexpr uint8_t magic[] = {
			  0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
			};

			if (memcmp(magic, header.identifier, sizeof(magic))) {
				return;
			}

			if (KTX_ENDIAN_REF != header.endianness) {
				swap(header.glType);
				swap(header.glTypeSize);
				swap(header.glFormat);
				swap(header.glInternalFormat);
				swap(header.glBaseInternalFormat);
				swap(header.pixelWidth);
				swap(header.pixelHeight);
				swap(header.pixelDepth);
				swap(header.numberOfArrayElements);
				swap(header.numberOfFaces);
				swap(header.numberOfMipmapLevels);
				swap(header.bytesOfKeyValueData);
			}

			header.numberOfArrayElements = std::max(1U, header.numberOfArrayElements);
			header.numberOfFaces = std::max(1U, header.numberOfFaces);
			header.numberOfMipmapLevels = std::max(1U, header.numberOfMipmapLevels);
			header.pixelDepth = std::max(1U, header.pixelDepth);

			format_ = GLtoImagingMode(header.glInternalFormat);
			if (format_ == MODE_ERROR) return;

			p += sizeof(Header);
			if (p + header.bytesOfKeyValueData > end) return;

			for (uint32_t i = 0; i < header.bytesOfKeyValueData; ) {
				uint32_t keyAndValueByteSize = *(uint32_t*)(p + i);
				if (KTX_ENDIAN_REF != header.endianness) swap(keyAndValueByteSize);
				std::string kv(p + i + 4, p + i + 4 + keyAndValueByteSize);
				i += keyAndValueByteSize + 4;
				i = (i + 3) & ~3;
			}

			p += header.bytesOfKeyValueData;
			for (uint32_t mipLevel = 0; mipLevel != header.numberOfMipmapLevels; ++mipLevel) {

				// bugfix for arraylayers and faces not being factored into final size for this mip
				uint32_t layerImageSize;
				if constexpr (WorkaroundLayerSizeDoubledInFileBug) {  // KTX ImageViewer export array texture doubles layer size written to file, sometimes...
					layerImageSize = *(uint32_t*)(p) / header.numberOfArrayElements;
				}
				else {
					layerImageSize = *(uint32_t*)(p);
				}

				layerImageSize = (layerImageSize + 3) & ~3;
				if (KTX_ENDIAN_REF != header.endianness) swap(layerImageSize);

				layerImageSizes_.push_back(layerImageSize);

				uint32_t imageSize = layerImageSize * header.numberOfFaces * header.numberOfArrayElements;

				imageSize = (imageSize + 3) & ~3;
				if (KTX_ENDIAN_REF != header.endianness) swap(imageSize);

				imageSizes_.push_back(imageSize);

				p += 4; // offset for reading layer imagesize above
				imageOffsets_.push_back((uint32_t)(p - begin));

				if (p + imageSize > end) {
					// see https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glPixelStore.xhtml
					// fix bugs... https://github.com/dariomanesku/cmft/issues/29
					header.numberOfMipmapLevels = mipLevel + 1;
					break;
				}
				p += imageSize; // next mip offset if there is one
			}

			ok_ = true;
		}

		uint32_t offset(uint32_t mipLevel, uint32_t arrayLayer, uint32_t face) const {

			return imageOffsets_[mipLevel] + (arrayLayer * header.numberOfFaces + face) * layerImageSizes_[mipLevel];
		}

		uint32_t size(uint32_t mipLevel) {
			return imageSizes_[mipLevel];
		}

		bool ok() const { return ok_; }
		eIMAGINGMODE const format() const { return format_; }
		uint32_t mipLevels() const { return header.numberOfMipmapLevels; }
		uint32_t arrayLayers() const { return header.numberOfArrayElements; }
		uint32_t faces() const { return header.numberOfFaces; }
		uint32_t width(uint32_t const mipLevel = 0) const { return mipScale(header.pixelWidth, mipLevel); }
		uint32_t height(uint32_t const mipLevel = 0) const { return mipScale(header.pixelHeight, mipLevel); }
		uint32_t depth(uint32_t const mipLevel = 0) const { return mipScale(header.pixelDepth, mipLevel); }

		ImagingMemoryInstance* const __restrict upload(uint8_t const* const __restrict pFileBegin) const {
			uint32_t totalActualSize(0);

			for (auto const& size : imageSizes_) {
				totalActualSize += size;
			}

			if (0 == totalActualSize)
				return(nullptr);

			switch (format())
			{
			case MODE_BGRA:
				return(ImagingLoadFromMemoryBGRA(pFileBegin + offset(0, 0, 0), width(), height()));
			case MODE_LA:
				return(ImagingLoadFromMemoryLA(pFileBegin + offset(0, 0, 0), width(), height()));
			case MODE_L:
				return(ImagingLoadFromMemoryL(pFileBegin + offset(0, 0, 0), width(), height()));
			}
			
			return(nullptr);
		}

	private:
		static void swap(uint32_t& value) {
			value = value >> 24 | (value & 0xff0000) >> 8 | (value & 0xff00) << 8 | value << 24;
		}

		struct Header {
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

		Header header;
		eIMAGINGMODE format_;
		bool ok_ = false;
		std::vector<uint32_t> imageOffsets_;
		std::vector<uint32_t> imageSizes_;
		std::vector<uint32_t> layerImageSizes_;
	};
} // end ns

ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadKTX(std::wstring_view const filenamepath)
{
	std::error_code error{};

	mio::mmap_source mmap = mio::make_mmap_source(filenamepath, error);
	if (!error) {

		if (mmap.is_open() && mmap.is_mapped()) {
			__prefetch_vmem(mmap.data(), mmap.size());

			uint8_t const* const pReadPointer((uint8_t*)mmap.data());

			KTXFileLayout const ktxFile(pReadPointer, pReadPointer + mmap.length());

			if (ktxFile.ok()) {

				return(ktxFile.upload(pReadPointer));
			}
		}

	}

	return(nullptr);
}