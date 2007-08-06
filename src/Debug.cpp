#include "stdafx.h"
#include "Debug.h"


#if defined(_DEBUG) || defined(DEBUG)
void TRACE(LPCTSTR str, ...)
{
	static TCHAR buf[20*1024];

	va_list ap;
	va_start(ap, str);

	_vstprintf_s(buf, 20*1024, str, ap);
	OutputDebugString(buf);
	va_end(ap);

};
#else
void TRACE(LPCTSTR /*str*/, ...) {};
#endif