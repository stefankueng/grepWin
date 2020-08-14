// grepWin - regex search and replace for Windows

// Copyright (C) 2007-2020 - Stefan Kueng

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
#include "SearchDlg.h"
#include "Registry.h"
#include "DirFileEnum.h"
#include "TextFile.h"
#include "SearchInfo.h"
#include "UnicodeUtils.h"
#include "StringUtils.h"
#include "BrowseFolder.h"
#include "SysImageList.h"
#include "ShellContextMenu.h"
#include "RegexTestDlg.h"
#include "NameDlg.h"
#include "BookmarksDlg.h"
#include "MultiLineEditDlg.h"
#include "AboutDlg.h"
#include "InfoDlg.h"
#include "DropFiles.h"
#include "RegexReplaceFormatter.h"
#include "LineData.h"
#include "Settings.h"
#include "SysInfo.h"
#include "Language.h"
#include "SmartHandle.h"
#include "PathUtils.h"
#include "DebugOutput.h"
#include "Theme.h"
#include "DarkModeHelper.h"
#include "ThreadPool.h"
#include "OnOutOfScope.h"
#include "COMPtrs.h"
#include "PreserveChdir.h"
#include "TempFile.h"
#include "version.h"

#include <string>
#include <map>
#include <iostream>
#include <fstream>
#include <iterator>
#include <algorithm>
#include <Commdlg.h>

#pragma warning(push)
#pragma warning(disable : 4996) // warning STL4010: Various members of std::allocator are deprecated in C++17
#include <boost/regex.hpp>
#include <boost/spirit/include/classic_file_iterator.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#pragma warning(pop)

#define GREPWIN_DATEBUFFER 100
#define LABELUPDATETIMER   10

DWORD WINAPI SearchThreadEntry(LPVOID lpParam);

UINT                    CSearchDlg::GREPWIN_STARTUPMSG = RegisterWindowMessage(L"grepWin_StartupMessage");
std::map<size_t, DWORD> linepositions;

extern ULONGLONG g_startTime;
extern HANDLE    hInitProtection;

CSearchDlg::CSearchDlg(HWND hParent)
    : m_searchedItems(0)
    , m_totalitems(0)
    , m_dwThreadRunning(FALSE)
    , m_Cancelled(FALSE)
    , m_bAscending(true)
    , m_pDropTarget(NULL)
    , m_hParent(hParent)
    , m_ExecuteImmediately(ExecuteAction::None)
    , m_endDialog(false)
    , m_bUseRegexForPaths(false)
    , m_bUseRegex(false)
    , m_bIncludeSystem(false)
    , m_bIncludeSystemC(false)
    , m_bIncludeHidden(false)
    , m_bIncludeHiddenC(false)
    , m_bIncludeSubfolders(false)
    , m_bIncludeSubfoldersC(false)
    , m_bIncludeBinary(false)
    , m_bIncludeBinaryC(false)
    , m_bCreateBackup(false)
    , m_bCreateBackupC(false)
    , m_bConfirmationOnReplace(true)
    , m_bUTF8(false)
    , m_bUTF8C(false)
    , m_bForceBinary(false)
    , m_bCaseSensitive(false)
    , m_bCaseSensitiveC(false)
    , m_bDotMatchesNewline(false)
    , m_bDotMatchesNewlineC(false)
    , m_bNOTSearch(false)
    , m_bCaptureSearch(false)
    , m_bSizeC(false)
    , m_bAllSize(false)
    , m_bReplace(false)
    , m_lSize(0)
    , m_sizeCmp(0)
    , m_totalmatches(0)
    , m_DateLimit(0)
    , m_Date1({0})
    , m_Date2({0})
    , m_bDateLimitC(false)
    , m_regUseRegex(L"Software\\grepWin\\UseRegex", 1)
    , m_regAllSize(L"Software\\grepWin\\AllSize")
    , m_regSize(L"Software\\grepWin\\Size", L"2000")
    , m_regSizeCombo(L"Software\\grepWin\\SizeCombo", 0)
    , m_regIncludeSystem(L"Software\\grepWin\\IncludeSystem")
    , m_regIncludeHidden(L"Software\\grepWin\\IncludeHidden")
    , m_regIncludeSubfolders(L"Software\\grepWin\\IncludeSubfolders", 1)
    , m_regIncludeBinary(L"Software\\grepWin\\IncludeBinary", 1)
    , m_regCreateBackup(L"Software\\grepWin\\CreateBackup")
    , m_regUTF8(L"Software\\grepWin\\UTF8")
    , m_regBinary(L"Software\\grepWin\\Binary")
    , m_regCaseSensitive(L"Software\\grepWin\\CaseSensitive")
    , m_regDotMatchesNewline(L"Software\\grepWin\\DotMatchesNewline")
    , m_regUseRegexForPaths(L"Software\\grepWin\\UseFileMatchRegex")
    , m_regPattern(L"Software\\grepWin\\pattern")
    , m_regExcludeDirsPattern(L"Software\\grepWin\\ExcludeDirsPattern")
    , m_regSearchPath(L"Software\\grepWin\\searchpath")
    , m_regEditorCmd(L"Software\\grepWin\\editorcmd")
    , m_regBackupInFolder(L"Software\\grepWin\\backupinfolder", FALSE)
    , m_regDateLimit(L"Software\\grepWin\\DateLimit", 0)
    , m_regDate1Low(L"Software\\grepWin\\Date1Low", 0)
    , m_regDate1High(L"Software\\grepWin\\Date1High", 0)
    , m_regDate2Low(L"Software\\grepWin\\Date2Low", 0)
    , m_regDate2High(L"Software\\grepWin\\Date2High", 0)
    , m_regShowContent(L"Software\\grepWin\\ShowContent", 0)
    , m_AutoCompleteFilePatterns(bPortable ? &g_iniFile : NULL)
    , m_AutoCompleteExcludeDirsPatterns(bPortable ? &g_iniFile : NULL)
    , m_AutoCompleteSearchPatterns(bPortable ? &g_iniFile : NULL)
    , m_AutoCompleteReplacePatterns(bPortable ? &g_iniFile : NULL)
    , m_AutoCompleteSearchPaths(bPortable ? &g_iniFile : NULL)
    , m_pBookmarksDlg(nullptr)
    , m_showContent(false)
    , m_showContentSet(false)
    , m_themeCallbackId(0)
{
}

CSearchDlg::~CSearchDlg(void)
{
    if (m_pDropTarget)
        delete m_pDropTarget;
    if (m_pBookmarksDlg)
        delete m_pBookmarksDlg;
}

