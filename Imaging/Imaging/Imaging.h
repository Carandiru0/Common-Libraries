#pragma once
#define INCLUDE_PNG_SUPPORT 0
#define INCLUDE_JPEG_SUPPORT 0

#include <stdint.h>
#include "LinkImaging.h"

#include <string_view>
#include <Math/vec4_t.h>

#if INCLUDE_JPEG_SUPPORT
#include <tbb/task_group.h>
#endif

// REQUIRES: LIBRARIES TO BE LINKED IN APPLICATION
// tbb.lib;tbbmalloc.lib;fmt.lib
// REQUIRES: DLLs
// tbb.dll;tbbmalloc.dll
// OPTIONAL: DLLs - Only if feature BC7 encoding is used, no need to link against associated .lib for these .dll
// Compressonator.dll

/* -------------------------------------------------------------------- */

/*
* Image data organization:
*
* mode				 bytes	byte order
* -------------------------------
* MODE_1BIT			 1		1
* MODE_L			 1		L
* MODE_LA			 2		L, A
* MODE_I					4           I (32-bit integer, native byte order)
* MODE_F					4           F (32-bit IEEE float, native byte order)
* MODE_BGRX			 4		B, G, R, -
* MODE_BGRA			 4		B, G, R, A
* MODE_CMYK			 4		C, M, Y, K
* MODE_YCbCr		 4		Y, Cb, Cr, -
* MODE_Lab			 4      L, a, b, -
*
*
* "P" is an 8-bit palette mode, which should be mapped through the
* palette member to get an output image.  Check palette->mode to
* find the corresponding "real" mode.
*/
enum eIMAGINGMODE
{
	MODE_1BIT = (1<<0),
	MODE_L = (1<<1), MODE_LA = (1 << 2),
	MODE_I = (1<<5),
	MODE_F = (1<<6),
	MODE_RGB = (1<<7),
	MODE_BGRX = (1<<8), MODE_BGRA = (1 << 9),
	MODE_CMYK = (1<<10),
	MODE_YCbCr = (1 << 11),
	MODE_LAB = (1 << 12),
	MODE_HSV = (1 << 13),
	MODE_BC7 = (1 << 14),
	MODE_BC6A = (1 << 15)
};

/* pixel types */
#define IMAGING_TYPE_UINT8 (1<<0)
#define IMAGING_TYPE_INT32 (1<<1)
#define IMAGING_TYPE_FLOAT32 (1<<2)
#define IMAGING_TYPE_SPECIAL (1<<3)

#define IMAGING_TRANSFORM_NEAREST 0
#define IMAGING_TRANSFORM_BOX 4
#define IMAGING_TRANSFORM_BILINEAR 2
#define IMAGING_TRANSFORM_HAMMING 5
#define IMAGING_TRANSFORM_BICUBIC 3
#define IMAGING_TRANSFORM_LANCZOS 1

#define IMAGING_PIXEL_I(im,x,y) ((im)->image32[(y)][(x)])
#define IMAGING_PIXEL_F(im,x,y) (((float*)(im)->image32[y])[x])
/* Exceptions */
/* ---------- */

extern void* ImagingError_IOError(void);
extern void* ImagingError_MemoryError(void);
extern void* ImagingError_ModeError(void); /* maps to ValueError by default */
extern void* ImagingError_Mismatch(void); /* maps to ValueError by default */
extern void* ImagingError_ValueError(const char* message);
extern void ImagingError_Clear(void);

/* Handles */

typedef struct ImagingMemoryInstance* Imaging;

struct ImagingMemoryInstance {

	/* Format */
	eIMAGINGMODE mode;
	int type;		/* Data type (IMAGING_TYPE_*) */
	int bands;		/* Number of bands (1, 2, 3, or 4) */
	int32_t xsize;		/* Image dimension. */
	int32_t ysize;

	/* Data pointers */
	uint8_t ** __restrict image8;	/* Set for 8-bit images (pixelsize=1). */
	int32_t ** __restrict image32;	/* Set for 32-bit images (pixelsize=4). */

						/* Internals */
	uint8_t ** image;	/* Actual raster data. */
	uint8_t * __restrict block;	/* Set if data is allocated in a single block. */

	int32_t pixelsize;	/* Size of a pixel, in bytes (1, 2 or 4) */
	int32_t linesize;	/* Size of a line, in bytes (xsize * pixelsize) */

					/* Virtual methods */
	void(*destroy)(Imaging im);
};

typedef struct ImagingSequenceInstance // much easier to redefine than to inherit ImagingMemoryInstance
{
	uint32_t delay;

	static inline constexpr uint32_t const bands = 3;		/* BGRX --======= */
	uint32_t xsize;		/* Image dimension. */
	uint32_t ysize;

	/* Data pointers */
	uint8_t* __restrict block;
	/* Virtual methods */
	void(*destroy)(ImagingSequenceInstance* __restrict im);

	static inline constexpr uint32_t const pixelsize = sizeof(uint32_t);	/* Size of a pixel, in bytes (1, 2 or 4) */
	uint32_t linesize;	/* Size of a line, in bytes (xsize * pixelsize) */

} ImagingSequenceInstance;

