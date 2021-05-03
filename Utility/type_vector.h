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
#include <vector>
#include <tbb/scalable_allocator.h>

// intention - must inherit this class to belong to unique vector for it's type //
// when object is constructed it's memory belongs to the type_vector
// when onject is destroyed it's memory is invalid, removed by the type_vector
// purposely restricted in not exposing methods that expose functionality to the internal vector


/// ####(child classes that inherit MUST in move ctor/assignment must free_ownership)####
/// ####(child classes that inherit MUST implement static swap method and at end of swap operation call revert_free_ownership)####

// *** must free_ownership if an element that inherits this class is moved externally (and thus dtor is inherently called)  
// *** move semantics must be implemented for class that inherits type_vector
// *** use static methods to access the underlying static unordered_vector
// *** there is only one static unordered_vector per type (T)
// *** inherit type_vector for automatic cleanup of the derived class o destruction

template<typename T>
class type_vector
{
	using internal_vector = std::vector<T, tbb::scalable_allocator<T>>;
public:
	using iterator = typename internal_vector::iterator;
	using const_iterator = typename internal_vector::const_iterator;
public:
	STATIC_INLINE const_iterator const cbegin() {
		return(_elements.cbegin());
	}
	STATIC_INLINE const_iterator const cend() {
		return(_elements.cend());
	}
	STATIC_INLINE iterator begin() {
		return(_elements.begin());
	}
	STATIC_INLINE iterator end() {
		return(_elements.end());
	}

	STATIC_INLINE T& emplace_back(T&& element)
	{
		return(_elements.emplace_back(std::forward<T&&>(element)));
	}

	STATIC_INLINE auto const remove(iterator it) // default remove method (recommended)
	{
		if (_elements.end() != it) {
			it->free_ownership(); // bugfix: prevent recursion of dtors
			std::swap(*it, _elements.back());
			_elements.pop_back();
			return(++it); // always return subsequent iterator
		}

		return(it); // iterator is the end
	}
	STATIC_INLINE bool const remove(T const* const element)
	{
		if (!_elements.empty()) {
			auto const it = std::find(_elements.begin(), _elements.end(), element);

			if (_elements.end() != it) {
				it->free_ownership(); // bugfix: prevent recursion of dtors
				std::swap(*it, _elements.back());
				_elements.pop_back();
				return(true);
			}
		}
		return(false);
	}
	STATIC_INLINE void clear()
	{
		for (auto it = T::begin(); it != T::end(); ++it) {
			it->free_ownership();
		}
		_elements.clear();
	}

	STATIC_INLINE void reserve(size_t const num_elements) {
		_elements.reserve(num_elements);
	}

	STATIC_INLINE void shrink_to_fit() {
		_elements.shrink_to_fit();
	}

	STATIC_INLINE size_t const size() {
		return(_elements.size());
	}

	STATIC_INLINE bool const empty() {
		return(_elements.empty());
	}

	void release() {

		if (_owned) {
			remove(*static_cast<T* const>(this));
		}
	}

	void free_ownership() {
		_owned = false;
	}

	void revert_free_ownership() {
		_owned = true;
	}

private:
	static inline internal_vector _elements;
	bool _owned = true;
public:
	type_vector() = default;

	~type_vector()
	{
		if (_owned) {
			remove(static_cast<T* const>(this));
		}
	}


private:
	type_vector(type_vector const&) = delete;
	type_vector& operator=(type_vector const&) = delete;
};