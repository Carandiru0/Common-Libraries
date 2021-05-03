#pragma once

#include "bm/bmsparsevec.h"

#ifndef NDEBUG
#include <Utility/assert_print.h>
#endif

template<typename T, uint32_t const X, uint32_t const Z>
class alignas(16) sparsevector2D
{
#ifndef NDEBUG
	static constexpr char const* const ERROR_MESSAGE_0 = "size exceeds capacity";
	static constexpr char const* const ERROR_MESSAGE_1 = "size not equal flatten size";
#endif
public:
	constexpr uint32_t const capacity() const { return(X*Z); }
	constexpr uint32_t const capacity_bytes() const { return(X*Z * sizeof(T)); }

	uint32_t const		   size() const { return(_size); }
	uint32_t const		   size_bytes() const { return(_size * sizeof(T)); }

	constexpr uint32_t const linesize_bytes() const { return(X * sizeof(T)); }
	constexpr uint32_t const elementsize_bytes() const { return(sizeof(T)); }

	// access - readonly
	//__inline T const* const& data() const { return(_block); }		// flat access

	/*
	__inline T const* const& operator[](uint32_t const Row) const		// 2D access
	{
		return(_grid[Row]);
	}
	*/

	// access - writeonly
	template<typename... Args>
	void emplace_back(uint32_t const x, uint32_t const z, Args&&... args)	// 2D emplace
	{
		_block.emplace_back( T(std::forward<Args>(args)...) );
		_sv[z * X + x] = _size; // save index to sparse grid
#ifndef NDEBUG
		assert_print(_size <= capacity(), ERROR_MESSAGE_0);
#endif
		++_size;
	}

	// non-concurrent operations
	uint32_t const flatten(T* __restrict pMapped)
	{
		uint32_t usedsize = size();
		std::vector<uint32_t> indices(usedsize);

		uint32_t const flattensize = _sv.decode(indices.data(), 0, usedsize);

		uint32_t const* pIndices = indices.data();
		do
		{
			*pMapped++ = _block[*pIndices++];

		} while (--usedsize);

#ifndef NDEBUG
		assert_print(_size <= capacity(), ERROR_MESSAGE_1);
#endif
		return(flattensize);
	}
	void clear()
	{
		_size = 0;
		_block.clear();
		_sv.clear();
		_sv.resize(Z*X);
	}
public:
	sparsevector2D()
		: _sv(bm::use_null)
	{
		_size = 0;
		_block.reserve(capacity());

		clear();
	}
	~sparsevector2D()
	{
	}

private:
	sparsevector2D(sparsevector2D const&) = delete;
	sparsevector2D const& operator=(sparsevector2D&) = delete;
	sparsevector2D(sparsevector2D const&&) = delete;
	sparsevector2D const& operator=(sparsevector2D&&) = delete;
private:
	std::vector<T>									_block;
	bm::sparse_vector<uint32_t, bm::bvector<> >		_sv;
	std::atomic_uint32_t							_size;
};