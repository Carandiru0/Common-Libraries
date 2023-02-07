#pragma once

#include <stdint.h>
#include "LinkImaging.h"

#include <string_view>
#include <Math/vec4_t.h>
#include <vector>

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
uvec4_v const  ImagingSRGBtoLinearVector(uint32_t const packed_srgb); // input 8bit SRGB, output 10bit LINEAR
uint32_t const ImagingSRGBtoLinear(uint32_t const packed_srgb);       // "" ""  ""   ""   ""  ""   ""  ""  ""


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

void __vectorcall ImagingChromaKey(ImagingMemoryInstance* const __restrict im);	// (INPLACE) key[ 0x00b140 ] r g b
void __vectorcall ImagingDither(ImagingMemoryInstance* const __restrict im);
void __vectorcall ImagingLerpL16(ImagingMemoryInstance* const __restrict A, ImagingMemoryInstance const* const __restrict B, float const tT);
void __vectorcall ImagingLerp(ImagingMemoryInstance* const __restrict im_dst, ImagingMemoryInstance const* const __restrict im_src, float const tT); // im_dst = A, im_src = B, op: A = lerp(A, B, t)
void __vectorcall ImagingLerp(ImagingMemoryInstance* const __restrict out, ImagingMemoryInstance const* const __restrict A, ImagingMemoryInstance const* const __restrict B, float const tT);
void __vectorcall ImagingLUTLerp(ImagingLUT* const __restrict lut_dst, ImagingLUT const* const __restrict lut_src, float const tT);
void __vectorcall ImagingBlend(ImagingMemoryInstance* const __restrict im_dst, ImagingMemoryInstance const* const __restrict im_src);
void __vectorcall ImagingVerticalFlip(ImagingMemoryInstance* const __restrict im); // flip Y / invert Y axis / vertical flip (INPLACE)

ImagingMemoryInstance* const __restrict __vectorcall ImagingOffset(ImagingMemoryInstance const* const __restrict im, int const xoffset, int const yoffset); // (NOT INPLACE) **
ImagingMemoryInstance* const __restrict __vectorcall ImagingRotateCW(ImagingMemoryInstance const* const __restrict im); // (NOT INPLACE)  ** 
ImagingMemoryInstance* const __restrict __vectorcall ImagingRotateCCW(ImagingMemoryInstance const* const __restrict im); // (NOT INPLACE) ** 

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
ImagingMemoryInstance* const __restrict __vectorcall ImagingLoadRawRGB(std::wstring_view const filenamepath, int const width, int const height);
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
void                                                  ImagingFastRGBTOBGRX(uint8_t* const __restrict blockOut, uint8_t const* const __restrict blockIn, uint32_t const width, uint32_t const height); // exposed for usage with other buffers, or direct access to two existing Imaging instances' raw data blocks
void                                                  ImagingFastRGB16TOBGRX16(uint16_t* const __restrict blockOut, uint16_t const* const __restrict blockIn, uint32_t const width, uint32_t const height);
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

// SAVING //
bool const __vectorcall ImagingSaveLUT(ImagingLUT const* const __restrict lut, std::string_view const title, std::wstring_view const cubefilenamepath);
bool const __vectorcall ImagingSaveRaw(ImagingMemoryInstance const* const __restrict pSrcImage, std::wstring_view const filenamepath);

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



// Extra, *public* KTX/KTX2 Imaging Interface

/// Scale a value by mip level, but do not reduce to zero.
inline uint32_t const mipScale(uint32_t const value, uint32_t const mipLevel) {
	return std::max(value >> mipLevel, (uint32_t)1);
}
/// KTX files use OpenGL format values. This converts some common ones to Vulkan equivalents.
static constexpr uint32_t const KTX_ENDIAN_REF(0x04030201);

uint32_t const GLtoVKFormat(uint32_t const glFormat);
eIMAGINGMODE const VKtoImagingMode(uint32_t const vkFormat);

enum KTX_VERSION
{
	KTX1 = 1,
	KTX2 = 2
};

