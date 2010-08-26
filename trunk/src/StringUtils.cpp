// grepWin - regex search and replace for Windows

// Copyright (C) 2007-2008 - Stefan Kueng

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
#include "stringutils.h"

int strwildcmp(const char *wild, const char *string)
{
    const char *cp = NULL;
    const char *mp = NULL;
    while ((*string) && (*wild != '*'))
    {
        if ((*wild != *string) && (*wild != '?'))
        {
            return 0;
        }
        wild++;
        string++;
    }
    while (*string)
    {
        if (*wild == '*')
        {
            if (!*++wild)
            {
                return 1;
            }
            mp = wild;
            cp = string+1;
        }
        else if ((*wild == *string) || (*wild == '?'))
        {
            wild++;
            string++;
        }
        else
        {
            wild = mp;
            string = cp++;
        }
    }

    while (*wild == '*')
    {
        wild++;
    }
    return !*wild;
}

int wcswildcmp(const wchar_t *wild, const wchar_t *string)
{
    const wchar_t *cp = NULL;
    const wchar_t *mp = NULL;
    while ((*string) && (*wild != '*'))
    {
        if ((*wild != *string) && (*wild != '?'))
        {
            return 0;
        }
        wild++;
        string++;
    }
    while (*string)
    {
        if (*wild == '*')
        {
            if (!*++wild)
            {
                return 1;
            }
            mp = wild;
            cp = string+1;
        }
        else if ((*wild == *string) || (*wild == '?'))
        {
            wild++;
            string++;
        }
        else
        {
            wild = mp;
            string = cp++;
        }
    }

    while (*wild == '*')
    {
        wild++;
    }
    return !*wild;
}

bool WriteAsciiStringToClipboard(const wchar_t * sClipdata, HWND hOwningWnd)
{
    if (OpenClipboard(hOwningWnd))
    {
        EmptyClipboard();
        HGLOBAL hClipboardData;
        size_t sLen = _tcslen(sClipdata);
        hClipboardData = GlobalAlloc(GMEM_DDESHARE, (sLen+1)*sizeof(wchar_t));
        if (hClipboardData)
        {
            wchar_t * pchData;
            pchData = (wchar_t*)GlobalLock(hClipboardData);
            if (pchData)
            {
                _tcscpy_s(pchData, sLen+1, sClipdata);
                if (GlobalUnlock(hClipboardData))
                {
                    if (SetClipboardData(CF_UNICODETEXT, hClipboardData)==NULL)
                    {
                        CloseClipboard();
                        return true;
                    }
                }
                else
                {
                    CloseClipboard();
                    return false;
                }
            }
            else
            {
                CloseClipboard();
                return false;
            }
        }
        else
        {
            CloseClipboard();
            return false;
        }
        CloseClipboard();
        return false;
    }
    return false;
}