LRESULT CSearchDlg::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == GREPWIN_STARTUPMSG)
    {
        if ((GetTickCount64() - 4000) < g_startTime)
        {
            if (wParam == 0)
                g_startTime = GetTickCount64();
            return TRUE;
        }
        if (wParam == 0)
            g_startTime = GetTickCount64();
        return FALSE;
    }
    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            SHAutoComplete(GetDlgItem(*this, IDC_SEARCHPATH), SHACF_FILESYSTEM | SHACF_AUTOSUGGEST_FORCE_ON);

            m_AutoCompleteFilePatterns.Load(L"Software\\grepWin\\History", L"FilePattern");
            m_AutoCompleteFilePatterns.Init(GetDlgItem(hwndDlg, IDC_PATTERN));
            m_AutoCompleteExcludeDirsPatterns.Load(L"Software\\grepWin\\History", L"ExcludeDirsPattern");
            m_AutoCompleteExcludeDirsPatterns.Init(GetDlgItem(hwndDlg, IDC_EXCLUDEDIRSPATTERN));
            m_AutoCompleteSearchPatterns.Load(L"Software\\grepWin\\History", L"SearchPattern");
            m_AutoCompleteSearchPatterns.Init(GetDlgItem(hwndDlg, IDC_SEARCHTEXT));
            m_AutoCompleteReplacePatterns.Load(L"Software\\grepWin\\History", L"ReplacePattern");
            m_AutoCompleteReplacePatterns.Init(GetDlgItem(hwndDlg, IDC_REPLACETEXT));
            m_AutoCompleteSearchPaths.Load(L"Software\\grepWin\\History", L"SearchPaths");
            m_AutoCompleteSearchPaths.Init(GetDlgItem(hwndDlg, IDC_SEARCHPATH));

            m_link.ConvertStaticToHyperlink(hwndDlg, IDC_ABOUTLINK, L"");

            m_themeCallbackId = CTheme::Instance().RegisterThemeChangeCallback(
                [this]() {
                    auto bDark = CTheme::Instance().IsDarkTheme();
                    if (bDark)
                        DarkModeHelper::Instance().AllowDarkModeForApp(bDark);
                    CTheme::Instance().SetThemeForDialog(*this, bDark);
                    DarkModeHelper::Instance().AllowDarkModeForWindow(GetToolTipHWND(), bDark);
                    if (!bDark)
                        DarkModeHelper::Instance().AllowDarkModeForApp(bDark);
                });
            auto bDark = CTheme::Instance().IsDarkTheme();
            if (bDark)
                DarkModeHelper::Instance().AllowDarkModeForApp(bDark);
            CTheme::Instance().SetThemeForDialog(*this, CTheme::Instance().IsDarkTheme());
            DarkModeHelper::Instance().AllowDarkModeForWindow(GetToolTipHWND(), bDark);
            if (!bDark)
                DarkModeHelper::Instance().AllowDarkModeForApp(bDark);
            SetWindowTheme(GetToolTipHWND(), L"Explorer", nullptr);

            CLanguage::Instance().TranslateWindow(*this);
            AddToolTip(IDC_PATTERN, TranslatedString(hResource, IDS_PATTERN_TT).c_str());
            AddToolTip(IDC_EXCLUDEDIRSPATTERN, TranslatedString(hResource, IDS_EXCLUDEDIR_TT).c_str());
            AddToolTip(IDC_SEARCHPATH, TranslatedString(hResource, IDS_SEARCHPATH_TT).c_str());
            AddToolTip(IDC_DOTMATCHNEWLINE, TranslatedString(hResource, IDS_DOTMATCHNEWLINE_TT).c_str());
            AddToolTip(IDC_SEARCHTEXT, TranslatedString(hResource, IDS_SEARCHTEXT_TT).c_str());
            AddToolTip(IDC_EDITMULTILINE1, TranslatedString(hResource, IDS_EDITMULTILINE_TT).c_str());
            AddToolTip(IDC_EDITMULTILINE2, TranslatedString(hResource, IDS_EDITMULTILINE_TT).c_str());
            AddToolTip(IDC_EXPORT, TranslatedString(hResource, IDS_EXPORT_TT).c_str());
            AddToolTip(IDOK, TranslatedString(hResource, IDS_SHIFT_NOTSEARCH).c_str());

            if (m_searchpath.empty())
            {
                if (bPortable)
                    m_searchpath = g_iniFile.GetValue(L"global", L"searchpath", L"");
                else
                    m_searchpath = std::wstring(m_regSearchPath);
            }
            else
            {
                // expand a possible 'short' path
                DWORD ret = 0;
                ret       = ::GetLongPathName(m_searchpath.c_str(), NULL, 0);
                if (ret)
                {
                    auto pathbuf = std::make_unique<wchar_t[]>(ret + 2);
                    ret          = ::GetLongPathName(m_searchpath.c_str(), pathbuf.get(), ret + 1);
                    m_searchpath = std::wstring(pathbuf.get(), ret);
                }
            }

            if (m_patternregex.empty())
            {
                if (bPortable)
                {
                    m_patternregex      = g_iniFile.GetValue(L"global", L"pattern", L"");
                    m_bUseRegexForPaths = !!_wtoi(g_iniFile.GetValue(L"global", L"UseFileMatchRegex", L""));
                }
                else
                {
                    m_patternregex      = std::wstring(m_regPattern);
                    m_bUseRegexForPaths = !!DWORD(m_regUseRegexForPaths);
                }
            }
            if (m_excludedirspatternregex.empty())
            {
                if (bPortable)
                    m_excludedirspatternregex = g_iniFile.GetValue(L"global", L"ExcludeDirsPattern", L"");
                else
                    m_excludedirspatternregex = std::wstring(m_regExcludeDirsPattern);
            }
            // initialize the controls
            SetDlgItemText(hwndDlg, IDC_SEARCHPATH, m_searchpath.c_str());
            SetDlgItemText(hwndDlg, IDC_SEARCHTEXT, m_searchString.c_str());
            SetDlgItemText(hwndDlg, IDC_EXCLUDEDIRSPATTERN, m_excludedirspatternregex.c_str());
            SetDlgItemText(hwndDlg, IDC_PATTERN, m_patternregex.c_str());
            SetDlgItemText(hwndDlg, IDC_REPLACETEXT, m_replaceString.c_str());

            // the path edit control should work as a drop target for files and folders
            HWND hSearchPath = GetDlgItem(hwndDlg, IDC_SEARCHPATH);
            m_pDropTarget    = new CFileDropTarget(hSearchPath);
            RegisterDragDrop(hSearchPath, m_pDropTarget);
            // create the supported formats:
            FORMATETC ftetc = {0};
            ftetc.cfFormat  = CF_TEXT;
            ftetc.dwAspect  = DVASPECT_CONTENT;
            ftetc.lindex    = -1;
            ftetc.tymed     = TYMED_HGLOBAL;
            m_pDropTarget->AddSuportedFormat(ftetc);
            ftetc.cfFormat = CF_HDROP;
            m_pDropTarget->AddSuportedFormat(ftetc);
            m_pDropTarget->SetMultipathConcatenate('|');

            m_editFilePatterns.Subclass(hwndDlg, IDC_PATTERN);
            m_editExcludeDirsPatterns.Subclass(hwndDlg, IDC_EXCLUDEDIRSPATTERN);
            m_editSearchPatterns.Subclass(hwndDlg, IDC_SEARCHTEXT);
            m_editReplacePatterns.Subclass(hwndDlg, IDC_REPLACETEXT);
            m_editSearchPaths.Subclass(hwndDlg, IDC_SEARCHPATH);

            // add an "About" entry to the system menu
            HMENU hSysMenu = GetSystemMenu(hwndDlg, FALSE);
            if (hSysMenu)
            {
                int menuItemsCount = GetMenuItemCount(hSysMenu);
                if (menuItemsCount > 2)
                {
                    InsertMenu(hSysMenu, menuItemsCount - 2, MF_STRING | MF_BYPOSITION, ID_ABOUTBOX, TranslatedString(hResource, IDS_ABOUT).c_str());
                    InsertMenu(hSysMenu, menuItemsCount - 2, MF_SEPARATOR | MF_BYPOSITION, NULL, NULL);
                }
                else
                {
                    AppendMenu(hSysMenu, MF_SEPARATOR, NULL, NULL);
                    AppendMenu(hSysMenu, MF_STRING, ID_ABOUTBOX, TranslatedString(hResource, IDS_ABOUT).c_str());
                }
            }

            wchar_t buf[MAX_PATH] = {0};
            if (m_bSizeC && (m_lSize != (uint64_t)-1))
            {
                _stprintf_s(buf, _countof(buf), L"%I64u", m_lSize);
                SetDlgItemText(hwndDlg, IDC_SIZEEDIT, buf);
            }
            else
            {
                uint64_t s = _wtoll(std::wstring(m_regSize).c_str());
                if (bPortable)
                    s = _wtoi(g_iniFile.GetValue(L"global", L"size", L"2000"));
                _stprintf_s(buf, _countof(buf), L"%I64u", s);
                SetDlgItemText(hwndDlg, IDC_SIZEEDIT, buf);
            }

            SendDlgItemMessage(hwndDlg, IDC_SIZECOMBO, CB_INSERTSTRING, (WPARAM)-1, (LPARAM)(LPCWSTR)TranslatedString(hResource, IDS_LESSTHAN).c_str());
            SendDlgItemMessage(hwndDlg, IDC_SIZECOMBO, CB_INSERTSTRING, (WPARAM)-1, (LPARAM)(LPCWSTR)TranslatedString(hResource, IDS_EQUALTO).c_str());
            SendDlgItemMessage(hwndDlg, IDC_SIZECOMBO, CB_INSERTSTRING, (WPARAM)-1, (LPARAM)(LPCWSTR)TranslatedString(hResource, IDS_GREATERTHAN).c_str());
            if (!m_bIncludeSubfoldersC)
                m_bIncludeSubfolders = bPortable ? !!_wtoi(g_iniFile.GetValue(L"global", L"IncludeSubfolders", L"1")) : !!DWORD(m_regIncludeSubfolders);
            if (!m_bIncludeSystemC)
                m_bIncludeSystem = bPortable ? !!_wtoi(g_iniFile.GetValue(L"global", L"IncludeSystem", L"1")) : !!DWORD(m_regIncludeSystem);
            if (!m_bIncludeHiddenC)
                m_bIncludeHidden = bPortable ? !!_wtoi(g_iniFile.GetValue(L"global", L"IncludeHidden", L"0")) : !!DWORD(m_regIncludeHidden);
            if (!m_bIncludeBinaryC)
                m_bIncludeBinaryC = bPortable ? !!_wtoi(g_iniFile.GetValue(L"global", L"IncludeBinary", L"0")) : !!DWORD(m_regIncludeBinary);
            if (!m_bCaseSensitiveC)
                m_bCaseSensitive = bPortable ? !!_wtoi(g_iniFile.GetValue(L"global", L"CaseSensitive", L"0")) : !!DWORD(m_regCaseSensitive);
            if (!m_bDotMatchesNewlineC)
                m_bDotMatchesNewline = bPortable ? !!_wtoi(g_iniFile.GetValue(L"global", L"DotMatchesNewline", L"0")) : !!DWORD(m_regDotMatchesNewline);
            if (!m_bCreateBackupC)
                m_bCreateBackup = bPortable ? !!_wtoi(g_iniFile.GetValue(L"global", L"CreateBackup", L"0")) : !!DWORD(m_regCreateBackup);
            if (!m_bUTF8C)
            {
                m_bUTF8        = bPortable ? !!_wtoi(g_iniFile.GetValue(L"global", L"UTF8", L"0")) : !!DWORD(m_regUTF8);
                m_bForceBinary = bPortable ? !!_wtoi(g_iniFile.GetValue(L"global", L"Binary", L"0")) : !!DWORD(m_regBinary);
            }
            if (!m_bDotMatchesNewlineC)
                m_bDotMatchesNewline = bPortable ? !!_wtoi(g_iniFile.GetValue(L"global", L"DotMatchesNewline", L"0")) : !!DWORD(m_regDotMatchesNewline);
            if (!m_bSizeC)
            {
                m_bAllSize = bPortable ? !!_wtoi(g_iniFile.GetValue(L"global", L"AllSize", L"0")) : !!DWORD(m_regAllSize);
                m_sizeCmp  = bPortable ? _wtoi(g_iniFile.GetValue(L"global", L"SizeCombo", L"0")) : (int)DWORD(m_regSizeCombo);
            }
            if (!m_bDateLimitC)
            {
                m_DateLimit            = bPortable ? _wtoi(g_iniFile.GetValue(L"global", L"DateLimit", L"0")) : (int)DWORD(m_regDateLimit);
                m_Date1.dwLowDateTime  = bPortable ? wcstoul(g_iniFile.GetValue(L"global", L"Date1Low", L"0"), nullptr, 10) : DWORD(m_regDate1Low);
                m_Date1.dwHighDateTime = bPortable ? wcstoul(g_iniFile.GetValue(L"global", L"Date1High", L"0"), nullptr, 10) : DWORD(m_regDate1High);
                m_Date2.dwLowDateTime  = bPortable ? wcstoul(g_iniFile.GetValue(L"global", L"Date2Low", L"0"), nullptr, 10) : DWORD(m_regDate2Low);
                m_Date2.dwHighDateTime = bPortable ? wcstoul(g_iniFile.GetValue(L"global", L"Date2High", L"0"), nullptr, 10) : DWORD(m_regDate2High);
            }

            SendDlgItemMessage(hwndDlg, IDC_SIZECOMBO, CB_SETCURSEL, m_sizeCmp, 0);

            SendDlgItemMessage(hwndDlg, IDC_INCLUDESUBFOLDERS, BM_SETCHECK, m_bIncludeSubfolders ? BST_CHECKED : BST_UNCHECKED, 0);
            SendDlgItemMessage(hwndDlg, IDC_CREATEBACKUP, BM_SETCHECK, m_bCreateBackup ? BST_CHECKED : BST_UNCHECKED, 0);
            SendDlgItemMessage(hwndDlg, IDC_UTF8, BM_SETCHECK, m_bUTF8 ? BST_CHECKED : BST_UNCHECKED, 0);
            SendDlgItemMessage(hwndDlg, IDC_BINARY, BM_SETCHECK, m_bForceBinary ? BST_CHECKED : BST_UNCHECKED, 0);
            SendDlgItemMessage(hwndDlg, IDC_INCLUDESYSTEM, BM_SETCHECK, m_bIncludeSystem ? BST_CHECKED : BST_UNCHECKED, 0);
            SendDlgItemMessage(hwndDlg, IDC_INCLUDEHIDDEN, BM_SETCHECK, m_bIncludeHidden ? BST_CHECKED : BST_UNCHECKED, 0);
            SendDlgItemMessage(hwndDlg, IDC_INCLUDEBINARY, BM_SETCHECK, m_bIncludeBinary ? BST_CHECKED : BST_UNCHECKED, 0);
            SendDlgItemMessage(hwndDlg, IDC_CASE_SENSITIVE, BM_SETCHECK, m_bCaseSensitive ? BST_CHECKED : BST_UNCHECKED, 0);
            SendDlgItemMessage(hwndDlg, IDC_DOTMATCHNEWLINE, BM_SETCHECK, m_bDotMatchesNewline ? BST_CHECKED : BST_UNCHECKED, 0);

            CheckRadioButton(hwndDlg, IDC_REGEXRADIO, IDC_TEXTRADIO, (bPortable ? _wtoi(g_iniFile.GetValue(L"global", L"UseRegex", L"0")) : DWORD(m_regUseRegex)) ? IDC_REGEXRADIO : IDC_TEXTRADIO);
            CheckRadioButton(hwndDlg, IDC_ALLSIZERADIO, IDC_SIZERADIO, m_bAllSize ? IDC_ALLSIZERADIO : IDC_SIZERADIO);
            CheckRadioButton(hwndDlg, IDC_FILEPATTERNREGEX, IDC_FILEPATTERNTEXT, m_bUseRegexForPaths ? IDC_FILEPATTERNREGEX : IDC_FILEPATTERNTEXT);

            if (!m_searchString.empty())
                CheckRadioButton(*this, IDC_REGEXRADIO, IDC_TEXTRADIO, m_bUseRegex ? IDC_REGEXRADIO : IDC_TEXTRADIO);

            DialogEnableWindow(IDC_TESTREGEX, !IsDlgButtonChecked(*this, IDC_TEXTRADIO));
            DialogEnableWindow(IDC_ADDTOBOOKMARKS, FALSE);
            DialogEnableWindow(IDC_EXCLUDEDIRSPATTERN, !!m_bIncludeSubfolders);

            ::SetDlgItemText(*this, IDOK, TranslatedString(hResource, IDS_SEARCH).c_str());
            if (!m_showContentSet)
            {
                if (bPortable)
                {
                    m_showContent = _wtoi(g_iniFile.GetValue(L"global", L"showcontent", L"0")) != 0;
                }
                else
                {
                    m_showContent = DWORD(m_regShowContent) != 0;
                }
            }
            CheckRadioButton(*this, IDC_RESULTFILES, IDC_RESULTCONTENT, m_showContent ? IDC_RESULTCONTENT : IDC_RESULTFILES);

            CheckRadioButton(hwndDlg, IDC_RADIO_DATE_ALL, IDC_RADIO_DATE_BETWEEN, m_DateLimit + IDC_RADIO_DATE_ALL);
            SYSTEMTIME sysTime;
            auto       hTime1 = GetDlgItem(hwndDlg, IDC_DATEPICK1);
            FileTimeToSystemTime(&m_Date1, &sysTime);
            DateTime_SetSystemtime(hTime1, GDT_VALID, &sysTime);
            auto hTime2 = GetDlgItem(hwndDlg, IDC_DATEPICK2);
            FileTimeToSystemTime(&m_Date2, &sysTime);
            DateTime_SetSystemtime(hTime2, GDT_VALID, &sysTime);
            ShowWindow(GetDlgItem(*this, IDC_DATEPICK2), (m_DateLimit == IDC_RADIO_DATE_BETWEEN - IDC_RADIO_DATE_ALL) ? SW_SHOW : SW_HIDE);
            ShowWindow(GetDlgItem(*this, IDC_DATEPICK1), (m_DateLimit != 0) ? SW_SHOW : SW_HIDE);

            SetFocus(GetDlgItem(hwndDlg, IDC_SEARCHTEXT));

            m_resizer.Init(hwndDlg);
            m_resizer.UseSizeGrip(!CTheme::Instance().IsDarkTheme());
            m_resizer.AddControl(hwndDlg, IDC_HELPLABEL, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_ABOUTLINK, RESIZER_TOPRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_GROUPSEARCHIN, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_PATHMRU, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_SEARCHPATH, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_SEARCHPATHBROWSE, RESIZER_TOPRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_GROUPSEARCHFOR, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_REGEXRADIO, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_TEXTRADIO, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_SEARCHFORLABEL, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_SEARCHTEXT, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_EDITMULTILINE1, RESIZER_TOPRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_REPLACEWITHLABEL, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_REPLACETEXT, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_EDITMULTILINE2, RESIZER_TOPRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_CASE_SENSITIVE, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_DOTMATCHNEWLINE, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_REGEXOKLABEL, RESIZER_TOPRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_CREATEBACKUP, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_UTF8, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_BINARY, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_TESTREGEX, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_ADDTOBOOKMARKS, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_BOOKMARKS, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_UPDATELINK, RESIZER_TOPRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_GROUPLIMITSEARCH, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_ALLSIZERADIO, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_SIZERADIO, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_SIZECOMBO, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_SIZEEDIT, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_KBTEXT, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_RADIO_DATE_ALL, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_RADIO_DATE_NEWER, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_RADIO_DATE_OLDER, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_RADIO_DATE_BETWEEN, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_DATEPICK1, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_DATEPICK2, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_INCLUDESYSTEM, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_INCLUDEHIDDEN, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_INCLUDESUBFOLDERS, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_INCLUDEBINARY, RESIZER_TOPLEFT);

            m_resizer.AddControl(hwndDlg, IDC_PATTERNLABEL, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_PATTERN, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_PATTERNMRU, RESIZER_TOPRIGHT);

            m_resizer.AddControl(hwndDlg, IDC_EXCLUDE_DIRS_PATTERNLABEL, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_EXCLUDEDIRSPATTERN, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_EXCLUDEDIRMRU, RESIZER_TOPRIGHT);

            m_resizer.AddControl(hwndDlg, IDC_FILEPATTERNREGEX, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_FILEPATTERNTEXT, RESIZER_TOPLEFT);

            m_resizer.AddControl(hwndDlg, IDC_SETTINGSBUTTON, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_PROGRESS, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_REPLACE, RESIZER_TOPRIGHT);
            m_resizer.AddControl(hwndDlg, IDOK, RESIZER_TOPRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_GROUPSEARCHRESULTS, RESIZER_TOPLEFTBOTTOMRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_RESULTLIST, RESIZER_TOPLEFTBOTTOMRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_SEARCHINFOLABEL, RESIZER_BOTTOMLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_EXPORT, RESIZER_BOTTOMRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_RESULTFILES, RESIZER_BOTTOMRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_RESULTCONTENT, RESIZER_BOTTOMRIGHT);

            InitDialog(hwndDlg, IDI_GREPWIN);

            WINDOWPLACEMENT wpl  = {0};
            DWORD           size = sizeof(wpl);
            if (bPortable)
            {
                std::wstring sPos = g_iniFile.GetValue(L"global", L"windowpos", L"");

                if (!sPos.empty())
                {
                    auto read  = swscanf_s(sPos.c_str(), L"%d;%d;%d;%d;%d;%d;%d;%d;%d;%d",
                                          &wpl.flags, &wpl.showCmd,
                                          &wpl.ptMinPosition.x, &wpl.ptMinPosition.y,
                                          &wpl.ptMaxPosition.x, &wpl.ptMaxPosition.y,
                                          &wpl.rcNormalPosition.left, &wpl.rcNormalPosition.top,
                                          &wpl.rcNormalPosition.right, &wpl.rcNormalPosition.bottom);
                    wpl.length = sizeof(wpl);
                    if (read == 10)
                        SetWindowPlacement(*this, &wpl);
                    else
                        ShowWindow(*this, SW_SHOW);
                }
                else
                    ShowWindow(*this, SW_SHOW);
            }
            else
            {
                if (SHGetValue(HKEY_CURRENT_USER, L"Software\\grepWin", L"windowpos", REG_NONE, &wpl, &size) == ERROR_SUCCESS)
                    SetWindowPlacement(*this, &wpl);
                else
                    ShowWindow(*this, SW_SHOW);
            }
            InitResultList();

            bool doCheck = true;
            if (bPortable)
                doCheck = !!_wtoi(g_iniFile.GetValue(L"global", L"CheckForUpdates", L"1"));
            else
                doCheck = !!DWORD(CRegStdDWORD(L"Software\\grepWin\\CheckForUpdates", 1));
            if (doCheck)
            {
                m_updateCheckThread = std::move(std::thread([&]() { CheckForUpdates(); }));
                ShowUpdateAvailable();
            }

            if (hInitProtection)
                CloseHandle(hInitProtection);
            hInitProtection = nullptr;

            switch (m_ExecuteImmediately)
            {
                case Search:
                    DoCommand(IDOK, 0);
                    break;
                case Replace:
                    DoCommand(IDC_REPLACE, 0);
                    break;
                case None:
                default:
                    break;
            }
        }
            return FALSE;
        case WM_CLOSE:
        {
            if (m_updateCheckThread.joinable())
                m_updateCheckThread.join();
            if (m_dwThreadRunning)
                InterlockedExchange(&m_Cancelled, TRUE);
            else
            {
                SaveSettings();
                m_AutoCompleteFilePatterns.Save();
                m_AutoCompleteSearchPatterns.Save();
                m_AutoCompleteReplacePatterns.Save();
                m_AutoCompleteSearchPaths.Save();
                EndDialog(*this, IDCANCEL);
            }
        }
        break;
        case WM_DESTROY:
            CTheme::Instance().RemoveRegisteredCallback(m_themeCallbackId);
            break;
        case WM_COMMAND:
            return DoCommand(LOWORD(wParam), HIWORD(wParam));
        case WM_CONTEXTMENU:
        {
            if (HWND(wParam) == GetDlgItem(*this, IDC_RESULTLIST))
            {
                ShowContextMenu(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            }
        }
        break;
        case WM_NOTIFY:
        {
            switch (wParam)
            {
                case IDC_RESULTLIST:
                    DoListNotify((LPNMITEMACTIVATE)lParam);
                    break;
                case IDOK:
                    switch (((LPNMHDR)lParam)->code)
                    {
                        case BCN_DROPDOWN:
                        {
                            const NMBCDROPDOWN* pDropDown = (NMBCDROPDOWN*)lParam;
                            // Get screen coordinates of the button.
                            POINT pt;
                            pt.x = pDropDown->rcButton.left;
                            pt.y = pDropDown->rcButton.bottom;
                            ClientToScreen(pDropDown->hdr.hwndFrom, &pt);
                            // Create a menu and add items.
                            HMENU hSplitMenu = CreatePopupMenu();
                            if (!hSplitMenu)
                                break;
                            OnOutOfScope(DestroyMenu(hSplitMenu));
                            if (pDropDown->hdr.hwndFrom == GetDlgItem(*this, IDOK))
                            {
                                auto buf    = GetDlgItemText(IDC_SEARCHPATH);
                                bool bIsDir = !!PathIsDirectory(buf.get());
                                if ((!bIsDir) && wcschr(buf.get(), '|'))
                                    bIsDir = true; // assume directories in case of multiple paths

                                auto sInverseSearch      = TranslatedString(hResource, IDS_INVERSESEARCH);
                                auto sSearchInFoundFiles = TranslatedString(hResource, IDS_SEARCHINFOUNDFILES);
                                auto sCaptureSearch      = TranslatedString(hResource, IDS_CAPTURESEARCH);
                                AppendMenu(hSplitMenu, bIsDir ? MF_STRING : MF_STRING | MF_DISABLED, IDC_INVERSESEARCH, sInverseSearch.c_str());
                                AppendMenu(hSplitMenu, m_items.empty() ? MF_STRING | MF_DISABLED : MF_STRING, IDC_SEARCHINFOUNDFILES, sSearchInFoundFiles.c_str());
                                AppendMenu(hSplitMenu, GetDlgItemTextLength(IDC_REPLACETEXT) ? MF_STRING : MF_STRING | MF_DISABLED, IDC_CAPTURESEARCH, sCaptureSearch.c_str());
                            }
                            // Display the menu.
                            TrackPopupMenu(hSplitMenu, TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, *this, nullptr);

                            return TRUE;
                        }
                        break;
                    }
                    break;
                case IDC_UPDATELINK:
                    switch (((LPNMHDR)lParam)->code)
                    {
                        case NM_CLICK:
                        case NM_RETURN:
                        {
                            PNMLINK pNMLink = (PNMLINK)lParam;
                            LITEM   item    = pNMLink->item;
                            if (item.iLink == 0)
                            {
                                ShellExecute(*this, L"open", item.szUrl, nullptr, nullptr, SW_SHOW);
                            }
                            break;
                        }
                    }
                    break;
            }
        }
        break;
        case WM_SIZE:
        {
            m_resizer.DoResize(LOWORD(lParam), HIWORD(lParam));
        }
        break;
        case WM_GETMINMAXINFO:
        {
            MINMAXINFO* mmi       = (MINMAXINFO*)lParam;
            mmi->ptMinTrackSize.x = m_resizer.GetDlgRect()->right;
            mmi->ptMinTrackSize.y = m_resizer.GetDlgRect()->bottom;
            return 0;
        }
        break;
        case WM_DPICHANGED:
        {
            const RECT* rect = reinterpret_cast<RECT*>(lParam);
            SetWindowPos(*this, NULL, rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
            ::RedrawWindow(*this, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE | RDW_ERASE | RDW_INTERNALPAINT | RDW_ALLCHILDREN | RDW_UPDATENOW);
        }
        break;
        case WM_SETCURSOR:
        {
            if (m_dwThreadRunning)
            {
                SetCursor(LoadCursor(NULL, IDC_APPSTARTING));
                return TRUE;
            }
            return FALSE;
        }
        break;
        case SEARCH_START:
        {
            m_totalitems    = 0;
            m_searchedItems = 0;
            m_totalmatches  = 0;
            UpdateInfoLabel();
            SetTimer(*this, LABELUPDATETIMER, 200, NULL);
        }
        break;
        case SEARCH_FOUND:
            m_totalmatches += (int)((CSearchInfo*)lParam)->matchcount;
            if ((wParam != 0) || (m_searchString.empty()) || ((CSearchInfo*)lParam)->readerror || m_bNOTSearch)
            {
                AddFoundEntry((CSearchInfo*)lParam);
            }
            break;
        case SEARCH_PROGRESS:
        {
            if (wParam)
                m_searchedItems++;
            m_totalitems++;
        }
        break;
        case SEARCH_END:
        {
            AddFoundEntry(NULL, true);
            AutoSizeAllColumns();
            UpdateInfoLabel();
            ::SetDlgItemText(*this, IDOK, TranslatedString(hResource, IDS_SEARCH).c_str());
            DialogEnableWindow(IDC_RESULTFILES, true);
            DialogEnableWindow(IDC_RESULTCONTENT, true);
            ShowWindow(GetDlgItem(*this, IDC_PROGRESS), SW_HIDE);
            SendDlgItemMessage(*this, IDC_PROGRESS, PBM_SETMARQUEE, 0, 0);
            ShowWindow(GetDlgItem(*this, IDC_EXPORT), m_items.empty() ? SW_HIDE : SW_SHOW);
            KillTimer(*this, LABELUPDATETIMER);
        }
        break;
        case WM_TIMER:
        {
            if (wParam == LABELUPDATETIMER)
            {
                AddFoundEntry(NULL, true);
                UpdateInfoLabel();
            }
        }
        break;
        case WM_HELP:
        {
            CInfoDlg::ShowDialog(IDR_INFODLG, hResource);
        }
        break;
        case WM_SYSCOMMAND:
        {
            if ((wParam & 0xFFF0) == ID_ABOUTBOX)
            {
                CAboutDlg dlgAbout(*this);
                dlgAbout.DoModal(hResource, IDD_ABOUT, *this);
            }
        }
        break;
        case WM_COPYDATA:
        {
            if (lParam)
            {
                PCOPYDATASTRUCT pCopyData = (PCOPYDATASTRUCT)lParam;
                std::wstring    newpath   = std::wstring((LPCTSTR)pCopyData->lpData, pCopyData->cbData / sizeof(wchar_t));
                if (!newpath.empty())
                {
                    auto buf     = GetDlgItemText(IDC_SEARCHPATH);
                    m_searchpath = buf.get();

                    if (wParam == 1)
                        m_searchpath.clear();
                    else
                        m_searchpath += L"|";
                    m_searchpath += newpath;
                    SetDlgItemText(hwndDlg, IDC_SEARCHPATH, m_searchpath.c_str());
                    g_startTime = GetTickCount64();
                }
            }
            return TRUE;
        }
        break;
        case WM_EDITDBLCLICK:
        {
            switch (wParam)
            {
                case IDC_PATTERN:
                {
                    m_AutoCompleteFilePatterns.SetOptions(ACO_UPDOWNKEYDROPSLIST | ACO_AUTOSUGGEST | ACO_NOPREFIXFILTERING);
                    ::SetFocus(GetDlgItem(*this, IDC_PATTERN));
                    SendDlgItemMessage(*this, IDC_PATTERN, WM_KEYDOWN, VK_DOWN, 0);
                }
                break;
                case IDC_EXCLUDEDIRSPATTERN:
                {
                    m_AutoCompleteExcludeDirsPatterns.SetOptions(ACO_UPDOWNKEYDROPSLIST | ACO_AUTOSUGGEST | ACO_NOPREFIXFILTERING);
                    ::SetFocus(GetDlgItem(*this, IDC_EXCLUDEDIRSPATTERN));
                    SendDlgItemMessage(*this, IDC_EXCLUDEDIRSPATTERN, WM_KEYDOWN, VK_DOWN, 0);
                }
                break;
                case IDC_SEARCHTEXT:
                {
                    m_AutoCompleteSearchPatterns.SetOptions(ACO_UPDOWNKEYDROPSLIST | ACO_AUTOSUGGEST | ACO_NOPREFIXFILTERING);
                    ::SetFocus(GetDlgItem(*this, IDC_SEARCHTEXT));
                    SendDlgItemMessage(*this, IDC_SEARCHTEXT, WM_KEYDOWN, VK_DOWN, 0);
                }
                break;
                case IDC_REPLACETEXT:
                {
                    m_AutoCompleteReplacePatterns.SetOptions(ACO_UPDOWNKEYDROPSLIST | ACO_AUTOSUGGEST | ACO_NOPREFIXFILTERING);
                    ::SetFocus(GetDlgItem(*this, IDC_REPLACETEXT));
                    SendDlgItemMessage(*this, IDC_REPLACETEXT, WM_KEYDOWN, VK_DOWN, 0);
                }
                break;
                case IDC_SEARCHPATH:
                {
                    m_AutoCompleteSearchPaths.SetOptions(ACO_UPDOWNKEYDROPSLIST | ACO_AUTOSUGGEST | ACO_NOPREFIXFILTERING);
                    ::SetFocus(GetDlgItem(*this, IDC_SEARCHPATH));
                    SendDlgItemMessage(*this, IDC_SEARCHPATH, WM_KEYDOWN, VK_DOWN, 0);
                }
                break;
            }
            return TRUE;
        }
        break;
        case WM_GREPWIN_THREADEND:
        {
            if (m_endDialog)
                EndDialog(m_hwnd, IDOK);
        }
        break;
        case WM_BOOKMARK:
        {
            if (m_pBookmarksDlg)
            {
                m_searchString  = m_pBookmarksDlg->GetSelectedSearchString();
                m_replaceString = m_pBookmarksDlg->GetSelectedReplaceString();
                m_bUseRegex     = m_pBookmarksDlg->GetSelectedUseRegex();

                m_bCaseSensitive          = m_pBookmarksDlg->GetSelectedSearchCase();
                m_bDotMatchesNewline      = m_pBookmarksDlg->GetSelectedDotMatchNewline();
                m_bCreateBackup           = m_pBookmarksDlg->GetSelectedBackup();
                m_bUTF8                   = m_pBookmarksDlg->GetSelectedTreatAsUtf8();
                m_bForceBinary            = m_pBookmarksDlg->GetSelectedTreatAsBinary();
                m_bIncludeSystem          = m_pBookmarksDlg->GetSelectedIncludeSystem();
                m_bIncludeSubfolders      = m_pBookmarksDlg->GetSelectedIncludeFolder();
                m_bIncludeHidden          = m_pBookmarksDlg->GetSelectedIncludeHidden();
                m_bIncludeBinary          = m_pBookmarksDlg->GetSelectedIncludeBinary();
                m_excludedirspatternregex = m_pBookmarksDlg->GetSelectedExcludeDirs();
                m_patternregex            = m_pBookmarksDlg->GetSelectedFileMatch();
                m_bUseRegexForPaths       = m_pBookmarksDlg->GetSelectedFileMatchRegex();
                if (!m_pBookmarksDlg->GetPath().empty())
                {
                    m_searchpath = m_pBookmarksDlg->GetPath();
                    SetDlgItemText(*this, IDC_SEARCHPATH, m_searchpath.c_str());
                }

                SetDlgItemText(*this, IDC_SEARCHTEXT, m_searchString.c_str());
                SetDlgItemText(*this, IDC_REPLACETEXT, m_replaceString.c_str());
                CheckRadioButton(*this, IDC_REGEXRADIO, IDC_TEXTRADIO, m_bUseRegex ? IDC_REGEXRADIO : IDC_TEXTRADIO);
                DialogEnableWindow(IDC_TESTREGEX, !IsDlgButtonChecked(*this, IDC_TEXTRADIO));

                SendDlgItemMessage(*this, IDC_INCLUDESUBFOLDERS, BM_SETCHECK, m_bIncludeSubfolders ? BST_CHECKED : BST_UNCHECKED, 0);
                SendDlgItemMessage(*this, IDC_CREATEBACKUP, BM_SETCHECK, m_bCreateBackup ? BST_CHECKED : BST_UNCHECKED, 0);
                SendDlgItemMessage(*this, IDC_UTF8, BM_SETCHECK, m_bUTF8 ? BST_CHECKED : BST_UNCHECKED, 0);
                SendDlgItemMessage(*this, IDC_BINARY, BM_SETCHECK, m_bForceBinary ? BST_CHECKED : BST_UNCHECKED, 0);
                SendDlgItemMessage(*this, IDC_INCLUDESYSTEM, BM_SETCHECK, m_bIncludeSystem ? BST_CHECKED : BST_UNCHECKED, 0);
                SendDlgItemMessage(*this, IDC_INCLUDEHIDDEN, BM_SETCHECK, m_bIncludeHidden ? BST_CHECKED : BST_UNCHECKED, 0);
                SendDlgItemMessage(*this, IDC_INCLUDEBINARY, BM_SETCHECK, m_bIncludeBinary ? BST_CHECKED : BST_UNCHECKED, 0);
                SendDlgItemMessage(*this, IDC_CASE_SENSITIVE, BM_SETCHECK, m_bCaseSensitive ? BST_CHECKED : BST_UNCHECKED, 0);
                SendDlgItemMessage(*this, IDC_DOTMATCHNEWLINE, BM_SETCHECK, m_bDotMatchesNewline ? BST_CHECKED : BST_UNCHECKED, 0);

                CheckRadioButton(*this, IDC_FILEPATTERNREGEX, IDC_FILEPATTERNTEXT, m_bUseRegexForPaths ? IDC_FILEPATTERNREGEX : IDC_FILEPATTERNTEXT);
                SetDlgItemText(*this, IDC_EXCLUDEDIRSPATTERN, m_excludedirspatternregex.c_str());
                SetDlgItemText(*this, IDC_PATTERN, m_patternregex.c_str());
            }
        }
        break;
        default:
            return FALSE;
    }
    return FALSE;
}

