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
#include "Resource.h"
#include "NameDlg.h"
#include <string>

#include <boost/regex.hpp>
using namespace std;


CNameDlg::CNameDlg(HWND hParent)
{
    m_hParent = hParent;
}

CNameDlg::~CNameDlg(void)
{
}

LRESULT CNameDlg::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
        {
            InitDialog(hwndDlg, IDI_GREPWIN);
            // initialize the controls

            SetFocus(GetDlgItem(hwndDlg, IDC_NAME));

            m_resizer.Init(hwndDlg);

            ExtendFrameIntoClientArea(IDC_NAME, IDC_NAME, IDC_NAME, IDC_NAME);
            m_aerocontrols.SubclassControl(GetDlgItem(*this, IDOK));
            m_aerocontrols.SubclassControl(GetDlgItem(*this, IDCANCEL));
            m_aerocontrols.SubclassControl(GetDlgItem(*this, IDC_INFOLABEL));
            if (m_Dwm.IsDwmCompositionEnabled())
                m_resizer.ShowSizeGrip(false);
        }
        return FALSE;
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

LRESULT CNameDlg::DoCommand(int id, int /*msg*/)
{
    switch (id)
    {
    case IDOK:
        {
            TCHAR buf[MAX_PATH] = {0};
            GetDlgItemText(*this, IDC_NAME, buf, MAX_PATH);
            m_name = buf;
        }
        // fall through
    case IDCANCEL:
        EndDialog(*this, id);
        break;
    }
    return 1;
}

