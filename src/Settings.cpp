// grepWin - regex search and replace for Windows

// Copyright (C) 2012-2013, 2016-2021 - Stefan Kueng

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
#include "Settings.h"
#include "ResString.h"
#include "DirFileEnum.h"
#include "Theme.h"
#include "DarkModeHelper.h"
#include <Commdlg.h>

CSettingsDlg::CSettingsDlg(HWND hParent)
    : m_hParent(hParent)
    , m_regEditorCmd(L"Software\\grepWin\\editorcmd")
    , m_regEsc(L"Software\\grepWin\\escclose", FALSE)
    , m_themeCallbackId(0)
{
}

CSettingsDlg::~CSettingsDlg()
{
}

LRESULT CSettingsDlg::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
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
            DarkModeHelper::Instance().AllowDarkModeForWindow(GetToolTipHWND(), CTheme::Instance().IsDarkTheme());
            SetWindowTheme(GetToolTipHWND(), L"Explorer", nullptr);

            CLanguage::Instance().TranslateWindow(*this);
            AddToolTip(IDC_ONLYONE, TranslatedString(hResource, IDS_ONLYONE_TT).c_str());

            SetDlgItemText(hwndDlg, IDC_EDITORCMD, bPortable ? g_iniFile.GetValue(L"global", L"editorcmd", L"") : std::wstring(m_regEditorCmd).c_str());

            wchar_t modulepath[MAX_PATH] = {0};
            GetModuleFileName(nullptr, modulepath, MAX_PATH);
            std::wstring path = modulepath;
            path              = path.substr(0, path.find_last_of('\\'));
            CDirFileEnum  fileEnumerator(path.c_str());
            bool          bRecurse     = false;
            bool          bIsDirectory = false;
            std::wstring  sPath;
            CRegStdString regLang(L"Software\\grepWin\\languagefile");
            std::wstring  setLang = regLang;
            if (bPortable)
                setLang = g_iniFile.GetValue(L"global", L"languagefile", L"");

            int index     = 1;
            int langIndex = 0;
            SendDlgItemMessage(hwndDlg, IDC_LANGUAGE, CB_INSERTSTRING, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(L"English"));
            while (fileEnumerator.NextFile(sPath, &bIsDirectory, bRecurse))
            {
                size_t dotPos = sPath.find_last_of('.');
                if (dotPos == std::wstring::npos)
                    continue;
                std::wstring ext = sPath.substr(dotPos);
                if (ext.compare(L".lang"))
                    continue;
                m_langPaths.push_back(sPath);
                if (sPath.compare(setLang) == 0)
                    langIndex = index;
                size_t slashPos = sPath.find_last_of('\\');
                if (slashPos == std::wstring::npos)
                    continue;
                sPath  = sPath.substr(slashPos + 1);
                dotPos = sPath.find_last_of('.');
                sPath  = sPath.substr(0, dotPos);
                SendDlgItemMessage(hwndDlg, IDC_LANGUAGE, CB_INSERTSTRING, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(sPath.c_str()));
                ++index;
            }

            SendDlgItemMessage(hwndDlg, IDC_LANGUAGE, CB_SETCURSEL, langIndex, 0);
            SendDlgItemMessage(hwndDlg, IDC_ESCKEY, BM_SETCHECK, bPortable ? !!_wtoi(g_iniFile.GetValue(L"settings", L"escclose", L"0")) : static_cast<DWORD>(CRegStdDWORD(L"Software\\grepWin\\escclose", FALSE)) ? BST_CHECKED : BST_UNCHECKED, 0);
            SendDlgItemMessage(hwndDlg, IDC_BACKUPINFOLDER, BM_SETCHECK, bPortable ? !!_wtoi(g_iniFile.GetValue(L"settings", L"backupinfolder", L"0")) : static_cast<DWORD>(CRegStdDWORD(L"Software\\grepWin\\backupinfolder", FALSE)) ? BST_CHECKED : BST_UNCHECKED, 0);
            SendDlgItemMessage(hwndDlg, IDC_NOWARNINGIFNOBACKUP, BM_SETCHECK, bPortable ? !!_wtoi(g_iniFile.GetValue(L"settings", L"nowarnifnobackup", L"0")) : static_cast<DWORD>(CRegStdDWORD(L"Software\\grepWin\\nowarnifnobackup", FALSE)) ? BST_CHECKED : BST_UNCHECKED, 0);
            SendDlgItemMessage(hwndDlg, IDC_ONLYONE, BM_SETCHECK, bPortable ? _wtoi(g_iniFile.GetValue(L"global", L"onlyone", L"0")) : static_cast<DWORD>(CRegStdDWORD(L"Software\\grepWin\\onlyone", FALSE)) ? BST_CHECKED : BST_UNCHECKED, 0);
            SendDlgItemMessage(hwndDlg, IDC_DOUPDATECHECKS, BM_SETCHECK, bPortable ? _wtoi(g_iniFile.GetValue(L"global", L"CheckForUpdates", L"1")) : static_cast<DWORD>(CRegStdDWORD(L"Software\\grepWin\\CheckForUpdates", FALSE)) ? BST_CHECKED : BST_UNCHECKED, 0);
            SendDlgItemMessage(hwndDlg, IDC_DARKMODE, BM_SETCHECK, CTheme::Instance().IsDarkTheme() ? BST_CHECKED : BST_UNCHECKED, 0);
            EnableWindow(GetDlgItem(*this, IDC_DARKMODE), CTheme::Instance().IsDarkModeAllowed());
            SetDlgItemText(*this, IDC_NUMNULL, bPortable ? g_iniFile.GetValue(L"settings", L"nullbytes", L"0") : std::to_wstring(static_cast<DWORD>(CRegStdDWORD(L"Software\\grepWin\\nullbytes", 0))).c_str());

            AddToolTip(IDC_BACKUPINFOLDER, TranslatedString(hResource, IDS_BACKUPINFOLDER_TT).c_str());
            if (!CTheme::Instance().IsDarkModeAllowed())
                SetDlgItemText(*this, IDC_DARKMODEINFO, TranslatedString(hResource, IDS_DARKMODE_TT).c_str());

            m_resizer.Init(hwndDlg);
            m_resizer.UseSizeGrip(!CTheme::Instance().IsDarkTheme());
            m_resizer.AddControl(hwndDlg, IDC_EDITORGROUP, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_EDITORCMD, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_SEARCHPATHBROWSE, RESIZER_TOPRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_STATIC1, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_STATIC2, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_STATIC3, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_STATIC4, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_NUMNULL, RESIZER_TOPRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_LANGUAGE, RESIZER_TOPRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_ESCKEY, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_BACKUPINFOLDER, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_NOWARNINGIFNOBACKUP, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_ONLYONE, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_DOUPDATECHECKS, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_DARKMODE, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_DARKMODEINFO, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDOK, RESIZER_BOTTOMRIGHT);
            m_resizer.AddControl(hwndDlg, IDCANCEL, RESIZER_BOTTOMRIGHT);
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
            MINMAXINFO* mmi       = reinterpret_cast<MINMAXINFO*>(lParam);
            mmi->ptMinTrackSize.x = m_resizer.GetDlgRectScreen()->right;
            mmi->ptMinTrackSize.y = m_resizer.GetDlgRectScreen()->bottom;
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
            int          langIndex = static_cast<int>(SendDlgItemMessage(*this, IDC_LANGUAGE, CB_GETCURSEL, 0, 0));
            std::wstring langPath  = langIndex == 0 ? L"" : m_langPaths[langIndex - 1];
            if (bPortable)
                g_iniFile.SetValue(L"global", L"languagefile", langPath.c_str());
            else
            {
                CRegStdString regLang(L"Software\\grepWin\\languagefile");
                if (langIndex == 0)
                {
                    regLang.removeValue();
                }
                else
                {
                    regLang = langPath;
                }
            }
            CLanguage::Instance().LoadFile(langPath);
            CLanguage::Instance().TranslateWindow(::GetParent(*this));

            std::wstring sNumNull = GetDlgItemText(IDC_NUMNULL).get();

            if (bPortable)
            {
                g_iniFile.SetValue(L"settings", L"escclose", (IsDlgButtonChecked(*this, IDC_ESCKEY) == BST_CHECKED) ? L"1" : L"0");
                g_iniFile.SetValue(L"settings", L"backupinfolder", (IsDlgButtonChecked(*this, IDC_BACKUPINFOLDER) == BST_CHECKED) ? L"1" : L"0");
                g_iniFile.SetValue(L"settings", L"nowarnifnobackup", (IsDlgButtonChecked(*this, IDC_NOWARNINGIFNOBACKUP) == BST_CHECKED) ? L"1" : L"0");
                g_iniFile.SetValue(L"global", L"onlyone", IsDlgButtonChecked(*this, IDC_ONLYONE) == BST_CHECKED ? L"1" : L"0");
                g_iniFile.SetValue(L"global", L"CheckForUpdates", IsDlgButtonChecked(*this, IDC_DOUPDATECHECKS) == BST_CHECKED ? L"1" : L"0");
                g_iniFile.SetValue(L"settings", L"nullbytes", sNumNull.c_str());
            }
            else
            {
                // ReSharper disable CppEntityAssignedButNoRead
                CRegStdDWORD esc(L"Software\\grepWin\\escclose", FALSE);
                esc = (IsDlgButtonChecked(*this, IDC_ESCKEY) == BST_CHECKED);
                CRegStdDWORD backup(L"Software\\grepWin\\backupinfolder", FALSE);
                backup = (IsDlgButtonChecked(*this, IDC_BACKUPINFOLDER) == BST_CHECKED);
                CRegStdDWORD nowarn(L"Software\\grepWin\\nowarnifnobackup", FALSE);
                nowarn = (IsDlgButtonChecked(*this, IDC_NOWARNINGIFNOBACKUP) == BST_CHECKED);
                CRegStdDWORD regOnlyOne(L"Software\\grepWin\\onlyone", FALSE);
                regOnlyOne = (IsDlgButtonChecked(*this, IDC_ONLYONE) == BST_CHECKED);
                CRegStdDWORD regCheckForUpdates(L"Software\\grepWin\\CheckForUpdates", FALSE);
                regCheckForUpdates = (IsDlgButtonChecked(*this, IDC_DOUPDATECHECKS) == BST_CHECKED);
                CRegStdDWORD regNumNull(L"Software\\grepWin\\nullbytes", FALSE);
                regNumNull = _wtoi(sNumNull.c_str());
                // ReSharper restore CppEntityAssignedButNoRead
            }
            CTheme::Instance().SetDarkTheme(IsDlgButtonChecked(*this, IDC_DARKMODE) == BST_CHECKED);
        }
            [[fallthrough]];
        case IDCANCEL:
            CTheme::Instance().RemoveRegisteredCallback(m_themeCallbackId);
            EndDialog(*this, id);
            break;
        case IDC_SEARCHPATHBROWSE:
        {
            OPENFILENAME ofn              = {0}; // common dialog box structure
            wchar_t      szFile[MAX_PATH] = {0}; // buffer for file name
            // Initialize OPENFILENAME
            ofn.lStructSize     = sizeof(OPENFILENAME);
            ofn.hwndOwner       = *this;
            ofn.lpstrFile       = szFile;
            ofn.nMaxFile        = _countof(szFile);
            std::wstring sTitle = TranslatedString(hResource, IDS_SELECTEDITOR);
            ofn.lpstrTitle      = sTitle.c_str();
            ofn.Flags           = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST | OFN_DONTADDTORECENT;
            auto sProgs         = TranslatedString(hResource, IDS_PROGRAMS);
            auto sAllFiles      = TranslatedString(hResource, IDS_ALLFILES);
            auto sFilter        = sProgs;
            sFilter.append(L"\0*.exe;*.com\0", _countof(L"\0*.exe;*.com\0") - 1);
            sFilter.append(sAllFiles);
            sFilter.append(L"\0*.*\0\0", _countof(L"\0*.*\0\0") - 1);
            ofn.lpstrFilter  = sFilter.c_str();
            ofn.nFilterIndex = 1;
            // Display the Open dialog box.
            if (GetOpenFileName(&ofn) == TRUE)
            {
                SetDlgItemText(*this, IDC_EDITORCMD, szFile);
            }
        }
        break;
    }
    return 1;
}