LRESULT CSearchDlg::DoCommand(int id, int msg)
{
    switch (id)
    {
        case IDC_REPLACE:
        case IDOK:
        case IDC_INVERSESEARCH:
        case IDC_SEARCHINFOUNDFILES:
        case IDC_CAPTURESEARCH:
        {
            if (m_dwThreadRunning)
            {
                InterlockedExchange(&m_Cancelled, TRUE);
            }
            else
            {
                ::SetFocus(GetDlgItem(*this, IDOK));
                if (!SaveSettings())
                    break;

                CStringUtils::rtrim(m_searchpath, L"\\/");

                if (PathIsRelative(m_searchpath.c_str()))
                {
                    ShowEditBalloon(IDC_SEARCHPATH, TranslatedString(hResource, IDS_ERR_INVALID_PATH).c_str(), TranslatedString(hResource, IDS_ERR_RELATIVEPATH).c_str());
                    break;
                }
                std::vector<std::wstring> searchpaths;
                stringtok(searchpaths, m_searchpath, true);
                for (const auto& sp : searchpaths)
                {
                    if (!PathFileExists(sp.c_str()))
                    {
                        ShowEditBalloon(IDC_SEARCHPATH, TranslatedString(hResource, IDS_ERR_INVALID_PATH).c_str(), TranslatedString(hResource, IDS_ERR_PATHNOTEXIST).c_str());
                        break;
                    }
                }

                if ((id == IDC_SEARCHINFOUNDFILES) && (!m_items.empty()))
                {
                    m_searchpath.clear();
                    for (const auto& item : m_items)
                    {
                        if (!m_searchpath.empty())
                            m_searchpath += L"|";
                        m_searchpath += item.filepath;
                    }
                }

                m_searchedItems = 0;
                m_totalitems    = 0;

                ShowWindow(GetDlgItem(*this, IDC_EXPORT), SW_HIDE);
                m_items.clear();
                m_listItems.clear();
                m_listItems.reserve(500000);
                m_backupandtempfiles.clear();
                if (m_searchString.empty())
                {
                    // switch to file view
                    CheckRadioButton(*this, IDC_RESULTFILES, IDC_RESULTCONTENT, IDC_RESULTFILES);
                }

                HWND hListControl = GetDlgItem(*this, IDC_RESULTLIST);
                ListView_SetItemCount(hListControl, 0);
                DialogEnableWindow(IDC_RESULTFILES, false);
                DialogEnableWindow(IDC_RESULTCONTENT, false);

                m_AutoCompleteFilePatterns.AddEntry(m_patternregex.c_str());
                m_AutoCompleteExcludeDirsPatterns.AddEntry(m_excludedirspatternregex.c_str());
                m_AutoCompleteSearchPatterns.AddEntry(m_searchString.c_str());
                m_AutoCompleteReplacePatterns.AddEntry(m_replaceString.c_str());
                m_AutoCompleteSearchPaths.AddEntry(m_searchpath.c_str());

                m_AutoCompleteFilePatterns.Save();
                m_AutoCompleteSearchPatterns.Save();
                m_AutoCompleteReplacePatterns.Save();
                m_AutoCompleteSearchPaths.Save();

                m_bReplace = id == IDC_REPLACE;

                if (m_bReplace && (!m_bCreateBackup || m_replaceString.empty()) && m_bConfirmationOnReplace)
                {
                    auto nowarnifnobackup = bPortable ? !!_wtoi(g_iniFile.GetValue(L"settings", L"nowarnifnobackup", L"0")) : DWORD(CRegStdDWORD(L"Software\\grepWin\\nowarnifnobackup", FALSE));
                    if (!nowarnifnobackup)
                    {
                        auto msgtext = CStringUtils::Format((LPCWSTR)TranslatedString(hResource, IDS_REPLACECONFIRM).c_str(),
                                                            m_searchString.c_str(),
                                                            m_replaceString.empty() ? (LPCWSTR)TranslatedString(hResource, IDS_ANEMPTYSTRING).c_str() : m_replaceString.c_str());
                        if (::MessageBox(*this, msgtext.c_str(), L"grepWin", MB_ICONQUESTION | MB_YESNO) != IDYES)
                        {
                            break;
                        }
                    }
                }
                m_bConfirmationOnReplace = true;
                m_bNOTSearch             = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                if (id == IDC_INVERSESEARCH)
                    m_bNOTSearch = true;
                if (id == IDC_CAPTURESEARCH)
                {
                    m_bCaptureSearch = true;
                    m_bNOTSearch     = false;
                    m_bReplace       = false;
                }

                InterlockedExchange(&m_dwThreadRunning, TRUE);
                InterlockedExchange(&m_Cancelled, FALSE);
                SetDlgItemText(*this, IDOK, TranslatedString(hResource, IDS_STOP).c_str());
                ShowWindow(GetDlgItem(*this, IDC_PROGRESS), SW_SHOW);
                SendDlgItemMessage(*this, IDC_PROGRESS, PBM_SETMARQUEE, 1, 0);
                // now start the thread which does the searching
                DWORD  dwThreadId = 0;
                HANDLE hThread    = CreateThread(
                    NULL, // no security attribute
                    0,    // default stack size
                    SearchThreadEntry,
                    (LPVOID)this, // thread parameter
                    0,            // not suspended
                    &dwThreadId); // returns thread ID
                if (hThread != nullptr)
                {
                    // Closing the handle of a running thread just decreases
                    // the ref count for the thread object.
                    CloseHandle(hThread);
                }
                else
                {
                    SendMessage(*this, SEARCH_END, 0, 0);
                }
            }
        }
        break;
        case IDCANCEL:
        {
            if (m_updateCheckThread.joinable())
                m_updateCheckThread.join();
            bool escClose = !!DWORD(CRegStdDWORD(L"Software\\grepWin\\escclose", FALSE));
            if (bPortable)
                escClose = !!_wtoi(g_iniFile.GetValue(L"settings", L"escclose", L"0"));
            if (escClose)
            {
                if (m_dwThreadRunning)
                    InterlockedExchange(&m_Cancelled, TRUE);
                else
                {
                    SaveSettings();
                    m_AutoCompleteFilePatterns.Save();
                    m_AutoCompleteSearchPatterns.Save();
                    m_AutoCompleteReplacePatterns.Save();
                    m_AutoCompleteSearchPaths.Save();
                    EndDialog(*this, IDCANCEL);
                }
            }
        }
        break;
        case IDC_RADIO_DATE_ALL:
        case IDC_RADIO_DATE_NEWER:
        case IDC_RADIO_DATE_OLDER:
        case IDC_RADIO_DATE_BETWEEN:
        {
            auto isBetween = IsDlgButtonChecked(*this, IDC_RADIO_DATE_BETWEEN) == BST_CHECKED;
            ShowWindow(GetDlgItem(*this, IDC_DATEPICK2), isBetween ? SW_SHOW : SW_HIDE);
            ShowWindow(GetDlgItem(*this, IDC_DATEPICK1), IsDlgButtonChecked(*this, IDC_RADIO_DATE_ALL) ? SW_HIDE : SW_SHOW);
        }
        break;
        case IDC_TESTREGEX:
        {
            // get all the information we need from the dialog
            auto buf        = GetDlgItemText(IDC_SEARCHTEXT);
            m_searchString  = buf.get();
            buf             = GetDlgItemText(IDC_REPLACETEXT);
            m_replaceString = buf.get();

            SaveSettings();
            CRegexTestDlg dlg(*this);
            dlg.bCaseSensitive     = m_bCaseSensitive;
            dlg.bDotMatchesNewline = m_bDotMatchesNewline;
            dlg.SetStrings(m_searchString, m_replaceString);
            if (dlg.DoModal(hResource, IDD_REGEXTEST, *this) == IDOK)
            {
                m_searchString  = dlg.GetSearchString();
                m_replaceString = dlg.GetReplaceString();
                SetDlgItemText(*this, IDC_SEARCHTEXT, m_searchString.c_str());
                SetDlgItemText(*this, IDC_REPLACETEXT, m_replaceString.c_str());
            }
        }
        break;
        case IDC_SEARCHPATHBROWSE:
        {
            CBrowseFolder browse;

            auto path = GetDlgItemText(IDC_SEARCHPATH);
            if (!PathFileExists(path.get()))
            {
                auto ptr = wcsstr(path.get(), L"|");
                if (ptr)
                    *ptr = 0;
                else
                    path.get()[0] = 0;
            }
            if (wcsstr(path.get(), L"..") != NULL)
            {
                ShowEditBalloon(IDC_SEARCHPATH, TranslatedString(hResource, IDS_ERR_INVALID_PATH).c_str(), TranslatedString(hResource, IDS_ERR_RELATIVEPATH).c_str());
                break;
            }

            auto pathbuf = std::make_unique<wchar_t[]>(MAX_PATH_NEW);
            wcscpy_s(pathbuf.get(), MAX_PATH_NEW, path.get());
            browse.SetInfo(TranslatedString(hResource, IDS_SELECTPATHTOSEARCH).c_str());
            if (browse.Show(*this, pathbuf.get(), MAX_PATH_NEW, m_searchpath.c_str()) == CBrowseFolder::OK)
            {
                SetDlgItemText(*this, IDC_SEARCHPATH, pathbuf.get());
                m_searchpath = pathbuf.get();
            }
        }
        break;
        case IDC_SEARCHPATH:
        {
            if (msg == EN_CHANGE)
            {
                if (m_AutoCompleteSearchPaths.GetOptions() & ACO_NOPREFIXFILTERING)
                    m_AutoCompleteSearchPaths.SetOptions(ACO_UPDOWNKEYDROPSLIST | ACO_AUTOSUGGEST);
                int  len    = GetDlgItemTextLength(IDC_SEARCHTEXT);
                auto buf    = GetDlgItemText(IDC_SEARCHPATH);
                bool bIsDir = !!PathIsDirectory(buf.get());
                if ((!bIsDir) && wcschr(buf.get(), '|'))
                    bIsDir = true; // assume directories in case of multiple paths
                bool bIncludeSubfolders = (IsDlgButtonChecked(*this, IDC_INCLUDESUBFOLDERS) == BST_CHECKED);
                DialogEnableWindow(IDC_ALLSIZERADIO, bIsDir);
                DialogEnableWindow(IDC_SIZERADIO, bIsDir);
                DialogEnableWindow(IDC_SIZECOMBO, bIsDir);
                DialogEnableWindow(IDC_SIZEEDIT, bIsDir);
                DialogEnableWindow(IDC_INCLUDESYSTEM, bIsDir);
                DialogEnableWindow(IDC_INCLUDEHIDDEN, bIsDir);
                DialogEnableWindow(IDC_INCLUDESUBFOLDERS, bIsDir);
                DialogEnableWindow(IDC_INCLUDEBINARY, bIsDir && len > 0);
                DialogEnableWindow(IDC_PATTERN, bIsDir);
                DialogEnableWindow(IDC_EXCLUDEDIRSPATTERN, bIsDir || bIncludeSubfolders);
                DialogEnableWindow(IDC_FILEPATTERNREGEX, bIsDir);
                DialogEnableWindow(IDC_FILEPATTERNTEXT, bIsDir);

                // change the dialog title to "grepWin : search/path"
                wchar_t compactPath[100] = {0};
                PathCompactPathEx(compactPath, buf.get(), 40, 0);
                wchar_t titleBuf[MAX_PATH] = {0};
                _stprintf_s(titleBuf, _countof(titleBuf), L"grepWin : %s", compactPath);
                SetWindowText(*this, titleBuf);
            }
        }
        break;
        case IDC_INCLUDESUBFOLDERS:
        {
            if (msg == BN_CLICKED)
            {
                auto buf                = GetDlgItemText(IDC_SEARCHPATH);
                bool bIncludeSubfolders = (IsDlgButtonChecked(*this, IDC_INCLUDESUBFOLDERS) == BST_CHECKED);
                bool bIsDir             = !!PathIsDirectory(buf.get());
                if ((!bIsDir) && wcschr(buf.get(), '|'))
                    bIsDir = true; // assume directories in case of multiple paths
                DialogEnableWindow(IDC_EXCLUDEDIRSPATTERN, bIsDir || bIncludeSubfolders);
            }
        }
        break;
        case IDC_SEARCHTEXT:
        {
            if (msg == EN_CHANGE)
            {
                if (m_AutoCompleteSearchPatterns.GetOptions() & ACO_NOPREFIXFILTERING)
                    m_AutoCompleteSearchPatterns.SetOptions(ACO_UPDOWNKEYDROPSLIST | ACO_AUTOSUGGEST);
                int len = CheckRegex();
                DialogEnableWindow(IDC_ADDTOBOOKMARKS, len > 0);
                DialogEnableWindow(IDC_INCLUDEBINARY, len > 0);
            }
        }
        break;
        case IDC_SIZEEDIT:
        {
            if (msg == EN_CHANGE)
            {
                wchar_t buf[20] = {0};
                ::GetDlgItemText(*this, IDC_SIZEEDIT, buf, _countof(buf));
                if (wcslen(buf))
                {
                    if (IsDlgButtonChecked(*this, IDC_ALLSIZERADIO) == BST_CHECKED)
                    {
                        CheckRadioButton(*this, IDC_ALLSIZERADIO, IDC_SIZERADIO, IDC_SIZERADIO);
                    }
                }
                else
                {
                    if (IsDlgButtonChecked(*this, IDC_SIZERADIO) == BST_CHECKED)
                    {
                        CheckRadioButton(*this, IDC_ALLSIZERADIO, IDC_SIZERADIO, IDC_ALLSIZERADIO);
                    }
                }
            }
        }
        break;
        case IDC_REGEXRADIO:
        case IDC_TEXTRADIO:
        {
            CheckRegex();
            DialogEnableWindow(IDC_TESTREGEX, !IsDlgButtonChecked(*this, IDC_TEXTRADIO));
        }
        break;
        case IDC_ADDTOBOOKMARKS:
        {
            auto buf                  = GetDlgItemText(IDC_SEARCHTEXT);
            m_searchString            = buf.get();
            buf                       = GetDlgItemText(IDC_REPLACETEXT);
            m_replaceString           = buf.get();
            buf                       = GetDlgItemText(IDC_EXCLUDEDIRSPATTERN);
            m_excludedirspatternregex = buf.get();
            buf                       = GetDlgItemText(IDC_PATTERN);
            m_patternregex            = buf.get();
            bool bUseRegex            = (IsDlgButtonChecked(*this, IDC_REGEXRADIO) == BST_CHECKED);

            CNameDlg nameDlg(*this);
            if (nameDlg.DoModal(hResource, IDD_NAME, *this) == IDOK)
            {
                // add the bookmark
                CBookmarks bks;
                Bookmark   bk;
                bk.Name              = nameDlg.GetName();
                bk.Path              = nameDlg.IncludePath() ? m_searchpath : L"";
                bk.Search            = m_searchString;
                bk.Replace           = m_replaceString;
                bk.UseRegex          = bUseRegex;
                bk.CaseSensitive     = (IsDlgButtonChecked(*this, IDC_CASE_SENSITIVE) == BST_CHECKED);
                bk.DotMatchesNewline = (IsDlgButtonChecked(*this, IDC_DOTMATCHNEWLINE) == BST_CHECKED);
                bk.Backup            = (IsDlgButtonChecked(*this, IDC_CREATEBACKUP) == BST_CHECKED);
                bk.Utf8              = (IsDlgButtonChecked(*this, IDC_UTF8) == BST_CHECKED);
                bk.IncludeSystem     = (IsDlgButtonChecked(*this, IDC_INCLUDESYSTEM) == BST_CHECKED);
                bk.IncludeFolder     = (IsDlgButtonChecked(*this, IDC_INCLUDESUBFOLDERS) == BST_CHECKED);
                bk.IncludeHidden     = (IsDlgButtonChecked(*this, IDC_INCLUDEHIDDEN) == BST_CHECKED);
                bk.IncludeBinary     = (IsDlgButtonChecked(*this, IDC_INCLUDEBINARY) == BST_CHECKED);
                bk.ExcludeDirs       = m_excludedirspatternregex;
                bk.FileMatch         = m_patternregex;
                bk.FileMatchRegex    = (IsDlgButtonChecked(*this, IDC_FILEPATTERNREGEX) == BST_CHECKED);
                bks.Load();
                bks.AddBookmark(bk);
                bks.Save();
            }
        }
        break;
        case IDC_BOOKMARKS:
        {
            if (m_pBookmarksDlg == nullptr)
                m_pBookmarksDlg = new CBookmarksDlg(*this);
            else
                m_pBookmarksDlg->InitBookmarks();
            m_pBookmarksDlg->ShowModeless(hResource, IDD_BOOKMARKS, *this);
        }
        break;
        case IDC_RESULTFILES:
        case IDC_RESULTCONTENT:
        {
            InitResultList();
            FillResultList();
        }
        break;
        case IDC_ABOUTLINK:
        {
            CAboutDlg dlgAbout(*this);
            dlgAbout.DoModal(hResource, IDD_ABOUT, *this);
        }
        break;
        case IDC_SETTINGSBUTTON:
        {
            CSettingsDlg dlgSettings(*this);
            dlgSettings.DoModal(hResource, IDD_SETTINGS, *this);
            m_regBackupInFolder.read();
        }
        break;
        case IDC_EDITMULTILINE1:
        case IDC_EDITMULTILINE2:
        {
            int          uID      = (id == IDC_EDITMULTILINE1 ? IDC_SEARCHTEXT : IDC_REPLACETEXT);
            auto         buf      = GetDlgItemText((int)uID);
            std::wstring ctrlText = buf.get();

            // replace all \r\n strings with real CRLFs
            try
            {
                int                                                ft         = boost::regex::normal;
                boost::wregex                                      expression = boost::wregex(L"\\\\r\\\\n", ft);
                boost::match_results<std::wstring::const_iterator> whatc;
                boost::match_flag_type                             rflags = boost::match_default | boost::format_all;
                ctrlText                                                  = regex_replace(ctrlText, expression, L"\\r\\n", rflags);
            }
            catch (const std::exception&)
            {
            }
            CMultiLineEditDlg editDlg(*this);
            editDlg.SetString(ctrlText);

            if (editDlg.DoModal(hResource, IDD_MULTILINEEDIT, *this) == IDOK)
            {
                std::wstring text = editDlg.GetSearchString();
                // replace all CRLFs with \r\n strings (literal)
                try
                {
                    int                                                ft         = boost::regex::normal;
                    boost::wregex                                      expression = boost::wregex(L"\\r\\n", ft);
                    boost::match_results<std::wstring::const_iterator> whatc;
                    boost::match_flag_type                             rflags = boost::match_default | boost::format_all;
                    text                                                      = regex_replace(text, expression, L"\\\\r\\\\n", rflags);
                }
                catch (const std::exception&)
                {
                }

                SetDlgItemText(*this, (int)uID, text.c_str());
            }
            ::SetFocus(GetDlgItem(*this, uID));
        }
        break;
        case IDC_PATHMRU:
        {
            m_AutoCompleteSearchPaths.SetOptions(ACO_UPDOWNKEYDROPSLIST | ACO_AUTOSUGGEST | ACO_NOPREFIXFILTERING);
            ::SetFocus(GetDlgItem(*this, IDC_SEARCHPATH));
            SendDlgItemMessage(*this, IDC_SEARCHPATH, WM_KEYDOWN, VK_DOWN, 0);
        }
        break;
        case IDC_EXCLUDEDIRMRU:
        {
            m_AutoCompleteExcludeDirsPatterns.SetOptions(ACO_UPDOWNKEYDROPSLIST | ACO_AUTOSUGGEST | ACO_NOPREFIXFILTERING);
            ::SetFocus(GetDlgItem(*this, IDC_EXCLUDEDIRSPATTERN));
            SendDlgItemMessage(*this, IDC_EXCLUDEDIRSPATTERN, WM_KEYDOWN, VK_DOWN, 0);
        }
        break;
        case IDC_PATTERNMRU:
        {
            m_AutoCompleteFilePatterns.SetOptions(ACO_UPDOWNKEYDROPSLIST | ACO_AUTOSUGGEST | ACO_NOPREFIXFILTERING);
            ::SetFocus(GetDlgItem(*this, IDC_PATTERN));
            SendDlgItemMessage(*this, IDC_PATTERN, WM_KEYDOWN, VK_DOWN, 0);
        }
        break;
        case IDC_PATTERN:
        {
            if (msg == EN_CHANGE)
            {
                if (m_AutoCompleteFilePatterns.GetOptions() & ACO_NOPREFIXFILTERING)
                    m_AutoCompleteFilePatterns.SetOptions(ACO_UPDOWNKEYDROPSLIST | ACO_AUTOSUGGEST);
            }
        }
        break;
        case IDC_EXCLUDEDIRSPATTERN:
        {
            if (msg == EN_CHANGE)
            {
                if (m_AutoCompleteExcludeDirsPatterns.GetOptions() & ACO_NOPREFIXFILTERING)
                    m_AutoCompleteExcludeDirsPatterns.SetOptions(ACO_UPDOWNKEYDROPSLIST | ACO_AUTOSUGGEST);
            }
        }
        break;
        case IDC_REPLACETEXT:
        {
            if (msg == EN_CHANGE)
            {
                if (m_AutoCompleteReplacePatterns.GetOptions() & ACO_NOPREFIXFILTERING)
                    m_AutoCompleteReplacePatterns.SetOptions(ACO_UPDOWNKEYDROPSLIST | ACO_AUTOSUGGEST);
            }
        }
        break;
        case IDC_EXPORT:
        {
            PreserveChdir      keepCWD;
            IFileSaveDialogPtr pfd;

            HRESULT hr = pfd.CreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER);
            if (FailedShowMessage(hr))
                break;

            // Set the dialog options
            DWORD dwOptions;
            hr = pfd->GetOptions(&dwOptions);
            if (FailedShowMessage(hr))
                break;
            hr = pfd->SetOptions(dwOptions | FOS_FORCEFILESYSTEM | FOS_OVERWRITEPROMPT);
            if (FailedShowMessage(hr))
                break;

            hr = pfd->SetTitle(TranslatedString(hResource, IDS_EXPORTTITLE).c_str());
            if (FailedShowMessage(hr))
                break;

            IFileDialogCustomizePtr pfdCustomize;
            hr = pfd.QueryInterface(IID_PPV_ARGS(&pfdCustomize));
            if (SUCCEEDED(hr))
            {
                auto exportpaths       = DWORD(CRegStdDWORD(L"Software\\grepWin\\export_paths")) != 0;
                auto exportlinenumbers = DWORD(CRegStdDWORD(L"Software\\grepWin\\export_linenumbers")) != 0;
                auto exportlinecontent = DWORD(CRegStdDWORD(L"Software\\grepWin\\export_linecontent")) != 0;
                if (bPortable)
                {
                    exportpaths       = _wtoi(g_iniFile.GetValue(L"export", L"paths", L"")) != 0;
                    exportlinenumbers = _wtoi(g_iniFile.GetValue(L"export", L"linenumbers", L"")) != 0;
                    exportlinecontent = _wtoi(g_iniFile.GetValue(L"export", L"linecontent", L"")) != 0;
                }

                if (!exportpaths && !exportlinenumbers && !exportlinecontent)
                    exportpaths = true;

                pfdCustomize->AddCheckButton(101, TranslatedString(hResource, IDS_EXPORTPATHS).c_str(), exportpaths);
                pfdCustomize->AddCheckButton(102, TranslatedString(hResource, IDS_EXPORTMATCHLINENUMBER).c_str(), exportlinenumbers);
                pfdCustomize->AddCheckButton(103, TranslatedString(hResource, IDS_EXPORTMATCHLINECONTENT).c_str(), exportlinecontent);
            }

            // Show the save file dialog
            hr = pfd->Show(*this);
            if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED))
                break;
            if (FailedShowMessage(hr))
                break;
            IShellItemPtr psiResult = nullptr;
            hr                      = pfd->GetResult(&psiResult);
            if (FailedShowMessage(hr))
                break;
            PWSTR pszPath = nullptr;
            hr            = psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
            if (FailedShowMessage(hr))
                break;
            std::wstring path = pszPath;
            CoTaskMemFree(pszPath);

            bool                    includePaths            = true;
            bool                    includeMatchLineNumbers = false;
            bool                    includeMatchLineTexts   = false;
            IFileDialogCustomizePtr pfdCustomizeRet;
            hr = pfd.QueryInterface(IID_PPV_ARGS(&pfdCustomizeRet));
            if (SUCCEEDED(hr))
            {
                BOOL bChecked = FALSE;
                pfdCustomizeRet->GetCheckButtonState(101, &bChecked);
                includePaths = (bChecked != 0);
                pfdCustomizeRet->GetCheckButtonState(102, &bChecked);
                includeMatchLineNumbers = (bChecked != 0);
                pfdCustomizeRet->GetCheckButtonState(103, &bChecked);
                includeMatchLineTexts = (bChecked != 0);
            }
            if (!includePaths && !includeMatchLineNumbers && !includeMatchLineTexts)
                includePaths = true;

            bool onlypaths = !includeMatchLineNumbers && !includeMatchLineTexts;
            if (!path.empty())
            {
                std::ofstream file;
                file.open(path);

                if (file.is_open())
                {
                    if (onlypaths)
                    {
                        for (const auto& item : m_items)
                        {
                            file << CUnicodeUtils::StdGetUTF8(item.filepath) << std::endl;
                        }
                    }
                    else
                    {
                        constexpr char separator = '*';
                        for (const auto& item : m_items)
                        {
                            for (size_t i = 0; i < item.matchlinesnumbers.size(); ++i)
                            {
                                bool needSeparator = false;
                                if (includePaths)
                                {
                                    file << CUnicodeUtils::StdGetUTF8(item.filepath);
                                    needSeparator = true;
                                }
                                if (includeMatchLineNumbers)
                                {
                                    if (needSeparator)
                                        file << separator;
                                    file << CStringUtils::Format("%lld", item.matchlinesnumbers[i]);
                                    needSeparator = true;
                                }
                                if (includeMatchLineTexts)
                                {
                                    if (needSeparator)
                                        file << separator;
                                    auto line = item.matchlines[i];
                                    CStringUtils::rtrim(line, L"\r\n");
                                    file << CUnicodeUtils::StdGetUTF8(line);
                                }
                                file << std::endl;
                            }
                        }
                    }

                    file.close();

                    if (bPortable)
                    {
                        g_iniFile.SetValue(L"export", L"paths", includePaths ? L"1" : L"0");
                        g_iniFile.SetValue(L"export", L"linenumbers", includeMatchLineNumbers ? L"1" : L"0");
                        g_iniFile.SetValue(L"export", L"linecontent", includeMatchLineTexts ? L"1" : L"0");
                    }
                    else
                    {
                        auto exportpaths       = CRegStdDWORD(L"Software\\grepWin\\export_paths");
                        auto exportlinenumbers = CRegStdDWORD(L"Software\\grepWin\\export_linenumbers");
                        auto exportlinecontent = CRegStdDWORD(L"Software\\grepWin\\export_linecontent");

                        exportpaths       = includePaths ? 1 : 0;
                        exportlinenumbers = includeMatchLineNumbers ? 1 : 0;
                        exportlinecontent = includeMatchLineTexts ? 1 : 0;
                    }
                    SHELLEXECUTEINFO sei = {0};
                    sei.cbSize           = sizeof(SHELLEXECUTEINFO);
                    sei.lpVerb           = TEXT("open");
                    sei.lpFile           = path.c_str();
                    sei.nShow            = SW_SHOWNORMAL;
                    ShellExecuteEx(&sei);
                }
            }
        }
        break;
        case IDC_UTF8:
        {
            if (IsDlgButtonChecked(*this, IDC_UTF8))
                CheckDlgButton(*this, IDC_BINARY, BST_UNCHECKED);
        }
        break;
        case IDC_BINARY:
        {
            if (IsDlgButtonChecked(*this, IDC_BINARY))
                CheckDlgButton(*this, IDC_UTF8, BST_UNCHECKED);
        }
        break;
    }
    return 1;
}

