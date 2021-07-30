#pragma once
#include <stdint.h>

// MUST define RANDOM_IMPLEMENTATION in only ONE c file before include

// define DETERMINISTIC_KEY_SEED as a uint32 value if program key seed can't be random
// good for creating the same numbers across two seperate machines
// otherwise reccomend using random program key seed if isolated to one machine
/*
eg.) in rng.c file:                                       *(only one C file)
	#define RANDOM_IMPLEMENTATION
	#define DETERMINISTIC_KEY_SEED 0xC00C1001
	#include "superrandom.h"
*/

// ** advanced usage:
// 
// HashSetSeed( "some constant key number representing an object or thing" );
// uint64_t const hash_key = Hash( "some changing value, eg.) x coordinate" ) ^ Hash( "some changing value, eg.) y coordinate" );
// -or-
// HashSetSeed then PsuedoRandom...() , generate your deterministic random numbers
// ...
//
// note: You do *not* have to reset seed!
//
#ifndef DETERMINISTIC_KEY_SEED
#define RANDOM_KEY_SEED 1
#define DETERMINISTIC_KEY_SEED 0
#else
#define RANDOM_KEY_SEED 0
#endif

void InitializeRandomNumberGenerators(uint64_t const deterministic_seed = 0); // optional parameter for setting the main seed
uint64_t const GetSecureSeed();

// These Functions are for the Psuedo RNG - Deterministic by setting seed value with HashSetSeed

float const PsuedoRandomFloat(/* Range is 0.0f to 1.0f*/);
int64_t const PsuedoRandomNumber64(int64_t const iMin = 0, int64_t const iMax = INT64_MAX); //* do note that with a min of 0 there will be no negative numbers
int32_t const PsuedoRandomNumber32(int32_t const iMin = 0, int32_t const iMax = INT32_MAX); // inclusive of min & max
int32_t const PsuedoRandomNumber16(int32_t const iMin = 0, int32_t const iMax = INT16_MAX); // inclusive " ""  " ""
bool const PsuedoRandom5050(void);

#define PsuedoRandomNumber PsuedoRandomNumber32	// default

uint64_t const Hash(int64_t const data);
uint32_t const Hash(int32_t const data);


void HashSetSeed(int64_t const Seed);
void HashSetSeed(int32_t const Seed);

// for randomdly shuffling a std::vector or similar
template<typename RandomIt>
void random_shuffle(RandomIt&& first, RandomIt&& last)
{
	auto n = last - first; // relative size of vector/array

	// fisher-yates shuffle
	// https://bost.ocks.org/mike/shuffle/
	// uniform random distribution on a decreasing size of elements 
	// left to shuffle. O(n) operation.
	while (n) {

		// *must pre-decrement*
		// inclusive 0...n, where n < (last - first)
		// as indices, or bounds could be exceeded.
		auto const i(PsuedoRandomNumber64(0, --n)); // needs range so only really supports 32bit numbers.

		std::swap(first[n], first[i]);
	}
}

// https://blog.demofox.org/2017/11/03/animating-noise-for-integration-over-time-2-uniform-over-time/
// 1D low-discrepancy sequence generation, try base 2
// I belive that a seamless back to back sequence of both directions would lead to non-convergance
// and would defeat the purpose of using this low discrepancy sequence in that manner
// like its either 
// "moving toward" the target
// or
// "moving away" from the target
// which leads to a faster convergancem even with the sudden break between the last element and the first element starting again
// back to back seamless sequence not recommended
template <size_t const NUM_SAMPLES, size_t const base, bool const direction = false> // direction defaults to descending, true ascending
std::array<float, NUM_SAMPLES> const GenerateVanDerCoruptSequence()
{
	std::array<float, NUM_SAMPLES> samples;

	for (size_t i = 0; i < NUM_SAMPLES; ++i)
	{
		samples[i] = 0.0f;
		float denominator = float(base);
		size_t n = i;
		while (n > 0)
		{
			size_t const multiplier = n % base;
			samples[i] += float(multiplier) / denominator;
			n = n / base;
			denominator *= base;
		}
	}

	if constexpr (direction) { // ascending
		std::reverse(std::begin(samples), std::end(samples));
	}

	return(samples);
}