typedef struct ImagingSequence
{
	ImagingSequenceInstance* __restrict images;

	uint32_t count;
	uint32_t xsize;		/* Image dimension. */
	uint32_t ysize;
	static inline constexpr uint32_t const pixelsize = sizeof(uint32_t);
	uint32_t linesize;	/* Size of a line, in bytes (xsize * pixelsize) */

	/* Virtual methods */
	void(*destroy)(ImagingSequence* __restrict im);
} ImagingSequence;

typedef struct ImagingLUT	// 16bit/channel 3D LUT
{
	static inline constexpr uint32_t const bands = 3;		/* BGRX --======= */
	uint32_t size; /* Image dimension. xsize = ysize = zsize always*/

	/* Data pointers */
	uint16_t* __restrict block;
	/* Virtual methods */
	void(*destroy)(ImagingLUT* __restrict im);

	static inline constexpr uint32_t const pixelsize = sizeof(uint16_t) * 4;	/* Size of a pixel, in bytes (8) */
	uint32_t slicesize;	/* Size of a slice, in bytes (size * size * pixelsize) */

} ImagingLUT;

// OPERATIONS //
ImagingMemoryInstance* const __restrict __vectorcall ImagingNew( eIMAGINGMODE const mode, int const xsize, int const ysize);
ImagingLUT* const __restrict __vectorcall ImagingNew(int const size);
ImagingMemoryInstance* const __restrict __vectorcall ImagingNewCompressed(eIMAGINGMODE const mode /*should be MODE_BC7 or MODE_BC6A*/, int const xsize, int const ysize, int BufferSize);

ImagingMemoryInstance* const __restrict __vectorcall ImagingCopy(ImagingMemoryInstance const* const __restrict im);
ImagingLUT* const __restrict __vectorcall ImagingCopy(ImagingLUT const* const __restrict lut);

void __vectorcall ImagingClear(ImagingMemoryInstance* __restrict im);

void __vectorcall ImagingDelete(ImagingMemoryInstance* __restrict im);
void __vectorcall ImagingDelete(ImagingMemoryInstance const* __restrict im);
void __vectorcall ImagingDelete(ImagingSequence* __restrict im);
void __vectorcall ImagingDelete(ImagingSequence const* __restrict im);
void __vectorcall ImagingDelete(ImagingLUT* __restrict im);
void __vectorcall ImagingDelete(ImagingLUT const* __restrict im);

extern ImagingMemoryInstance* const __restrict __vectorcall ImagingResample(ImagingMemoryInstance const* const __restrict imIn, int const xsize, int const ysize, int const filter);
extern ImagingSequence* const __restrict __vectorcall		 ImagingResample(ImagingSequence const* const __restrict imIn, int const xsize, int const ysize, int const filter);
void __vectorcall ImagingDither(ImagingMemoryInstance* const __restrict im);
void __vectorcall ImagingLerp(ImagingMemoryInstance* const __restrict im_dst, ImagingMemoryInstance const* const __restrict im_src, float const tT); // im_dst = A, im_src = B, op: A = lerp(A, B, t)
void __vectorcall ImagingLUTLerp(ImagingLUT* const __restrict lut_dst, ImagingLUT const* const __restrict lut_src, float const tT);
void __vectorcall ImagingBlend(ImagingMemoryInstance* const __restrict im_dst, ImagingMemoryInstance const* const __restrict im_src);
void __vectorcall ImagingVerticalFlip(ImagingMemoryInstance* const __restrict im); // flip Y / invert Y axis / vertical flip (INPLACE)


// BGRX in - BGR out
void __vectorcall Parallel_BGRX2BGR(uint8_t* const* const __restrict& __restrict pDst, uint8_t const* const* const __restrict& __restrict pSrc,
					   int const& xSize, int const& ySize);
// BGR in - BGRX out
void __vectorcall Parallel_BGR2BGRX(uint8_t* const* const __restrict& __restrict pDst, uint8_t const* const* const __restrict& __restrict pSrc,
					   int const& xSize, int const& ySize);

// L in - BGR out
void __vectorcall Parallel_Gray2BGR(uint8_t* const* const __restrict& __restrict pDst, uint8_t const* const* const __restrict& __restrict pSrc,
						int const& xSize, int const& ySize);
// L in - BGRX out
void __vectorcall Parallel_Gray2BGRX(uint8_t* const* const __restrict& __restrict pDst, uint8_t const* const* const __restrict& __restrict pSrc,
						int const& xSize, int const& ySize);

// BGR in - BGRX out
void __vectorcall Parallel_GrayBGRXReduction(uint8_t* const* const  __restrict& pDst, uint8_t const* const* const __restrict& pSrc,
							    int const& xSize, int const& ySize, double const& dLevels,
							    uint8_t const(&__restrict rLUT)[256]);