void CSearchDlg::SaveWndPosition()
{
    WINDOWPLACEMENT wpl = {0};
    wpl.length          = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(*this, &wpl);
    if (bPortable)
    {
        auto sPos = CStringUtils::Format(L"%d;%d;%d;%d;%d;%d;%d;%d;%d;%d",
                                         wpl.flags, wpl.showCmd,
                                         wpl.ptMinPosition.x, wpl.ptMinPosition.y,
                                         wpl.ptMaxPosition.x, wpl.ptMaxPosition.y,
                                         wpl.rcNormalPosition.left, wpl.rcNormalPosition.top, wpl.rcNormalPosition.right, wpl.rcNormalPosition.bottom);
        g_iniFile.SetValue(L"global", L"windowpos", sPos.c_str());
    }
    else
    {
        SHSetValue(HKEY_CURRENT_USER, L"Software\\grepWin", L"windowpos", REG_NONE, &wpl, sizeof(wpl));
    }
}

void CSearchDlg::UpdateInfoLabel()
{
    std::wstring sText;
    wchar_t      buf[1024] = {0};
    _stprintf_s(buf, _countof(buf), TranslatedString(hResource, IDS_INFOLABEL).c_str(),
                m_searchedItems, m_totalitems - m_searchedItems, m_totalmatches, m_items.size());
    sText = buf;

    SetDlgItemText(*this, IDC_SEARCHINFOLABEL, sText.c_str());
}

bool CSearchDlg::InitResultList()
{
    HWND  hListControl = GetDlgItem(*this, IDC_RESULTLIST);
    bool  filelist     = (IsDlgButtonChecked(*this, IDC_RESULTFILES) == BST_CHECKED);
    DWORD exStyle      = LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP | LVS_EX_FULLROWSELECT;
    SetWindowTheme(hListControl, L"Explorer", NULL);
    ListView_SetItemCount(hListControl, 0);

    int c = Header_GetItemCount(ListView_GetHeader(hListControl)) - 1;
    while (c >= 0)
        ListView_DeleteColumn(hListControl, c--);

    ListView_SetExtendedListViewStyle(hListControl, exStyle);
    ListView_SetImageList(hListControl, (WPARAM)(HIMAGELIST)CSysImageList::GetInstance(), LVSIL_SMALL);

    std::wstring sName         = TranslatedString(hResource, IDS_NAME);
    std::wstring sSize         = TranslatedString(hResource, IDS_SIZE);
    std::wstring sLine         = TranslatedString(hResource, IDS_LINE);
    std::wstring sMatches      = TranslatedString(hResource, IDS_MATCHES);
    std::wstring sText         = TranslatedString(hResource, IDS_TEXT);
    std::wstring sPath         = TranslatedString(hResource, IDS_PATH);
    std::wstring sEncoding     = TranslatedString(hResource, IDS_ENCODING);
    std::wstring sDateModified = TranslatedString(hResource, IDS_DATEMODIFIED);
    std::wstring sExtension    = TranslatedString(hResource, IDS_FILEEXT);

    LVCOLUMN lvc = {0};
    lvc.mask     = LVCF_TEXT | LVCF_FMT;
    lvc.fmt      = LVCFMT_LEFT;
    lvc.cx       = -1;
    lvc.pszText  = const_cast<LPWSTR>((LPCWSTR)sName.c_str());
    ListView_InsertColumn(hListControl, 0, &lvc);
    lvc.pszText = filelist ? const_cast<LPWSTR>((LPCWSTR)sSize.c_str()) : const_cast<LPWSTR>((LPCWSTR)sLine.c_str());
    lvc.fmt     = filelist ? LVCFMT_RIGHT : LVCFMT_LEFT;
    ListView_InsertColumn(hListControl, 1, &lvc);
    lvc.fmt     = LVCFMT_LEFT;
    lvc.pszText = filelist ? const_cast<LPWSTR>((LPCWSTR)sMatches.c_str()) : const_cast<LPWSTR>((LPCWSTR)sText.c_str());
    ListView_InsertColumn(hListControl, 2, &lvc);
    lvc.pszText = const_cast<LPWSTR>((LPCWSTR)sPath.c_str());
    ListView_InsertColumn(hListControl, 3, &lvc);
    if (filelist)
    {
        lvc.pszText = const_cast<LPWSTR>((LPCWSTR)sExtension.c_str());
        ListView_InsertColumn(hListControl, 4, &lvc);
        lvc.pszText = const_cast<LPWSTR>((LPCWSTR)sEncoding.c_str());
        ListView_InsertColumn(hListControl, 5, &lvc);
        lvc.pszText = const_cast<LPWSTR>((LPCWSTR)sDateModified.c_str());
        ListView_InsertColumn(hListControl, 6, &lvc);
    }

    ListView_SetColumnWidth(hListControl, 0, 300);
    ListView_SetColumnWidth(hListControl, 1, 50);
    ListView_SetColumnWidth(hListControl, 2, LVSCW_AUTOSIZE_USEHEADER);
    ListView_SetColumnWidth(hListControl, 3, LVSCW_AUTOSIZE_USEHEADER);
    ListView_SetColumnWidth(hListControl, 4, LVSCW_AUTOSIZE_USEHEADER);
    ListView_SetColumnWidth(hListControl, 5, LVSCW_AUTOSIZE_USEHEADER);
    ListView_SetColumnWidth(hListControl, 6, LVSCW_AUTOSIZE_USEHEADER);

    SendMessage(ListView_GetToolTips(hListControl), TTM_SETDELAYTIME, TTDT_AUTOPOP, SHRT_MAX);

    return true;
}

bool CSearchDlg::AddFoundEntry(CSearchInfo* pInfo, bool bOnlyListControl)
{
    if (!bOnlyListControl)
    {
        m_items.push_back(*pInfo);
        int index    = int(m_items.size() - 1);
        int subIndex = 0;
        for (const auto& linenumber : pInfo->matchlinesnumbers)
        {
            UNREFERENCED_PARAMETER(linenumber);
            m_listItems.push_back(std::make_tuple(index, subIndex));
            ++subIndex;
        }
    }
    else
    {
        HWND hListControl = GetDlgItem(*this, IDC_RESULTLIST);
        bool filelist     = (IsDlgButtonChecked(*this, IDC_RESULTFILES) == BST_CHECKED);
        ListView_SetItemCount(hListControl, filelist ? m_items.size() : m_listItems.size());
    }
    return true;
}

void CSearchDlg::FillResultList()
{
    ProfileTimer profile(L"FillResultList");
    SetCursor(LoadCursor(NULL, IDC_APPSTARTING));
    // refresh cursor
    POINT pt;
    GetCursorPos(&pt);
    SetCursorPos(pt.x, pt.y);

    bool filelist     = (IsDlgButtonChecked(*this, IDC_RESULTFILES) == BST_CHECKED);
    HWND hListControl = GetDlgItem(*this, IDC_RESULTLIST);
    SendMessage(hListControl, WM_SETREDRAW, FALSE, 0);
    ListView_SetItemCount(hListControl, filelist ? m_items.size() : m_listItems.size());
    AutoSizeAllColumns();
    SendMessage(hListControl, WM_SETREDRAW, TRUE, 0);
    SetCursor(LoadCursor(NULL, IDC_ARROW));
    // refresh cursor
    GetCursorPos(&pt);
    SetCursorPos(pt.x, pt.y);

    RedrawWindow(hListControl, NULL, NULL, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
}

void CSearchDlg::ShowContextMenu(int x, int y)
{
    HWND hListControl = GetDlgItem(*this, IDC_RESULTLIST);
    int  nCount       = ListView_GetItemCount(hListControl);
    if (nCount == 0)
        return;
    CShellContextMenu        shellMenu;
    int                      iItem = -1;
    std::vector<CSearchInfo> paths;
    while ((iItem = ListView_GetNextItem(hListControl, iItem, LVNI_SELECTED)) != (-1))
    {
        int selIndex = GetSelectedListIndex(iItem);
        if ((selIndex < 0) || (selIndex >= (int)m_items.size()))
            continue;
        paths.push_back(m_items[selIndex]);
    }

    if (paths.empty())
        return;

    std::vector<LineData> lines;
    bool                  filelist = (IsDlgButtonChecked(*this, IDC_RESULTFILES) == BST_CHECKED);
    if (!filelist)
    {
        WCHAR numbuf[40] = {0};
        while ((iItem = ListView_GetNextItem(hListControl, iItem, LVNI_SELECTED)) != (-1))
        {
            ListView_GetItemText(hListControl, iItem, 1, numbuf, 40);
            DWORD line = _wtoi(numbuf);
            if (line)
            {
                LineData          data;
                const CSearchInfo info       = m_items[GetSelectedListIndex(iItem)];
                data.path                    = info.filepath;
                const auto matchlinesnumbers = info.matchlinesnumbers;
                size_t     lineindex         = 0;
                for (auto it = matchlinesnumbers.cbegin(); it != matchlinesnumbers.cend(); ++it)
                {
                    if (*it == line)
                    {
                        LineDataLine dataline;
                        dataline.number = info.matchlinesnumbers[lineindex];
                        if (info.matchlines.size() > lineindex)
                            dataline.text = info.matchlines[lineindex];
                        data.lines.push_back(dataline);
                    }
                    ++lineindex;
                }
                lines.push_back(data);
            }
        }
    }

    shellMenu.SetObjects(paths, lines);

    POINT pt = {x, y};
    if ((x == -1) && (y == -1))
    {
        RECT rc;
        ListView_GetItemRect(hListControl, ListView_GetSelectionMark(hListControl), &rc, LVIR_LABEL);
        pt.x = (rc.right - rc.left) / 2;
        pt.y = (rc.bottom - rc.top) / 2;
        ClientToScreen(hListControl, &pt);
    }
    shellMenu.ShowContextMenu(hListControl, pt);
}

bool CSearchDlg::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN)
    {
        HWND hListControl = GetDlgItem(*this, IDC_RESULTLIST);
        auto bCtrl        = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        auto bShift       = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        auto bAlt         = (GetKeyState(VK_MENU) & 0x8000) != 0;
        switch (pMsg->wParam)
        {
            case VK_RETURN:
            {
                if (GetFocus() == hListControl)
                {
                    int iItem = -1;
                    while ((iItem = ListView_GetNextItem(hListControl, iItem, LVNI_SELECTED)) != (-1))
                    {
                        NMITEMACTIVATE itemactivate = {0};
                        itemactivate.hdr.code       = NM_DBLCLK;
                        itemactivate.iItem          = iItem;
                        DoListNotify(&itemactivate);
                    }
                    return true;
                }
            }
            break;
            case 'A':
            {
                if ((GetFocus() == hListControl) && bCtrl && !bShift && !bAlt)
                {
                    // select all entries
                    int nCount = ListView_GetItemCount(hListControl);
                    for (int i = 0; i < nCount; ++i)
                    {
                        ListView_SetItemState(hListControl, i, LVIS_SELECTED, LVIS_SELECTED);
                    }
                    return true;
                }
            }
            break;
            case 'C':
            {
                if ((GetFocus() == hListControl) && bCtrl && !bShift && !bAlt)
                {
                    // copy all selected entries to the clipboard
                    std::wstring clipBoardText;
                    HWND         hHeader       = ListView_GetHeader(hListControl);
                    int          columns       = Header_GetItemCount(hHeader);
                    WCHAR        buf[MAX_PATH] = {0};
                    for (int i = 0; i < columns; ++i)
                    {
                        HD_ITEM hdi    = {0};
                        hdi.mask       = HDI_TEXT;
                        hdi.pszText    = buf;
                        hdi.cchTextMax = _countof(buf);
                        Header_GetItem(hHeader, i, &hdi);
                        if (i > 0)
                            clipBoardText += L"\t";
                        clipBoardText += hdi.pszText;
                    }
                    clipBoardText += L"\r\n";

                    int iItem = -1;
                    while ((iItem = ListView_GetNextItem(hListControl, iItem, LVNI_SELECTED)) != (-1))
                    {
                        for (int i = 0; i < columns; ++i)
                        {
                            ListView_GetItemText(hListControl, iItem, i, buf, _countof(buf));
                            if (i > 0)
                                clipBoardText += L"\t";
                            clipBoardText += buf;
                        }
                        clipBoardText += L"\r\n";
                    }
                    WriteAsciiStringToClipboard(clipBoardText.c_str(), *this);
                }
            }
            break;
            case VK_DELETE:
            {
                m_AutoCompleteFilePatterns.RemoveSelected();
                m_AutoCompleteSearchPatterns.RemoveSelected();
                m_AutoCompleteReplacePatterns.RemoveSelected();
                m_AutoCompleteSearchPaths.RemoveSelected();
            }
            break;
            case 'K':
            case 'S':
            case 'F':
            case 'E':
            {
                if (bCtrl && !bShift && !bAlt)
                {
                    SetFocus(GetDlgItem(*this, IDC_SEARCHTEXT));
                }
            }
            break;
            case 'L':
            {
                if (bCtrl && !bShift && !bAlt)
                {
                    SetFocus(GetDlgItem(*this, IDC_PATTERN));
                }
            }
            break;
            case 'O':
            {
                if (bCtrl && !bShift && !bAlt)
                {
                    int iItem = -1;
                    while ((iItem = ListView_GetNextItem(hListControl, iItem, LVNI_SELECTED)) != (-1))
                    {
                        int selIndex = GetSelectedListIndex(iItem);
                        if ((selIndex < 0) || (selIndex >= (int)m_items.size()))
                            continue;
                        OpenFileAtListIndex(selIndex);
                    }
                }
            }
            break;
        }
    }
    return false;
}

