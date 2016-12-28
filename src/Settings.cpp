// grepWin - regex search and replace for Windows

// Copyright (C) 2012-2013, 2016 - Stefan Kueng

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
#include "maxpath.h"
#include "Settings.h"
#include "BrowseFolder.h"
#include "DirFileEnum.h"
#include <Commdlg.h>


CSettingsDlg::CSettingsDlg(HWND hParent)
    : m_hParent(hParent)
    , m_regEditorCmd(_T("Software\\grepWin\\editorcmd"))
    , m_regEsc(_T("Software\\grepWin\\escclose"), FALSE)
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

            CLanguage::Instance().TranslateWindow(*this);

            SetDlgItemText(hwndDlg, IDC_EDITORCMD, bPortable ? g_iniFile.GetValue(L"global", L"editorcmd", L"") : std::wstring(m_regEditorCmd).c_str());

            wchar_t modulepath[MAX_PATH] = {0};
            GetModuleFileName(NULL, modulepath, MAX_PATH);
            std::wstring path = modulepath;
            path = path.substr(0, path.find_last_of('\\'));
            CDirFileEnum fileEnumerator(path.c_str());
            bool bRecurse = false;
            bool bIsDirectory = false;
            std::wstring sPath;
            CRegStdString regLang(L"Software\\grepWin\\languagefile");
            std::wstring setLang = regLang;
            if (bPortable)
                setLang = g_iniFile.GetValue(L"global", L"languagefile", L"");

            int index = 1;
            int langIndex = 0;
            SendDlgItemMessage(hwndDlg, IDC_LANGUAGE, CB_INSERTSTRING, (WPARAM)-1, (LPARAM)L"English");
            while (fileEnumerator.NextFile(sPath, &bIsDirectory, bRecurse))
            {
                size_t dotpos = sPath.find_last_of('.');
                if (dotpos == std::wstring::npos)
                    continue;
                std::wstring ext = sPath.substr(dotpos);
                if (ext.compare(L".lang"))
                    continue;
                m_langpaths.push_back(sPath);
                if (sPath.compare(setLang)==0)
                    langIndex = index;
                size_t slashpos = sPath.find_last_of('\\');
                if (slashpos == std::wstring::npos)
                    continue;
                sPath = sPath.substr(slashpos+1);
                dotpos = sPath.find_last_of('.');
                sPath = sPath.substr(0, dotpos);
                SendDlgItemMessage(hwndDlg, IDC_LANGUAGE, CB_INSERTSTRING, (WPARAM)-1, (LPARAM)sPath.c_str());
                ++index;
            }
            SendDlgItemMessage(hwndDlg, IDC_LANGUAGE, CB_SETCURSEL, langIndex, 0);
            SendDlgItemMessage(hwndDlg, IDC_ESCKEY, BM_SETCHECK, DWORD(CRegStdDWORD(L"Software\\grepWin\\escclose", FALSE)) ? BST_CHECKED : BST_UNCHECKED, 0);

            m_resizer.Init(hwndDlg);
            m_resizer.AddControl(hwndDlg, IDC_EDITORGROUP, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_EDITORCMD, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_STATIC1, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_STATIC2, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_STATIC3, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_STATIC4, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_LANGUAGE, RESIZER_TOPRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_ESCKEY, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_DWM, RESIZER_BOTTOMLEFT);
            m_resizer.AddControl(hwndDlg, IDOK, RESIZER_BOTTOMRIGHT);
            m_resizer.AddControl(hwndDlg, IDCANCEL, RESIZER_BOTTOMRIGHT);
            ExtendFrameIntoClientArea(0, 0, 0, IDC_DWM);
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
            mmi->ptMinTrackSize.x = m_resizer.GetDlgRectScreen()->right;
            mmi->ptMinTrackSize.y = m_resizer.GetDlgRectScreen()->bottom;
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
            auto buf = GetDlgItemText(IDC_EDITORCMD);
            if (bPortable)
                g_iniFile.SetValue(L"global", L"editorcmd", buf.get());
            else
                m_regEditorCmd = buf.get();
            int langIndex = (int)SendDlgItemMessage(*this, IDC_LANGUAGE, CB_GETCURSEL, 0, 0);
            std::wstring langpath = langIndex==0 ? L"" : m_langpaths[langIndex-1];
            if (bPortable)
                g_iniFile.SetValue(L"global", L"languagefile", langpath.c_str());
            else
            {
                CRegStdString regLang(L"Software\\grepWin\\languagefile");
                if (langIndex==0)
                {
                    regLang.removeValue();
                }
                else
                {
                    regLang = langpath;
                }
            }
            CLanguage::Instance().LoadFile(langpath);
            CLanguage::Instance().TranslateWindow(::GetParent(*this));
            CRegStdDWORD esc(L"Software\\grepWin\\escclose", FALSE);
            esc = (IsDlgButtonChecked(*this, IDC_ESCKEY) == BST_CHECKED);
        }
        // fall through
    case IDCANCEL:
        EndDialog(*this, id);
        break;
    case IDC_SEARCHPATHBROWSE:
        {
            OPENFILENAME ofn = {0};     // common dialog box structure
            TCHAR szFile[MAX_PATH] = {0};  // buffer for file name
            // Initialize OPENFILENAME
            ofn.lStructSize = sizeof(OPENFILENAME);
            ofn.hwndOwner = *this;
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = _countof(szFile);
            std::wstring sTitle = TranslatedString(hResource, IDS_SELECTEDITOR);
            ofn.lpstrTitle = sTitle.c_str();
            ofn.Flags = OFN_FILEMUSTEXIST|OFN_HIDEREADONLY|OFN_PATHMUSTEXIST|OFN_DONTADDTORECENT;
            auto sProgs = TranslatedString(hResource, IDS_PROGRAMS);
            auto sAllFiles = TranslatedString(hResource, IDS_ALLFILES);
            auto sFilter = sProgs;
            sFilter.append(L"\0*.exe;*.com\0", _countof(L"\0*.exe;*.com\0")-1);
            sFilter.append(sAllFiles);
            sFilter.append(L"\0*.*\0\0", _countof(L"\0*.*\0\0")-1);
            ofn.lpstrFilter = sFilter.c_str();
            ofn.nFilterIndex = 1;
            // Display the Open dialog box.
            if (GetOpenFileName(&ofn)==TRUE)
            {
                SetDlgItemText(*this, IDC_EDITORCMD, szFile);
            }
        }
        break;
    }
    return 1;
}