/// ############# IMPL #################### //
#ifdef RANDOM_IMPLEMENTATION

#if RANDOM_KEY_SEED
#include <Utility/instructionset.h>
#endif

#include <Math/superfastmath.h>
#include <intrin.h>
#pragma intrinsic(_rotl)
#pragma intrinsic(_rotl64)
#pragma intrinsic(_umul128)

/* BEGIN IMPLEMENTATION */
// begin private //
static constexpr uint64_t const XORSHIFT_STATE_RESET[4] = { 0x00000000000000D1,0x00000000000000D3,0x00000000000000D7,0x00000000000000DA };

typedef struct alignas(32) sRandom
{
	__m256i xorshift_state{ _mm256_set_epi64x(XORSHIFT_STATE_RESET[0],XORSHIFT_STATE_RESET[1],XORSHIFT_STATE_RESET[2],XORSHIFT_STATE_RESET[3]) };  // psuedo rng seeds must be non zero
																						   // *** very important - seeds must be non zero, high bits are better than low bits
																						   // * Hash * used on the state
																						   //  This is what decorrelates the different parts of the state
																						   // see: https://www.shadertoy.com/view/MdVBWK
	__m256i reserved_xorshift_state{};	// hashSeed is changeable, do not change reserved_xorshift_state after initial INIT!!


	uint64_t    reservedSeed;			// hashSeed is changeable, do not change reservedSeed after initial INIT!!

	uint64_t	hashSeed;

	static inline std::atomic_uint64_t									   count{ 0 };
	static inline std::atomic_uint64_t									   latest_instance_index{ 0 };
	static inline tbb::concurrent_unordered_map<ptrdiff_t const, sRandom*> instances;

	bool Initialized = false;
	uint64_t instance_index = count;
} Random;

// redefinition inline std::atomic_uint64_t sRandom::instance(0);

/* [xoshiro256** 1.0] */
// http://prng.di.unimi.it
/*  Written in 2018 by David Blackman and Sebastiano Vigna (vigna@acm.org)
/* This is xoshiro256** 1.0, one of our all-purpose, rock-solid
   generators. It has excellent (sub-ns) speed, a state (256 bits) that is
   large enough for any parallel application, and it passes all tests we
   are aware of.

   The state must be seeded so that it is not everywhere zero. */
static inline Random oRandomMaster;

thread_local static Random oRandom; // thread local randoms that mirror the oRandomMaster configuration
static void InitializeRandomNumberGeneratorInstance(); // forward declaration