void CSearchDlg::DoListNotify(LPNMITEMACTIVATE lpNMItemActivate)
{
    if (lpNMItemActivate->hdr.code == NM_DBLCLK)
    {
        if (lpNMItemActivate->iItem >= 0)
        {
            OpenFileAtListIndex(lpNMItemActivate->iItem);
        }
    }
    if (lpNMItemActivate->hdr.code == LVN_BEGINDRAG)
    {
        CDropFiles dropFiles; // class for creating DROPFILES struct

        HWND hListControl = GetDlgItem(*this, IDC_RESULTLIST);
        int  nCount       = ListView_GetItemCount(hListControl);
        if (nCount == 0)
            return;

        int iItem = -1;
        while ((iItem = ListView_GetNextItem(hListControl, iItem, LVNI_SELECTED)) != (-1))
        {
            dropFiles.AddFile(m_items[GetSelectedListIndex(iItem)].filepath);
        }

        if (dropFiles.GetCount() > 0)
        {
            dropFiles.CreateStructure(hListControl);
        }
    }
    if (lpNMItemActivate->hdr.code == LVN_COLUMNCLICK)
    {
        bool filelist = (IsDlgButtonChecked(*this, IDC_RESULTFILES) == BST_CHECKED);
        m_bAscending  = !m_bAscending;
        bool bDidSort = false;
        switch (lpNMItemActivate->iSubItem)
        {
            case 0:
                if (m_bAscending)
                    sort(m_items.begin(), m_items.end(), NameCompareAsc);
                else
                    sort(m_items.begin(), m_items.end(), NameCompareDesc);
                bDidSort = true;
                break;
            case 1:
                if (filelist)
                {
                    if (m_bAscending)
                        sort(m_items.begin(), m_items.end(), SizeCompareAsc);
                    else
                        sort(m_items.begin(), m_items.end(), SizeCompareDesc);
                    bDidSort = true;
                }
                break;
            case 2:
                if (filelist)
                {
                    if (m_bAscending)
                        sort(m_items.begin(), m_items.end(), MatchesCompareAsc);
                    else
                        sort(m_items.begin(), m_items.end(), MatchesCompareDesc);
                    bDidSort = true;
                }
                break;
            case 3:
                if (m_bAscending)
                    sort(m_items.begin(), m_items.end(), PathCompareAsc);
                else
                    sort(m_items.begin(), m_items.end(), PathCompareDesc);
                bDidSort = true;
                break;
            case 4:
                if (m_bAscending)
                    sort(m_items.begin(), m_items.end(), ExtCompareAsc);
                else
                    sort(m_items.begin(), m_items.end(), ExtCompareDesc);
                bDidSort = true;
                break;
            case 5:
                if (m_bAscending)
                    sort(m_items.begin(), m_items.end(), EncodingCompareAsc);
                else
                    sort(m_items.begin(), m_items.end(), EncodingCompareDesc);
                bDidSort = true;
                break;
            case 6:
                if (m_bAscending)
                    sort(m_items.begin(), m_items.end(), ModifiedTimeCompareAsc);
                else
                    sort(m_items.begin(), m_items.end(), ModifiedTimeCompareDesc);
                bDidSort = true;
                break;
        }
        if (bDidSort)
        {
            auto size = m_listItems.size();
            m_listItems.clear();
            m_listItems.reserve(size);

            int index = 0;
            for (const auto& item : m_items)
            {
                int subIndex = 0;
                for (const auto& linenumber : item.matchlinesnumbers)
                {
                    UNREFERENCED_PARAMETER(linenumber);
                    m_listItems.push_back(std::make_tuple(index, subIndex));
                    ++subIndex;
                }
                ++index;
            }
        }

        HWND hListControl = GetDlgItem(*this, IDC_RESULTLIST);
        SendMessage(hListControl, WM_SETREDRAW, FALSE, 0);
        ListView_SetItemCount(hListControl, filelist ? m_items.size() : m_listItems.size());

        AutoSizeAllColumns();
        HDITEM hd    = {0};
        hd.mask      = HDI_FORMAT;
        HWND hHeader = ListView_GetHeader(hListControl);
        int  iCount  = Header_GetItemCount(hHeader);
        for (int i = 0; i < iCount; ++i)
        {
            Header_GetItem(hHeader, i, &hd);
            hd.fmt &= ~(HDF_SORTDOWN | HDF_SORTUP);
            Header_SetItem(hHeader, i, &hd);
        }
        if (bDidSort)
        {
            Header_GetItem(hHeader, lpNMItemActivate->iSubItem, &hd);
            hd.fmt |= (m_bAscending ? HDF_SORTUP : HDF_SORTDOWN);
            Header_SetItem(hHeader, lpNMItemActivate->iSubItem, &hd);
        }
        SendMessage(hListControl, WM_SETREDRAW, TRUE, 0);
        RedrawWindow(hListControl, NULL, NULL, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
    }
    if (lpNMItemActivate->hdr.code == LVN_GETINFOTIP)
    {
        NMLVGETINFOTIP* pInfoTip = reinterpret_cast<NMLVGETINFOTIP*>(lpNMItemActivate);

        // Which item number?
        size_t itemid        = pInfoTip->iItem;
        int    iItem         = GetSelectedListIndex((int)itemid);
        pInfoTip->pszText[0] = 0;
        if ((int)m_items.size() > iItem)
        {
            CSearchInfo inf = m_items[iItem];

            std::wstring matchString = inf.filepath + L"\n";
            std::wstring sFormat     = TranslatedString(hResource, IDS_CONTEXTLINE);
            for (size_t i = 0; i < min(inf.matchlines.size(), 5); ++i)
            {
                std::wstring matchtext = inf.matchlines[i];
                CStringUtils::trim(matchtext);
                matchString += CStringUtils::Format(sFormat.c_str(), inf.matchlinesnumbers[i], matchtext.c_str());
            }
            if (inf.matchlines.size() > 5)
            {
                std::wstring sx  = TranslatedString(hResource, IDS_XMOREMATCHES);
                std::wstring ssx = CStringUtils::Format(sx.c_str(), int(inf.matchlines.size() - 5));
                matchString += ssx;
            }
            lstrcpyn(pInfoTip->pszText, matchString.c_str(), pInfoTip->cchTextMax);
        }
    }
    if (lpNMItemActivate->hdr.code == LVN_GETDISPINFO)
    {
        static const std::wstring sBinary = TranslatedString(hResource, IDS_BINARY);

        NMLVDISPINFO* pDispInfo = reinterpret_cast<NMLVDISPINFO*>(lpNMItemActivate);
        LV_ITEM*      pItem     = &(pDispInfo)->item;

        int  iItem    = pItem->iItem;
        bool filelist = (IsDlgButtonChecked(*this, IDC_RESULTFILES) == BST_CHECKED);

        if (filelist)
        {
            const auto& pInfo = &m_items[iItem];
            if (pItem->mask & LVIF_TEXT)
            {
                switch (pItem->iSubItem)
                {
                    case 0: // name of the file
                        wcsncpy_s(pItem->pszText, pItem->cchTextMax, pInfo->filepath.substr(pInfo->filepath.find_last_of('\\') + 1).c_str(), pItem->cchTextMax - 1);
                        break;
                    case 1: // file size
                        if (!pInfo->folder)
                            StrFormatByteSizeW(pInfo->filesize, pItem->pszText, pItem->cchTextMax);
                        break;
                    case 2: // match count or read error
                        if (pInfo->readerror)
                            wcsncpy_s(pItem->pszText, pItem->cchTextMax, TranslatedString(hResource, IDS_READERROR).c_str(), pItem->cchTextMax - 1);
                        else
                            swprintf_s(pItem->pszText, pItem->cchTextMax, L"%lld", pInfo->matchcount);
                        break;
                    case 3: // path
                        if (m_searchpath.find('|') != std::wstring::npos)
                            wcsncpy_s(pItem->pszText, pItem->cchTextMax, pInfo->filepath.substr(0, pInfo->filepath.size() - pInfo->filepath.substr(pInfo->filepath.find_last_of('\\')).size()).c_str(), pItem->cchTextMax - 1);
                        else
                        {
                            auto filepart = pInfo->filepath.substr(pInfo->filepath.find_last_of('\\'));
                            auto len      = pInfo->filepath.size() - m_searchpath.size() - filepart.size();
                            if (len > 0)
                                --len;
                            if (m_searchpath.size() < pInfo->filepath.size())
                            {
                                wcsncpy_s(pItem->pszText, pItem->cchTextMax, pInfo->filepath.substr(m_searchpath.size() + 1, len).c_str(), pItem->cchTextMax - 1);
                                if (pItem->pszText[0] == 0)
                                    wcscpy_s(pItem->pszText, pItem->cchTextMax, L"\\.");
                            }
                            else
                                wcsncpy_s(pItem->pszText, pItem->cchTextMax, pInfo->filepath.c_str(), pItem->cchTextMax - 1);
                        }
                        break;
                    case 4: // extension of the file
                    {
                        pItem->pszText[0] = 0;
                        if (!pInfo->folder)
                        {
                            auto dotpos = pInfo->filepath.find_last_of('.');
                            if (dotpos != std::wstring::npos)
                            {
                                if (pInfo->filepath.find('\\', dotpos) == std::wstring::npos)
                                    wcsncpy_s(pItem->pszText, pItem->cchTextMax, pInfo->filepath.substr(dotpos + 1).c_str(), pItem->cchTextMax - 1);
                            }
                        }
                    }
                    break;
                    case 5: // encoding
                        switch (pInfo->encoding)
                        {
                            case CTextFile::ANSI:
                                wcsncpy_s(pItem->pszText, pItem->cchTextMax, L"ANSI", pItem->cchTextMax - 1);
                                break;
                            case CTextFile::UNICODE_LE:
                                wcsncpy_s(pItem->pszText, pItem->cchTextMax, L"UTF-16-LE", pItem->cchTextMax - 1);
                                break;
                            case CTextFile::UNICODE_BE:
                                wcsncpy_s(pItem->pszText, pItem->cchTextMax, L"UTF-16-BE", pItem->cchTextMax - 1);
                                break;
                            case CTextFile::UTF8:
                                wcsncpy_s(pItem->pszText, pItem->cchTextMax, L"UTF8", pItem->cchTextMax - 1);
                                break;
                            case CTextFile::BINARY:
                                wcsncpy_s(pItem->pszText, pItem->cchTextMax, L"BINARY", pItem->cchTextMax - 1);
                                break;
                            default:
                                wcsncpy_s(pItem->pszText, pItem->cchTextMax, L"", pItem->cchTextMax - 1);
                                break;
                        }
                        break;
                    case 6: // modification date
                        formatDate(pItem->pszText, pInfo->modifiedtime, true);
                        break;
                    default:
                        pItem->pszText[0] = 0;
                        break;
                }
            }
            if (pItem->mask & LVIF_IMAGE)
            {
                pItem->iImage = pInfo->folder ? CSysImageList::GetInstance().GetDirIconIndex() : CSysImageList::GetInstance().GetFileIconIndex(pInfo->filepath);
            }
        }
        else
        {
            auto tup      = m_listItems[iItem];
            auto index    = std::get<0>(tup);
            auto subIndex = std::get<1>(tup);

            const auto& item  = m_items[index];
            const auto& pInfo = &item;
            if (item.encoding == CTextFile::BINARY)
            {
                if (pItem->mask & LVIF_TEXT)
                {
                    switch (pItem->iSubItem)
                    {
                        case 0: // name of the file
                            wcsncpy_s(pItem->pszText, pItem->cchTextMax, pInfo->filepath.substr(pInfo->filepath.find_last_of('\\') + 1).c_str(), pItem->cchTextMax - 1);
                            break;
                        case 1: // binary
                            wcsncpy_s(pItem->pszText, pItem->cchTextMax, sBinary.c_str(), pItem->cchTextMax);
                            break;
                        case 3: // path
                            wcsncpy_s(pItem->pszText, pItem->cchTextMax, pInfo->filepath.substr(0, pInfo->filepath.size() - pInfo->filepath.substr(pInfo->filepath.find_last_of('\\') + 1).size() - 1).c_str(), pItem->cchTextMax - 1);
                            break;
                        default:
                            pItem->pszText[0] = 0;
                            break;
                    }
                }
                if (pItem->mask & LVIF_IMAGE)
                {
                    pItem->iImage = pInfo->folder ? CSysImageList::GetInstance().GetDirIconIndex() : CSysImageList::GetInstance().GetFileIconIndex(pInfo->filepath);
                }
            }
            else
            {
                if (pItem->mask & LVIF_TEXT)
                {
                    switch (pItem->iSubItem)
                    {
                        case 0: // name of the file
                            wcsncpy_s(pItem->pszText, pItem->cchTextMax, pInfo->filepath.substr(pInfo->filepath.find_last_of('\\') + 1).c_str(), pItem->cchTextMax - 1);
                            break;
                        case 1: // line number
                            swprintf_s(pItem->pszText, pItem->cchTextMax, L"%ld", pInfo->matchlinesnumbers[subIndex]);
                            break;
                        case 2: // line
                        {
                            std::wstring line;
                            if (pInfo->matchlines.size() > (size_t)subIndex)
                                line = pInfo->matchlines[subIndex];
                            std::replace(line.begin(), line.end(), '\t', ' ');
                            std::replace(line.begin(), line.end(), '\n', ' ');
                            std::replace(line.begin(), line.end(), '\r', ' ');
                            wcsncpy_s(pItem->pszText, pItem->cchTextMax, line.c_str(), pItem->cchTextMax - 1);
                        }
                        break;
                        case 3: // path
                            wcsncpy_s(pItem->pszText, pItem->cchTextMax, pInfo->filepath.substr(0, pInfo->filepath.size() - pInfo->filepath.substr(pInfo->filepath.find_last_of('\\') + 1).size() - 1).c_str(), pItem->cchTextMax - 1);
                            break;
                        default:
                            pItem->pszText[0] = 0;
                            break;
                    }
                }

                if (pItem->mask & LVIF_IMAGE)
                {
                    pItem->iImage = pInfo->folder ? CSysImageList::GetInstance().GetDirIconIndex() : CSysImageList::GetInstance().GetFileIconIndex(pInfo->filepath);
                }
            }
        }
    }
}

void CSearchDlg::OpenFileAtListIndex(int listIndex)
{
    int          iItem  = GetSelectedListIndex(listIndex);
    CSearchInfo  inf    = m_items[iItem];
    size_t       dotPos = inf.filepath.rfind('.');
    std::wstring ext;
    if (dotPos != std::wstring::npos)
        ext = inf.filepath.substr(dotPos);

    CRegStdString regEditorCmd(L"Software\\grepWin\\editorcmd");
    std::wstring  cmd = regEditorCmd;
    if (bPortable)
        cmd = g_iniFile.GetValue(L"global", L"editorcmd", L"");
    if (!cmd.empty())
    {
        bool filelist = (IsDlgButtonChecked(*this, IDC_RESULTFILES) == BST_CHECKED);
        if (!filelist)
        {
            HWND    hListControl          = GetDlgItem(*this, IDC_RESULTLIST);
            wchar_t textlinebuf[MAX_PATH] = {0};
            LVITEM  lv                    = {0};
            lv.iItem                      = listIndex;
            lv.iSubItem                   = 1; // line number
            lv.mask                       = LVIF_TEXT;
            lv.pszText                    = textlinebuf;
            lv.cchTextMax                 = _countof(textlinebuf);
            if (ListView_GetItem(hListControl, &lv))
            {
                SearchReplace(cmd, L"%line%", textlinebuf);
            }
        }
        else
        {
            // use the first matching line in this file
            if (!inf.matchlinesnumbers.empty())
                SearchReplace(cmd, L"%line%", CStringUtils::Format(L"%Id", inf.matchlinesnumbers[0]));
            else
                SearchReplace(cmd, L"%line%", L"0");
        }

        SearchReplace(cmd, L"%path%", inf.filepath.c_str());

        STARTUPINFO         startupInfo;
        PROCESS_INFORMATION processInfo;
        SecureZeroMemory(&startupInfo, sizeof(startupInfo));
        startupInfo.cb = sizeof(STARTUPINFO);
        SecureZeroMemory(&processInfo, sizeof(processInfo));
        CreateProcess(NULL, const_cast<wchar_t*>(cmd.c_str()), NULL, NULL, FALSE, 0, 0, NULL, &startupInfo, &processInfo);
        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);
        return;
    }

    DWORD buflen = 0;
    AssocQueryString(ASSOCF_INIT_DEFAULTTOSTAR, ASSOCSTR_DDECOMMAND, ext.c_str(), NULL, NULL, &buflen);
    if (buflen)
    {
        // application requires DDE to open the file:
        // since we can't do this the easy way with CreateProcess, we use ShellExecute instead
        ShellExecute(*this, NULL, inf.filepath.c_str(), NULL, NULL, SW_SHOW);
        return;
    }

    AssocQueryString(ASSOCF_INIT_DEFAULTTOSTAR, ASSOCSTR_COMMAND, ext.c_str(), NULL, NULL, &buflen);
    auto cmdbuf = std::make_unique<wchar_t[]>(buflen + 1);
    AssocQueryString(ASSOCF_INIT_DEFAULTTOSTAR, ASSOCSTR_COMMAND, ext.c_str(), NULL, cmdbuf.get(), &buflen);
    std::wstring application = cmdbuf.get();
    // normalize application path
    DWORD len = ExpandEnvironmentStrings(application.c_str(), NULL, 0);
    cmdbuf    = std::make_unique<wchar_t[]>(buflen + 1);
    ExpandEnvironmentStrings(application.c_str(), cmdbuf.get(), len);
    application = cmdbuf.get();

    // resolve parameters
    if (application.find(L"%1") == std::wstring::npos)
        application += L" %1";

    bool         filelist = (IsDlgButtonChecked(*this, IDC_RESULTFILES) == BST_CHECKED);
    std::wstring linenumberparam_before;
    std::wstring linenumberparam;
    wchar_t      textlinebuf[MAX_PATH] = {0};
    if (!filelist)
    {
        HWND   hListControl = GetDlgItem(*this, IDC_RESULTLIST);
        LVITEM lv           = {0};
        lv.iItem            = listIndex;
        lv.iSubItem         = 1; // line number
        lv.mask             = LVIF_TEXT;
        lv.pszText          = textlinebuf;
        lv.cchTextMax       = _countof(textlinebuf);
        if (!ListView_GetItem(hListControl, &lv))
        {
            textlinebuf[0] = '\0';
        }
    }
    else if (!inf.matchlinesnumbers.empty())
    {
        // use the first matching line in this file
        swprintf_s(textlinebuf, L"%ld", inf.matchlinesnumbers[0]);
    }
    if (textlinebuf[0] == 0)
        wcscpy_s(textlinebuf, L"0");
    std::wstring appname = application;
    std::transform(appname.begin(), appname.end(), appname.begin(), ::towlower);

    // now find out if the application which opens the file is known to us
    // and if it has a 'linenumber' switch to jump directly to a specific
    // line number.
    if (appname.find(L"notepad++.exe") != std::wstring::npos)
    {
        // notepad++
        linenumberparam = CStringUtils::Format(L"-n%s", textlinebuf);
    }
    else if (appname.find(L"xemacs.exe") != std::wstring::npos)
    {
        // XEmacs
        linenumberparam = CStringUtils::Format(L"+%s", textlinebuf);
    }
    else if (appname.find(L"uedit32.exe") != std::wstring::npos)
    {
        // UltraEdit
        linenumberparam = CStringUtils::Format(L"-l%s", textlinebuf);
    }
    else if (appname.find(L"codewright.exe") != std::wstring::npos)
    {
        // CodeWright
        linenumberparam = CStringUtils::Format(L"-G%s", textlinebuf);
    }
    else if (appname.find(L"notepad2.exe") != std::wstring::npos)
    {
        // Notepad2
        auto escapedsearch = m_searchString;
        SearchReplace(escapedsearch, L"\"", L"\\\"");
        linenumberparam_before = CStringUtils::Format(L"/g %s /mr \"%s\"", textlinebuf, escapedsearch.c_str());
    }
    else if ((appname.find(L"bowpad.exe") != std::wstring::npos) || (appname.find(L"bowpad64.exe") != std::wstring::npos))
    {
        // BowPad
        linenumberparam = CStringUtils::Format(L"/line:%s", textlinebuf);
    }
    else if (appname.find(L"code.exe") != std::wstring::npos)
    {
        // Visual Studio Code
        linenumberparam_before = L"--goto";
        linenumberparam        = CStringUtils::Format(L":%s", textlinebuf);
    }

    // replace "%1" with %1
    std::wstring           tag      = L"\"%1\"";
    std::wstring           repl     = L"%1";
    std::wstring::iterator it_begin = search(application.begin(), application.end(), tag.begin(), tag.end());
    if (it_begin != application.end())
    {
        std::wstring::iterator it_end = it_begin + tag.size();
        application.replace(it_begin, it_end, repl);
    }
    // replace %1 with "path/of/selected/file"
    tag = L"%1";
    if (application.find(L"rundll32.exe") == std::wstring::npos)
        repl = L"\"" + inf.filepath + L"\"";
    else
        repl = inf.filepath;
    if (!linenumberparam_before.empty())
    {
        repl = linenumberparam_before + L" " + repl;
    }
    it_begin = search(application.begin(), application.end(), tag.begin(), tag.end());
    if (it_begin != application.end())
    {
        std::wstring::iterator it_end = it_begin + tag.size();
        application.replace(it_begin, it_end, repl);
    }
    if (!linenumberparam.empty())
    {
        if (!linenumberparam.starts_with(L":"))
        {
            application += L" ";
        }
        application += linenumberparam;
    }

    STARTUPINFO         startupInfo;
    PROCESS_INFORMATION processInfo;
    SecureZeroMemory(&startupInfo, sizeof(startupInfo));
    startupInfo.cb = sizeof(STARTUPINFO);

    SecureZeroMemory(&processInfo, sizeof(processInfo));
    CreateProcess(NULL, const_cast<wchar_t*>(application.c_str()), NULL, NULL, FALSE, 0, 0, NULL, &startupInfo, &processInfo);
    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
}

