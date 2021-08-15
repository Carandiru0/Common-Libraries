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

#include <atomic>
#include <cstddef>

#include <tbb/scalable_allocator.h>
//#include <Utility/asmlib.h>

#ifndef NDEBUG
#include <Utility/assert_print.h>
#endif

#ifdef ALIAS_TO_2D_ARRAY
#define USE_2D_ARRAY 1
#else
#define USE_2D_ARRAY 0
#endif

//// ######### 2D ######### //
template<typename T, uint32_t const X, uint32_t const Z>
class contiguous2D
{
#ifndef NDEBUG
	static constexpr char const* const ERROR_MESSAGE = "contiguous size exceeds capacity";
#endif
public:
	static constexpr uint32_t const capacity() { return(X*Z); }
	static constexpr size_t const   capacity_bytes() { return(X*Z * sizeof(T)); }

	uint32_t const		   size() const { return(_size); }
	size_t const		   size_bytes() const { return(_size * sizeof(T)); }

	static constexpr uint32_t const linesize() { return(X); }
	static constexpr uint32_t const linesize_bytes() { return(X * sizeof(T)); }
	static constexpr uint32_t const elementsize_bytes() { return(sizeof(T)); }

	// access - readonly
	__declspec(safebuffers) __inline T const* const& __vectorcall data() const { return(_block); }		// flat access

#if USE_2D_ARRAY
	__inline T const* const& operator[](uint32_t const Row) const		// 2D access
	{
		return(_grid[Row]);
	}
#endif
	
	// access - writeonly
	template<typename... Args>
	__declspec(safebuffers) __forceinline void __vectorcall emplace_back(uint32_t const x, uint32_t const z, Args&&... args)	// 2D emplace
	{		
		uint32_t const index_spatial(z * X + x);
		
		_block[index_spatial] = T(std::forward<Args>(args)...);
		_occupy[index_spatial] = TRUE;

		_minZ = SFM::min(z, (uint32_t)_minZ); _minX = SFM::min(x, (uint32_t)_minX);
		_maxZ = SFM::max(z, (uint32_t)_maxZ); _maxX = SFM::max(x, (uint32_t)_maxX);

#ifndef NDEBUG
		assert_print(_size <= capacity(), ERROR_MESSAGE);
#endif
		++_size;
	}

	// non-concurrent operations
	__declspec(safebuffers) uint32_t const __vectorcall flatten(T* __restrict Mapped) const  // brings all active elements to front of buffer, flattening it
	{
		uint32_t const minZ(_minZ), minX(_minX),
			           maxZ(_maxZ + 1), maxX(_maxX + 1);

		for (uint32_t z = minZ; z < maxZ; ++z) {

			uint32_t const index_spatial(z * X + minX);

			T const* __restrict block{ _block + index_spatial };
			uint8_t const* __restrict occupy{ _occupy + index_spatial };

			_mm_prefetch((const CHAR*)occupy, _MM_HINT_T0);

			for (uint32_t x = minX; x < maxX; ++x) {

				if (0 != *occupy++) {

					*Mapped = *block;
					++Mapped;
				}
				++block;
			}
		}

		return(size());
	}
	__declspec(safebuffers) void __vectorcall clear()
	{
		_size = 0; // atomic
		_minZ = UINT32_MAX;
		_minX = UINT32_MAX;
		_maxZ = 0;
		_maxX = 0;
	}

public:
	contiguous2D()
		: _block(nullptr), _occupy{}
#if USE_2D_ARRAY
		, _grid((T const**)nullptr)
#endif
	{
		static_assert(std::atomic_uint32_t::is_always_lock_free);

		_size = 0; // atomic

		_block = (T* const __restrict)scalable_aligned_malloc(capacity_bytes(), 32ULL);
		_occupy = (uint8_t* const __restrict)scalable_aligned_malloc(capacity() * sizeof(uint8_t), 32ULL);

#if USE_2D_ARRAY
		{
			tbb::cache_aligned_allocator<void*> allocator;
			_grid = (T const**)allocator.allocate(Z + 1);
		}

		// map alias lines to block memory
		uint32_t z, i;
		for (z = i = 0; z < Z; ++z) {
			const_cast<T const**>(_grid)[z] = _block + i;	// map contiguous block of memory to 2D Array;
			i += linesize_bytes();							// grid is specifically readonly on purpose
		}
#endif
		//clear();
	}
	~contiguous2D()
	{
		if (_occupy) {
			scalable_aligned_free(_occupy);
			_occupy = nullptr;
		}

		if (_block) {
			scalable_aligned_free(_block);
			_block = nullptr;
		}

#if USE_2D_ARRAY
		if (_grid) {
			tbb::cache_aligned_allocator<void*> deallocator;
			deallocator.deallocate((void**)_grid, 0);
			_grid = nullptr;
		}
#endif
	}

private:
	contiguous2D(contiguous2D const&) = delete;
	contiguous2D const& operator=(contiguous2D&) = delete;
	contiguous2D(contiguous2D const&&) = delete;
	contiguous2D const& operator=(contiguous2D&&) = delete;
private:
	alignas(32) T* __restrict			_block;
	
