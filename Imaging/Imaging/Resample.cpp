#include "stdafx.h"
#include "Imaging.h"

#include <stdint.h>
#include <emmintrin.h>
#include <mmintrin.h>
#include <smmintrin.h>

#if defined(__AVX2__)
    #include <immintrin.h>
#endif

#include "ResampleSIMD.h"
#include <tbb/scalable_allocator.h>

struct filter {
    double const (*_filter)(double x);
    double const support;
};

static inline double const box_filter(double x)
{
    if (x > -0.5 && x <= 0.5)
        return 1.0;
    return 0.0;
}

static inline double const bilinear_filter(double x)
{
    if (x < 0.0)
        x = -x;
    if (x < 1.0)
        return 1.0-x;
    return 0.0;
}

static inline double const hamming_filter(double x)
{
    if (x < 0.0)
        x = -x;
    if (x == 0.0)
        return 1.0;
    if (x >= 1.0)
        return 0.0;
    x = x * M_PI;
    return sin((float)x) / x * (0.54 + 0.46 * cos((float)x));
}

static inline double const bicubic_filter(double x)
{
    /* https://en.wikipedia.org/wiki/Bicubic_interpolation#Bicubic_convolution_algorithm */
#define a -0.5
    if (x < 0.0)
        x = -x;
    if (x < 1.0)
        return ((a + 2.0) * x - (a + 3.0)) * x*x + 1;
    if (x < 2.0)
        return (((x - 5) * x + 8) * x - 4) * a;
    return 0.0;
#undef a
}

static inline double const sinc_filter(double x)
{
    if (x == 0.0)
        return 1.0;
    x = x * M_PI;
    return sin((float)x) / x;
}

static inline double const lanczos_filter(double x)
{
    /* truncated sinc */
    if (-3.0 <= x && x < 3.0)
        return sinc_filter(x) * sinc_filter(x/3);
    return 0.0;
}

namespace { // local to this file only

    constinit static inline struct filter BOX = { box_filter, 0.5 };
    constinit static inline struct filter BILINEAR = { bilinear_filter, 1.0 };
    constinit static inline struct filter HAMMING = { hamming_filter, 1.0 };
    constinit static inline struct filter BICUBIC = { bicubic_filter, 2.0 };
    constinit static inline struct filter LANCZOS = { lanczos_filter, 3.0 };
} // end ns

/* 8 bits for result. Filter can have negative areas.
   In one cases the sum of the coefficients will be negative,
   in the other it will be more than 1.0. That is why we need
   two extra bits for overflow and int type. */
#define PRECISION_BITS (32 - 8 - 2)


/* We use signed int16_t type to store coefficients. */
#define MAX_COEFS_PRECISION (16 - 1)

namespace { // local to this file only

    constinit static inline uint8_t const _lookups[512] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
        16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
        32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
        48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
        64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
        80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
        96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
        112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127,
        128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143,
        144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
        160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175,
        176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191,
        192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207,
        208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223,
        224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
        240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255
    };

    constinit static inline uint8_t const* const lookups = &_lookups[128];

} // end ns

static inline uint8_t clip8(int in, int precision)
{
    return(lookups[in >> precision]);
}


