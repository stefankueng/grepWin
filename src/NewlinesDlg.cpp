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
#include "stdafx.h"
#include "resource.h"
#include "NewlinesDlg.h"
#include "Theme.h"
#include <boost/regex.hpp>

CNewlinesDlg::CNewlinesDlg(HWND hParent)
    : m_hParent(hParent)
    , m_themeCallbackId(0)
    , m_newLines(eNewlines::None)
{
}

CNewlinesDlg::~CNewlinesDlg()
{
}
void CNewlinesDlg::setNewLines(eNewlines nl)
{
    m_newLines = nl;
}
eNewlines CNewlinesDlg::getNewLines() const
{
    return m_newLines;
}
void CNewlinesDlg::setShowBoth(bool show)
{
    m_showBoth = show;
}

LRESULT CNewlinesDlg::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            m_themeCallbackId = CTheme::Instance().RegisterThemeChangeCallback(
                [this]() {
                    CTheme::Instance().SetThemeForDialog(*this, CTheme::Instance().IsDarkTheme());
                });
            CTheme::Instance().SetThemeForDialog(*this, CTheme::Instance().IsDarkTheme());

            InitDialog(hwndDlg, IDI_GREPWIN);
            CLanguage::Instance().TranslateWindow(*this);
            // initialize the controls
            switch (m_newLines)
            {
                case eNewlines::None:
                    CheckRadioButton(hwndDlg, IDC_CRLF, IDC_BOTH, IDC_CRLF);

                    break;
                case eNewlines::Linux:
                    CheckRadioButton(hwndDlg, IDC_CRLF, IDC_BOTH, IDC_LF);
                    break;
                case eNewlines::Windows:
                    CheckRadioButton(hwndDlg, IDC_CRLF, IDC_BOTH, IDC_CRLF);
                    break;
                case eNewlines::Both:
                    CheckRadioButton(hwndDlg, IDC_CRLF, IDC_BOTH, IDC_BOTH);
                    break;
            }
            if (!m_showBoth)
            {
                ShowWindow(GetDlgItem(hwndDlg, IDC_BOTH), SW_HIDE);
            }
        }
            return FALSE;
        case WM_COMMAND:
            return DoCommand(LOWORD(wParam), HIWORD(wParam));
        case WM_CLOSE:
            CTheme::Instance().RemoveRegisteredCallback(m_themeCallbackId);
            break;
        default:
            return FALSE;
    }
    return FALSE;
}

LRESULT CNewlinesDlg::DoCommand(int id, int /*msg*/)
{
    switch (id)
    {
        case IDOK:
            if (IsDlgButtonChecked(*this, IDC_LF) == BST_CHECKED)
            {
                m_newLines = eNewlines::Linux;
            }
            else if (IsDlgButtonChecked(*this, IDC_CRLF) == BST_CHECKED)
            {
                m_newLines = eNewlines::Windows;
            }
            else if (IsDlgButtonChecked(*this, IDC_BOTH) == BST_CHECKED)
            {
                m_newLines = eNewlines::Both;
            }
            [[fallthrough]];
        case IDCANCEL:
            EndDialog(*this, id);
            break;
    }
    return 1;
}
