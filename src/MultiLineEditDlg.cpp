// grepWin - regex search and replace for Windows

// Copyright (C) 2011-2013, 2019-2021, 2024 - Stefan Kueng

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
#include "MultiLineEditDlg.h"

#include "NewlinesDlg.h"
#include "Theme.h"
#include "../sktoolslib/StringUtils.h"

#include <string>
#include <Richedit.h>
#include <boost/regex.hpp>

CMultiLineEditDlg::CMultiLineEditDlg(HWND hParent)
    : m_hParent(hParent)
    , m_themeCallbackId(0)
    , m_forReplace(false)
    , m_forRegex(false)
{
}

CMultiLineEditDlg::~CMultiLineEditDlg()
{
}

void CMultiLineEditDlg::setForReplace()
{
    m_forReplace = true;
}
void CMultiLineEditDlg::setForRegex()
{
    m_forRegex = true;
}

LRESULT CMultiLineEditDlg::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
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
            if (!m_forRegex)
            {
                ShowWindow(GetDlgItem(hwndDlg, IDC_CONVERTNEWLINES_BTN), SW_HIDE);
                ShowWindow(GetDlgItem(hwndDlg, IDC_INFOLABEL), SW_HIDE);
            }
            else
            {
                // does the string already contain newlines?
                if (m_regexText.find(L"\n") == std::wstring::npos)
                {
                    // use boost regex to append all \\r\\n, \\r, \\n with newlines, but only if they're not already followed by newlines
                    m_regexText = boost::regex_replace(m_regexText, boost::wregex(L"(\\\\r\\\\n)(?!\\r|\\n|\\||\\))"), L"\\\\r\\\\n\\r\\n");
                    m_regexText = boost::regex_replace(m_regexText, boost::wregex(L"(\\\\r)(?!\\\\n|\\r|\\n|\\||\\))"), L"\\\\r\\r\\n");
                    m_regexText = boost::regex_replace(m_regexText, boost::wregex(L"(\\\\n)(?!\\r|\\n|\\||\\))"), L"\\\\n\\r\\n");
                    m_regexText = boost::regex_replace(m_regexText, boost::wregex(L"(\\(\\\\r\\\\n\\|\\\\r\\|\\\\n\\))(?!\\r|\\n|\\||\\))"), L"\\(\\\\r\\\\n\\|\\\\r\\|\\\\n\\)\\r\\n");
                }
            }

            SetDlgItemText(hwndDlg, IDC_TEXTCONTENT, m_regexText.c_str());

            SetFocus(GetDlgItem(hwndDlg, IDC_TEXTCONTENT));

            m_resizer.Init(hwndDlg);
            m_resizer.UseSizeGrip(!CTheme::Instance().IsDarkTheme());
            m_resizer.AddControl(hwndDlg, IDC_TEXTCONTENT, RESIZER_TOPLEFTBOTTOMRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_INFOLABEL, RESIZER_BOTTOMLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_CONVERTNEWLINES_BTN, RESIZER_BOTTOMLEFT);
            m_resizer.AddControl(hwndDlg, IDOK, RESIZER_BOTTOMRIGHT);
            m_resizer.AddControl(hwndDlg, IDCANCEL, RESIZER_BOTTOMRIGHT);

            SendMessage(GetDlgItem(*this, IDC_TEXTCONTENT), EM_SETEVENTMASK, 0, ENM_CHANGE);
            SendMessage(GetDlgItem(*this, IDC_TEXTCONTENT), EM_EXLIMITTEXT, 0, 200 * 1024);
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
            MINMAXINFO* mmi       = reinterpret_cast<MINMAXINFO*>(lParam);
            mmi->ptMinTrackSize.x = m_resizer.GetDlgRect()->right;
            mmi->ptMinTrackSize.y = m_resizer.GetDlgRect()->bottom;
            return 0;
        }
        case WM_CLOSE:
            CTheme::Instance().RemoveRegisteredCallback(m_themeCallbackId);
            break;
        default:
            return FALSE;
    }
    return FALSE;
}

LRESULT CMultiLineEditDlg::DoCommand(int id, int msg)
{
    switch (id)
    {
        case IDOK:
        {
            auto buf    = GetDlgItemText(IDC_TEXTCONTENT);
            m_regexText = std::wstring(buf.get());
            if (m_forRegex)
            {
                // remove formatting newlines
                SearchReplace(m_regexText, L"\n", L"");
                SearchReplace(m_regexText, L"\r", L"");
            }
        }
            [[fallthrough]];
        case IDCANCEL:
            EndDialog(*this, id);
            break;
        case IDC_TEXTCONTENT:
        {
            if (msg == EN_CHANGE)
            {
                auto buf    = GetDlgItemText(IDC_TEXTCONTENT);
                m_regexText = std::wstring(buf.get());
            }
        }
        break;
        case IDC_CONVERTNEWLINES_BTN:
            CNewlinesDlg newLinesDlg(*this);
            newLinesDlg.setShowBoth(!m_forReplace);
            if (newLinesDlg.DoModal(hResource, IDD_NEWLINESDLG, *this) == IDOK)
            {
                auto buf    = GetDlgItemText(IDC_TEXTCONTENT);
                m_regexText = std::wstring(buf.get());
                // normalize newlines
                SearchReplace(m_regexText, L"\r\n", L"\n");
                SearchReplace(m_regexText, L"\r", L"\n");

                switch (newLinesDlg.getNewLines())
                {
                    case eNewlines::Linux:
                    {
                        // convert newlines to regex newlines
                        SearchReplace(m_regexText, L"\n", L"\\n\r\n");
                    }
                    break;
                    case eNewlines::Windows:
                    {
                        // convert newlines to regex newlines
                        SearchReplace(m_regexText, L"\n", L"\\r\\n\r\n");
                    }
                    break;
                    case eNewlines::Both:
                    {
                        // convert newlines to regex newlines
                        SearchReplace(m_regexText, L"\n", L"(\\r\\n|\\r|\\n)\r\n");
                    }
                    break;
                    default:
                        break;
                }
                SetDlgItemText(*this, IDC_TEXTCONTENT, m_regexText.c_str());
            }
            break;
    }
    return 1;
}
