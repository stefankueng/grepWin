// grepWin - regex search and replace for Windows

// Copyright (C) 2007-2012 - Stefan Kueng

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//
#include "StdAfx.h"
#include "TextFile.h"
#include "maxpath.h"
#include "auto_buffer.h"

CTextFile::CTextFile(void) : pFileBuf(NULL)
    , filelen(0)
    , hasBOM(false)
{
}

CTextFile::~CTextFile(void)
{
    if (pFileBuf)
        delete [] pFileBuf;
}

bool CTextFile::Save(LPCTSTR path)
{
    if (pFileBuf == NULL)
        return false;
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

bool CTextFile::Load(LPCTSTR path, UnicodeType& type, bool bUTF8)
{
    encoding = AUTOTYPE;
    type = AUTOTYPE;
    LARGE_INTEGER lint;
    if (pFileBuf)
        delete [] pFileBuf;
    pFileBuf = NULL;
    auto_buffer<TCHAR> pathbuf(MAX_PATH_NEW);
    HANDLE hFile = INVALID_HANDLE_VALUE;
    int retrycounter = 0;

    if ((_tcslen(path) > 2 )&&(path[0] == '\\')&&(path[1] == '\\'))
    {
        // UNC path
        _tcscpy_s(pathbuf, MAX_PATH_NEW, _T("\\\\?\\UNC"));
        _tcscat_s(pathbuf, MAX_PATH_NEW, &path[1]);
    }
    else
    {
        // 'normal' path
        _tcscpy_s(pathbuf, MAX_PATH_NEW, _T("\\\\?\\"));
        _tcscat_s(pathbuf, MAX_PATH_NEW, path);
    }

    do
    {
        if (retrycounter)
            Sleep(20);
        hFile = CreateFile(pathbuf, GENERIC_READ, FILE_SHARE_READ,
            NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
        retrycounter++;
    } while (hFile == INVALID_HANDLE_VALUE && retrycounter < 5);

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

    MEMORYSTATUSEX memex = {sizeof(MEMORYSTATUSEX)};
    GlobalMemoryStatusEx(&memex);

    DWORD bytesread = 0;
    DWORD bytestoread = min(lint.LowPart, DWORD(memex.ullAvailPhys/100UL));
    // if there isn't enough RAM available, only load a small part of the file
    // to do the encoding check. Then only load the full file in case
    // the encoding is UNICODE_LE since that's the only encoding we have
    // to convert first to do a proper search with.
    if ((bytestoread < lint.LowPart)&&((memex.ullAvailPhys>>32UL)==0))
    {
        std::unique_ptr<BYTE[]> tempfilebuf(new BYTE[bytestoread+1]);
        if (!ReadFile(hFile, tempfilebuf.get(), bytestoread, &bytesread, NULL))
        {
            CloseHandle(hFile);
            return false;
        }
        encoding = CheckUnicodeType(tempfilebuf.get(), bytesread);
        type = encoding;
        switch(encoding)
        {
        case BINARY:
        case UTF8:
        case ANSI:
            CloseHandle(hFile);
            return false;
            break;
        default:
            pFileBuf = new (std::nothrow) BYTE[lint.LowPart];
            for (unsigned long bc = 0; bc < bytesread; ++bc)
            {
                pFileBuf[bc] = tempfilebuf[bc];
            }
            break;
        }
    }
    else
    {
        pFileBuf = new (std::nothrow) BYTE[lint.LowPart];
    }
    if ((pFileBuf==NULL) || (!ReadFile(hFile, pFileBuf, lint.LowPart, &bytesread, NULL)))
    {
        delete [] pFileBuf;
        pFileBuf = NULL;
        CloseHandle(hFile);
        return false;
    }
    CloseHandle(hFile);
    filelen = lint.LowPart;

    // we have the file read into memory, now we have to find out what
    // kind of text file we have here.
    if (encoding==AUTOTYPE)
    {
        encoding = CheckUnicodeType(pFileBuf, bytesread);
        if ((bUTF8)&&(encoding != BINARY))
            encoding = UTF8;
    }

    if (encoding == UNICODE_LE)
    {
        if (*(char*)pFileBuf == 0xFF)
        {
            // remove the BOM
            textcontent = wstring(((wchar_t*)pFileBuf+1), (bytesread/sizeof(wchar_t))-1);
            hasBOM = true;
        }
        else
            textcontent = wstring((wchar_t*)pFileBuf, bytesread/sizeof(wchar_t));
    }
    else if ((encoding == UTF8)||((encoding == BINARY)&&(bUTF8)))
    {
        int ret = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)pFileBuf, bytesread, NULL, 0);
        wchar_t * pWideBuf = new (std::nothrow) wchar_t[ret];
        if (pWideBuf==NULL)
            return false;
        int ret2 = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)pFileBuf, bytesread, pWideBuf, ret);
        if (ret2 == ret)
        {
            if (*pWideBuf == 0xFEFF)
            {
                // remove the BOM
                textcontent = wstring(pWideBuf+1, ret-1);
                hasBOM = true;
            }
            else
                textcontent = wstring(pWideBuf, ret);
        }
        delete [] pWideBuf;
    }
    else //if (encoding == ANSI)
    {
        int ret = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, (LPCSTR)pFileBuf, bytesread, NULL, 0);
        wchar_t * pWideBuf = new (std::nothrow) wchar_t[ret];
        if (pWideBuf == NULL)
            return false;
        int ret2 = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, (LPCSTR)pFileBuf, bytesread, pWideBuf, ret);
        if (ret2 == ret)
            textcontent = wstring(pWideBuf, ret);
        delete [] pWideBuf;
    }
    type = encoding;
    if (type == BINARY)
        return true;
    return CalculateLines();
}

