#pragma once

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

enum eIMAGINGMODE
{
	MODE_1BIT = (1<<0),
	
	MODE_L = (1<<1), MODE_LA = (1<<2),
	MODE_L16 = (1<<3), MODE_LA16 = (1<<4),
	
	MODE_U32 = (1<<5),
	
	MODE_RGB = (1<<7),
	MODE_BGRX = (1<<8), MODE_BGRA = (1 << 9),
	
	MODE_RGB16 = (1 << 10),
	MODE_BGRX16 = (1 << 11), MODE_BGRA16 = (1 << 12),
	
	MODE_F32 = (1 << 13),

	MODE_BC7 = (1 << 14),
	MODE_BC6A = (1 << 15),
	
	MODE_ERROR
};

/* pixel types */
#define IMAGING_TYPE_UINT8 (1<<0)        
#define IMAGING_TYPE_UINT32 (1<<1)       
#define IMAGING_TYPE_UINT64 (1<<2)
#define IMAGING_TYPE_FLOAT32 (1<<3)
#define IMAGING_TYPE_SPECIAL (1<<4)

#define IMAGING_TRANSFORM_NEAREST 0
#define IMAGING_TRANSFORM_BOX 4
#define IMAGING_TRANSFORM_BILINEAR 2
#define IMAGING_TRANSFORM_HAMMING 5
#define IMAGING_TRANSFORM_BICUBIC 3
#define IMAGING_TRANSFORM_LANCZOS 1

#define IMAGING_PIXEL_U32(im,x,y) ((im)->image32[(y)][(x)])
#define IMAGING_PIXEL_U16(im,x,y) ((reinterpret_cast<uint16_t* const* const>((im)->image32))[(y)][(x)])

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
	uint8_t ** __restrict image8;	/* Set for 8-bit per component images (pixelsize=1). */
	uint32_t ** __restrict image32;	/* Set for 32-bit per compoenent images (pixelsize=4). */

						/* Internals */
	uint8_t ** image;	/* Actual raster data. */
	uint8_t * __restrict block;	/* Set if data is allocated in a single block. */

	int32_t pixelsize;	/* Size of a pixel, in bytes (1, 2 or 4) */
	int32_t linesize;	/* Size of a line, in bytes (xsize * pixelsize) */

					/* Virtual methods */
	void(*destroy)(Imaging im);

	constexpr ImagingMemoryInstance(int const bands_, int32_t const pixelsize_)
		:
		bands(bands_),
		pixelsize(pixelsize_)
	{}

	constexpr ImagingMemoryInstance() = default;
};

