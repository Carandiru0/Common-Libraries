#pragma once

// Optimized AVX2 implementation of Recursive Bilateral Filter
// 
#define RBF_MAX_THREADS 8
#define STAGE_BUFFER_COUNT 3

#define EDGE_COLOR_USE_MAXIMUM 0			// results in much smoother background gradients at loss of some detail - excellent for vectorization

#define EDGE_COLOR_USE_ADDITION	1			// results in detail preserving backgroubd gradients, may be acceptable for vectorization

#define EDGE_GRAYSCALE 2					// same as maximum, just optimized cause all components are equal

// if EDGE_COLOR_USE_MAXIMUM is defined, then edge color detection works by calculating
// maximum difference among 3 components (RGB) of 2 colors, which tends to result in lower differences (since only largest among 3 is selected)
// if EDGE_COLOR_USE_ADDITION is defined, then edge color detection works by calculating
// sum of all 3 components, while enforcing 255 maximum. This method is much more sensitive to small differences 

template<uint32_t const edge_detection = EDGE_COLOR_USE_MAXIMUM, uint32_t const thread_count = RBF_MAX_THREADS>
class CRBFilterAVX2
{
	int				m_reserved_width = 0;
	int				m_reserved_height = 0;

	float			m_sigma_spatial = 0.f;
	float			m_sigma_range = 0.f;
	float			m_inv_alpha_f = 0.f;
	float*			m_range_table = nullptr;

	int				m_filter_counter = 0; // used in pipelined mode
	unsigned char*	m_stage_buffer[STAGE_BUFFER_COUNT] = { nullptr }; // size width * height * 4, others are null if not pipelined
	float**			m_h_line_cache = nullptr; // line cache for horizontal filter pass, 1 per thread
	float**			m_v_line_cache = nullptr; // line cache for vertical filter pass, 1 per thread
	unsigned char*	m_out_buffer[STAGE_BUFFER_COUNT] = { nullptr }; // used for keeping track of current output buffer in pipelined mode 
	int				m_image_width = 0; // cache of sizes for pipelined mode
	int				m_image_height = 0;
	int				m_image_pitch = 0;

	// core filter functions
	void horizontalFilter(int const thread_index, const unsigned char* __restrict img_src, unsigned char* __restrict img_dst, int const width, int const height, int const pitch);
	void verticalFilter(int const thread_index, const unsigned char* __restrict img_src, unsigned char* __restrict img_dst, int const width, int const height, int const pitch);

public:

	CRBFilterAVX2();
	~CRBFilterAVX2();

	// given specified image width, return optimal row size in bytes that has been rounded up to better fit YMM registers
	// image buffers should use this pitch for input and output
	static int const getOptimalPitch(int width, int bytesperpixel);

	// 'sigma_spatial' - unlike the original implementation of Recursive Bilateral Filter, 
	// the value if sigma_spatial is not influence by image width/height.
	// In this implementation, sigma_spatial is assumed over image width 255, height 255
	void setSigma(float sigma_spatial, float sigma_range);

	// Source and destination images are assumed to be 4 component
	// 'width' - maximum image width
	// 'height' - maximum image height
	// Return true if successful, had very basic error checking
	bool const initialize(int width, int height);
	
	// de-initialize, free memory
	void release();

	// synchronous filter function, returns only when everything finished, goes faster if there's multiple threads
	// initialize() and setSigma() should be called before this
	// 'out_data' - output image buffer, assumes 4 byte per pixel
	// 'in_data' - input image buffer, assumes 4 byte per pixel
	// 'width' - width of both input and output buffers, must be same for both
	// 'height' - height of both input and output buffers, must be same for both
	// 'pitch' - row size in bytes, must be same for both buffers (ideally, this should be divisible by 16)
	// return false if failed for some reason
	bool const filter(unsigned char* __restrict out_data, const unsigned char* __restrict in_data, int const width, int const height, int const pitch);

private:
	static __inline __declspec(noalias) void __vectorcall getDiffFactor3x(__m256i pix8, __m256i pix8p, __m256i* diff8x);
};

// example of edge color difference calculation from original implementation
// idea is to fit maximum edge color difference as single number in 0-255 range
// colors are added then 2 components are scaled 4x while 1 complement is scaled 2x
// this means 1 of the components is more dominant 

//int getDiffFactor(const unsigned char* color1, const unsigned char* color2)
//{
//	int c1 = abs(color1[0] - color2[0]);
//	int c2 = abs(color1[1] - color2[1]);
//	int c3 = abs(color1[2] - color2[2]);
//
//	return ((c1 + c3) >> 2) + (c2 >> 1);
//}

template<uint32_t const edge_detection, uint32_t const thread_count>
__inline __declspec(noalias) void __vectorcall CRBFilterAVX2<edge_detection, thread_count>::getDiffFactor3x(__m256i pix8, __m256i pix8p, __m256i* diff8x)
{
	__m256i const byte_mask = _mm256_set1_epi32(255);
	__m256i diff;

	if constexpr (EDGE_COLOR_USE_MAXIMUM == edge_detection) {
		// get absolute difference for each component per pixel
		diff = _mm256_sub_epi8(_mm256_max_epu8(pix8, pix8p), _mm256_min_epu8(pix8, pix8p));

		// get maximum of 3 components
		__m256i diff_shift1 = _mm256_srli_epi32(diff, 8); // 2nd component
		diff = _mm256_max_epu8(diff, diff_shift1);
		diff_shift1 = _mm256_srli_epi32(diff_shift1, 8); // 3rd component
		diff = _mm256_max_epu8(diff, diff_shift1);
		// skip alpha component
	}

	if constexpr (EDGE_COLOR_USE_ADDITION == edge_detection) {
		// get absolute difference for each component per pixel
		diff = _mm256_sub_epi8(_mm256_max_epu8(pix8, pix8p), _mm256_min_epu8(pix8, pix8p));

		// add all component differences and saturate 
		__m256i diff_shift1 = _mm256_srli_epi32(diff, 8); // 2nd component
		diff = _mm256_adds_epu8(diff, diff_shift1);
		diff_shift1 = _mm256_srli_epi32(diff_shift1, 8); // 3rd component
		diff = _mm256_adds_epu8(diff, diff_shift1);
	}

	if constexpr (EDGE_GRAYSCALE == edge_detection) {
		// All components are equal optimization 
		diff = _mm256_sub_epi8(_mm256_max_epu8(pix8, pix8p), _mm256_min_epu8(pix8, pix8p));
	}

	diff = _mm256_and_si256(diff, byte_mask); // zero out all but 1st byte
	_mm256_store_si256(diff8x, diff);
}


//template inline include
#include "RBFilter_AVX2.inl"


