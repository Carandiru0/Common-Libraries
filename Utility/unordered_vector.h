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
#include <utility>
#include <tbb/scalable_allocator.h>

// unordered version of vector - order is mutable because of remove() implementation
// extends and overrides std::vector adding fast removal of singular random elements
template<typename T, typename Allocator = tbb::scalable_allocator<T>>
class unordered_vector : public std::vector<T, Allocator>
{
public:
	auto const remove(unordered_vector<T>::iterator& it)	// efficient removal and erasure of singular element : https://stackoverflow.com/questions/39912/how-do-i-remove-an-item-from-a-stl-vector-with-a-certain-value
	{
		if (this->end() != it) {

			std::swap(*it, this->back());

			this->pop_back();
		}

		return(++it); // always return subsequent iterator
	}
	bool const remove(T& element)	// efficient removal and erasure of singular element : https://stackoverflow.com/questions/39912/how-do-i-remove-an-item-from-a-stl-vector-with-a-certain-value
	{
		auto it = std::find(this->begin(), this->end(), element);
		if (this->end() != it) {

			std::swap(*it, this->back());

			this->pop_back();

			return(true);
		}

		return(false);
	}
};