void CTextFile::SetFileContent(const wstring& content)
{
    if (pFileBuf)
        delete [] pFileBuf;
    pFileBuf = NULL;
    filelen = 0;

    if (encoding == UNICODE_LE)
    {
        if (hasBOM)
        {
            pFileBuf = new (std::nothrow) BYTE[(content.size()+2)*sizeof(wchar_t)];
            if (pFileBuf)
            {
                memcpy(pFileBuf, _T("\xFE\xFF"), 2*sizeof(wchar_t));
                memcpy(pFileBuf+4, content.c_str(), content.size()*sizeof(wchar_t));
                filelen = ((int)content.size()+2)*sizeof(wchar_t);
            }
        }
        else
        {
            pFileBuf = new (std::nothrow) BYTE[content.size()*sizeof(wchar_t)];
            if (pFileBuf)
            {
                memcpy(pFileBuf, content.c_str(), content.size()*sizeof(wchar_t));
                filelen = (int)content.size()*sizeof(wchar_t);
            }
        }
    }
    else if (encoding == UTF8)
    {
        if (hasBOM)
        {
            int ret = WideCharToMultiByte(CP_UTF8, 0, content.c_str(), -1, NULL, 0, NULL, NULL);
            pFileBuf = new (std::nothrow) BYTE[ret+3];
            if (pFileBuf)
            {
                memcpy(pFileBuf, "\xEF\xBB\xBF", 3);
                int ret2 = WideCharToMultiByte(CP_UTF8, 0, content.c_str(), -1, (LPSTR)pFileBuf+3, ret, NULL, NULL);
                filelen = ret2+2;
                if (ret2 != ret)
                {
                    delete [] pFileBuf;
                    pFileBuf = NULL;
                    filelen = 0;
                }
            }
        }
        else
        {
            int ret = WideCharToMultiByte(CP_UTF8, 0, content.c_str(), -1, NULL, 0, NULL, NULL);
            pFileBuf = new (std::nothrow) BYTE[ret];
            if (pFileBuf)
            {
                int ret2 = WideCharToMultiByte(CP_UTF8, 0, content.c_str(), -1, (LPSTR)pFileBuf, ret, NULL, NULL);
                filelen = ret2-1;
                if (ret2 != ret)
                {
                    delete [] pFileBuf;
                    pFileBuf = NULL;
                    filelen = 0;
                }
            }
        }
    }
    else if ((encoding == ANSI)||(encoding == BINARY))
    {
        int ret = WideCharToMultiByte(CP_ACP, 0, content.c_str(), (int)content.size()+1, NULL, 0, NULL, NULL);
        pFileBuf = new (std::nothrow) BYTE[ret];
        if (pFileBuf)
        {
            int ret2 = WideCharToMultiByte(CP_ACP, 0, content.c_str(), (int)content.size()+1, (LPSTR)pFileBuf, ret, NULL, NULL);
            filelen = ret2-1;
            if (ret2 != ret)
            {
                delete [] pFileBuf;
                pFileBuf = NULL;
                filelen = 0;
            }
        }
    }
    if (pFileBuf)
        textcontent = content;
    else
        textcontent = _T("");
}

