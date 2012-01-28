// grepWin - regex search and replace for Windows

// Copyright (C) 2012 - Stefan Kueng

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
#include "Resource.h"
#include "maxpath.h"
#include "Settings.h"


CSettingsDlg::CSettingsDlg(HWND hParent)
    : m_hParent(hParent)
    , m_regEditorCmd(_T("Software\\grepWin\\editorcmd"))
{
}

CSettingsDlg::~CSettingsDlg(void)
{
}

LRESULT CSettingsDlg::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
        {
            InitDialog(hwndDlg, IDI_GREPWIN);

            SetDlgItemText(hwndDlg, IDC_EDITORCMD, std::wstring(m_regEditorCmd).c_str());

            m_resizer.Init(hwndDlg);
            m_resizer.AddControl(hwndDlg, IDC_EDITORGROUP, RESIZER_TOPLEFTBOTTOMRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_EDITORCMD, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_STATIC1, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_STATIC1, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDOK, RESIZER_BOTTOMRIGHT);
            m_resizer.AddControl(hwndDlg, IDCANCEL, RESIZER_BOTTOMRIGHT);
            ExtendFrameIntoClientArea(0, 0, 0, IDC_EDITORGROUP);
            m_aerocontrols.SubclassControl(GetDlgItem(*this, IDOK));
            m_aerocontrols.SubclassControl(GetDlgItem(*this, IDCANCEL));
            if (m_Dwm.IsDwmCompositionEnabled())
                m_resizer.ShowSizeGrip(false);
        }
        return TRUE;
    case WM_COMMAND:
        return DoCommand(LOWORD(wParam), HIWORD(wParam));
    case WM_SIZE:
        {
            m_resizer.DoResize(LOWORD(lParam), HIWORD(lParam));
        }
        break;
    case WM_GETMINMAXINFO:
        {
            MINMAXINFO * mmi = (MINMAXINFO*)lParam;
            mmi->ptMinTrackSize.x = m_resizer.GetDlgRect()->right;
            mmi->ptMinTrackSize.y = m_resizer.GetDlgRect()->bottom;
            return 0;
        }
        break;
    default:
        return FALSE;
    }
    return FALSE;
}

LRESULT CSettingsDlg::DoCommand(int id, int /*msg*/)
{
    switch (id)
    {
    case IDOK:
        {
            TCHAR buf[MAX_PATH*4] = {0};

            GetDlgItemText(*this, IDC_EDITORCMD, buf, MAX_PATH*4);
            m_regEditorCmd = buf;
        }
        // fall through
    case IDCANCEL:
        EndDialog(*this, id);
        break;
    }
    return 1;
}