// ** xxHash ** Function https://blogs.unity3d.com/2015/01/07/a-primer-on-repeatable-random-numbers/
// SIMD optimized variant for usage on 4 numbers simultaneously to speed up PsuedoSetSeed()
// internal usuage only //
STATIC_INLINE_PURE __m256i const __vectorcall rotlAVX2(__m256i const x, int const k) {

	//return((x << k) | (x >> (64 - k)));

	__m256i const xmReg0 = _mm256_slli_epi64(x, k);
	__m256i const xmReg1 = _mm256_srli_epi64(x, 64 - k);

	return(_mm256_or_si256(xmReg0, xmReg1));
}
// internal usuage only //
STATIC_INLINE_PURE __m256i const __vectorcall HashState(__m256i const pendingState)
{
	static constexpr uint64_t const PRIME64_2 = 0x85EBCA77C2B2AE63ULL;
	static constexpr uint64_t const PRIME64_3 = 0xC2B2AE3D27D4EB4FULL;
	static constexpr uint64_t const PRIME64_4 = 0x27D4EB2F165667C5ULL;
	static constexpr uint64_t const PRIME64_5 = 0x165667B19E3779F9ULL;

	__m256i h64( _mm256_add_epi64(_mm256_set1_epi64x(oRandom.hashSeed), _mm256_set1_epi64x(PRIME64_5)) );		// this where HashSetSeed has influence on random numbers generated going forward
	h64 = _mm256_add_epi64(h64, _mm256_set1_epi64x(4));
	h64 = _mm256_add_epi64(h64, _mm256_mullo_epi32(pendingState, _mm256_set1_epi64x(PRIME64_3)));
	__m256i const rotate(rotlAVX2(h64, 17 << 1)); // this avoids an illegal instruction being generated by compiler, leave it alone
	h64 = _mm256_mullo_epi32(rotate, _mm256_set1_epi64x(PRIME64_4));
	h64 = _mm256_xor_si256(h64, _mm256_srli_epi64(h64, 15 << 1));
	h64 = _mm256_mullo_epi32(h64, _mm256_set1_epi64x(PRIME64_2));
	h64 = _mm256_xor_si256(h64, _mm256_srli_epi64(h64, 13 << 1));
	h64 = _mm256_mullo_epi32(h64, _mm256_set1_epi64x(PRIME64_3));
	h64 = _mm256_xor_si256(h64, _mm256_srli_epi64(h64, 16 << 1));

	return(h64);
}

//updated to use new algo: xoshiro256**
//STATIC_INLINE_PURE uint64_t const rotl64(uint64_t const x, int const k) {
//	return((x << k) | (x >> (64 - k)));
//}

static __inline uint64_t const xorshift_next(void) {

	[[unlikely]] if (!oRandom.Initialized) {
		InitializeRandomNumberGeneratorInstance();
	}

	vec4_t<uint64_t, 32> state;
	_mm256_store_si256((__m256i*)state.data, HashState(oRandom.xorshift_state));

	uint64_t const result_starstar = _rotl64(state.y * 5, 7) * 9;

	uint64_t const t = state.y << 17;

	state.z ^= state.x;
	state.w ^= state.y;
	state.y ^= state.z;
	state.x ^= state.w;

	state.z ^= t;

	state.w = _rotl64(state.w, 45);

	oRandom.xorshift_state = _mm256_load_si256((__m256i*)state.data);

	Random::latest_instance_index = oRandom.instance_index;

	return(result_starstar - 1LL);	// bugfix: avoids a strangle failure case by using the - 1, still allowing zero to be returned to the caller (this is the only safe place for zero to be introduced!)
}

// internally used for distribution on range, 16bit numbers will be faster on ARM, but 32bit numbers on a x64 Intel will be faster
// this avoids using the modulo operator (no division), ref: https://github.com/lemire/fastrange
#ifndef uint128_t
struct uint128_t {
	uint64_t u64[2]; 
};
uint128_t _mul128(uint64_t const a_lo, uint64_t const b_lo)
{
	uint64_t lolo_high;
	uint64_t const lolo = _umul128(a_lo, b_lo, &lolo_high);
	return { {lolo, lolo_high} };
}
#endif
STATIC_INLINE_PURE uint64_t const RandomNumber_Limit_64(uint64_t const xrandx, uint64_t const uiMax) { // 1 to UINT32_MAX
	return((uint64_t)(_mul128(xrandx, uiMax).u64[1])); // same as shift down 64 bits, by just selecting the upper 64bits 
}
#ifdef uint128_t
#undef uint128_t
#undef mul128x64
#endif
STATIC_INLINE_PURE uint32_t const RandomNumber_Limit_32(uint32_t const xrandx, uint32_t const uiMax) { // 1 to UINT32_MAX
	return((uint32_t)(((uint64_t)xrandx * (uint64_t)uiMax) >> 32));
}
STATIC_INLINE_PURE uint16_t const RandomNumber_Limit_16(uint16_t const xrandx, uint16_t const uiMax) { // 1 to 65535
	return((uint16_t)(((uint32_t)xrandx * (uint32_t)uiMax) >> 16));
}

