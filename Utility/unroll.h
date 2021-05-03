#pragma once
#include <utility>

// a very helpful function for unrolling a loop at compile time! aka for constexpr

// usage example:
//
// int x = 1;
//
// unroll<0, 5>([&](auto i) {
//	func<i.value>(x);
// });
//
// ** where func is a template function that takes a compile time index parameter in the template arguments

template<std::size_t first, std::size_t last, typename Function>
constexpr void unroll(Function&& function) {
	[&] <std::size_t... indexes>(std::index_sequence<indexes...>) {
		(..., function(std::integral_constant<std::size_t, indexes + first>()));
	}(std::make_index_sequence<last - first>());
}