bool grepWin_is_regex_valid(const std::wstring& m_searchString)
{
    // check if the regex is valid
    bool bValid = true;
    try
    {
        boost::wregex expression = boost::wregex(m_searchString);
    }
    catch (const std::exception&)
    {
        bValid = false;
    }
    return bValid;
}

bool CSearchDlg::SaveSettings()
{
    // get all the information we need from the dialog
    auto buf     = GetDlgItemText(IDC_SEARCHPATH);
    m_searchpath = buf.get();

    buf            = GetDlgItemText(IDC_SEARCHTEXT);
    m_searchString = buf.get();

    buf             = GetDlgItemText(IDC_REPLACETEXT);
    m_replaceString = buf.get();

    buf                       = GetDlgItemText(IDC_EXCLUDEDIRSPATTERN);
    m_excludedirspatternregex = buf.get();

    buf            = GetDlgItemText(IDC_PATTERN);
    m_patternregex = buf.get();

    // split the pattern string into single patterns and
    // add them to an array
    auto   pBuf = buf.get();
    size_t pos  = 0;
    m_patterns.clear();
    do
    {
        pos            = wcscspn(pBuf, L"|");
        std::wstring s = std::wstring(pBuf, pos);
        if (!s.empty())
        {
            std::transform(s.begin(), s.end(), s.begin(), ::towlower);
            m_patterns.push_back(s);
            auto endpart = s.rbegin();
            if (*endpart == '*' && s.size() > 2)
            {
                ++endpart;
                if (*endpart == '.')
                {
                    m_patterns.push_back(s.substr(0, s.size() - 2));
                }
            }
        }
        pBuf += pos;
        pBuf++;
    } while (*pBuf && (*(pBuf - 1)));

    if (m_searchpath.empty())
        return false;
    if (bPortable)
        g_iniFile.SetValue(L"global", L"searchpath", m_searchpath.c_str());
    else
        m_regSearchPath = m_searchpath;
    m_bUseRegex = (IsDlgButtonChecked(*this, IDC_REGEXRADIO) == BST_CHECKED);
    if (bPortable)
        g_iniFile.SetValue(L"global", L"UseRegex", m_bUseRegex ? L"1" : L"0");
    else
        m_regUseRegex = (DWORD)m_bUseRegex;
    if (m_bUseRegex)
    {
        // check if the regex is valid before doing the search
        if (!grepWin_is_regex_valid(m_searchString) && !m_searchString.empty())
        {
            return false;
        }
    }
    m_bUseRegexForPaths = (IsDlgButtonChecked(*this, IDC_FILEPATTERNREGEX) == BST_CHECKED);
    if (bPortable)
        g_iniFile.SetValue(L"global", L"UseFileMatchRegex", m_bUseRegexForPaths ? L"1" : L"0");
    else
        m_regUseRegexForPaths = (DWORD)m_bUseRegexForPaths;
    if (m_bUseRegexForPaths)
    {
        // check if the regex is valid before doing the search
        if (!grepWin_is_regex_valid(m_patternregex) && !m_patternregex.empty())
        {
            return false;
        }
    }

    // check if the Exclude Dirs regex is valid before doing the search
    if (!grepWin_is_regex_valid(m_excludedirspatternregex) && !m_excludedirspatternregex.empty())
    {
        return false;
    }

    m_bAllSize = (IsDlgButtonChecked(*this, IDC_ALLSIZERADIO) == BST_CHECKED);
    if (bPortable)
        g_iniFile.SetValue(L"global", L"AllSize", m_bAllSize ? L"1" : L"0");
    else
        m_regAllSize = (DWORD)m_bAllSize;
    m_lSize   = 0;
    m_sizeCmp = 0;
    if (!m_bAllSize)
    {
        buf     = GetDlgItemText(IDC_SIZEEDIT);
        m_lSize = _tstol(buf.get());
        if (bPortable)
            g_iniFile.SetValue(L"global", L"Size", CStringUtils::Format(L"%I64u", m_lSize).c_str());
        else
            m_regSize = CStringUtils::Format(L"%I64u", m_lSize).c_str();
        m_lSize *= 1024;
        m_sizeCmp = (int)SendDlgItemMessage(*this, IDC_SIZECOMBO, CB_GETCURSEL, 0, 0);
        if (bPortable)
            g_iniFile.SetValue(L"global", L"SizeCombo", CStringUtils::Format(L"%d", m_sizeCmp).c_str());
        else
            m_regSizeCombo = m_sizeCmp;
    }
    m_bIncludeSystem     = (IsDlgButtonChecked(*this, IDC_INCLUDESYSTEM) == BST_CHECKED);
    m_bIncludeHidden     = (IsDlgButtonChecked(*this, IDC_INCLUDEHIDDEN) == BST_CHECKED);
    m_bIncludeSubfolders = (IsDlgButtonChecked(*this, IDC_INCLUDESUBFOLDERS) == BST_CHECKED);
    m_bIncludeBinary     = (IsDlgButtonChecked(*this, IDC_INCLUDEBINARY) == BST_CHECKED);
    m_bCreateBackup      = (IsDlgButtonChecked(*this, IDC_CREATEBACKUP) == BST_CHECKED);
    m_bUTF8              = (IsDlgButtonChecked(*this, IDC_UTF8) == BST_CHECKED);
    m_bForceBinary       = (IsDlgButtonChecked(*this, IDC_BINARY) == BST_CHECKED);
    m_bCaseSensitive     = (IsDlgButtonChecked(*this, IDC_CASE_SENSITIVE) == BST_CHECKED);
    m_bDotMatchesNewline = (IsDlgButtonChecked(*this, IDC_DOTMATCHNEWLINE) == BST_CHECKED);

    m_DateLimit = 0;
    if (IsDlgButtonChecked(*this, IDC_RADIO_DATE_ALL) == BST_CHECKED)
        m_DateLimit = 0;
    if (IsDlgButtonChecked(*this, IDC_RADIO_DATE_NEWER) == BST_CHECKED)
        m_DateLimit = IDC_RADIO_DATE_NEWER - IDC_RADIO_DATE_ALL;
    if (IsDlgButtonChecked(*this, IDC_RADIO_DATE_OLDER) == BST_CHECKED)
        m_DateLimit = IDC_RADIO_DATE_OLDER - IDC_RADIO_DATE_ALL;
    if (IsDlgButtonChecked(*this, IDC_RADIO_DATE_BETWEEN) == BST_CHECKED)
        m_DateLimit = IDC_RADIO_DATE_BETWEEN - IDC_RADIO_DATE_ALL;
    SYSTEMTIME sysTime = {0};
    DateTime_GetSystemtime(GetDlgItem(*this, IDC_DATEPICK1), &sysTime);
    SystemTimeToFileTime(&sysTime, &m_Date1);
    DateTime_GetSystemtime(GetDlgItem(*this, IDC_DATEPICK2), &sysTime);
    SystemTimeToFileTime(&sysTime, &m_Date2);
    m_showContent = IsDlgButtonChecked(*this, IDC_RESULTCONTENT) == BST_CHECKED;

    if (bPortable)
    {
        g_iniFile.SetValue(L"global", L"IncludeSystem", m_bIncludeSystem ? L"1" : L"0");
        g_iniFile.SetValue(L"global", L"IncludeHidden", m_bIncludeHidden ? L"1" : L"0");
        g_iniFile.SetValue(L"global", L"IncludeSubfolders", m_bIncludeSubfolders ? L"1" : L"0");
        g_iniFile.SetValue(L"global", L"IncludeBinary", m_bIncludeBinary ? L"1" : L"0");
        g_iniFile.SetValue(L"global", L"CreateBackup", m_bCreateBackup ? L"1" : L"0");
        g_iniFile.SetValue(L"global", L"UTF8", m_bUTF8 ? L"1" : L"0");
        g_iniFile.SetValue(L"global", L"Binary", m_bForceBinary ? L"1" : L"0");
        g_iniFile.SetValue(L"global", L"CaseSensitive", m_bCaseSensitive ? L"1" : L"0");
        g_iniFile.SetValue(L"global", L"DotMatchesNewline", m_bDotMatchesNewline ? L"1" : L"0");
        g_iniFile.SetValue(L"global", L"pattern", m_patternregex.c_str());
        g_iniFile.SetValue(L"global", L"ExcludeDirsPattern", m_excludedirspatternregex.c_str());
        g_iniFile.SetValue(L"global", L"DateLimit", std::to_wstring(m_DateLimit).c_str());
        g_iniFile.SetValue(L"global", L"Date1Low", std::to_wstring(m_Date1.dwLowDateTime).c_str());
        g_iniFile.SetValue(L"global", L"Date1High", std::to_wstring(m_Date1.dwHighDateTime).c_str());
        g_iniFile.SetValue(L"global", L"Date2Low", std::to_wstring(m_Date2.dwLowDateTime).c_str());
        g_iniFile.SetValue(L"global", L"Date2High", std::to_wstring(m_Date2.dwHighDateTime).c_str());
        if (!m_showContentSet)
            g_iniFile.SetValue(L"global", L"showcontent", m_showContent ? L"1" : L"0");
    }
    else
    {
        m_regIncludeSystem      = (DWORD)m_bIncludeSystem;
        m_regIncludeHidden      = (DWORD)m_bIncludeHidden;
        m_regIncludeSubfolders  = (DWORD)m_bIncludeSubfolders;
        m_regIncludeBinary      = (DWORD)m_bIncludeBinary;
        m_regCreateBackup       = (DWORD)m_bCreateBackup;
        m_regUTF8               = (DWORD)m_bUTF8;
        m_regBinary             = (DWORD)m_bForceBinary;
        m_regCaseSensitive      = (DWORD)m_bCaseSensitive;
        m_regDotMatchesNewline  = (DWORD)m_bDotMatchesNewline;
        m_regPattern            = m_patternregex;
        m_regExcludeDirsPattern = m_excludedirspatternregex;
        m_regDateLimit          = m_DateLimit;
        m_regDate1Low           = m_Date1.dwLowDateTime;
        m_regDate1High          = m_Date1.dwHighDateTime;
        m_regDate2Low           = m_Date2.dwLowDateTime;
        m_regDate2High          = m_Date2.dwHighDateTime;
        if (!m_showContentSet)
            m_regShowContent = m_showContent;
    }

    SaveWndPosition();

    return true;
}

bool CSearchDlg::NameCompareAsc(const CSearchInfo& Entry1, const CSearchInfo& Entry2)
{
    std::wstring name1 = Entry1.filepath.substr(Entry1.filepath.find_last_of('\\') + 1);
    std::wstring name2 = Entry2.filepath.substr(Entry2.filepath.find_last_of('\\') + 1);
    return StrCmpLogicalW(name1.c_str(), name2.c_str()) < 0;
}

bool CSearchDlg::SizeCompareAsc(const CSearchInfo& Entry1, const CSearchInfo& Entry2)
{
    return Entry1.filesize < Entry2.filesize;
}

bool CSearchDlg::MatchesCompareAsc(const CSearchInfo& Entry1, const CSearchInfo& Entry2)
{
    return Entry1.matchcount < Entry2.matchcount;
}

bool CSearchDlg::PathCompareAsc(const CSearchInfo& Entry1, const CSearchInfo& Entry2)
{
    std::wstring name1 = Entry1.filepath.substr(Entry1.filepath.find_last_of('\\') + 1);
    std::wstring name2 = Entry2.filepath.substr(Entry2.filepath.find_last_of('\\') + 1);
    std::wstring path1 = Entry1.filepath.substr(0, Entry1.filepath.size() - name1.size() - 1);
    std::wstring path2 = Entry2.filepath.substr(0, Entry2.filepath.size() - name2.size() - 1);
    int          cmp   = path1.compare(path2);
    if (cmp != 0)
        return cmp < 0;
    return StrCmpLogicalW(name1.c_str(), name2.c_str()) < 0;
}

bool CSearchDlg::EncodingCompareAsc(const CSearchInfo& Entry1, const CSearchInfo& Entry2)
{
    return Entry1.encoding < Entry2.encoding;
}

bool CSearchDlg::ModifiedTimeCompareAsc(const CSearchInfo& Entry1, const CSearchInfo& Entry2)
{
    return CompareFileTime(&Entry1.modifiedtime, &Entry2.modifiedtime) < 0;
}

bool CSearchDlg::ExtCompareAsc(const CSearchInfo& Entry1, const CSearchInfo& Entry2)
{
    auto         dotpos1 = Entry1.filepath.find_last_of('.');
    auto         dotpos2 = Entry2.filepath.find_last_of('.');
    std::wstring ext1    = dotpos1 != std::wstring::npos ? Entry1.filepath.substr(dotpos1 + 1) : L"";
    std::wstring ext2    = dotpos2 != std::wstring::npos ? Entry2.filepath.substr(dotpos2 + 1) : L"";
    return StrCmpLogicalW(ext1.c_str(), ext2.c_str()) < 0;
}

bool CSearchDlg::NameCompareDesc(const CSearchInfo& Entry1, const CSearchInfo& Entry2)
{
    std::wstring name1 = Entry1.filepath.substr(Entry1.filepath.find_last_of('\\') + 1);
    std::wstring name2 = Entry2.filepath.substr(Entry2.filepath.find_last_of('\\') + 1);
    return StrCmpLogicalW(name1.c_str(), name2.c_str()) > 0;
}

bool CSearchDlg::SizeCompareDesc(const CSearchInfo& Entry1, const CSearchInfo& Entry2)
{
    return Entry1.filesize > Entry2.filesize;
}

bool CSearchDlg::MatchesCompareDesc(const CSearchInfo& Entry1, const CSearchInfo& Entry2)
{
    return Entry1.matchcount > Entry2.matchcount;
}

bool CSearchDlg::PathCompareDesc(const CSearchInfo& Entry1, const CSearchInfo& Entry2)
{
    std::wstring name1 = Entry1.filepath.substr(Entry1.filepath.find_last_of('\\') + 1);
    std::wstring name2 = Entry2.filepath.substr(Entry2.filepath.find_last_of('\\') + 1);
    std::wstring path1 = Entry1.filepath.substr(0, Entry1.filepath.size() - name1.size() - 1);
    std::wstring path2 = Entry2.filepath.substr(0, Entry2.filepath.size() - name2.size() - 1);
    int          cmp   = path1.compare(path2);
    if (cmp != 0)
        return cmp > 0;
    return StrCmpLogicalW(name1.c_str(), name2.c_str()) > 0;
}

bool CSearchDlg::EncodingCompareDesc(const CSearchInfo& Entry1, const CSearchInfo& Entry2)
{
    return Entry1.encoding > Entry2.encoding;
}

bool CSearchDlg::ModifiedTimeCompareDesc(const CSearchInfo& Entry1, const CSearchInfo& Entry2)
{
    return CompareFileTime(&Entry1.modifiedtime, &Entry2.modifiedtime) > 0;
}

bool CSearchDlg::ExtCompareDesc(const CSearchInfo& Entry1, const CSearchInfo& Entry2)
{
    auto         dotpos1 = Entry1.filepath.find_last_of('.');
    auto         dotpos2 = Entry2.filepath.find_last_of('.');
    std::wstring ext1    = dotpos1 != std::wstring::npos ? Entry1.filepath.substr(dotpos1 + 1) : L"";
    std::wstring ext2    = dotpos2 != std::wstring::npos ? Entry2.filepath.substr(dotpos2 + 1) : L"";
    return StrCmpLogicalW(ext1.c_str(), ext2.c_str()) > 0;
}

bool grepWin_match_i(const std::wstring& the_regex, const wchar_t* pText)
{
    try
    {
        boost::wregex  expression = boost::wregex(the_regex, boost::regex::normal | boost::regbase::icase);
        boost::wcmatch whatc;
        if (boost::regex_match(pText, whatc, expression))
        {
            return true;
        }
    }
    catch (const std::exception&)
    {
    }
    return false;
}

