#pragma once
#include <stdint.h>
#include <random>
#include <cmath>

// MUST define RANDOM_IMPLEMENTATION in only ONE c file before include

// define DETERMINISTIC_KEY_SEED as a uint32 value if program key seed can't be random
// good for creating the same numbers across two seperate machines
// otherwise reccomend using random program key seed if isolated to one machine
/*
eg.) in rng.c file:
	#define DETERMINISTIC_KEY_SEED 0xC00C1001
	#include "superrandom.h"
*/

#ifndef DETERMINISTIC_KEY_SEED
#define RANDOM_KEY_SEED 1
#define DETERMINISTIC_KEY_SEED 0
#else
#define RANDOM_KEY_SEED 0
#endif

#ifdef RANDOM_IMPLEMENTATION
void InitializeRandomNumberGenerators();
#endif //RANDOM_IMPLEMENTATION

// These Functions are for the Psuedo RNG - Deterministic by setting seed value
void PsuedoResetSeed();
void PsuedoSetSeed(int32_t const Seed);
float const PsuedoRandomFloat(/* Range is 0.0f to 1.0f*/);
int32_t const PsuedoRandomNumber(int32_t const iMin = 0, int32_t const iMax = UINT32_MAX);
int32_t const PsuedoRandomNumber16(int32_t const iMin = 0, int32_t const iMax = UINT16_MAX);
bool const PsuedoRandom5050(void);

// deprecated functions PsuedoRandomNumber32, PsuedoRandomNumber8 left for compatability
uint32_t const PsuedoRandomNumber32(uint32_t const uiMin = 0, uint32_t const uiMax = UINT32_MAX);
uint8_t const PsuedoRandomNumber8(uint8_t const uiMin = 0, uint8_t const uiMax = UINT8_MAX);

void HashSetSeed(int32_t const Seed);
uint32_t const Hash(int32_t const data);


/// ############# IMPLK #################### //

/* USER CODE BEGIN 0 */
typedef struct sRandom
{
	uint32_t	hashSeed, reservedSeed;			// hashSeed is changeable, do not change reservedSed after initial INIT!!

	static uint32_t xorshift_state[4]; // seeds must be non zero

} Random;

#ifndef RANDOM_IMPLEMENTATION
extern Random oRandom;
#else

/* BEGIN IMPLEMENTATION */
static Random oRandom;
uint32_t sRandom::xorshift_state[4] = { 1,1,1,1 }; // seeds must be non zero

//updated to use new algo: xoshiro128**
static __inline uint32_t const rotl(uint32_t const x, int const k) {
	return((x << k) | (x >> (32 - k)));
}

static __inline uint32_t const xorshift_next(void) {
	uint32_t const result_starstar = rotl(sRandom::xorshift_state[0] * 5, 7) * 9;

	uint32_t const t = sRandom::xorshift_state[1] << 9;

	sRandom::xorshift_state[2] ^= sRandom::xorshift_state[0];
	sRandom::xorshift_state[3] ^= sRandom::xorshift_state[1];
	sRandom::xorshift_state[1] ^= sRandom::xorshift_state[2];
	sRandom::xorshift_state[0] ^= sRandom::xorshift_state[3];

	sRandom::xorshift_state[2] ^= t;

	sRandom::xorshift_state[3] = rotl(sRandom::xorshift_state[3], 11);

	return(result_starstar - 1);	// avoids a strangle failure case by using the - 1
}

// internally used for distribution on range, 16bit numbers will be faster on ARM, but 32bit numbers on a x64 Intel will be faster
// this avoids using the modulo operator (no division), ref: https://github.com/lemire/fastrange
static __inline uint32_t const RandomNumber_Limit_32(uint32_t const xrandx, uint32_t const uiMax) { // 1 to UINT32_MAX
	return((uint32_t)(((uint64_t)xrandx * (uint64_t)uiMax) >> 32));
}
static __inline uint16_t const RandomNumber_Limit_16(uint16_t const xrandx, uint16_t const uiMax) { // 1 to 65535
	return((uint16_t)(((uint32_t)xrandx * (uint32_t)uiMax) >> 16));
}
static __inline int32_t const RandomNumber_Limits_32(uint32_t const xrandx, int32_t const iMin, int32_t const iMax)
{
	return(((int32_t)RandomNumber_Limit_32(xrandx, iMax)) + iMin);
}
static __inline int32_t const RandomNumber_Limits_16(uint32_t xrandx, int32_t const iMin, int32_t const iMax)
{
	// choose high or low halfword with comparison with zero (fast) to get usuable 16bit random number
	xrandx = ((int32_t)xrandx) >= 0 ? xrandx >> 16 : xrandx;
	return(((int32_t)RandomNumber_Limit_16(xrandx, iMax)) + iMin);
}