typedef struct ImagingSequenceInstance : ImagingMemoryInstance // much easier to redefine than to inherit ImagingMemoryInstance
{
	uint32_t delay;

	/* Virtual methods */
	void(*destroy)(ImagingSequenceInstance* __restrict im);

	constexpr ImagingSequenceInstance()
		: ImagingMemoryInstance(3, sizeof(uint32_t)) /* initially limited here to be comptabile with BGRX for gif output when gif is loaded, can be changed after load of gif */
	{}
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

typedef struct ImagingHistogram // Histogram for ImagingMemoryInstance
{
	uint32_t* __restrict block;// 1D Array of count - could be 65536 or 256 depending on image used to create the histogram
	uint32_t             count;

	/* Virtual methods */
	void(*destroy)(ImagingHistogram* __restrict im);
} ImagingHistogram;


// color operations //
uvec4_v const ImagingSRGBtoLinearVector(uint32_t const packed_srgb);
uint32_t const ImagingSRGBtoLinear(uint32_t const packed_srgb);


// OPERATIONS //
ImagingMemoryInstance* const __restrict __vectorcall ImagingNew( eIMAGINGMODE const mode, int const xsize, int const ysize);
ImagingLUT* const __restrict __vectorcall			 ImagingNew(int const size);
ImagingHistogram* const __restrict __vectorcall		 ImagingNewHistogram(ImagingMemoryInstance const* const __restrict im);
ImagingMemoryInstance* const __restrict __vectorcall ImagingNewCompressed(eIMAGINGMODE const mode /*should be MODE_BC7 or MODE_BC6A*/, int const xsize, int const ysize, int BufferSize);

ImagingMemoryInstance* const __restrict __vectorcall ImagingCopy(ImagingMemoryInstance const* const __restrict im);
ImagingLUT* const __restrict __vectorcall			 ImagingCopy(ImagingLUT const* const __restrict lut);

void __vectorcall ImagingClear(ImagingMemoryInstance* __restrict im);

void __vectorcall ImagingDelete(ImagingMemoryInstance* __restrict im);
void __vectorcall ImagingDelete(ImagingMemoryInstance const* __restrict im);
void __vectorcall ImagingDelete(ImagingSequence* __restrict im);
void __vectorcall ImagingDelete(ImagingSequence const* __restrict im);
void __vectorcall ImagingDelete(ImagingLUT* __restrict im);
void __vectorcall ImagingDelete(ImagingLUT const* __restrict im);
void __vectorcall ImagingDelete(ImagingHistogram* __restrict im);
void __vectorcall ImagingDelete(ImagingHistogram const* __restrict im);

// SPECIAL FUNCTIONS //
extern ImagingMemoryInstance* const __restrict __vectorcall ImagingResample(ImagingMemoryInstance const* const __restrict imIn, int const xsize, int const ysize, int const filter);
//extern ImagingSequence* const __restrict __vectorcall		ImagingResample(ImagingSequence const* const __restrict imIn, int const xsize, int const ysize, int const filter); // alternatively for sequences you can specify dimensions in ImagingLoadGIFSequence, which performs resampling to the desired dimensions aswell.
void __vectorcall ImagingChromaKey(ImagingMemoryInstance* const __restrict im);	// (INPLACE) key[ 0x00b140 ] r g b
void __vectorcall ImagingDither(ImagingMemoryInstance* const __restrict im);
void __vectorcall ImagingLerpL16(ImagingMemoryInstance* const __restrict A, ImagingMemoryInstance const* const __restrict B, float const tT);
void __vectorcall ImagingLerp(ImagingMemoryInstance* const __restrict im_dst, ImagingMemoryInstance const* const __restrict im_src, float const tT); // im_dst = A, im_src = B, op: A = lerp(A, B, t)
void __vectorcall ImagingLerp(ImagingMemoryInstance* const __restrict out, ImagingMemoryInstance const* const __restrict A, ImagingMemoryInstance const* const __restrict B, float const tT);
void __vectorcall ImagingLUTLerp(ImagingLUT* const __restrict lut_dst, ImagingLUT const* const __restrict lut_src, float const tT);
void __vectorcall ImagingBlend(ImagingMemoryInstance* const __restrict im_dst, ImagingMemoryInstance const* const __restrict im_src);
void __vectorcall ImagingVerticalFlip(ImagingMemoryInstance* const __restrict im); // flip Y / invert Y axis / vertical flip (INPLACE)
ImagingMemoryInstance* const __restrict __vectorcall ImagingRotateCW(ImagingMemoryInstance* const __restrict im); // (NOT INPLACE)  ** image must be square (width == height)
ImagingMemoryInstance* const __restrict __vectorcall ImagingRotateCCW(ImagingMemoryInstance* const __restrict im); // (NOT INPLACE) ** image must be square (width == height)
ImagingMemoryInstance* const __restrict __vectorcall ImagingTangentSpaceNormalMapToDerivativeMapBGRA16(ImagingMemoryInstance* const __restrict im); // (NOT INPLACE) - new image returned of LA16 type, requires normal map of type BGRA16 input. RGB16 images should be converted to BGRX16 first.
                                                                                                                                                    // Tangent space Normal map is standards TS. red X+ (right), green Y+ (down), blue Z+ (near) [set as default coordinate system in ShaderMap (TS)]
                                                                                                                                                    
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


// LOADING //
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadRawBGRA16(std::wstring_view const filenamepath, int const width, int const height);
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadRawRGB16(std::wstring_view const filenamepath, int const width, int const height);
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadRawBGRA(std::wstring_view const filenamepath, int const width, int const height);
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadRawRGB16(std::wstring_view const filenamepath, int const width, int const height);
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadRawLA16(std::wstring_view const filenamepath, int const width, int const height);
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadRawLA(std::wstring_view const filenamepath, int const width, int const height);
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadRawL16(std::wstring_view const filenamepath, int const width, int const height);
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadRawL(std::wstring_view const filenamepath, int const width, int const height);
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadRawF32(std::wstring_view const filenamepath, int const width, int const height);

ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadFromMemoryBGRA16(uint8_t const* __restrict pMemLoad, int const width, int const height);
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadFromMemoryRGB16(uint8_t const* __restrict pMemLoad, int const width, int const height);
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadFromMemoryBGRA(uint8_t const* __restrict pMemLoad, int const width, int const height);
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadFromMemoryRGB(uint8_t const* __restrict pMemLoad, int const width, int const height);
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadFromMemoryLA16(uint8_t const* __restrict pMemLoad, int const width, int const height);
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadFromMemoryLA(uint8_t const* __restrict pMemLoad, int const width, int const height);
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadFromMemoryL16(uint8_t const* __restrict pMemLoad, int const width, int const height);
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadFromMemoryL(uint8_t const* __restrict pMemLoad, int const width, int const height);
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadFromMemoryF32(uint8_t const* __restrict pMemLoad, int const width, int const height);

// GREYSCALE //

// 2 seperate greyscale L images to one combined LA image
ImagingMemoryInstance* const __restrict __vectorcall  ImagingLLToLA(ImagingMemoryInstance const* const __restrict pSrcImageL, ImagingMemoryInstance const* const __restrict pSrcImageA);
ImagingMemoryInstance* const __restrict __vectorcall  ImagingL16L16ToLA16(ImagingMemoryInstance const* const __restrict pSrcImageL, ImagingMemoryInstance const* const __restrict pSrcImageA);
ImagingMemoryInstance* const __restrict __vectorcall  ImagingF32ToL16(ImagingMemoryInstance const* const __restrict pSrcImageF, double dMin = FLT_MAX, double dMax = -FLT_MAX);

// CONVERSION //
ImagingMemoryInstance* const __restrict __vectorcall  ImagingRGBToBGRX(ImagingMemoryInstance const* const __restrict pSrcImageRGB);
ImagingMemoryInstance* const __restrict __vectorcall  ImagingRGB16ToBGRX16(ImagingMemoryInstance const* const __restrict pSrcImageRGB16);

// FILE SUPPORT, DEFAULT SUPPORTED : KTX, KTX2, GIF and LUT's

// ImagingLoadKTX will load the format (UNORM / SRGB) as is, no colorspace manipulations occur. *does not support mipmaps, file cannot contain mipmaps*
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadKTX(std::wstring_view const filenamepath); // RGB images loaded are internally promoted to BGRX (16bpc versions aswell)
ImagingSequence* const __restrict __vectorcall		 ImagingLoadGIFSequence(std::wstring_view const giffilenamepath, uint32_t width = 0, uint32_t height = 0);  // chroma-key[ 0x00b140 ] enabled internally  ** do not load multiple sequences at the same time in multiple threads - this is because of lockless file reading (optimization) **
ImagingLUT* const __restrict __vectorcall			 ImagingLoadLUT(std::wstring_view const cubefilenamepath);

#if INCLUDE_TIF_SUPPORT
// supports loading L, L16, LA, LA16, RGB, RGB16, RGBA, RGBA16
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadTif(std::wstring_view const filenamepath);
#endif

// RAW COPY 
void __vectorcall ImagingCopyRaw(void* const pDstMemory, ImagingMemoryInstance const* const __restrict pSrcImage);

// CONVERSION //
void __vectorcall ImagingSwapRB(ImagingMemoryInstance* const __restrict im); // Red and Blue component swap (INPLACE)
ImagingMemoryInstance* const __restrict __vectorcall ImagingBits_8To16(ImagingMemoryInstance const* const __restrict pSrcImageL);
ImagingMemoryInstance* const __restrict __vectorcall ImagingBits_16To8(ImagingMemoryInstance const* const __restrict pSrcImageL);
ImagingMemoryInstance* const __restrict __vectorcall ImagingBits_16To32(ImagingMemoryInstance const* const __restrict pSrcImageL); // (NOT INPLACE)   // **** uses tbb (parallel) **** -- 16bit images must be upscaled to 32bit before any resampling. then the grayscale image should be converted back to 16 bits only at the end of what ever filter process is taking place.
ImagingMemoryInstance* const __restrict __vectorcall ImagingBits_32To16(ImagingMemoryInstance const* const __restrict pSrcImageL);

// SAVING //
bool const __vectorcall ImagingSaveLUT(ImagingLUT const* const __restrict lut, std::string_view const title, std::wstring_view const cubefilenamepath);
bool const __vectorcall ImagingSaveRaw(ImagingMemoryInstance const* const __restrict pSrcImage, std::wstring_view const filenamepath);
#define ImagingSaveRaw8bit ImagingSaveRaw

// only MODE_L and MODE_RGB seem to work when saving JPEGS
#if INCLUDE_JPEG_SUPPORT
bool const ImagingSaveJPEG(eIMAGINGMODE const outClrSpace, ImagingMemoryInstance const* const __restrict pSrcImage, std::wstring_view const filenamepath);
void ImagingSaveJPEG(tbb::task_group& __restrict tG, eIMAGINGMODE const outClrSpace, ImagingMemoryInstance const* const __restrict pSrcImage, std::wstring_view const filenamepath);
#endif

#if INCLUDE_TIF_SUPPORT
// supports saving L, L16, LA, LA16, RGBA, RGBA16
bool const __vectorcall ImagingSaveToTif(ImagingMemoryInstance const* const __restrict pSrcImage, std::wstring_view const filenamepath);
#endif

// SaveToKTX will save in the as is (no colorspace conversion) [ linear ]. If the data for the image is supposed to be SRGB, use ImageView to export a srgb copy.
bool const __vectorcall ImagingSaveToKTX(ImagingMemoryInstance const* const __restrict pSrcImage, std::wstring_view const filenamepath); // RGB images should be converted to BGRX first
bool const __vectorcall ImagingSaveLayersToKTX(ImagingMemoryInstance const* const* const __restrict pSrcImages, uint32_t const numLayers, std::wstring_view const filenamepath); // RGB images should be converted to BGRX first
bool const __vectorcall ImagingSaveCompressedBC7ToKTX(ImagingMemoryInstance const* const __restrict pSrcImage, std::wstring_view const filenamepath);

ImagingMemoryInstance* const __restrict __vectorcall ImagingCompressBGRAToBC7(ImagingMemoryInstance const* const __restrict pSrcImage, float const normalizedQuality = 0.82f);







