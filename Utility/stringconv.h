#pragma once

// Single Header Library for String Conversion
// STRINGCONV_IMPLEMENTATION must be defined in only one SOURCE FILE

#include <string>
#include <string_view>
#include <cctype>
#include <cwctype>
#include <algorithm>
#include <Math/superfastmath.h>

#define MAX_GUID 40
#define HRESULT long

namespace stringconv
{
	std::string const ws2s(std::wstring_view const& wstr);

	std::wstring const s2ws(std::string_view const& str);

	std::string const getCOMError2s(HRESULT const& hr);


	template<typename character_type>
	struct case_insensitive_less
	{
		template<typename q = character_type>
		typename std::enable_if<std::is_same<q, wchar_t>::value, bool const>::type operator () (wchar_t const x, wchar_t const y) const
		{
			return(towupper(x) == towupper(y));
		}

		template<typename q = character_type>
		typename std::enable_if<std::is_same<q, char>::value, bool const>::type operator () (char const x, char const y) const
		{
			return(toupper(x) == toupper(y));
		}
	};
	bool const case_insensitive_compare(std::wstring_view const& a, std::wstring_view const& b);
	bool const case_insensitive_compare(std::string_view const& a, std::string_view const& b);

	/*
	inline std::wstring const trim(const std::wstring &s)
	{
		auto const wsfront = std::find_if_not(s.begin(), s.end(), std::iswspace);
		auto const wsback = std::find_if_not(s.rbegin(), s.rend(), std::iswspace).base();
		return (wsback <= wsfront ? std::wstring() : std::wstring(wsfront, wsback));
	}
	inline std::string const trim(const std::string &s)
	{
		auto const wsfront = std::find_if_not(s.begin(), s.end(), std::isspace);
		auto const wsback = std::find_if_not(s.rbegin(), s.rend(), std::isspace).base();
		return (wsback <= wsfront ? std::string() : std::string(wsfront, wsback));
	}
	*/

	inline std::wstring const& removeAllWhiteSpace(std::wstring &s)
	{
		s.erase(remove_if(s.begin(), s.end(), iswspace), s.end());
		return(s);
	}
	inline std::string const& removeAllWhiteSpace(std::string &s)
	{
		s.erase(remove_if(s.begin(), s.end(), isspace), s.end());
		return(s);
	}

	STATIC_INLINE_PURE char const toLower(char const c) { // speedy variant for ascii characters only
		//return(('A' <= c && c <= 'Z') ? c ^ ' ' : c);    // ^ autovectorizes to PXOR: runs on more ports than paddb
		return(tolower(c));
	}
	STATIC_INLINE_PURE char const toUpper(char const c) { // speedy variant for ascii characters only
		//return(('a' <= c && c <= 'z') ? c ^ ' ' : c);    // ^ autovectorizes to PXOR: runs on more ports than paddb
		return(toupper(c));
	}
	inline std::wstring const& toLower(std::wstring& rsIn)
	{
		std::transform(rsIn.begin(), rsIn.end(), rsIn.begin(), towlower);
		return(rsIn);
	}
	inline std::wstring const& toUpper(std::wstring& rsIn)
	{
		std::transform(rsIn.begin(), rsIn.end(), rsIn.begin(), towupper);
		return(rsIn);
	}
	inline std::string const& toLower(std::string& rsIn)
	{
		std::transform(rsIn.begin(), rsIn.end(), rsIn.begin(), tolower);
		return(rsIn);
	}
	inline std::string const& toUpper(std::string& rsIn)
	{
		std::transform(rsIn.begin(), rsIn.end(), rsIn.begin(), toupper);
		return(rsIn);
	}
	inline std::string const toLower(std::string_view const sIn)
	{
		std::string rsIn(sIn);
		std::transform(rsIn.begin(), rsIn.end(), rsIn.begin(), tolower);
		return(rsIn);
	}
	inline std::string const toUpper(std::string_view const sIn)
	{
		std::string rsIn(sIn);
		std::transform(rsIn.begin(), rsIn.end(), rsIn.begin(), toupper);
		return(rsIn);
	}
	inline std::string const toLower(char const* const sIn)
	{
		std::string rsIn(sIn);
		std::transform(rsIn.begin(), rsIn.end(), rsIn.begin(), tolower);
		return(rsIn);
	}
	inline std::string const toUpper(char const* const sIn)
	{
		std::string rsIn(sIn);
		std::transform(rsIn.begin(), rsIn.end(), rsIn.begin(), toupper);
		return(rsIn);
	}
	
}

#ifdef STRINGCONV_IMPLEMENTATION
#include <comdef.h>

namespace stringconv
{
	std::string const ws2s(std::wstring_view const& wstr)  // threadsafe and not deprecated, as long as setlocale is not called simulataneously in a different thread
	{
		mbstate_t state{}; // must be null for thread safety

		size_t const in_size = wstr.length();
		size_t const out_size = in_size + 1; // +1 for null terminator
		std::string str; str.reserve(out_size); str.resize(out_size);

		size_t charsConverted = 0;
		wchar_t const * indirect_wstr = wstr.data();
		wcsrtombs_s(&charsConverted, &str[0], out_size, &indirect_wstr, in_size, &state);
		str.resize(in_size); // remove additional null terminator that has been compensated for above that mbsrtowcs_s uneccesarrily adds
		return(str);
	}

	std::wstring const s2ws(std::string_view const& str)  // threadsafe and not deprecated, as long as setlocale is not called simulataneously in a different thread
	{
		mbstate_t state{}; // must be null for thread safety

		size_t const in_size = str.length();
		size_t const out_size = in_size + 1; // +1 for null terminator
		std::wstring wstr; wstr.reserve(out_size); wstr.resize(out_size);

		size_t charsConverted = 0;
		char const * indirect_str = str.data();
		mbsrtowcs_s(&charsConverted, &wstr[0], out_size, &indirect_str, in_size, &state);
		wstr.resize(in_size); // remove additional null terminator that has been compensated for above that mbsrtowcs_s uneccesarrily adds
		return(wstr);
	}

	std::string const getCOMError2s(HRESULT const& hr)
	{
		_com_error err(hr);
		TCHAR const* const errMsg = err.ErrorMessage();
		return(ws2s(errMsg));
	}

	bool const case_insensitive_compare(std::wstring_view const& a, std::wstring_view const& b)
	{
		return(std::equal(a.cbegin(), a.cend(), b.cbegin(), b.cend(), case_insensitive_less<wchar_t>()));
	}
	bool const case_insensitive_compare(std::string_view const& a, std::string_view const& b)
	{
		return(std::equal(a.cbegin(), a.cend(), b.cbegin(), b.cend(), case_insensitive_less<char>()));
	}
}
#endif