// BGR in - BGR out
void __vectorcall Parallel_BrightnessContrast(uint8_t* const* const  __restrict& pDst, uint8_t const* const* const __restrict& pSrc, 
								 int const& xSize, int const& ySize,
								 double const& BrightFactor, double const& ContrastFactor);


// PALETTE GENERAL // 

// **** xmRGB should be converted to linear space range  b4 call to this function
uint32_t const __vectorcall ImagingPaletteIndexClosestColor(uvec4_v const xmRGB, ImagingMemoryInstance const* const __restrict palette);


// SUPER PALETTE //	

// ** note for the indices in ImagingPaletteInfo, they should be treated as inclusive beginning & end
// ie.) for( i = info.inferno[0] ; i <= info.inferno[1] ; ++i )   ** less than or *equal* to includes the very last palette index of the range
typedef struct ImagingSuperPaletteInfo {

	uint32_t
		width,			// width/size of palette
		option[2],		// optional (suppplied to ImagingGeneratePalette1D) begin & end indices
		skinny[2],		// skinny begin & end indices
		inferno[2],		// inferno ""      ""   ""
		viridis[2],		// viridis  ""      ""   ""
		hsv[2],			// hsv rainbow  ""      ""   ""
		grey[2];		// greyscale  ""      ""   ""

} ImagingSuperPaletteInfo;

// **** Palette stores all color in linear space
//palette_basic - optional parameter - can be a 256 color palette to insert into superpalette. greyscale elements are ignored, ONLY color elements are inserted in superpalette.
ImagingMemoryInstance* const __restrict __vectorcall ImagingGenerateSuperPalette1D(ImagingSuperPaletteInfo& __restrict out_info, ImagingMemoryInstance const* const __restrict palette_basic = nullptr);


// INDIVIDUAL SPECIAL PALETTES //


// LOADING //
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadRawBGRA(std::wstring_view const filenamepath, int const width, int const height);
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadRawLA(std::wstring_view const filenamepath, int const width, int const height);
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadRawL(std::wstring_view const filenamepath, int const width, int const height);
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadFromMemoryBGRA(uint8_t const* __restrict pMemLoad, int const width, int const height);
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadFromMemoryLA(uint8_t const* __restrict pMemLoad, int const width, int const height);
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadFromMemoryL(uint8_t const* __restrict pMemLoad, int const width, int const height);
ImagingSequence* const __restrict __vectorcall		 ImagingLoadGIFSequence(std::wstring_view const giffilenamepath, uint32_t width = 0, uint32_t height = 0);
ImagingLUT* const __restrict __vectorcall			 ImagingLoadLUT(std::wstring_view const cubefilenamepath);

#if INCLUDE_PNG_SUPPORT
// supports only loading BGRA
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadPNG(std::wstring_view const filenamepath);
#endif

// SAVING //
bool const __vectorcall ImagingSaveLUT(ImagingLUT const* const __restrict lut, std::string_view const title, std::wstring_view const cubefilenamepath);
bool const __vectorcall ImagingSaveRaw(ImagingMemoryInstance const* const __restrict pSrcImage, std::wstring_view const filenamepath);
#define ImagingSaveRaw8bit ImagingSaveRaw

// only MODE_L and MODE_RGB seem to work when saving JPEGS
#if INCLUDE_JPEG_SUPPORT
bool const ImagingSaveJPEG(eIMAGINGMODE const outClrSpace, ImagingMemoryInstance const* const __restrict pSrcImage, std::wstring_view const filenamepath);
void ImagingSaveJPEG(tbb::task_group& __restrict tG, eIMAGINGMODE const outClrSpace, ImagingMemoryInstance const* const __restrict pSrcImage, std::wstring_view const filenamepath);
#endif

#if INCLUDE_PNG_SUPPORT
// supports saving L, LA, BGRX, BGRA
bool const __vectorcall ImagingSaveToPNG(ImagingMemoryInstance const* const __restrict pSrcImage, std::wstring_view const filenamepath);						// single image //
bool const __vectorcall ImagingSaveToPNG(ImagingMemoryInstance const* __restrict const* const __restrict pSrcImage, uint32_t const image_count, std::wstring_view const filenamepath);		// sequence/array images //
#endif

bool const __vectorcall ImagingSaveToKTX(ImagingMemoryInstance const* const __restrict pSrcImage, std::wstring_view const filenamepath);
bool const __vectorcall ImagingSaveLayersToKTX(ImagingMemoryInstance const* const* const __restrict pSrcImages, uint32_t const numLayers, std::wstring_view const filenamepath);
bool const __vectorcall ImagingSaveCompressedBC7ToKTX(ImagingMemoryInstance const* const __restrict pSrcImage, std::wstring_view const filenamepath);

ImagingMemoryInstance* const __restrict __vectorcall ImagingCompressBGRAToBC7(ImagingMemoryInstance const* const __restrict pSrcImage, float const normalizedQuality = 0.82f);