/// Layout of a KTX or KTX2 file in a buffer. - Unsupported KTX2 "super compression", always just the data in it's original format
template<uint32_t const version = KTX_VERSION::KTX1>  // KTX "1"  or  KTX "2"
class KTXFileLayout {

public:
	KTXFileLayout(uint8_t const* const __restrict begin, uint8_t const* const __restrict end)
	{
		uint8_t const* p = begin;

		if constexpr (KTX_VERSION::KTX1 == version) {

			if (p + sizeof(Header) > end) return;

			header_data.v1 = *(Header*)p;

			static constexpr uint8_t magic[] = {
				0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A // KTX1
			};

			if (memcmp(magic, header_data.v1.identifier, sizeof(magic))) {
				return;
			}

			static constexpr uint32_t const KTX_ENDIAN_REF(0x04030201);
			if (KTX_ENDIAN_REF != header_data.v1.endianness) {
				swap(header_data.v1.glType);
				swap(header_data.v1.glTypeSize);
				swap(header_data.v1.glFormat);
				swap(header_data.v1.glInternalFormat);
				swap(header_data.v1.glBaseInternalFormat);
				swap(header_data.v1.pixelWidth);
				swap(header_data.v1.pixelHeight);
				swap(header_data.v1.pixelDepth);
				swap(header_data.v1.numberOfArrayElements);
				swap(header_data.v1.numberOfFaces);
				swap(header_data.v1.numberOfMipmapLevels);
				swap(header_data.v1.bytesOfKeyValueData);
			}

			header_data.v1.numberOfArrayElements = std::max(1U, header_data.v1.numberOfArrayElements);
			header_data.v1.numberOfFaces = std::max(1U, header_data.v1.numberOfFaces);
			header_data.v1.numberOfMipmapLevels = std::max(1U, header_data.v1.numberOfMipmapLevels);
			header_data.v1.pixelDepth = std::max(1U, header_data.v1.pixelDepth);

			vkformat_ = GLtoVKFormat(header_data.v1.glInternalFormat);
			if (!vkformat_) return;
			format_ = VKtoImagingMode(vkformat_);

			p += sizeof(Header);
			if (p + header_data.v1.bytesOfKeyValueData > end) return;
			p += header_data.v1.bytesOfKeyValueData;

			for (uint32_t mipLevel = 0; mipLevel != header_data.v1.numberOfMipmapLevels; ++mipLevel) {

				// bugfix for arraylayers and faces not being factored into final size for this mip
				uint32_t layerImageSize;
				if (header_data.v1.numberOfArrayElements > 1) { // avoid div by zero
					layerImageSize = *(uint32_t*)(p) / header_data.v1.numberOfArrayElements;
				}
				else if (header_data.v1.numberOfFaces > 1) {
					header_data.v1.numberOfArrayElements = header_data.v1.numberOfFaces; header_data.v1.numberOfFaces = 1;
					layerImageSize = *(uint32_t*)(p) / header_data.v1.numberOfArrayElements;
				}
				else {
					layerImageSize = *(uint32_t*)(p);
				}

				layerImageSize = (layerImageSize + 3) & ~3;
				if (KTX_ENDIAN_REF != header_data.v1.endianness) swap(layerImageSize);

				layerImageSizes_.push_back(layerImageSize);

				uint32_t imageSize = layerImageSize * header_data.v1.numberOfFaces * header_data.v1.numberOfArrayElements;

				imageSize = (imageSize + 3) & ~3;
				if (KTX_ENDIAN_REF != header_data.v1.endianness) swap(imageSize);

				imageSizes_.push_back(imageSize);

				p += 4; // offset for reading layer imagesize above
				imageOffsets_.push_back((uint32_t)(p - begin));

				if (p + imageSize > end) {
					// see https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glPixelStore.xhtml
					// fix bugs... https://github.com/dariomanesku/cmft/issues/29
					header_data.v1.numberOfMipmapLevels = mipLevel + 1;
					break;
				}
				p += imageSize; // next mip offset if there is one
			}
		}
		else { // KTX2

			if (p + sizeof(HeaderV2) > end) return;

			header_data.v2 = *(HeaderV2*)p;

			static constexpr uint8_t magic[] = {
				0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A  // KTX2
			};

			if (memcmp(magic, header_data.v2.identifier, sizeof(magic))) {
				return;
			}

			header_data.v2.numberOfArrayElements = std::max(1U, header_data.v2.numberOfArrayElements);
			header_data.v2.numberOfFaces = std::max(1U, header_data.v2.numberOfFaces);
			header_data.v2.numberOfMipmapLevels = std::max(1U, header_data.v2.numberOfMipmapLevels);
			header_data.v2.pixelDepth = std::max(1U, header_data.v2.pixelDepth);

			vkformat_ = header_data.v2.format;
			if (!vkformat_) return;
			format_ = VKtoImagingMode(vkformat_);

			p += sizeof(HeaderV2);

			uint32_t imageOffset((uint32_t)(header_data.v2.sgdOffset + header_data.v2.sgdLength)); // ktx2 image data start offset

			for (uint32_t mipLevel = 0; mipLevel != header_data.v2.numberOfMipmapLevels; ++mipLevel) {

				p += 16; // skip byte offset & length, now on mip image image size

				// bugfix for arraylayers and faces not being factored into final size for this mip
				uint32_t layerImageSize;
				if (header_data.v2.numberOfArrayElements > 1) { // avoid div by zero
					layerImageSize = *(uint32_t*)(p) / header_data.v2.numberOfArrayElements;
				}
				else if (header_data.v2.numberOfFaces > 1) {
					header_data.v2.numberOfArrayElements = header_data.v2.numberOfFaces; header_data.v2.numberOfFaces = 1;
					layerImageSize = *(uint32_t*)(p) / header_data.v2.numberOfArrayElements;
				}
				else {
					layerImageSize = *(uint32_t*)(p);
				}

				layerImageSize = (layerImageSize + 3) & ~3;
				layerImageSizes_.push_back(layerImageSize);

				uint32_t imageSize = layerImageSize * header_data.v2.numberOfFaces * header_data.v2.numberOfArrayElements;

				imageSize = (imageSize + 3) & ~3;
				imageSizes_.push_back(imageSize);

				p += 8; // offset for reading layer imagesize above
				imageOffsets_.push_back(imageOffset); // relative to start of image data in ktx v2
				imageOffset += imageSize;
			}
		}
		ok_ = true;
	}

