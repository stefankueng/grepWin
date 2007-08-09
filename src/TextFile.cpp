#include "StdAfx.h"
#include "TextFile.h"

CTextFile::CTextFile(void) : pFileBuf(NULL)
	, filelen(0)
{
}

CTextFile::~CTextFile(void)
{
	if (pFileBuf)
		delete [] pFileBuf;
}

bool CTextFile::Save(LPCTSTR path)
{
	HANDLE hFile = CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ,
		NULL, CREATE_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return false;
	DWORD byteswritten;
	if (!WriteFile(hFile, pFileBuf, filelen, &byteswritten, NULL))
	{
		CloseHandle(hFile);
		return false;
	}
	CloseHandle(hFile);
	return true;
}

bool CTextFile::Load(LPCTSTR path)
{
	LARGE_INTEGER lint;
	if (pFileBuf)
		delete [] pFileBuf;
	pFileBuf = NULL;
	HANDLE hFile = CreateFile(path, GENERIC_READ, FILE_SHARE_READ,
		NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return false;
	wstring wpath(path);
	size_t pos = wpath.find_last_of('\\');
	filename = wpath.substr(pos+1);
	if (!GetFileSizeEx(hFile, &lint))
	{
		CloseHandle(hFile);
		return false;
	}
	if (lint.HighPart)
	{
		// file is way too big for us!
		CloseHandle(hFile);
		return false;
	}
	DWORD bytesread;
	pFileBuf = new BYTE[lint.LowPart];
	if (!ReadFile(hFile, pFileBuf, lint.LowPart, &bytesread, NULL))
	{
		CloseHandle(hFile);
		return false;
	}
	CloseHandle(hFile);
	filelen = bytesread;

	// we have the file read into memory, now we have to find out what
	// kind of text file we have here.
	encoding = CheckUnicodeType(pFileBuf, bytesread);

	if (encoding == UNICODE_LE)
		textcontent = wstring((wchar_t*)pFileBuf, bytesread/sizeof(wchar_t));
	else if (encoding == UTF8)
	{
		int ret = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)pFileBuf, bytesread, NULL, 0);
		wchar_t * pWideBuf = new wchar_t[ret];
		int ret2 = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)pFileBuf, bytesread, pWideBuf, ret);
		if (ret2 == ret)
			textcontent = wstring(pWideBuf, ret);
		delete [] pWideBuf;
	}
	else if (encoding == ANSI)
	{
		int ret = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, (LPCSTR)pFileBuf, bytesread, NULL, 0);
		wchar_t * pWideBuf = new wchar_t[ret];
		int ret2 = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, (LPCSTR)pFileBuf, bytesread, pWideBuf, ret);
		if (ret2 == ret)
			textcontent = wstring(pWideBuf, ret);
		delete [] pWideBuf;
	}
	else
		return false;
	return CalculateLines();
}

bool CTextFile::ContentsModified(LPVOID pBuf, DWORD newLen)
{
	if (pFileBuf)
		delete [] pFileBuf;
	pFileBuf = pBuf;
	filelen = newLen;
	return true;
}

CTextFile::UnicodeType CTextFile::CheckUnicodeType(LPVOID pBuffer, int cb)
{
	if (cb < 2)
		return ANSI;
	UINT16 * pVal = (UINT16 *)pBuffer;
	UINT8 * pVal2 = (UINT8 *)(pVal+1);
	// scan the whole buffer for a 0x0000 sequence
	// if found, we assume a binary file
	bool bNull = false;
	for (int i=0; i<(cb-1); i=i+2)
	{
		if (0x0000 == *pVal++)
			return BINARY;
		if (0x00 == *pVal2++)
			bNull = true;
		if (0x00 == *pVal2++)
			bNull = true;
	}
	if ((bNull)&&((cb % 2)==0))
		return UNICODE_LE;
	pVal = (UINT16 *)pBuffer;
	pVal2 = (UINT8 *)(pVal+1);
	if (*pVal == 0xFEFF)
		return UNICODE_LE;
	if (cb < 3)
		return ANSI;
	if (*pVal == 0xBBEF)
	{
		if (*pVal2 == 0xBF)
			return UTF8;
	}
	// check for illegal UTF8 chars
	pVal2 = (UINT8 *)pBuffer;
	for (int i=0; i<cb; ++i)
	{
		if ((*pVal2 == 0xC0)||(*pVal2 == 0xC1)||(*pVal2 >= 0xF5))
			return ANSI;
		pVal2++;
	}
	pVal2 = (UINT8 *)pBuffer;
	bool bUTF8 = false;
	for (int i=0; i<(cb-3); ++i)
	{
		if ((*pVal2 & 0xE0)==0xC0)
		{
			pVal2++;i++;
			if ((*pVal2 & 0xC0)!=0x80)
				return ANSI;
			bUTF8 = true;
		}
		if ((*pVal2 & 0xF0)==0xE0)
		{
			pVal2++;i++;
			if ((*pVal2 & 0xC0)!=0x80)
				return ANSI;
			pVal2++;i++;
			if ((*pVal2 & 0xC0)!=0x80)
				return ANSI;
			bUTF8 = true;
		}
		if ((*pVal2 & 0xF8)==0xF0)
		{
			pVal2++;i++;
			if ((*pVal2 & 0xC0)!=0x80)
				return ANSI;
			pVal2++;i++;
			if ((*pVal2 & 0xC0)!=0x80)
				return ANSI;
			pVal2++;i++;
			if ((*pVal2 & 0xC0)!=0x80)
				return ANSI;
			bUTF8 = true;
		}
		pVal2++;
	}
	if (bUTF8)
		return UTF8;
	return ANSI;
}

bool CTextFile::CalculateLines()
{
	// fill an array with starting positions for every line in the loaded file
	if (pFileBuf == NULL)
		return false;
	if (textcontent.empty())
		return false;
	linepositions.clear();
	size_t pos = 0;
	for (wstring::iterator it = textcontent.begin(); it != textcontent.end(); ++it)
	{
		if (*it == '\r')
		{
			++it;
			++pos;
			if (it != textcontent.end())
			{
				if (*it == '\n')
				{
					// crlf lineending
					linepositions.push_back(pos);
				}
				else
				{
					// cr lineending
					linepositions.push_back(pos-1);
				}
			}
			else
				break;
		}
		else if (*it == '\n')
		{
			// lf lineending
			linepositions.push_back(pos);
		}
		++pos;
	}
	return true;
}

long CTextFile::LineFromPosition(long pos) const
{
	long line = 0;
	for (vector<size_t>::const_iterator it = linepositions.begin(); it != linepositions.end(); ++it)
	{
		line++;
		if (pos <= long(*it))
			break;
	}
	return line;
}

wstring CTextFile::GetFileNameWithoutExtension()
{
	size_t pos = filename.find_last_of('.');
	if (pos != string::npos)
		return filename.substr(0, pos);
	return filename;
}

wstring CTextFile::GetFileNameExtension()
{
	size_t pos = filename.find_last_of('.');
	if (pos != string::npos)
		return filename.substr(pos);
	return L"";
}