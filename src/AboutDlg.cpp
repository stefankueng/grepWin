// grepWin - regex search and replace for Windows

// Copyright (C) 2007-2013, 2018, 2020-2021 - Stefan Kueng

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
#include "AboutDlg.h"
#include "version.h"
#include "Theme.h"
#include <shellapi.h>
#include <string>

CAboutDlg::CAboutDlg(HWND hParent)
    : m_hParent(hParent)
    , m_themeCallbackId(0)
{
}

CAboutDlg::~CAboutDlg()
{
}

LRESULT CAboutDlg::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
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
            wchar_t buf[MAX_PATH] = {0};
            swprintf_s(buf, _countof(buf), L"grepWin version %ld.%ld.%ld.%ld", GREPWIN_VERMAJOR, GREPWIN_VERMINOR, GREPWIN_VERMICRO, GREPWIN_VERBUILD);
            SetDlgItemText(*this, IDC_VERSIONINFO, buf);
            SetDlgItemText(*this, IDC_DATE, TEXT(GREPWIN_VERDATE));
        }
            return TRUE;
        case WM_COMMAND:
            return DoCommand(LOWORD(wParam), HIWORD(wParam));
        case WM_CLOSE:
            CTheme::Instance().RemoveRegisteredCallback(m_themeCallbackId);
            break;
        case WM_NOTIFY:
        {
            switch (wParam)
            {
                case IDC_WEBLINK:
                    switch (reinterpret_cast<LPNMHDR>(lParam)->code)
                    {
                        case NM_CLICK:
                        case NM_RETURN:
                        {
                            PNMLINK pNMLink = reinterpret_cast<PNMLINK>(lParam);
                            LITEM   item    = pNMLink->item;
                            if (item.iLink == 0)
                            {
                                ShellExecute(*this, L"open", item.szUrl, nullptr, nullptr, SW_SHOW);
                            }
                            break;
                        }
                    }
                    break;
                default:
                    break;
            }
        }
        break;
        default:
            return FALSE;
    }
    return FALSE;
}

LRESULT CAboutDlg::DoCommand(int id, int /*msg*/)
{
    switch (id)
    {
        case IDOK:
            // fall through
        case IDCANCEL:
            EndDialog(*this, id);
            break;
        default:
            break;
    }
    return 1;
}