	uint32_t const offset(uint32_t const mipLevel, uint32_t const arrayLayer, uint32_t const face) const {

		if constexpr (KTX_VERSION::KTX2 == version) {
			return imageOffsets_[mipLevel] + (arrayLayer * header_data.v2.numberOfFaces + face) * layerImageSizes_[mipLevel];
		}

		return imageOffsets_[mipLevel] + (arrayLayer * header_data.v1.numberOfFaces + face) * layerImageSizes_[mipLevel];
	}

	uint32_t const size(uint32_t const mipLevel) const {
		return imageSizes_[mipLevel];
	}
	std::vector<uint32_t> const& sizes() const { return(imageSizes_); }

	uint32_t const layer_size(uint32_t const mipLevel) const {
		return layerImageSizes_[mipLevel];
	}
	std::vector<uint32_t> const& layer_sizes() const { return(layerImageSizes_); }

	bool const         ok() const { return ok_; }
	eIMAGINGMODE const format() const { return format_; }
	uint32_t const     vkformat() const { return vkformat_; } // can be cast to vk::Format by user
	uint32_t const     mipLevels() const { if constexpr (KTX_VERSION::KTX2 == version) return header_data.v2.numberOfMipmapLevels; return header_data.v1.numberOfMipmapLevels; }
	uint32_t const     arrayLayers() const { if constexpr (KTX_VERSION::KTX2 == version) return header_data.v2.numberOfArrayElements; return header_data.v1.numberOfArrayElements; }
	uint32_t const     width(uint32_t const mipLevel) const { if constexpr (KTX_VERSION::KTX2 == version) return mipScale(header_data.v2.pixelWidth, mipLevel); return mipScale(header_data.v1.pixelWidth, mipLevel); }
	uint32_t const     height(uint32_t const mipLevel) const { if constexpr (KTX_VERSION::KTX2 == version) return mipScale(header_data.v2.pixelHeight, mipLevel); return mipScale(header_data.v1.pixelHeight, mipLevel); }
	uint32_t const     depth(uint32_t const mipLevel) const { if constexpr (KTX_VERSION::KTX2 == version) return mipScale(header_data.v2.pixelDepth, mipLevel); return mipScale(header_data.v1.pixelDepth, mipLevel); }