STATIC_INLINE_PURE int64_t const RandomNumber_Limits_64(uint64_t const xrandx, int64_t const iMin, int64_t const iMax)
{
	return(((int64_t)RandomNumber_Limit_64(xrandx, iMax - iMin)) + iMin);
}
STATIC_INLINE_PURE int32_t const RandomNumber_Limits_32(uint32_t const xrandx, int32_t const iMin, int32_t const iMax)
{
	return(((int32_t)RandomNumber_Limit_32(xrandx, iMax - iMin)) + iMin);
}
STATIC_INLINE_PURE int32_t const RandomNumber_Limits_16(uint32_t const xrandx, int32_t const iMin, int32_t const iMax)
{
	// choose high or low halfword with comparison with zero (fast) to get usuable 16bit random number
	return(((int32_t)RandomNumber_Limit_16(uint32_t(((int32_t)xrandx) >= 0 ? xrandx >> 16 : xrandx), iMax - iMin)) + iMin);
}
// end private //

float const PsuedoRandomFloat(/* Range is 0.0f to 1.0f*/)
{
	union
	{
		uint32_t const i;
		float const f;
	} const pun = { 0x3F800000U | (uint32_t(xorshift_next() >> 32ULL) >> 9U) };

	return(pun.f - 1.0f);

}

int64_t const PsuedoRandomNumber64(int64_t const iMin, int64_t const iMax)
{
	if (iMin == iMax) return(iMin);
	return(RandomNumber_Limits_64(uint64_t(xorshift_next()), iMin, iMax));
}
int32_t const PsuedoRandomNumber32(int32_t const iMin, int32_t const iMax)
{
	if (iMin == iMax) return(iMin);
	return(RandomNumber_Limits_32(uint32_t(xorshift_next() >> 32ULL), iMin, iMax));
}
int32_t const PsuedoRandomNumber16(int32_t const iMin, int32_t const iMax)
{
	if (iMin == iMax) return(iMin);
	return(RandomNumber_Limits_16(uint32_t(xorshift_next() >> 32ULL), iMin, iMax));
}
bool const PsuedoRandom5050(void)
{
	return(PsuedoRandomNumber64(INT64_MIN, INT64_MAX) < 0);
}
// deprecated functions PsuedoRandomNumber32, PsuedoRandomNumber8 left for compatability
/*DECLSPEC_DEPRECATED
uint32_t const PsuedoRandomNumber32(uint32_t const uiMin, uint32_t const uiMax)
{
	return(PsuedoRandomNumber(uiMin, uiMax));
}*/
/*DECLSPEC_DEPRECATED
uint8_t const PsuedoRandomNumber8(uint8_t const uiMin, uint8_t const uiMax)
{
	return(PsuedoRandomNumber(uiMin, uiMax));
}*/

/*public override uint GetHash (int buf) {    // ** xxHash ** Function https://blogs.unity3d.com/2015/01/07/a-primer-on-repeatable-random-numbers/
uint h32 = (uint)seed + PRIME32_5;
h32 += 4U;
h32 += (uint)buf * PRIME32_3;
h32 = RotateLeft (h32, 17) * PRIME32_4;
h32 ^= h32 >> 15;
h32 *= PRIME32_2;
h32 ^= h32 >> 13;
h32 *= PRIME32_3;
h32 ^= h32 >> 16;
return h32;
}*/

// private for internal usage only (forward decl) //
static void PsuedoSetSeed(int64_t Seed);

void HashSetSeed(int64_t Seed)
{
	[[unlikely]] if (!oRandom.Initialized) {
		InitializeRandomNumberGeneratorInstance();
	}

	// seeds must be nonzero (so important don't remove)
	if (0LL == Seed)
		++Seed;

	oRandom.hashSeed = (uint64_t)Seed;
	PsuedoSetSeed(Seed);
}