bool CTextFile::ContentsModified(BYTE * pBuf, DWORD newLen)
{
    if (pFileBuf)
        delete [] pFileBuf;
    pFileBuf = pBuf;
    filelen = newLen;
    return true;
}

CTextFile::UnicodeType CTextFile::CheckUnicodeType(BYTE * pBuffer, int cb)
{
    if (cb < 2)
        return ANSI;
    UINT16 * pVal16 = (UINT16 *)pBuffer;
    UINT8 * pVal8 = (UINT8 *)(pVal16+1);
    // scan the whole buffer for a 0x0000 sequence
    // if found, we assume a binary file
    bool bNull = false;
    for (int i=0; i<(cb-2); i=i+2)
    {
        if (0x0000 == *pVal16++)
            return BINARY;
        if (0x00 == *pVal8++)
            bNull = true;
        if (0x00 == *pVal8++)
            bNull = true;
    }
    if ((bNull)&&((cb % 2)==0))
        return UNICODE_LE;
    pVal16 = (UINT16 *)pBuffer;
    pVal8 = (UINT8 *)(pVal16+1);
    if (*pVal16 == 0xFEFF)
        return UNICODE_LE;
    if (cb < 3)
        return ANSI;
    if (*pVal16 == 0xBBEF)
    {
        if (*pVal8 == 0xBF)
            return UTF8;
    }
    // check for illegal UTF8 chars
    pVal8 = (UINT8 *)pBuffer;
    for (int i=0; i<cb; ++i)
    {
        if ((*pVal8 == 0xC0)||(*pVal8 == 0xC1)||(*pVal8 >= 0xF5))
            return ANSI;
        pVal8++;
    }
    pVal8 = (UINT8 *)pBuffer;
    bool bUTF8 = false;
    for (int i=0; i<(cb-4); ++i)
    {
        if ((*pVal8 & 0xE0)==0xC0)
        {
            pVal8++;i++;
            if ((*pVal8 & 0xC0)!=0x80)
                return ANSI;
            bUTF8 = true;
        }
        if ((*pVal8 & 0xF0)==0xE0)
        {
            pVal8++;i++;
            if ((*pVal8 & 0xC0)!=0x80)
                return ANSI;
            pVal8++;i++;
            if ((*pVal8 & 0xC0)!=0x80)
                return ANSI;
            bUTF8 = true;
        }
        if ((*pVal8 & 0xF8)==0xF0)
        {
            pVal8++;i++;
            if ((*pVal8 & 0xC0)!=0x80)
                return ANSI;
            pVal8++;i++;
            if ((*pVal8 & 0xC0)!=0x80)
                return ANSI;
            pVal8++;i++;
            if ((*pVal8 & 0xC0)!=0x80)
                return ANSI;
            bUTF8 = true;
        }
        pVal8++;
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
        return true;
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
    linepositions.push_back(pos);
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

wstring CTextFile::GetLineString(long lineNumber) const
{
    if ((lineNumber-2) >= (long)linepositions.size())
        return wstring();
    if (lineNumber <= 0)
        return wstring();

    long startpos = 0;
    if (lineNumber > 1)
        startpos = (long)linepositions[lineNumber-2];
    size_t endpos = textcontent.find(_T("\n"), startpos+1);
    wstring line;
    if (endpos != wstring::npos)
        line = wstring(textcontent.begin()+startpos, textcontent.begin() + endpos);
    else
        line = wstring(textcontent.begin()+startpos, textcontent.end());

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