static int
precompute_coeffs(int const inSize, int const outSize, struct filter * const __restrict filterp,
                  int ** const __restrict xboundsp, double ** const __restrict kkp) {
    double support, scale, filterscale;
    double center, ww, ss;
    int xx, x, kmax, xmin, xmax;
    int *xbounds;
    double *kk, *k;

    /* prepare for horizontal stretch */
    filterscale = scale = (double) inSize / outSize;
    if (filterscale < 1.0) {
        filterscale = 1.0;
    }

    /* determine support size (length of resampling filter) */
    support = filterp->support * filterscale;

    /* maximum number of coeffs */
    kmax = (int)SFM::ceil(support) * 2 + 1;

    // check for overflow
    if (outSize > INT32_MAX / (kmax * sizeof(double)))
        return 0;

    /* coefficient buffer */
    /* malloc check ok, overflow checked above */
    kk = (double*)scalable_malloc(outSize * kmax * sizeof(double));
    if ( ! kk)
        return 0;

    /* malloc check ok, kmax*sizeof(double) > 2*sizeof(int) */
    xbounds = (int*)scalable_malloc(outSize * 2 * sizeof(int));
    if ( ! xbounds) {
        scalable_free(kk);
        return 0;
    }

    for (xx = 0; xx < outSize; xx++) {
        center = (xx + 0.5) * scale;
        ww = 0.0;
        ss = 1.0 / filterscale;
        // Round the value
        xmin = (int) (center - support + 0.5);
        if (xmin < 0)
            xmin = 0;
        // Round the value
        xmax = (int) (center + support + 0.5);
        if (xmax > inSize)
            xmax = inSize;
        xmax -= xmin;
        k = &kk[xx * kmax];
        for (x = 0; x < xmax; x++) {
            double w = filterp->_filter((x + xmin - center + 0.5) * ss);
            k[x] = w;
            ww += w;
        }
        for (x = 0; x < xmax; x++) {
            if (ww != 0.0)
                k[x] /= ww;
        }
        // Remaining values should stay empty if they are used despite of xmax.
        for (; x < kmax; x++) {
            k[x] = 0;
        }
        xbounds[xx * 2 + 0] = xmin;
        xbounds[xx * 2 + 1] = xmax;
    }
    *xboundsp = xbounds;
    *kkp = kk;
    return kmax;
}


static int
normalize_coeffs(int const outSize, int const kmax, double * const __restrict prekk, int16_t ** const __restrict kkp)
{
    int x;
    int16_t *kk;
    int coefs_precision;
    double maxkk;

    /* malloc check ok, overflow checked in precompute_coeffs */
    kk = (int16_t*)scalable_malloc(outSize * kmax * sizeof(int16_t));
    if ( ! kk) {
        return 0;
    }

    maxkk = prekk[0];
    for (x = 0; x < outSize * kmax; x++) {
        if (maxkk < prekk[x]) {
            maxkk = prekk[x];
        }
    }

    for (coefs_precision = 0; coefs_precision < PRECISION_BITS; coefs_precision += 1) {
        int next_value = (int)(0.5 + maxkk * (1 << (coefs_precision + 1)));
        // The next value will be outside of the range, so just stop
        if (next_value >= (1 << MAX_COEFS_PRECISION))
            break;
    }

    for (x = 0; x < outSize * kmax; x++) {
        if (prekk[x] < 0) {
            kk[x] = (int) (-0.5 + prekk[x] * (1 << coefs_precision));
        } else {
            kk[x] = (int) (0.5 + prekk[x] * (1 << coefs_precision));
        }
    }

    *kkp = kk;
    return coefs_precision;
}


static Imaging
ImagingResampleHorizontal_8bpc(ImagingMemoryInstance const* const __restrict imIn, int const xsize, struct filter * const __restrict filterp)
{
	Imaging imOut;
	int ss0;
	int xx, yy, x, kmax, xmin, xmax;
	int *xbounds;
	int16_t *k, *kk;
	double *prekk;
	int coefs_precision;

	kmax = precompute_coeffs(imIn->xsize, xsize, filterp, &xbounds, &prekk);
	if (!kmax) {
		return (Imaging)ImagingError_MemoryError();
	}

	coefs_precision = normalize_coeffs(xsize, kmax, prekk, &kk);
    scalable_free(prekk);
	if (!coefs_precision) {
        scalable_free(xbounds);
		return (Imaging)ImagingError_MemoryError();
	}

	imOut = ImagingNew(imIn->mode, xsize, imIn->ysize);
	if (!imOut) {
        scalable_free(kk);
        scalable_free(xbounds);
		return NULL;
	}

{
	if (imIn->image8) {
		for (yy = 0; yy < imOut->ysize; yy++) {
			for (xx = 0; xx < xsize; xx++) {
				xmin = xbounds[xx * 2 + 0];
				xmax = xbounds[xx * 2 + 1];
				k = &kk[xx * kmax];
				ss0 = 1 << (coefs_precision - 1);
				for (x = 0; x < xmax; x++)
					ss0 += ((uint8_t)imIn->image8[yy][x + xmin]) * k[x];
				imOut->image8[yy][xx] = clip8(ss0, coefs_precision);
			}
		}
	}
	else if (imIn->type == IMAGING_TYPE_UINT8) {
		yy = 0;
		for (; yy < imOut->ysize - 3; yy += 4) {
			ImagingResampleHorizontalConvolution8u4x(
				(uint32_t *)imOut->image32[yy],
				(uint32_t *)imOut->image32[yy + 1],
				(uint32_t *)imOut->image32[yy + 2],
				(uint32_t *)imOut->image32[yy + 3],
				(uint32_t *)imIn->image32[yy],
				(uint32_t *)imIn->image32[yy + 1],
				(uint32_t *)imIn->image32[yy + 2],
				(uint32_t *)imIn->image32[yy + 3],
				xsize, xbounds, kk, kmax,
				coefs_precision
			);
		}
		for (; yy < imOut->ysize; yy++) {
			ImagingResampleHorizontalConvolution8u(
				(uint32_t *)imOut->image32[yy],
				(uint32_t *)imIn->image32[yy],
				xsize, xbounds, kk, kmax,
				coefs_precision
			);
		}
	}

}
    scalable_free(kk);
    scalable_free(xbounds);
    return imOut;
}