	// only these image formats are supported natively for ktx to Imaging, **only the first miplevel is uploaded to an ImagingMemoryInstance**
	ImagingMemoryInstance* const __restrict upload(uint8_t const* const __restrict pFileBegin) const {

		switch (format()) 
		{
		case MODE_BGRA16:
			return(ImagingLoadFromMemoryBGRA16(pFileBegin + offset(0, 0, 0), width(0), height(0)));
		case MODE_RGB16: // promote to BGRX16, so that the returned image is consistent and the end user never has to work if it's RGB or BGRX or BGRA, it's always BGRX or BGRA when dealing with these images. BGRX and BGRA are the same, in memory usage.
		{
			Imaging image(ImagingLoadFromMemoryRGB16(pFileBegin + offset(0, 0, 0), width(0), height(0)));
			Imaging const promoted_image(ImagingRGB16ToBGRX16(image));
			ImagingDelete(image); image = nullptr;
			return(promoted_image);
		}
		case MODE_BGRA:
			return(ImagingLoadFromMemoryBGRA(pFileBegin + offset(0, 0, 0), width(0), height(0)));
		case MODE_RGB: // promote to BGRX, so that the returned image is consistent and the end user never has to work if it's RGB or BGRX or BGRA, it's always BGRX or BGRA when dealing with these images. BGRX and BGRA are the same, in memory usage.
		{
			Imaging image(ImagingLoadFromMemoryRGB(pFileBegin + offset(0, 0, 0), width(0), height(0)));
			Imaging const promoted_image(ImagingRGBToBGRX(image));
			ImagingDelete(image); image = nullptr;
			return(promoted_image);
		}
		case MODE_LA16:
			return(ImagingLoadFromMemoryLA16(pFileBegin + offset(0, 0, 0), width(0), height(0)));
		case MODE_LA:
			return(ImagingLoadFromMemoryLA(pFileBegin + offset(0, 0, 0), width(0), height(0)));
		case MODE_L16:
			return(ImagingLoadFromMemoryL16(pFileBegin + offset(0, 0, 0), width(0), height(0)));
		case MODE_L:
			return(ImagingLoadFromMemoryL(pFileBegin + offset(0, 0, 0), width(0), height(0)));
		default:
			break;
		}

		return(nullptr);
	}
private:
	static inline void swap(uint32_t& value) {
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

	struct HeaderV2 {
		uint8_t identifier[12];
		uint32_t format;
		uint32_t typeSize;
		uint32_t pixelWidth;
		uint32_t pixelHeight;
		uint32_t pixelDepth;
		uint32_t numberOfArrayElements; // layer count / number of array elements
		uint32_t numberOfFaces;
		uint32_t numberOfMipmapLevels;
		uint32_t supercompressionScheme;
		uint32_t dfdOffset;
		uint32_t dfdLength;
		uint32_t kvdOffset;
		uint32_t kvdLength;
		uint64_t sgdOffset;
		uint64_t sgdLength;
	};

	struct {
		Header   v1{};
		HeaderV2 v2{};
	} header_data{};

	eIMAGINGMODE format_;
	uint32_t vkformat_; // aka. vk::Format
	bool ok_ = false;
	std::vector<uint32_t> imageOffsets_;
	std::vector<uint32_t> imageSizes_;
	std::vector<uint32_t> layerImageSizes_;
};




