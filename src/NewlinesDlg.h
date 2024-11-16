// grepWin - regex search and replace for Windows

// Copyright (C) 2024 - Stefan Kueng

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
#include "BaseDialog.h"

enum class eNewlines
{
    None,
    Linux,
    Windows,
    Both,
};

class CNewlinesDlg : public CDialog
{
public:
    CNewlinesDlg(HWND hParent);
    ~CNewlinesDlg() override;

    void setNewLines(eNewlines nl);
    eNewlines getNewLines() const;
    void      setShowBoth(bool show);

protected:
    LRESULT CALLBACK DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) override;
    LRESULT          DoCommand(int id, int msg);

private:
    HWND         m_hParent;
    int          m_themeCallbackId;
    eNewlines    m_newLines;
    bool         m_showBoth;
};