static Imaging
ImagingResampleVertical_8bpc(ImagingMemoryInstance const* const __restrict imIn, int const ysize, struct filter * const __restrict filterp)
{
	Imaging imOut;
    int ss0;
    int xx, yy, y, kmax, ymin, ymax;
    int *xbounds;
    int16_t *k, *kk;
    double *prekk;
    int coefs_precision;

    kmax = precompute_coeffs(imIn->ysize, ysize, filterp, &xbounds, &prekk);
    if ( ! kmax) {
        return (Imaging) ImagingError_MemoryError();
    }
    
    coefs_precision = normalize_coeffs(ysize, kmax, prekk, &kk);
    scalable_free(prekk);
    if ( ! coefs_precision) {
        scalable_free(xbounds);
        return (Imaging) ImagingError_MemoryError();
    }

    imOut = ImagingNew(imIn->mode, imIn->xsize, ysize);
    if ( ! imOut) {
        scalable_free(kk);
        scalable_free(xbounds);
        return NULL;
    }

{
    if (imIn->image8) {
        for (yy = 0; yy < ysize; yy++) {
            k = &kk[yy * kmax];
            ymin = xbounds[yy * 2 + 0];
            ymax = xbounds[yy * 2 + 1];
            for (xx = 0; xx < imOut->xsize; xx++) {
                ss0 = 1 << (coefs_precision -1);
                for (y = 0; y < ymax; y++)
                    ss0 += ((uint8_t) imIn->image8[y + ymin][xx]) * k[y];
                imOut->image8[yy][xx] = clip8(ss0, coefs_precision);
            }
        }
    } else if (imIn->type == IMAGING_TYPE_UINT8) {
        for (yy = 0; yy < ysize; yy++) {
            k = &kk[yy * kmax];
            ymin = xbounds[yy * 2 + 0];
            ymax = xbounds[yy * 2 + 1];
            ImagingResampleVerticalConvolution8u(
                (uint32_t *) imOut->image32[yy], imIn,
                ymin, ymax, k, coefs_precision
            );
        }
    }

}
    scalable_free(kk);
    scalable_free(xbounds);
    return imOut;
}


static Imaging
ImagingResampleHorizontal_32bpc(ImagingMemoryInstance const* const __restrict imIn, int const xsize, struct filter * const __restrict filterp)
{
	Imaging imOut;
	double ss;
	int xx, yy, x, kmax, xmin, xmax;
	int *xbounds;
	double *k, *kk;

	kmax = precompute_coeffs(imIn->xsize, xsize, filterp, &xbounds, &kk);
	if (!kmax) {
		return (Imaging)ImagingError_MemoryError();
	}

	imOut = ImagingNew(imIn->mode, xsize, imIn->ysize);
	if (!imOut) {
        scalable_free(kk);
        scalable_free(xbounds);
		return NULL;
	}

{
		for (yy = 0; yy < imOut->ysize; yy++) {
			for (xx = 0; xx < xsize; xx++) {
				xmin = xbounds[xx * 2 + 0];
				xmax = xbounds[xx * 2 + 1];
				k = &kk[xx * kmax];
				ss = 0.0;
				for (x = 0; x < xmax; x++)
					ss += IMAGING_PIXEL_I(imIn, x + xmin, yy) * k[x];
				IMAGING_PIXEL_I(imOut, xx, yy) = SFM::round_to_u32(SFM::abs(ss));
			}
		}
}
    scalable_free(kk);
    scalable_free(xbounds);
    return imOut;
}


