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
#include "plf/plf_colony.h"
#include <tbb/scalable_allocator.h>

// replacement for type_vector in use cases where memory pointer invalidation is required not to happen until any contained objects
// are inserted or removed. std::vector backs type_vector which does invalidate pointers. tbb::concurrent vector would fit the 
// requirements but unfortunately does not implement individual element deletions.

// type_colony //

// same shit but deletions can happen and you don't have to worry about your memory locations changing on you.


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
class type_colony
{
private:
	using internal_vector = plf::colony<T, tbb::scalable_allocator<T> >;
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

	template<class... Args>
	STATIC_INLINE T& emplace_back(Args&&... args)
	{
		return(*_elements.emplace(std::forward<Args&&...>(args...)));
	}

	STATIC_INLINE auto const remove(iterator const& it) // default remove method (recommended)
	{
		if (_elements.end() != it) {
			it->free_ownership(); // bugfix: prevent recursion of dtors
			return(_elements.erase(it));
		}

		return(it); // iterator is the end
	}
	STATIC_INLINE auto const remove(T const* const element) // slower special use case version of remove
	{
		if (!_elements.empty()) {
			auto const it = _elements.get_iterator_from_pointer(const_cast<T* const>(element));
			if (_elements.end() != it) {
				it->free_ownership(); // bugfix: prevent recursion of dtors
				return(_elements.erase(it));
			}
		}
		return(_elements.end());
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
	type_colony() = default;

	~type_colony()
	{
		if (_owned) {	
			remove(static_cast<T* const>(this));
		}
	}


private:
	type_colony(type_colony const&) = delete;
	type_colony& operator=(type_colony const&) = delete;
};