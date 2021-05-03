#pragma once

// Single Header Library for String Conversion
// STRINGCONV_IMPLEMENTATION must be defined in only one SOURCE FILE

#include <string>
#include <string_view>
#include <cctype>
#include <cwctype>

#define MAX_GUID 40

namespace stringconv
{
	std::string const ws2s(std::wstring_view const& wstr);

	std::wstring const s2ws(std::string_view const& str);

	std::string const getCOMError2s(HRESULT const& hr);

	struct case_insensitive_less
	{
		bool const operator () (wchar_t const x, wchar_t const y) const
		{
			return(towlower(x) < towlower(y));
		}
	};
	bool const NoCaseLess(std::wstring_view const& a, std::wstring_view const& b);
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
	inline void toLower(std::wstring& rsIn)
	{
		std::transform(rsIn.begin(), rsIn.end(), rsIn.begin(), towlower);
	}
	inline void toUpper(std::wstring& rsIn)
	{
		std::transform(rsIn.begin(), rsIn.end(), rsIn.begin(), towupper);
	}
	STATIC_INLINE_PURE char const ascii_toupper(char const c) { // speedy variant for ascii characters only
		return( ('a' <= c && c <= 'z') ? c^0x20 : c);    // ^ autovectorizes to PXOR: runs on more ports than paddb
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

	bool const NoCaseLess(std::wstring_view const& a, std::wstring_view const& b)
	{
		return(std::lexicographical_compare(a.cbegin(), a.cend(), b.cbegin(), b.cend(), case_insensitive_less()));
	}
}
#endif