static Imaging
ImagingResampleVertical_32bpc(ImagingMemoryInstance const* const __restrict imIn, int const ysize, struct filter * const __restrict filterp)
{
	Imaging imOut;
	double ss;
	int xx, yy, y, kmax, ymin, ymax;
	int *xbounds;
	double *k, *kk;

	kmax = precompute_coeffs(imIn->ysize, ysize, filterp, &xbounds, &kk);
	if (!kmax) {
		return (Imaging)ImagingError_MemoryError();
	}

	imOut = ImagingNew(imIn->mode, imIn->xsize, ysize);
	if (!imOut) {
        scalable_free(kk);
        scalable_free(xbounds);
		return NULL;
	}

{
	for (yy = 0; yy < ysize; yy++) {
		ymin = xbounds[yy * 2 + 0];
		ymax = xbounds[yy * 2 + 1];
		k = &kk[yy * kmax];
		for (xx = 0; xx < imOut->xsize; xx++) {
			ss = 0.0;
			for (y = 0; y < ymax; y++)
				ss += IMAGING_PIXEL_I(imIn, xx, y + ymin) * k[y];
			IMAGING_PIXEL_I(imOut, xx, yy) = SFM::round_to_u32(SFM::abs(ss));
		}
	}
}
    scalable_free(kk);
    scalable_free(xbounds);
    return imOut;
}


ImagingMemoryInstance* const __restrict __vectorcall
ImagingResample(ImagingMemoryInstance const* const __restrict imIn, int const xsize, int const ysize, int const filter)
{
    struct filter *filterp;
    Imaging (*ResampleHorizontal)(ImagingMemoryInstance const* const __restrict imIn, int const xsize, struct filter * const __restrict filterp);
    Imaging (*ResampleVertical)(ImagingMemoryInstance const* const __restrict imIn, int const xsize, struct filter * const __restrict filterp);

    if ((MODE_L) & imIn->mode)
        return (Imaging) ImagingError_ModeError();

    if (IMAGING_TYPE_SPECIAL == imIn->type) {
        return (Imaging) ImagingError_ModeError();
    } else if (imIn->image8) {
        ResampleHorizontal = ImagingResampleHorizontal_8bpc;
        ResampleVertical = ImagingResampleVertical_8bpc;
    } else {
        switch(imIn->type) {
            case IMAGING_TYPE_UINT8:
                ResampleHorizontal = ImagingResampleHorizontal_8bpc;
                ResampleVertical = ImagingResampleVertical_8bpc;
                break;
            case IMAGING_TYPE_UINT32:
                ResampleHorizontal = ImagingResampleHorizontal_32bpc;
                ResampleVertical = ImagingResampleVertical_32bpc;
                break;
            default:
                return (Imaging) ImagingError_ModeError();
        }
    }

    /* check filter */
    switch (filter) {
    case IMAGING_TRANSFORM_BOX:
        filterp = &BOX;
        break;
    case IMAGING_TRANSFORM_BILINEAR:
        filterp = &BILINEAR;
        break;
    case IMAGING_TRANSFORM_HAMMING:
        filterp = &HAMMING;
        break;
    case IMAGING_TRANSFORM_BICUBIC:
        filterp = &BICUBIC;
        break;
    case IMAGING_TRANSFORM_LANCZOS:
        filterp = &LANCZOS;
        break;
    default:
        return (Imaging) ImagingError_ValueError(
            "unsupported resampling filter"
            );
    }

    Imaging imOut(nullptr);

    /* two-pass resize, first pass */
    if (imIn->xsize != xsize) {

        imOut = ResampleHorizontal(imIn, xsize, filterp);
        if (!imOut)
            return nullptr;
    }

    /* second pass */
    if (imIn->ysize != ysize) {

        ImagingMemoryInstance const* imTemp(imOut);

        /* imIn can be the original image or horizontally resampled one */
        imOut = ResampleVertical((imOut ? imOut : imIn), ysize, filterp);
        if (imTemp) {
            ImagingDelete(imTemp); imTemp = nullptr;
        }
        if (!imOut)
            return nullptr;
    }

    /* if none of the previous steps are performed, image size is no different than source image*/
    if (!imOut) {
        imOut = ImagingCopy(imIn);
    }

    return imOut;
}



