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
#include <string>
#include <sstream>
#include <Utility/stringconv.h>

// for one .cpp file #define CMDLINE_IMPLEMENTATION before #include of this file

namespace cmdline
{
	namespace internal
	{
		// supporting a simple string of ints separated by spaces
		extern std::vector<uint32_t> _arguments;
	} // end ns
	
	static inline uint32_t const getArgument(uint32_t const index)
	{
		return(internal::_arguments[index]);
	}

	static inline void pushArgument(uint32_t const argument)
	{
		internal::_arguments.emplace_back(argument);
	}
	
	static inline void resetArguments()
	{
		internal::_arguments.clear();
	}
	
	// for reading command line
	static inline void arguments(wchar_t const* const* const argv, int const argc)
	{
		for (int32_t i = 1; i < argc; ++i) {

			internal::_arguments.emplace_back((uint32_t)strtoul(stringconv::ws2s(argv[i]).c_str(), nullptr, 0));
		}
	}

	// for writing command line
	static inline std::string const arguments()
	{
		std::stringstream ss;
		for (auto const& i : internal::_arguments) {
			ss << i << " ";
		}
		
		return(ss.str());
	}
	
} // endns

#ifdef CMDLINE_IMPLEMENTATION

namespace cmdline
{
	namespace internal
	{
		// supporting a simple string of ints separated by spaces
		inline std::vector<uint32_t> _arguments;
	} // end ns
} // endns

#endif

