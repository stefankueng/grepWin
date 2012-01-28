// grepWin - regex search and replace for Windows

// Copyright (C) 2007-2008, 2012 - Stefan Kueng

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
#pragma once

#include <string>

#ifdef UNICODE
#define _tcswildcmp	wcswildcmp
#else
#define _tcswildcmp strwildcmp
#endif

/**
 * Performs a wildcard compare of two strings.
 * \param wild the wildcard string
 * \param string the string to compare the wildcard to
 * \return TRUE if the wildcard matches the string, 0 otherwise
 * \par example
 * \code
 * if (strwildcmp("bl?hblah.*", "bliblah.jpeg"))
 *  printf("success\n");
 * else
 *  printf("not found\n");
 * if (strwildcmp("bl?hblah.*", "blabblah.jpeg"))
 *  printf("success\n");
 * else
 *  printf("not found\n");
 * \endcode
 * The output of the above code would be:
 * \code
 * success
 * not found
 * \endcode
 */
int strwildcmp(const char * wild, const char * string);
int wcswildcmp(const wchar_t * wild, const wchar_t * string);

bool WriteAsciiStringToClipboard(const wchar_t * sClipdata, HWND hOwningWnd);
void SearchReplace(std::wstring& str, const std::wstring& toreplace, const std::wstring& replacewith);

