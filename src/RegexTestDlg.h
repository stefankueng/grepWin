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
#pragma once
#include "basedialog.h"
#include "DlgResizer.h"
#include "AeroControls.h"
#include <string>
#include <vector>

using namespace std;

#define ID_REGEXTIMER       100

/**
 * regex test dialog.
 */
class CRegexTestDlg : public CDialog
{
public:
    CRegexTestDlg(HWND hParent);
    ~CRegexTestDlg(void);

    void                    SetStrings(const wstring& search, const wstring& replace);
    wstring                 GetSearchString() {return m_searchText;}
    wstring                 GetReplaceString() {return m_replaceText;}

    bool                    bDotMatchesNewline;
    bool                    bCaseSensitive;
protected:
    LRESULT CALLBACK        DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT                 DoCommand(int id, int msg);
    void                    DoRegex();

private:
    HWND                    m_hParent;
    wstring                 m_searchText;
    wstring                 m_replaceText;
    wstring                 m_textContent;

    CDlgResizer             m_resizer;
    AeroControlBase         m_aerocontrols;
};