uint64_t const Hash(int64_t const data)
{
	[[unlikely]] if (!oRandom.Initialized) {
		InitializeRandomNumberGeneratorInstance();
	}

	static constexpr uint64_t const PRIME64_2 = 0x85EBCA77C2B2AE63ULL;
	static constexpr uint64_t const PRIME64_3 = 0xC2B2AE3D27D4EB4FULL;
	static constexpr uint64_t const PRIME64_4 = 0x27D4EB2F165667C5ULL;
	static constexpr uint64_t const PRIME64_5 = 0x165667B19E3779F9ULL;

	uint64_t h64 = (uint64_t)oRandom.hashSeed + PRIME64_5;
	h64 += 4U;
	h64 += (uint64_t)data * PRIME64_3;
	h64 = uint64_t(_rotl64(h64, 17 << 1)) * PRIME64_4;
	h64 ^= h64 >> (15 << 1);
	h64 *= PRIME64_2;
	h64 ^= h64 >> (13 << 1);
	h64 *= PRIME64_3;
	h64 ^= h64 >> (16 << 1);

	return(h64);
}

void HashSetSeed(int32_t Seed)
{
	[[unlikely]] if (!oRandom.Initialized) {
		InitializeRandomNumberGeneratorInstance();
	}

	// seeds must be nonzero (so important don't remove)
	if (0LL == Seed)
		++Seed;

	oRandom.hashSeed = (uint64_t)Seed;
	PsuedoSetSeed(Seed);
}

uint32_t const Hash(int32_t const data)
{
	[[unlikely]] if (!oRandom.Initialized) {
		InitializeRandomNumberGeneratorInstance();
	}

	static constexpr uint32_t const PRIME32_2 = 0x85EBCA77U;
	static constexpr uint32_t const PRIME32_3 = 0xC2B2AE3DU;
	static constexpr uint32_t const PRIME32_4 = 0x27D4EB2FU;
	static constexpr uint32_t const PRIME32_5 = 0x165667B1U;

	uint32_t h32 = (uint32_t)oRandom.hashSeed + PRIME32_5;
	h32 += 4U;
	h32 += (uint32_t)data * PRIME32_3;
	h32 = uint32_t(_rotl(h32, 17)) * PRIME32_4;
	h32 ^= h32 >> 15;
	h32 *= PRIME32_2;
	h32 ^= h32 >> 13;
	h32 *= PRIME32_3;
	h32 ^= h32 >> 16;

	return(h32);
}

// private for init only  //
static __inline void SetXorShiftState(uint64_t const seed)
{
	__m256i const pendingState(_mm256_add_epi64(_mm256_set1_epi64x(seed),
							   _mm256_set_epi64x(XORSHIFT_STATE_RESET[0], XORSHIFT_STATE_RESET[1], XORSHIFT_STATE_RESET[2], XORSHIFT_STATE_RESET[3]))); // psuedo rng seeds must be non zero
	oRandom.xorshift_state = HashState(pendingState); // high quality hash of state before handoff to xorshift state
}

// private for init only  //
static void InitializeRandomNumberGeneratorInstance()
{
	oRandom = oRandomMaster; // copy starting state (synchronize) //

	Random::instances[((ptrdiff_t)&oRandom)] = &oRandom;

	SetXorShiftState(oRandom.hashSeed);
	++Random::count;
}

// private for init only  //
static void PsuedoSetSeed(int64_t Seed)
{
	[[unlikely]] if (!oRandom.Initialized) {
		InitializeRandomNumberGeneratorInstance();
	}

	// seeds must be nonzero (so important don't remove)
	if (0LL == Seed)
		++Seed;

	uint64_t const save_hash_seed = oRandom.hashSeed;
	oRandom.hashSeed = oRandom.reservedSeed; // always use reserved seed for the pre-liminary high quality hash 

	// performs internally a high quality hash on user desired seed for psuedo rng before seeding the rng (quality increase++++)
	SetXorShiftState(Hash(Seed));
	xorshift_next(); // discard first value of rng sequence

	oRandom.hashSeed = save_hash_seed; // restore saved hash seed value
}
// private for init only  //
static void PsuedoResetSeed()
{
	oRandom.xorshift_state = oRandom.reserved_xorshift_state;
}

