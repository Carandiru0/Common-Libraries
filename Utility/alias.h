/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */
#pragma once

#include "..\Random\superrandom.hpp"
#include <tbb/scalable_allocator.h>

// *** please note - the random number generator needs to be initialized before any instance of alias_t is created, otherwise behaviour is undefined.

template<typename T>
class alias_t
{
public:
	__inline bool const ok() const {

		if (nullptr == (*memory_location))
			return(false);

		return(Hash((int64_t)(uintptr_t)(*memory_location), seed) == id);
	}

	__inline T* const operator*() const {
		return(*memory_location);
	}

	__inline T* const operator->() const {
		return(*memory_location);
	}

public:
	alias_t(T* const* const memory_location_) // pass by reference your pointer you want to alias
		: memory_location(memory_location_), seed(PsuedoRandomNumber64()), id(Hash((int64_t)(uintptr_t)(*memory_location_), seed))
	{
	}
	alias_t(alias_t&&) = default;
	alias_t& operator=(alias_t&&) = default;

private:
	alias_t(alias_t const&) = delete;
	alias_t& operator=(alias_t const&) = delete;

private:
	uint64_t seed, id;
	T* const* memory_location;
};

