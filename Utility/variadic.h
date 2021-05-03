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

#include <cstddef>
#include <utility>

// variadic argument iteration : https://gist.github.com/nabijaczleweli/37cdd8c28039ea41a999
//
// usage:
//
// 
//template<class... Args>
//inline your_class::your_function(Args &&... args) {
//
//	size_t total_number_arguments(sizeof...(Args));
//	over_all<Args...>::for_each([&](auto && val) {
//		// do something with val
//		}, std::forward<Args>(args)...);
//
// }

template<typename T, typename... TT>
struct over_all {
	using next = over_all<TT...>;
	static const constexpr std::size_t size = 1 + next::size;

	template<typename C>
	inline constexpr static C for_each(C cbk, T && tval, TT &&... ttval){
		cbk(std::forward<T&&>(tval));
		next::for_each(cbk, std::forward<TT&&>(ttval)...);
		return(cbk);
	}

	template<typename C>
	inline constexpr C operator()(C cbk, T && tval, TT &&... ttval) const {
		return( for_each(cbk, std::forward<T&&>(tval), std::forward<TT&&>(ttval)...) );
	}
};

template<typename T>
struct over_all<T> {
	static const constexpr std::size_t size = 1;

	template<typename C>
	inline constexpr static C for_each(C cbk, T && tval) {
		cbk(std::forward<T&&>(tval));
		return(cbk);
	}

	template<typename C>
	inline constexpr C operator()(C cbk, T && tval) const {
		return( for_each(cbk, std::forward<T&&>(tval)) );
	}
};