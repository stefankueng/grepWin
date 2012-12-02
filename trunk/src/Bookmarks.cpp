// grepWin - regex search and replace for Windows

// Copyright (C) 2007-2009, 2012 - Stefan Kueng

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
#include "stdafx.h"
#include "Bookmarks.h"
#include "maxpath.h"
#include "CmdLineParser.h"
#include <shlobj.h>

CBookmarks::CBookmarks(void)
{
}

CBookmarks::~CBookmarks(void)
{
}

void CBookmarks::Load()
{
    std::unique_ptr<TCHAR[]> path(new TCHAR[MAX_PATH_NEW]);
    GetModuleFileName(NULL, path.get(), MAX_PATH_NEW);
    CCmdLineParser parser(GetCommandLine());
    if ((_tcsstr(path.get(), _T("portable")))||(parser.HasKey(_T("portable"))))
    {
        m_iniPath = path.get();
        m_iniPath = m_iniPath.substr(0, m_iniPath.rfind('\\'));
    }
    else
    {
        SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, path.get());
        m_iniPath = path.get();
        m_iniPath += _T("\\grepWin");
    }
    CreateDirectory(m_iniPath.c_str(), NULL);
    m_iniPath += _T("\\bookmarks");
    LoadFile(m_iniPath.c_str());
}

void CBookmarks::Save()
{
    std::unique_ptr<TCHAR[]> path(new TCHAR[MAX_PATH_NEW]);
    GetModuleFileName(NULL, path.get(), MAX_PATH_NEW);
    CCmdLineParser parser(GetCommandLine());
    if ((_tcsstr(path.get(), _T("portable")))||(parser.HasKey(_T("portable"))))
    {
        m_iniPath = path.get();
        m_iniPath = m_iniPath.substr(0, m_iniPath.rfind('\\'));
    }
    else
    {
        SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, path.get());
        m_iniPath = path.get();
        m_iniPath += _T("\\grepWin");
    }
    CreateDirectory(m_iniPath.c_str(), NULL);
    m_iniPath += _T("\\bookmarks");
    FILE * pFile = NULL;
    _tfopen_s(&pFile, m_iniPath.c_str(), _T("wb"));
    SaveFile(pFile);
    fclose(pFile);
}

void CBookmarks::AddBookmark(const std::wstring& name, const std::wstring& search, const std::wstring& replace, bool bRegex)
{
    std::wstring val = _T("\"");
    val += search;
    val += _T("\"");
    SetValue(name.c_str(), _T("searchString"), val.c_str());

    val = _T("\"");
    val += replace;
    val += _T("\"");
    SetValue(name.c_str(), _T("replaceString"), val.c_str());
    SetValue(name.c_str(), _T("useregex"), bRegex ? _T("true") : _T("false"));
}

void CBookmarks::RemoveBookmark(const std::wstring& name)
{
    Delete(name.c_str(), _T("searchString"), true);
    Delete(name.c_str(), _T("replaceString"), true);
    Delete(name.c_str(), _T("useregex"), true);
}

