#pragma once

#include <string>

#pragma warning (push,1)
#ifndef stdstring
	typedef std::wstring wide_string;
#	ifdef UNICODE
#		define stdstring wide_string
#	else
#		define stdstring std::string
#	endif
#endif
#pragma warning (pop)

class CUnicodeUtils
{
public:
	CUnicodeUtils(void);
	~CUnicodeUtils(void);
#ifdef UNICODE
	static std::string StdGetUTF8(const wide_string& wide);
	static wide_string StdGetUnicode(const std::string& multibyte);
	static std::string StdGetANSI(const wide_string& wide);
#else
	static std::string StdGetUTF8(std::string str) {return str;}
	static std::string StdGetUnicode(std::string multibyte) {return multibyte;}
#endif
};

std::string WideToMultibyte(const wide_string& wide);
std::string WideToUTF8(const wide_string& wide);
wide_string MultibyteToWide(const std::string& multibyte);
wide_string UTF8ToWide(const std::string& multibyte);

#ifdef UNICODE
	stdstring UTF8ToString(const std::string& string); 
	std::string StringToUTF8(const stdstring& string); 
#else
	stdstring UTF8ToString(const std::string& string); 
	std::string StringToUTF8(const stdstring& string);
#endif

int LoadStringEx(HINSTANCE hInstance, UINT uID, LPTSTR lpBuffer, int nBufferMax, WORD wLanguage);