// Only call this function ONCE!!!
void InitializeRandomNumberGenerators(uint64_t const deterministic_seed)
{
#if (0 != RANDOM_KEY_SEED)
	if (0 == deterministic_seed) {
		static constexpr uint32_t DUMP_ITERATIONS = 5;			// go thru both sides of buffer and fill values n/2 - 1 times
																// Seed 16 numbers to randomize startup a little

		// get true random number from hardware if available
		bool use_seed(false), use_rand(false);
		uint64_t seed(0);

		use_seed = InstructionSet::RDSEED();	// broadwell+

		if (!use_seed) { //ivy bridge+, amd ryzen
			use_rand = InstructionSet::RDRAND();
		}

		if ((use_seed | use_rand)) {
			uint32_t attempts(8);

			int success(0);
			do
			{
				if (use_seed) { // function pointers aren't allowed on intrinsic functions - intrinsic functions map directly to instruction
					success = _rdseed64_step(&seed);
				}
				else {
					success = _rdrand64_step(&seed);
				}

				_mm_pause();
			} while (!success && --attempts);
		}

		// add processor tick count so if above is not supported, and too add more randomness to seed
		uint32_t uiDump(DUMP_ITERATIONS);
		while (0 != uiDump) {

			LARGE_INTEGER llCount;
			QueryPerformanceCounter(&llCount);

			//using overflow on purpose
			seed += seed + llCount.QuadPart;			// dumping into reservedSeed, initting reserveSeed with a random number

			_mm_pause();
			--uiDump;
		}
		oRandom.reservedSeed = seed;
		
	}
	else {
		oRandom.reservedSeed = deterministic_seed; // for initializing / reinitializing internal secure seed
	}
#else
	if (0 == deterministic_seed) {
		oRandom.reservedSeed = DETERMINISTIC_KEY_SEED;
	}
	else {
		oRandom.reservedSeed = deterministic_seed; // for initializing / reinitializing internal secure seed
	}
#endif

	// [placed here for security of hash alway being not equal to zero]
	if (0ULL == oRandom.reservedSeed)// seeds must be nonzero (so important don't remove)
		++oRandom.reservedSeed;

	// init step 0
	oRandom.Initialized = true; // set first so we prevent recursive initialization!
	// init step 1
	PsuedoSetSeed(oRandom.reservedSeed);
	oRandom.reserved_xorshift_state = oRandom.xorshift_state;  // only place should reserved_xorshift_state ever be "set"
	// init step 2
	oRandom.hashSeed = oRandom.reservedSeed;
	// init final step 3
	PsuedoResetSeed(); // hashSeed is set here for init, while during runtime it does reset both hashSeed to the beginning,
					   // and reserved_xorshift_state is resumed from where it was
					   // *random number sequence*
					   // thread local implementation for concurrency and is thread safe protecting determinism of sequence.

	// who ever manually calls InitializeRandomNumberGenerators on program init is the master random instance (1st thread local instance)
	// all other new thread local instances then init based off of the initial program state, saved in oRandomMaster
	oRandomMaster = oRandom;

	// for re-initialization with potentially new deterministic seed, if not has no effect but synchronizing state for other threads (thread-local access) with the master state.
	// This in effect calls to for each thread_local instance - InitializeRandomNumberGeneratorInstance() - on first usage (including primary thread).
	for (auto instance = Random::instances.cbegin(); instance != Random::instances.cend(); ++instance) {
		instance->second->Initialized = false;
	}
}

uint64_t const GetSecureSeed()
{
	[[unlikely]] if (!oRandom.Initialized) {
		return(0); // if random library is initialized return 0 indicating failure (0 is always an invalid number for a hash)
	}

	return(oRandom.reservedSeed);
}

#endif //RANDOM_IMPLEMENTATION