	alignas(32) uint8_t* __restrict		_occupy;

	std::atomic_uint32_t	_size,
							_minZ, _minX,
							_maxZ, _maxX;
#if USE_2D_ARRAY
	T const* const*			_grid;
#endif
};

/* 3D is prohibetively slow in flatten() operation
//// ######### 3D ######### //
//// Y defines # of slices //
template<typename T, uint32_t const X, uint32_t const Y, uint32_t const Z>
class alignas(16) contiguous3D
{
public:
	static constexpr uint32_t const capacity() { return(Y * contiguous2D<T, X, Z>::capacity()); }
	static constexpr uint32_t const capacity_bytes() { return(Y * contiguous2D<T, X, Z>::capacity_bytes()); }

	static constexpr uint32_t const slicesize() { return(contiguous2D<T, X, Z>::capacity()); }
	static constexpr uint32_t const slicesize_bytes() { return(contiguous2D<T, X, Z>::capacity_bytes()); }
	static constexpr uint32_t const linesize() { return(contiguous2D<T, X, Z>::linesize()); }
	static constexpr uint32_t const linesize_bytes() { return(contiguous2D<T, X, Z>::linesize_bytes()); }
	static constexpr uint32_t const elementsize_bytes() { return(sizeof(T)); }

	// access - writeonly
	template<typename... Args>
	void emplace_back(uint32_t const x, uint32_t const y, uint32_t const z, Args&&... args)	// 3D emplace
	{
		if (nullptr == _slices[y]) {
			_slices[y] = new contiguous2D<T, X, Z>();
		}
		_slices[y]->emplace_back(x, z, args...);
	}

	// non-concurrent operations
	uint32_t const flatten(T* __restrict Mapped) const
	{
		uint32_t total_size(0);
		uint32_t slice (0);
		do
		{
			if ( _slices[slice] ) {
				uint32_t const slice_actual_size(_slices[slice]->flatten(Mapped));
				Mapped += slice_actual_size;
				total_size += slice_actual_size;
			}

		} while (++slice < Y);

		return(total_size);
	}

	void clear()
	{
		for (int32_t slice = Y - 1; slice >= 0; --slice) {
			if (_slices[slice]) {
				_slices[slice]->clear();
			}
		}
	}
	uint32_t const totalsize()
	{
		uint32_t totalsize(0);
		for (int32_t slice = Y - 1; slice >= 0; --slice) {
			if (_slices[slice]) {
				totalsize += _slices[slice]->size();
			}
		}

		return(totalsize);
	}

public:
	contiguous3D()
		: _slices{ nullptr }
	{}
	~contiguous3D()
	{
		for (int32_t slice = Y - 1; slice >= 0; --slice) {
			if (_slices[slice]) {
				delete _slices[slice];
				_slices[slice] = nullptr;
			}
		}
	}

private:
	contiguous3D(contiguous3D const&) = delete;
	contiguous3D const& operator=(contiguous3D&) = delete;
	contiguous3D(contiguous3D const&&) = delete;
	contiguous3D const& operator=(contiguous3D&&) = delete;
private:
	contiguous2D<T, X, Z>*	_slices[Y];
};*/