void PsuedoSetSeed(int32_t const Seed)
{
	// save temp of current hashSeed
	uint32_t const tmpSaveHashSeed(oRandom.hashSeed);

	// set hashSeed temporarily to the static reservedSeed (used to maintain deterministic property of psuedo rng)
	oRandom.hashSeed = oRandom.reservedSeed;

	// perform high quality hash on user desired seed for psuedo rng before seeding the rng (quality increase++++)
	sRandom::xorshift_state[0] = sRandom::xorshift_state[1] = sRandom::xorshift_state[2] = sRandom::xorshift_state[3] = (Hash(Seed)) + 1; // psuedo rng seeds must be non zero
	xorshift_next(); // discard first value of rng sequence

					 // restore originally saved  hash seed
	oRandom.hashSeed = tmpSaveHashSeed;
}
void PsuedoResetSeed()
{
	PsuedoSetSeed(oRandom.reservedSeed);
}
float const PsuedoRandomFloat(/* Range is 0.0f to 1.0f*/)
{
	union
	{
		uint32_t const i;
		float const f;
	} const pun = { 0x3F800000U | (xorshift_next() >> 9) };

	return(pun.f - 1.0f);

}

int32_t const PsuedoRandomNumber(int32_t const iMin, int32_t const iMax)
{
	if (iMin == iMax) return(iMin);
	return(RandomNumber_Limits_32(xorshift_next(), iMin, iMax));
}
int32_t const PsuedoRandomNumber16(int32_t const iMin, int32_t const iMax)
{
	if (iMin == iMax) return(iMin);
	return(RandomNumber_Limits_16(xorshift_next(), iMin, iMax));
}
bool const PsuedoRandom5050(void)
{
	static constexpr int32_t const LIMIT5050 = 4095;
	return(RandomNumber_Limit_32(-LIMIT5050, LIMIT5050) < 0);
}
// deprecated functions PsuedoRandomNumber32, PsuedoRandomNumber8 left for compatability
uint32_t const PsuedoRandomNumber32(uint32_t const uiMin, uint32_t const uiMax)
{
	return(PsuedoRandomNumber(uiMin, uiMax));
}
uint8_t const PsuedoRandomNumber8(uint8_t const uiMin, uint8_t const uiMax)
{
	return(PsuedoRandomNumber(uiMin, uiMax));
}

/*public override uint GetHash (int buf) {    // xxHash Function https://blogs.unity3d.com/2015/01/07/a-primer-on-repeatable-random-numbers/
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
void HashSetSeed(int32_t const Seed)
{
	oRandom.hashSeed = (uint32_t)Seed;
}
uint32_t const Hash(int32_t const data)
{
	static constexpr uint32_t const PRIME32_1 = 2654435761U;
	static constexpr uint32_t const PRIME32_2 = 2246822519U;
	static constexpr uint32_t const PRIME32_3 = 3266489917U;
	static constexpr uint32_t const PRIME32_4 = 668265263U;
	static constexpr uint32_t const PRIME32_5 = 374761393U;

	uint32_t h32 = (uint32_t)oRandom.hashSeed + PRIME32_5;
	h32 += 4U;
	h32 += (uint32_t)data * PRIME32_3;
	h32 = rotl(h32, 17) * PRIME32_4;
	h32 ^= h32 >> 15;
	h32 *= PRIME32_2;
	h32 ^= h32 >> 13;
	h32 *= PRIME32_3;
	h32 ^= h32 >> 16;

	return(h32);
}

void InitializeRandomNumberGenerators()
{
#if (0 != RANDOM_KEY_SEED)
	static constexpr uint32_t DUMP_ITERATIONS = 5;			// go thru both sides of buffer and fill values n/2 - 1 times
															// Seed 16 numbers to randomize startup a little
	srand(((UINT32_MAX >> 1) - 1));

	uint32_t uiDump(DUMP_ITERATIONS);
	while (0 != uiDump) {

		oRandom.reservedSeed = rand();			// dumping into reservedSeed, initting reserveSeed with a random number
		--uiDump;
	}
	oRandom.reservedSeed = oRandom.reservedSeed + 1; // seeds must be nonzero
#else
	oRandom.reservedSeed = DETERMINISTIC_KEY_SEED;
#endif
	oRandom.hashSeed = oRandom.reservedSeed;	// reserved seed is used for the hashing of seeds pushed into the psuedo rng, initial hashSeed 
												// is set to this aswell but can be changed, whereas reserved seed cannot to maintain deterministic psuedo rngs
	PsuedoResetSeed();
}

#endif //RANDOM_IMPLEMENTATION