DWORD CSearchDlg::SearchThread()
{
    ProfileTimer profile(L"SearchThread");

    auto pathbuf = std::make_unique<wchar_t[]>(MAX_PATH_NEW);

    // split the path string into single paths and
    // add them to an array
    const auto*               pBufSearchPath = m_searchpath.c_str();
    size_t                    pos            = 0;
    std::vector<std::wstring> pathvector;
    do
    {
        pos            = wcscspn(pBufSearchPath, L"|");
        std::wstring s = std::wstring(pBufSearchPath, pos);
        if (!s.empty())
        {
            // Remove extra backslashes except for the UNC identifier
            std::string::size_type found = s.find_first_of('\\', 1);
            while (found != std::string::npos)
            {
                while (s[found + 1] == '\\')
                {
                    s.erase(found + 1, 1);
                }
                found = s.find_first_of('\\', found + 1);
            }
            CStringUtils::rtrim(s, L"\\/");
            pathvector.push_back(s);
        }
        pBufSearchPath += pos;
        pBufSearchPath++;
    } while (*pBufSearchPath && (*(pBufSearchPath - 1)));

    if (!m_bUseRegex && !m_replaceString.empty())
    {
        // escape all characters in the replace string
        std::wstring srepl;
        for (const auto& c : m_replaceString)
        {
            switch (c)
            {
                case '$':
                case '\\':
                case '(':
                case ')':
                case '?':
                case ':':
                    srepl += L"\\";
                    break;
                default:
                    break;
            }
            srepl += c;
        }
        m_replaceString = srepl;
    }

    SendMessage(*this, SEARCH_START, 0, 0);

    std::wstring SearchStringutf16;
    for (auto c : m_searchString)
    {
        SearchStringutf16 += c;
        SearchStringutf16 += L"\\x00";
    }

    // use a thread pool:
    // use 2 threads less than processors are available,
    // because we already have two threads in use:
    // the UI thread and this one.
    ThreadPool tp(max(std::thread::hardware_concurrency() - 2, 1));

    for (auto it = pathvector.begin(); it != pathvector.end(); ++it)
    {
        std::wstring searchpath = *it;
        size_t       endpos     = searchpath.find_last_not_of(L" \\");
        if (std::wstring::npos != endpos)
        {
            searchpath = searchpath.substr(0, endpos + 1);
            if (searchpath[searchpath.length() - 1] == ':')
                searchpath += L"\\";
        }
        std::wstring searchRoot = searchpath;
        if (!searchpath.empty())
        {
            bool bAlwaysSearch = false;
            if (!PathIsDirectory(searchpath.c_str()))
            {
                bAlwaysSearch = true;
                searchRoot    = searchRoot.substr(0, searchRoot.find_last_of('\\'));
            }
            bool         bIsDirectory = false;
            CDirFileEnum fileEnumerator(searchpath.c_str());
            bool         bRecurse = m_bIncludeSubfolders;
            std::wstring sPath;
            while ((fileEnumerator.NextFile(sPath, &bIsDirectory, bRecurse)) && ((!m_Cancelled) || (bAlwaysSearch)))
            {
                if (bAlwaysSearch && _wcsicmp(searchpath.c_str(), sPath.c_str()))
                    bAlwaysSearch = false;
                if (m_backupandtempfiles.find(sPath) != m_backupandtempfiles.end())
                    continue;
                wcscpy_s(pathbuf.get(), MAX_PATH_NEW, sPath.c_str());
                if (!bIsDirectory)
                {
                    bool     bSearch      = false;
                    DWORD    nFileSizeLow = 0;
                    uint64_t fullfilesize = 0;
                    FILETIME ft           = {0};
                    if (bAlwaysSearch)
                    {
                        wcscpy_s(pathbuf.get(), MAX_PATH_NEW, searchpath.c_str());
                        CAutoFile hFile = CreateFile(searchpath.c_str(), FILE_READ_EA, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                        if (hFile)
                        {
                            BY_HANDLE_FILE_INFORMATION bhfi = {0};
                            GetFileInformationByHandle(hFile, &bhfi);
                            nFileSizeLow = bhfi.nFileSizeLow;
                            fullfilesize = (((uint64_t)bhfi.nFileSizeHigh) << 32) | bhfi.nFileSizeLow;
                            ft           = bhfi.ftLastWriteTime;
                        }
                    }
                    else
                    {
                        const WIN32_FIND_DATA* pFindData = fileEnumerator.GetFileInfo();
                        bSearch                          = ((m_bIncludeHidden) || ((pFindData->dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) == 0));
                        bSearch                          = bSearch && ((m_bIncludeSystem) || ((pFindData->dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) == 0));
                        nFileSizeLow                     = pFindData->nFileSizeLow;
                        fullfilesize                     = (((uint64_t)pFindData->nFileSizeHigh) << 32) | pFindData->nFileSizeLow;
                        ft                               = pFindData->ftLastWriteTime;
                        if (!m_bAllSize && bSearch)
                        {
                            switch (m_sizeCmp)
                            {
                                case 0: // less than
                                    bSearch = bSearch && (fullfilesize < m_lSize);
                                    break;
                                case 1: // equal
                                    bSearch = bSearch && (fullfilesize == m_lSize);
                                    break;
                                case 2: // greater than
                                    bSearch = bSearch && (fullfilesize > m_lSize);
                                    break;
                            }
                        }
                        if (bSearch)
                        {
                            switch (m_DateLimit + IDC_RADIO_DATE_ALL)
                            {
                                default:
                                case IDC_RADIO_DATE_ALL:
                                    break;
                                case IDC_RADIO_DATE_NEWER:
                                    bSearch = CompareFileTime(&ft, &m_Date1) >= 0;
                                    break;
                                case IDC_RADIO_DATE_OLDER:
                                    bSearch = CompareFileTime(&ft, &m_Date1) <= 0;
                                    break;
                                case IDC_RADIO_DATE_BETWEEN:
                                    bSearch = CompareFileTime(&ft, &m_Date1) >= 0;
                                    bSearch = bSearch && (CompareFileTime(&ft, &m_Date2) <= 0);
                                    break;
                            }
                        }
                    }
                    bRecurse      = ((m_bIncludeSubfolders) && (bSearch));
                    bool bPattern = MatchPath(pathbuf.get());

                    int nFound = -1;
                    if ((bSearch && bPattern) || (bAlwaysSearch))
                    {
                        CSearchInfo sinfo(pathbuf.get());
                        sinfo.filesize     = fullfilesize;
                        sinfo.modifiedtime = ft;
                        if (m_searchString.empty())
                        {
                            SendMessage(*this, SEARCH_FOUND, 1, (LPARAM)&sinfo);
                            nFound = 1;
                            SendMessage(*this, SEARCH_PROGRESS, 1, 0);
                        }
                        else
                        {
                            auto searchFN = [=]() {
                                SearchFile(sinfo, searchRoot, bAlwaysSearch, m_bIncludeBinary, m_bUseRegex, m_bCaseSensitive, m_bDotMatchesNewline, m_searchString, SearchStringutf16, &m_Cancelled);
                            };
                            tp.enqueueWait(searchFN);
                            //SearchFile(std::move(sinfo), searchRoot, bAlwaysSearch, m_bIncludeBinary, m_bUseRegex, m_bCaseSensitive, m_bDotMatchesNewline, m_searchString, SearchStringutf16, &m_Cancelled);
                        }
                    }
                    else
                        SendMessage(*this, SEARCH_PROGRESS, 0, 0);
                }
                else
                {
                    const WIN32_FIND_DATA* pFindData = fileEnumerator.GetFileInfo();
                    bool                   bSearch   = ((m_bIncludeHidden) || ((pFindData->dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) == 0));
                    bSearch                          = bSearch && ((m_bIncludeSystem) || ((pFindData->dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) == 0));
                    std::wstring relpath             = pathbuf.get();
                    relpath                          = relpath.substr(searchpath.size());
                    if (!relpath.empty())
                    {
                        if (relpath[0] == '\\')
                            relpath = relpath.substr(1);
                    }
                    bool bExcludeDir = bSearch && !m_excludedirspatternregex.empty() &&
                                       (grepWin_match_i(m_excludedirspatternregex, pFindData->cFileName) ||
                                        grepWin_match_i(m_excludedirspatternregex, pathbuf.get()) ||
                                        grepWin_match_i(m_excludedirspatternregex, relpath.c_str()));
                    bSearch  = bSearch && !bExcludeDir;
                    bRecurse = ((bIsDirectory) && (m_bIncludeSubfolders) && (bSearch));
                    if (m_searchString.empty() && m_replaceString.empty())
                    {
                        // if there's no search and replace string, include folders in the 'matched' list if they
                        // match the specified file pattern
                        if (MatchPath(pathbuf.get()))
                        {
                            if (bSearch)
                            {
                                switch (m_DateLimit + IDC_RADIO_DATE_ALL)
                                {
                                    default:
                                    case IDC_RADIO_DATE_ALL:
                                        break;
                                    case IDC_RADIO_DATE_NEWER:
                                        bSearch = CompareFileTime(&pFindData->ftLastWriteTime, &m_Date1) >= 0;
                                        break;
                                    case IDC_RADIO_DATE_OLDER:
                                        bSearch = CompareFileTime(&pFindData->ftLastWriteTime, &m_Date1) <= 0;
                                        break;
                                    case IDC_RADIO_DATE_BETWEEN:
                                        bSearch = CompareFileTime(&pFindData->ftLastWriteTime, &m_Date1) >= 0;
                                        bSearch = bSearch && (CompareFileTime(&pFindData->ftLastWriteTime, &m_Date2) <= 0);
                                        break;
                                }
                            }
                            if (bSearch)
                            {
                                CSearchInfo sinfo(pathbuf.get());
                                sinfo.modifiedtime = pFindData->ftLastWriteTime;
                                sinfo.folder       = true;
                                SendMessage(*this, SEARCH_FOUND, 1, (LPARAM)&sinfo);
                            }
                        }
                    }
                }
                bAlwaysSearch = false;
            }
        }
    }
    tp.waitFinished();
    SendMessage(*this, SEARCH_END, 0, 0);
    InterlockedExchange(&m_dwThreadRunning, FALSE);

    // refresh cursor
    POINT pt;
    GetCursorPos(&pt);
    SetCursorPos(pt.x, pt.y);

    PostMessage(m_hwnd, WM_GREPWIN_THREADEND, 0, 0);

    return 0L;
}

void CSearchDlg::SetPreset(const std::wstring& preset)
{
    CBookmarks bookmarks;
    bookmarks.Load();
    auto bk = bookmarks.GetBookmark(preset);
    if (bk.Name == preset)
    {
        auto RemoveQuotes = [](std::wstring& str) {
            if (!str.empty())
            {
                if (str[0] == '"')
                    str = str.substr(1);
                if (!str.empty())
                {
                    if (str[str.size() - 1] == '"')
                        str = str.substr(0, str.size() - 1);
                }
            }
        };
        m_searchString            = bk.Search;
        m_replaceString           = bk.Replace;
        m_bUseRegex               = bk.UseRegex;
        m_bCaseSensitive          = bk.CaseSensitive;
        m_bDotMatchesNewline      = bk.DotMatchesNewline;
        m_bCreateBackup           = bk.Backup;
        m_bUTF8                   = bk.Utf8;
        m_bForceBinary            = bk.Binary;
        m_bIncludeSystem          = bk.IncludeSystem;
        m_bIncludeSubfolders      = bk.IncludeFolder;
        m_bIncludeHidden          = bk.IncludeHidden;
        m_bIncludeBinary          = bk.IncludeBinary;
        m_excludedirspatternregex = bk.ExcludeDirs;
        m_patternregex            = bk.FileMatch;
        m_bUseRegexForPaths       = bk.FileMatchRegex;
        if (!bk.Path.empty())
            m_searchpath = bk.Path;

        m_bIncludeSystemC         = true;
        m_bIncludeHiddenC         = true;
        m_bIncludeSubfoldersC     = true;
        m_bIncludeBinaryC         = true;
        m_bCreateBackupC          = true;
        m_bCreateBackupInFoldersC = true;
        m_bUTF8C                  = true;
        m_bCaseSensitiveC         = true;
        m_bDotMatchesNewlineC     = true;

        RemoveQuotes(m_searchString);
        RemoveQuotes(m_replaceString);
        RemoveQuotes(m_excludedirspatternregex);
        RemoveQuotes(m_patternregex);
    }
}

bool CSearchDlg::MatchPath(LPCTSTR pathbuf)
{
    bool bPattern = false;
    // find start of pathname
    const auto* pName = wcsrchr(pathbuf, '\\');
    if (pName == NULL)
        pName = pathbuf;
    else
        pName++; // skip the last '\\' char
    if (m_bUseRegexForPaths)
    {
        if (m_patterns.empty())
            bPattern = true;
        else
        {
            if (grepWin_match_i(m_patternregex, pName))
                bPattern = true;
            // for a regex check, also test with the full path
            else if (grepWin_match_i(m_patternregex, pathbuf))
                bPattern = true;
        }
    }
    else
    {
        if (!m_patterns.empty())
        {
            if (m_patterns[0].size() && (m_patterns[0][0] == '-'))
                bPattern = true;

            std::wstring fname = pName;
            std::transform(fname.begin(), fname.end(), fname.begin(), ::towlower);

            for (auto it = m_patterns.begin(); it != m_patterns.end(); ++it)
            {
                if (!it->empty() && it->at(0) == '-')
                    bPattern = bPattern && !wcswildcmp(&(*it)[1], fname.c_str());
                else
                    bPattern = bPattern || wcswildcmp(it->c_str(), fname.c_str());
            }
        }
        else
            bPattern = true;
    }
    return bPattern;
}

void CSearchDlg::SearchFile(CSearchInfo sinfo, const std::wstring& searchRoot, bool bSearchAlways, bool bIncludeBinary, bool bUseRegex, bool bCaseSensitive, bool bDotMatchesNewline, const std::wstring& searchString, const std::wstring& searchStringUtf16le, volatile LONG* bCancelled)
{
    int nFound = 0;
    // we keep it simple:
    // files bigger than 30MB are considered binary. Binary files are searched
    // as if they're ANSI text files.
    std::wstring localSearchString = searchString;

    if (!bUseRegex)
    {
        SearchReplace(localSearchString, L"\\E", L"\\\\E");
        localSearchString = L"\\Q" + localSearchString + L"\\E";
    }

    SearchReplace(localSearchString, L"${filepath}", sinfo.filepath);
    std::wstring filenamefull = sinfo.filepath.substr(sinfo.filepath.find_last_of('\\') + 1);
    auto         dotpos       = filenamefull.find_last_of('.');
    if (dotpos != std::string::npos)
    {
        std::wstring filename = filenamefull.substr(0, dotpos);
        SearchReplace(localSearchString, L"${filepath}", filename);
        if (filenamefull.size() > dotpos)
        {
            std::wstring fileext = filenamefull.substr(dotpos + 1);
            SearchReplace(localSearchString, L"${fileext}", fileext);
        }
    }

    CTextFile              textfile;
    CTextFile::UnicodeType type        = CTextFile::AUTOTYPE;
    bool                   bLoadResult = false;
    if (m_bForceBinary)
    {
        type = CTextFile::BINARY;
    }
    else
    {
        ProfileTimer profile((L"file load and parse: " + sinfo.filepath).c_str());
        auto         nNullCount = bPortable ? _wtoi(g_iniFile.GetValue(L"settings", L"nullbytes", L"0")) : int(DWORD(CRegStdDWORD(L"Software\\grepWin\\nullbytes", 0)));
        if (nNullCount > 0)
        {
            constexpr __int64 oneMB = 1024 * 1024;
            auto              megs  = sinfo.filesize / oneMB;
            textfile.SetNullbyteCountForBinary(nNullCount * ((int)megs + 1));
        }
        bLoadResult = textfile.Load(sinfo.filepath.c_str(), type, m_bUTF8, bCancelled);
    }
    sinfo.encoding = type;
    if ((bLoadResult) && ((type != CTextFile::BINARY) || (bIncludeBinary) || bSearchAlways))
    {
        sinfo.readerror = false;
        std::wstring::const_iterator start, end;
        start = textfile.GetFileString().begin();
        end   = textfile.GetFileString().end();
        boost::match_results<std::wstring::const_iterator> what;
        try
        {
            int ft = boost::regex::normal;
            if (!bCaseSensitive)
                ft |= boost::regbase::icase;
            boost::wregex                                      expression = boost::wregex(localSearchString, ft);
            boost::match_results<std::wstring::const_iterator> whatc;
            boost::match_flag_type                             flags = boost::match_default | boost::format_all;
            if (!bDotMatchesNewline)
                flags |= boost::match_not_dot_newline;
            long prevlinestart = 0;
            long prevlineend   = 0;
            while (!(InterlockedExchangeAdd(bCancelled, 0)) && regex_search(start, end, whatc, expression, flags))
            {
                if (whatc[0].matched)
                {
                    nFound++;
                    if (m_bNOTSearch)
                        break;
                    long linestart = textfile.LineFromPosition(long(whatc[0].first - textfile.GetFileString().begin()));
                    long lineend   = textfile.LineFromPosition(long(whatc[0].second - textfile.GetFileString().begin()));
                    if ((linestart != prevlinestart) || (lineend != prevlineend))
                    {
                        for (long l = linestart; l <= lineend; ++l)
                        {
                            auto sLine = textfile.GetLineString(l);
                            if (m_bCaptureSearch)
                            {
                                auto out = whatc.format(m_replaceString, flags);
                                sinfo.matchlines.push_back(std::move(out));
                            }
                            else
                                sinfo.matchlines.push_back(std::move(sLine));
                            sinfo.matchlinesnumbers.push_back(l);
                        }
                    }
                    ++sinfo.matchcount;
                    prevlinestart = linestart;
                    prevlineend   = lineend;
                }
                // update search position:
                if (start == whatc[0].second)
                {
                    if (start == end)
                        break;
                    ++start;
                }
                else
                    start = whatc[0].second;
                // update flags:
                flags |= boost::match_prev_avail;
                flags |= boost::match_not_bob;
            }
            if (type == CTextFile::BINARY)
            {
                boost::wregex expressionutf16 = boost::wregex(searchStringUtf16le, ft);

                while (!(InterlockedExchangeAdd(bCancelled, 0)) && regex_search(start, end, whatc, expressionutf16, flags))
                {
                    if (whatc[0].matched)
                    {
                        nFound++;
                        if (m_bNOTSearch)
                            break;
                        long linestart = textfile.LineFromPosition(long(whatc[0].first - textfile.GetFileString().begin()));
                        long lineend   = textfile.LineFromPosition(long(whatc[0].second - textfile.GetFileString().begin()));
                        if (m_bCaptureSearch)
                        {
                            auto out = whatc.format(m_replaceString, flags);
                            sinfo.matchlines.push_back(out);
                            sinfo.matchlinesnumbers.push_back(linestart);
                        }
                        else
                        {
                            if ((linestart != prevlinestart) || (lineend != prevlineend))
                            {
                                for (long l = linestart; l <= lineend; ++l)
                                {
                                    sinfo.matchlines.push_back(textfile.GetLineString(l));
                                    sinfo.matchlinesnumbers.push_back(l);
                                }
                            }
                        }
                        ++sinfo.matchcount;
                        prevlinestart = linestart;
                        prevlineend   = lineend;
                    }
                    // update search position:
                    if (start == whatc[0].second)
                    {
                        if (start == end)
                            break;
                        ++start;
                    }
                    else
                        start = whatc[0].second;
                    // update flags:
                    flags |= boost::match_prev_avail;
                    flags |= boost::match_not_bob;
                }
            }
            if ((m_bReplace) && (nFound))
            {
                flags &= ~boost::match_prev_avail;
                flags &= ~boost::match_not_bob;
                RegexReplaceFormatter replaceFmt(m_replaceString);
                replaceFmt.SetReplacePair(L"${filepath}", sinfo.filepath);
                std::wstring filenamefullW = sinfo.filepath.substr(sinfo.filepath.find_last_of('\\') + 1);
                auto         dotposW       = filenamefullW.find_last_of('.');
                if (dotposW != std::string::npos)
                {
                    std::wstring filename = filenamefullW.substr(0, dotposW);
                    replaceFmt.SetReplacePair(L"${filename}", filename);
                    if (filenamefullW.size() > dotposW)
                    {
                        std::wstring fileext = filenamefullW.substr(dotposW + 1);
                        replaceFmt.SetReplacePair(L"${fileext}", fileext);
                    }
                }
                std::wstring replaced = regex_replace(textfile.GetFileString(), expression, replaceFmt, flags);
                if (replaced.compare(textfile.GetFileString()))
                {
                    textfile.SetFileContent(replaced);
                    if (m_bCreateBackup)
                    {
                        std::wstring backupfile     = sinfo.filepath + L".bak";
                        auto         backupInFolder = bPortable
                                                  ? (_wtoi(g_iniFile.GetValue(L"settings", L"backupinfolder", L"0")) != 0)
                                                  : (DWORD(m_regBackupInFolder) != 0);
                        if (backupInFolder)
                        {
                            std::wstring backupFolder = searchRoot + L"\\grepWin_backup\\";
                            backupFolder += sinfo.filepath.substr(searchRoot.size() + 1);
                            backupFolder = CPathUtils::GetParentDirectory(backupFolder);
                            CPathUtils::CreateRecursiveDirectory(backupFolder);
                            backupfile = backupFolder + L"\\" + CPathUtils::GetFileName(sinfo.filepath);
                        }
                        CopyFile(sinfo.filepath.c_str(), backupfile.c_str(), FALSE);
                        m_backupandtempfiles.insert(backupfile);
                    }
                    if (!textfile.Save(sinfo.filepath.c_str()))
                    {
                        // saving the file failed. Find out why...
                        DWORD err = GetLastError();
                        if (err == ERROR_ACCESS_DENIED)
                        {
                            // access denied can happen if the file has the
                            // read-only flag and/or the hidden flag set
                            // those are not situations where we should fail, so
                            // we reset those flags and restore them
                            // again after saving the file
                            DWORD origAttributes = GetFileAttributes(sinfo.filepath.c_str());
                            DWORD newAttributes  = origAttributes & (~(FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM));
                            SetFileAttributes(sinfo.filepath.c_str(), newAttributes);
                            bool bRet = textfile.Save(sinfo.filepath.c_str());
                            // restore the attributes
                            SetFileAttributes(sinfo.filepath.c_str(), origAttributes);
                            if (!bRet)
                            {
                                SendMessage(*this, SEARCH_PROGRESS, 0, 0);
                                return;
                            }
                        }
                    }
                }
            }
        }
        catch (const std::exception&)
        {
            SendMessage(*this, SEARCH_PROGRESS, 0, 0);
            return;
        }
    }
    else
    {
        if (type == CTextFile::AUTOTYPE)
        {
            sinfo.readerror = true;
            SendMessage(*this, SEARCH_FOUND, 0, (LPARAM)&sinfo);
            SendMessage(*this, SEARCH_PROGRESS, 1, 0);
            return;
        }

        // file is either too big or binary.
        // in any case, use the search function that uses a file iterator
        // instead of a string iterator to reduce the memory consumption

        if ((type != CTextFile::BINARY) || bIncludeBinary || bSearchAlways || m_bForceBinary)
        {
            sinfo.encoding        = type;
            std::string filepath  = CUnicodeUtils::StdGetANSI(sinfo.filepath);
            std::string searchfor = (type == CTextFile::ANSI) ? CUnicodeUtils::StdGetANSI(searchString) : CUnicodeUtils::StdGetUTF8(searchString);

            if (!bUseRegex)
            {
                searchfor = "\\Q";
                searchfor += CUnicodeUtils::StdGetUTF8(searchString);
                searchfor += "\\E";
            }

            boost::match_results<std::string::const_iterator> what;
            boost::match_flag_type                            flags = boost::match_default | boost::format_all;
            if (!bDotMatchesNewline)
                flags |= boost::match_not_dot_newline;
            int ft = boost::regex::normal;
            if (!bCaseSensitive)
                ft |= boost::regbase::icase;

            try
            {
                boost::regex       expression = boost::regex(searchfor, ft);
                std::vector<DWORD> matchlinesnumbers;
                bool               bFound = false;
                {
                    boost::spirit::classic::file_iterator<>                       start(filepath.c_str());
                    boost::spirit::classic::file_iterator<>                       fbeg = start;
                    boost::spirit::classic::file_iterator<>                       end  = start.make_end();
                    boost::match_results<boost::spirit::classic::file_iterator<>> whatc;
                    while (boost::regex_search(start, end, whatc, expression, flags) && !(InterlockedExchangeAdd(bCancelled, 0)))
                    {
                        nFound++;
                        if (m_bNOTSearch)
                            break;
                        matchlinesnumbers.push_back(DWORD(whatc[0].first - fbeg));
                        ++sinfo.matchcount;
                        // update search position:
                        start = whatc[0].second;
                        // update flags:
                        flags |= boost::match_prev_avail;
                        flags |= boost::match_not_bob;
                        bFound = true;
                        if ((InterlockedExchangeAdd(bCancelled, 0)))
                            break;
                    }
                }
                if (type == CTextFile::BINARY)
                {
                    boost::regex                                                  expressionUtf16le = boost::regex(CUnicodeUtils::StdGetUTF8(searchStringUtf16le), ft);
                    boost::spirit::classic::file_iterator<>                       start(filepath.c_str());
                    boost::spirit::classic::file_iterator<>                       fbeg = start;
                    boost::spirit::classic::file_iterator<>                       end  = start.make_end();
                    boost::match_results<boost::spirit::classic::file_iterator<>> whatc;
                    while (boost::regex_search(start, end, whatc, expression, flags))
                    {
                        nFound++;
                        if (m_bNOTSearch)
                            break;
                        matchlinesnumbers.push_back(DWORD(whatc[0].first - fbeg));
                        ++sinfo.matchcount;
                        // update search position:
                        start = whatc[0].second;
                        // update flags:
                        flags |= boost::match_prev_avail;
                        flags |= boost::match_not_bob;
                        bFound = true;
                        if ((InterlockedExchangeAdd(bCancelled, 0)))
                            break;
                    }
                }

                if (bFound && !(InterlockedExchangeAdd(bCancelled, 0)))
                {
                    if (!bLoadResult && (type != CTextFile::BINARY) && !m_bNOTSearch)
                    {
                        linepositions.clear();
                        // open the file and set up a vector of all lines
                        CAutoFile hFile;
                        int       retrycounter = 0;
                        do
                        {
                            if (retrycounter)
                                Sleep(20);
                            hFile = CreateFile(sinfo.filepath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                               NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
                            retrycounter++;
                        } while (!hFile && retrycounter < 5);
                        if (hFile)
                        {
                            auto   fbuf      = std::make_unique<char[]>(4096);
                            DWORD  bytesread = 0;
                            size_t pos       = 0;
                            while (ReadFile(hFile, fbuf.get(), 4096, &bytesread, NULL))
                            {
                                if (bytesread == 0)
                                    break;
                                for (DWORD br = 0; br < bytesread; ++br)
                                {
                                    if (fbuf[br] == '\r')
                                    {
                                        ++br;
                                        ++pos;
                                        if (br < bytesread)
                                        {
                                            if (fbuf[br] == '\n')
                                            {
                                                // crlf lineending
                                                auto lp            = linepositions.size();
                                                linepositions[pos] = (DWORD)lp;
                                            }
                                            else
                                            {
                                                // cr lineending
                                                auto lp                = linepositions.size();
                                                linepositions[pos - 1] = (DWORD)lp;
                                            }
                                        }
                                        else
                                            break;
                                    }
                                    else if (fbuf[br] == '\n')
                                    {
                                        // lf lineending
                                        auto lp            = linepositions.size();
                                        linepositions[pos] = (DWORD)lp;
                                    }
                                    ++pos;
                                }
                            }
                            for (size_t mp = 0; mp < matchlinesnumbers.size(); ++mp)
                            {
                                auto fp = linepositions.lower_bound(matchlinesnumbers[mp]);
                                if (fp != linepositions.end())
                                    matchlinesnumbers[mp] = fp->second;
                            }
                        }
                    }
                    sinfo.matchlinesnumbers = matchlinesnumbers;

                    if (m_bReplace)
                    {
                        std::wstring backupfile = sinfo.filepath + L".bak";
                        if (m_bCreateBackup)
                        {
                            if (DWORD(m_regBackupInFolder))
                            {
                                std::wstring backupFolder = searchRoot + L"\\grepWin_backup\\";
                                backupFolder += sinfo.filepath.substr(searchRoot.size() + 1);
                                backupFolder = CPathUtils::GetParentDirectory(backupFolder);
                                CPathUtils::CreateRecursiveDirectory(backupFolder);
                                backupfile = backupFolder + L"\\" + CPathUtils::GetFileName(sinfo.filepath);
                            }
                            CopyFile(sinfo.filepath.c_str(), backupfile.c_str(), FALSE);
                            m_backupandtempfiles.insert(backupfile);
                        }

                        flags &= ~boost::match_prev_avail;
                        flags &= ~boost::match_not_bob;
                        RegexReplaceFormatterA replaceFmt(CUnicodeUtils::StdGetUTF8(m_replaceString));
                        replaceFmt.SetReplacePair("${filepath}", CUnicodeUtils::StdGetUTF8(sinfo.filepath));
                        std::string filenamefullA = CUnicodeUtils::StdGetUTF8(sinfo.filepath.substr(sinfo.filepath.find_last_of('\\') + 1));
                        auto        dotposA       = filenamefullA.find_last_of('.');
                        if (dotposA != std::string::npos)
                        {
                            std::string filename = filenamefullA.substr(0, dotposA);
                            replaceFmt.SetReplacePair("${filename}", filename);
                            if (filenamefull.size() > dotposA)
                            {
                                std::string fileext = filenamefullA.substr(dotposA + 1);
                                replaceFmt.SetReplacePair("${fileext}", fileext);
                            }
                        }
                        std::string filepathout = m_bCreateBackup ? filepath : filepath + ".grepwinreplaced";
                        if (!m_bCreateBackup)
                            m_backupandtempfiles.insert(sinfo.filepath + L".grepwinreplaced");
                        boost::iostreams::mapped_file_source replaceinfile(m_bCreateBackup ? CUnicodeUtils::StdGetANSI(backupfile).c_str() : filepath.c_str());
                        std::ofstream                        os(filepathout.c_str(), std::ios::out | std::ios::trunc);
                        std::ostream_iterator<char, char>    out(os);
                        regex_replace(out, replaceinfile.begin(), replaceinfile.end(), expression, replaceFmt, flags);
                        os.close();
                        replaceinfile.close();
                        if (!m_bCreateBackup)
                            MoveFileExA(filepathout.c_str(), filepath.c_str(), MOVEFILE_REPLACE_EXISTING);
                    }
                }
            }
            catch (const std::exception&)
            {
                SendMessage(*this, SEARCH_PROGRESS, 0, 0);
                return;
            }
            catch (...)
            {
                SendMessage(*this, SEARCH_PROGRESS, 0, 0);
                return;
            }
        }
    }
    if (m_bNOTSearch)
    {
        if (nFound == 0)
            SendMessage(*this, SEARCH_FOUND, nFound, (LPARAM)&sinfo);
        SendMessage(*this, SEARCH_PROGRESS, (nFound >= 0), 0);
        return;
    }

    if (nFound >= 0)
        SendMessage(*this, SEARCH_FOUND, nFound, (LPARAM)&sinfo);
    SendMessage(*this, SEARCH_PROGRESS, (nFound >= 0), 0);
}

DWORD WINAPI SearchThreadEntry(LPVOID lpParam)
{
    CSearchDlg* pThis = (CSearchDlg*)lpParam;
    if (pThis)
        return pThis->SearchThread();
    return 0L;
}

void CSearchDlg::formatDate(wchar_t date_native[], const FILETIME& filetime, bool force_short_fmt)
{
    date_native[0] = '\0';

    // Convert UTC to local time
    SYSTEMTIME systemtime;
    FileTimeToSystemTime(&filetime, &systemtime);

    static TIME_ZONE_INFORMATION timeZone = {-1};
    if (timeZone.Bias == -1)
        GetTimeZoneInformation(&timeZone);

    SYSTEMTIME localsystime;
    SystemTimeToTzSpecificLocalTime(&timeZone, &systemtime, &localsystime);

    wchar_t timebuf[GREPWIN_DATEBUFFER] = {0};
    wchar_t datebuf[GREPWIN_DATEBUFFER] = {0};

    LCID locale = MAKELCID(MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), SORT_DEFAULT);

    /// reusing this instance is vital for \ref formatDate performance

    DWORD flags = force_short_fmt ? DATE_SHORTDATE : DATE_LONGDATE;

    GetDateFormat(locale, flags, &localsystime, NULL, datebuf, GREPWIN_DATEBUFFER);
    GetTimeFormat(locale, 0, &localsystime, NULL, timebuf, GREPWIN_DATEBUFFER);
    wcsncat_s(date_native, GREPWIN_DATEBUFFER, datebuf, GREPWIN_DATEBUFFER);
    wcsncat_s(date_native, GREPWIN_DATEBUFFER, L" ", GREPWIN_DATEBUFFER);
    wcsncat_s(date_native, GREPWIN_DATEBUFFER, timebuf, GREPWIN_DATEBUFFER);
}

int CSearchDlg::CheckRegex()
{
    auto buf = GetDlgItemText(IDC_SEARCHTEXT);
    int  len = (int)wcslen(buf.get());
    if (IsDlgButtonChecked(*this, IDC_REGEXRADIO) == BST_CHECKED)
    {
        // check if the regex is valid
        bool bValid = true;
        if (len)
        {
            try
            {
                std::wstring localSearchString = buf.get();
                SearchReplace(localSearchString, L"${filepath}", L"");
                SearchReplace(localSearchString, L"${filepath}", L"");
                SearchReplace(localSearchString, L"${fileext}", L"");

                boost::wregex expression = boost::wregex(localSearchString);
            }
            catch (const std::exception&)
            {
                bValid = false;
            }
        }
        if (len)
        {
            if (bValid)
            {
                SetDlgItemText(*this, IDC_REGEXOKLABEL, TranslatedString(hResource, IDS_REGEXOK).c_str());
                DialogEnableWindow(IDOK, true);
                DialogEnableWindow(IDC_REPLACE, true);
                DialogEnableWindow(IDC_CREATEBACKUP, true);
            }
            else
            {
                SetDlgItemText(*this, IDC_REGEXOKLABEL, TranslatedString(hResource, IDS_REGEXINVALID).c_str());
                DialogEnableWindow(IDOK, false);
                DialogEnableWindow(IDC_REPLACE, false);
                DialogEnableWindow(IDC_CREATEBACKUP, false);
            }
        }
        else
        {
            SetDlgItemText(*this, IDC_REGEXOKLABEL, L"");
            DialogEnableWindow(IDOK, true);
            DialogEnableWindow(IDC_REPLACE, false);
            DialogEnableWindow(IDC_CREATEBACKUP, false);
        }
    }
    else
    {
        SetDlgItemText(*this, IDC_REGEXOKLABEL, L"");
        DialogEnableWindow(IDOK, true);
        DialogEnableWindow(IDC_REPLACE, len > 0);
        DialogEnableWindow(IDC_CREATEBACKUP, len > 0);
    }

    return len;
}

void CSearchDlg::AutoSizeAllColumns()
{
    HWND    hListControl          = GetDlgItem(*this, IDC_RESULTLIST);
    auto    headerCtrl            = ListView_GetHeader(hListControl);
    int     nItemCount            = ListView_GetItemCount(hListControl);
    wchar_t textbuf[MAX_PATH * 4] = {0};
    if (headerCtrl)
    {
        int  maxcol   = Header_GetItemCount(headerCtrl) - 1;
        int  imgWidth = 0;
        auto hImgList = ListView_GetImageList(hListControl, LVSIL_SMALL);
        if ((hImgList) && (ImageList_GetImageCount(hImgList)))
        {
            IMAGEINFO imginfo;
            ImageList_GetImageInfo(hImgList, 0, &imginfo);
            imgWidth = (imginfo.rcImage.right - imginfo.rcImage.left) + 3; // 3 pixels between icon and text
        }
        for (int col = 0; col <= maxcol; col++)
        {
            HDITEM hdi     = {0};
            hdi.mask       = HDI_TEXT;
            hdi.pszText    = textbuf;
            hdi.cchTextMax = _countof(textbuf);
            Header_GetItem(headerCtrl, col, &hdi);
            int cx = ListView_GetStringWidth(hListControl, hdi.pszText) + 20; // 20 pixels for col separator and margin

            int inc = max(1, nItemCount / 1000);
            for (int index = 0; index < nItemCount; index = index + inc)
            {
                // get the width of the string and add 14 pixels for the column separator and margins
                ListView_GetItemText(hListControl, index, col, textbuf, _countof(textbuf));
                int linewidth = ListView_GetStringWidth(hListControl, textbuf) + 14;
                // add the image size
                if (col == 0)
                    linewidth += imgWidth;
                if (cx < linewidth)
                    cx = linewidth;
            }
            ListView_SetColumnWidth(hListControl, col, cx);
        }
    }
}

int CSearchDlg::GetSelectedListIndex(int index)
{
    bool filelist = (IsDlgButtonChecked(*this, IDC_RESULTFILES) == BST_CHECKED);
    if (filelist)
        return index;
    auto tup = m_listItems[index];
    return std::get<0>(tup);
}

bool CSearchDlg::FailedShowMessage(HRESULT hr)
{
    if (FAILED(hr))
    {
        _com_error err(hr);
        MessageBox(nullptr, L"grepWin", err.ErrorMessage(), MB_ICONERROR);
        return true;
    }
    return false;
}

void CSearchDlg::CheckForUpdates(bool force)
{
    bool bNewerAvailable = false;
    // check for newer versions
    bool doCheck = true;
    if (bPortable)
        doCheck = !!_wtoi(g_iniFile.GetValue(L"global", L"CheckForUpdates", L"1"));
    else
        doCheck = !!DWORD(CRegStdDWORD(L"Software\\grepWin\\CheckForUpdates", 1));
    if (doCheck)
    {
        time_t now;
        time(&now);
        time_t last = 0;
        if (bPortable)
        {
            last = _wtoll(g_iniFile.GetValue(L"global", L"CheckForUpdatesLast", L"0"));
        }
        else
        {
            last = _wtoll(((std::wstring)CRegStdString(L"Software\\grepWin\\CheckForUpdatesLast", L"0")).c_str());
        }
        double days = std::difftime(now, last) / (60LL * 60LL * 24LL);
        if ((days >= 7.0) || force)
        {
            std::wstring tempfile = CTempFiles::Instance().GetTempFilePath(true);

            std::wstring sCheckURL = L"https://raw.githubusercontent.com/stefankueng/grepWin/main/version.txt";
            HRESULT      res       = URLDownloadToFile(nullptr, sCheckURL.c_str(), tempfile.c_str(), 0, nullptr);
            if (res == S_OK)
            {
                if (bPortable)
                {
                    g_iniFile.SetValue(L"global", L"CheckForUpdatesLast", std::to_wstring(now).c_str());
                }
                else
                {
                    auto regLast = CRegStdString(L"Software\\grepWin\\CheckForUpdatesLast", L"0");
                    regLast      = std::to_wstring(now);
                }
                std::ifstream File;
                File.open(tempfile.c_str());
                if (File.good())
                {
                    char line[1024];
                    File.getline(line, sizeof(line));
                    auto verLine    = CUnicodeUtils::StdGetUnicode(line);
                    bNewerAvailable = IsVersionNewer(verLine);
                    File.getline(line, sizeof(line));
                    auto updateurl = CUnicodeUtils::StdGetUnicode(line);
                    if (bNewerAvailable)
                    {
                        if (bPortable)
                        {
                            g_iniFile.SetValue(L"global", L"CheckForUpdatesVersion", verLine.c_str());
                            g_iniFile.SetValue(L"global", L"CheckForUpdatesUrl", updateurl.c_str());
                        }
                        else
                        {
                            auto regVersion   = CRegStdString(L"Software\\grepWin\\CheckForUpdatesVersion", L"");
                            regVersion        = verLine;
                            auto regUpdateUrl = CRegStdString(L"Software\\grepWin\\CheckForUpdatesUrl", L"");
                            regUpdateUrl      = updateurl;
                        }
                        ShowUpdateAvailable();
                    }
                }
                File.close();
                DeleteFile(tempfile.c_str());
            }
        }
    }
}

void CSearchDlg::ShowUpdateAvailable()
{
    std::wstring sVersion;
    std::wstring updateUrl;
    if (bPortable)
    {
        sVersion  = g_iniFile.GetValue(L"global", L"CheckForUpdatesVersion", L"");
        updateUrl = g_iniFile.GetValue(L"global", L"CheckForUpdatesUrl", L"");
    }
    else
    {
        sVersion  = CRegStdString(L"Software\\grepWin\\CheckForUpdatesVersion", L"");
        updateUrl = CRegStdString(L"Software\\grepWin\\CheckForUpdatesUrl", L"");
    }
    if (IsVersionNewer(sVersion))
    {
        auto sUpdateAvailable = TranslatedString(hResource, IDS_UPDATEAVAILABLE);
        sUpdateAvailable      = CStringUtils::Format(sUpdateAvailable.c_str(), sVersion.c_str());
        auto sLinkText        = CStringUtils::Format(L"<a href=\"%s\">%s</a>", updateUrl.c_str(), sUpdateAvailable.c_str());
        SetDlgItemText(*this, IDC_UPDATELINK, sLinkText.c_str());
        ShowWindow(GetDlgItem(*this, IDC_UPDATELINK), SW_SHOW);
    }
}

bool CSearchDlg::IsVersionNewer(const std::wstring& sVer)
{
    int major = 0;
    int minor = 0;
    int micro = 0;
    int build = 0;

    const wchar_t* pLine = sVer.c_str();

    major = _wtoi(pLine);
    pLine = wcschr(pLine, '.');
    if (pLine)
    {
        pLine++;
        minor = _wtoi(pLine);
        pLine = wcschr(pLine, '.');
        if (pLine)
        {
            pLine++;
            micro = _wtoi(pLine);
            pLine = wcschr(pLine, '.');
            if (pLine)
            {
                pLine++;
                build = _wtoi(pLine);
            }
        }
    }
    bool isNewer = false;
    if (major > GREPWIN_VERMAJOR)
        isNewer = true;
    else if ((minor > GREPWIN_VERMINOR) && (major == GREPWIN_VERMAJOR))
        isNewer = true;
    else if ((micro > GREPWIN_VERMICRO) && (minor == GREPWIN_VERMINOR) && (major == GREPWIN_VERMAJOR))
        isNewer = true;
    else if ((build > GREPWIN_VERBUILD) && (micro == GREPWIN_VERMICRO) && (minor == GREPWIN_VERMINOR) && (major == GREPWIN_VERMAJOR))
        isNewer = true;
    return isNewer;
}
