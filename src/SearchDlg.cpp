// grepWin - regex search and replace for Windows

// Copyright (C) 2007-2023 - Stefan Kueng

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
#include "SearchDlg.h"
#include "AboutDlg.h"
#include "BookmarksDlg.h"
#include "BrowseFolder.h"
#include "COMPtrs.h"
#include "DarkModeHelper.h"
#include "DebugOutput.h"
#include "DirFileEnum.h"
#include "DPIAware.h"
#include "DropFiles.h"
#include "Language.h"
#include "LineData.h"
#include "Monitor.h"
#include "MultiLineEditDlg.h"
#include "NameDlg.h"
#include "OnOutOfScope.h"
#include "PathUtils.h"
#include "PreserveChdir.h"
#include "RegexReplaceFormatter.h"
#include "RegexTestDlg.h"
#include "Registry.h"
#include "resource.h"
#include "ResString.h"
#include "SearchInfo.h"
#include "Settings.h"
#include "ShellContextMenu.h"
#include "SmartHandle.h"
#include "StringUtils.h"
#include "SysImageList.h"
#include "TempFile.h"
#include "TextFile.h"
#include "Theme.h"
#include "ThreadPool.h"
#include "UnicodeUtils.h"
#include "version.h"

#include <algorithm>
#include <Commdlg.h>
#include <fstream>
#include <iterator>
#include <map>
#include <numeric>
#include <ranges>
#include <string>

#pragma warning(push)
#pragma warning(disable : 4996) // warning STL4010: Various members of std::allocator are deprecated in C++17
#include <boost/regex.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/spirit/include/classic_file_iterator.hpp>
#pragma warning(pop)

#define GREPWIN_DATEBUFFER 100
#define LABELUPDATETIMER   10

constexpr auto          SearchEditSubclassID = 4321;

DWORD WINAPI            SearchThreadEntry(LPVOID lpParam);

// ReSharper disable once CppInconsistentNaming
UINT                    CSearchDlg::m_grepwinStartupmsg = RegisterWindowMessage(L"grepWin_StartupMessage");
std::map<size_t, DWORD> linePositions;

extern ULONGLONG        g_startTime;
extern HANDLE           hInitProtection;

void                    drawRedEditBox(HWND hWnd, WPARAM wParam)
{
    // make the border of the edit control red in case
    // the regex is invalid
    HDC hdc = nullptr;
    if (wParam == NULLREGION)
        hdc = GetDC(hWnd);
    else
        hdc = GetDCEx(hWnd, reinterpret_cast<HRGN>(wParam), DCX_WINDOW | DCX_INTERSECTRGN);
    RECT rc = {0};
    GetWindowRect(hWnd, &rc);
    MapWindowPoints(nullptr, hWnd, reinterpret_cast<LPPOINT>(&rc), 2);
    ::SetBkColor(hdc, RGB(255, 0, 0));
    ::ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &rc, nullptr, 0, nullptr);
    ReleaseDC(hWnd, hdc);
}

LRESULT CALLBACK SearchEditWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR /*uIdSubclass*/, DWORD_PTR dwRefData)
{
    switch (uMsg)
    {
        case WM_NCPAINT:
        {
            auto searchDlg = reinterpret_cast<CSearchDlg*>(dwRefData);
            if (!searchDlg->isRegexValid())
            {
                drawRedEditBox(hWnd, wParam);
                return 0;
            }
        }
        default:
            break;
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK ExcludeDirEditWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR /*uIdSubclass*/, DWORD_PTR dwRefData)
{
    switch (uMsg)
    {
        case WM_NCPAINT:
        {
            auto searchDlg = reinterpret_cast<CSearchDlg*>(dwRefData);
            if (!searchDlg->isExcludeDirsRegexValid())
            {
                drawRedEditBox(hWnd, wParam);
                return 0;
            }
        }
        default:
            break;
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK FileNameMatchEditWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR /*uIdSubclass*/, DWORD_PTR dwRefData)
{
    switch (uMsg)
    {
        case WM_NCPAINT:
        {
            auto searchDlg = reinterpret_cast<CSearchDlg*>(dwRefData);
            if (!searchDlg->isFileNameMatchRegexValid())
            {
                drawRedEditBox(hWnd, wParam);
                return 0;
            }
        }
        default:
            break;
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

CSearchDlg::CSearchDlg(HWND hParent)
    : m_hParent(hParent)
    , m_dwThreadRunning(FALSE)
    , m_cancelled(FALSE)
    , m_bBlockUpdate(false)
    , m_bookmarksDlg(nullptr)
    , m_patternRegexC(false)
    , m_excludeDirsPatternRegexC(false)
    , m_bUseRegex(false)
    , m_bUseRegexC(false)
    , m_bUseRegexForPaths(false)
    , m_bAllSize(false)
    , m_lSize(0)
    , m_sizeCmp(0)
    , m_bIncludeSystem(false)
    , m_bIncludeSystemC(false)
    , m_bIncludeHidden(false)
    , m_bIncludeHiddenC(false)
    , m_bIncludeSubfolders(false)
    , m_bIncludeSubfoldersC(false)
    , m_bIncludeSymLinks(false)
    , m_bIncludeSymLinksC(false)
    , m_bIncludeBinary(false)
    , m_bIncludeBinaryC(false)
    , m_bCreateBackup(false)
    , m_bCreateBackupC(false)
    , m_bCreateBackupInFolders(false)
    , m_bCreateBackupInFoldersC(false)
    , m_bKeepFileDate(false)
    , m_bKeepFileDateC(false)
    , m_bWholeWords(false)
    , m_bWholeWordsC(false)
    , m_bUTF8(false)
    , m_bUTF8C(false)
    , m_bForceBinary(false)
    , m_bCaseSensitive(false)
    , m_bCaseSensitiveC(false)
    , m_bDotMatchesNewline(false)
    , m_bDotMatchesNewlineC(false)
    , m_bNotSearch(false)
    , m_bCaptureSearch(false)
    , m_bSizeC(false)
    , m_endDialog(false)
    , m_executeImmediately(ExecuteAction::None)
    , m_dateLimit(0)
    , m_bDateLimitC(false)
    , m_date1({0})
    , m_date2({0})
    , m_bNoSaveSettings(false)
    , m_bReplace(false)
    , m_bConfirmationOnReplace(true)
    , m_showContent(false)
    , m_showContentSet(false)
    , m_totalItems(0)
    , m_searchedItems(0)
    , m_totalMatches(0)
    , m_selectedItems(0)
    , m_bAscending(true)
    , m_isRegexValid(true)
    , m_isExcludeDirsRegexValid(true)
    , m_isFileNameMatchingRegexValid(true)
    , m_themeCallbackId(0)
    , m_pDropTarget(nullptr)
    , m_autoCompleteFilePatterns(bPortable ? &g_iniFile : nullptr)
    , m_autoCompleteExcludeDirsPatterns(bPortable ? &g_iniFile : nullptr)
    , m_autoCompleteSearchPatterns(bPortable ? &g_iniFile : nullptr)
    , m_autoCompleteReplacePatterns(bPortable ? &g_iniFile : nullptr)
    , m_autoCompleteSearchPaths(bPortable ? &g_iniFile : nullptr)
    , m_regUseRegex(L"Software\\grepWin\\UseRegex", 1)
    , m_regAllSize(L"Software\\grepWin\\AllSize")
    , m_regSize(L"Software\\grepWin\\Size", L"2000")
    , m_regSizeCombo(L"Software\\grepWin\\SizeCombo", 0)
    , m_regIncludeSystem(L"Software\\grepWin\\IncludeSystem")
    , m_regIncludeHidden(L"Software\\grepWin\\IncludeHidden")
    , m_regIncludeSubfolders(L"Software\\grepWin\\IncludeSubfolders", 1)
    , m_regIncludeSymLinks(L"Software\\grepWin\\IncludeSymLinks", 0)
    , m_regIncludeBinary(L"Software\\grepWin\\IncludeBinary", 1)
    , m_regCreateBackup(L"Software\\grepWin\\CreateBackup")
    , m_regKeepFileDate(L"Software\\grepWin\\KeepFileDate")
    , m_regWholeWords(L"Software\\grepWin\\WholeWords")
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
{
    if (FAILED(CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(m_pTaskbarList.GetAddressOf()))))
        m_pTaskbarList = nullptr;
    else if (m_pTaskbarList)
        m_pTaskbarList->HrInit();
}

CSearchDlg::~CSearchDlg()
{
    if (m_pDropTarget)
        delete m_pDropTarget;
}

bool CSearchDlg::isRegexValid() const
{
    return m_isRegexValid;
}

bool CSearchDlg::isExcludeDirsRegexValid() const
{
    return m_isExcludeDirsRegexValid;
}

bool CSearchDlg::isFileNameMatchRegexValid() const
{
    return m_isFileNameMatchingRegexValid;
}

LRESULT CSearchDlg::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == m_grepwinStartupmsg)
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

            m_autoCompleteFilePatterns.Load(L"Software\\grepWin\\History", L"FilePattern");
            m_autoCompleteFilePatterns.Init(GetDlgItem(hwndDlg, IDC_PATTERN));
            m_autoCompleteExcludeDirsPatterns.Load(L"Software\\grepWin\\History", L"ExcludeDirsPattern");
            m_autoCompleteExcludeDirsPatterns.Init(GetDlgItem(hwndDlg, IDC_EXCLUDEDIRSPATTERN));
            m_autoCompleteSearchPatterns.Load(L"Software\\grepWin\\History", L"SearchPattern");
            m_autoCompleteSearchPatterns.Init(GetDlgItem(hwndDlg, IDC_SEARCHTEXT));
            m_autoCompleteReplacePatterns.Load(L"Software\\grepWin\\History", L"ReplacePattern");
            m_autoCompleteReplacePatterns.Init(GetDlgItem(hwndDlg, IDC_REPLACETEXT));
            m_autoCompleteSearchPaths.Load(L"Software\\grepWin\\History", L"SearchPaths");
            m_autoCompleteSearchPaths.Init(GetDlgItem(hwndDlg, IDC_SEARCHPATH));

            m_themeCallbackId = CTheme::Instance().RegisterThemeChangeCallback(
                [this]() {
                    auto bDark = CTheme::Instance().IsDarkTheme();
                    DarkModeHelper::Instance().AllowDarkModeForApp(bDark);
                    CTheme::Instance().SetThemeForDialog(*this, bDark);
                    DarkModeHelper::Instance().AllowDarkModeForWindow(GetToolTipHWND(), bDark);
                    DarkModeHelper::Instance().RefreshTitleBarThemeColor(*this, bDark);
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
            AddToolTip(IDC_NEWINSTANCE, TranslatedString(hResource, IDS_NEWINSTANCE_TT).c_str());
            AddToolTip(IDC_PATTERN, TranslatedString(hResource, IDS_PATTERN_TT).c_str());
            AddToolTip(IDC_EXCLUDEDIRSPATTERN, TranslatedString(hResource, IDS_EXCLUDEDIR_TT).c_str());
            AddToolTip(IDC_SEARCHPATH, TranslatedString(hResource, IDS_SEARCHPATH_TT).c_str());
            AddToolTip(IDC_DOTMATCHNEWLINE, TranslatedString(hResource, IDS_DOTMATCHNEWLINE_TT).c_str());
            AddToolTip(IDC_SEARCHTEXT, TranslatedString(hResource, IDS_SEARCHTEXT_TT).c_str());
            AddToolTip(IDC_EDITMULTILINE1, TranslatedString(hResource, IDS_EDITMULTILINE_TT).c_str());
            AddToolTip(IDC_EDITMULTILINE2, TranslatedString(hResource, IDS_EDITMULTILINE_TT).c_str());
            AddToolTip(IDC_EXPORT, TranslatedString(hResource, IDS_EXPORT_TT).c_str());
            AddToolTip(IDC_SEARCHPATHMULTILINEEDIT, TranslatedString(hResource, IDS_EDITMULTILINE_TT).c_str());
            AddToolTip(IDOK, TranslatedString(hResource, IDS_SHIFT_NOTSEARCH).c_str());
            AddToolTip(IDC_PATHMRU, TranslatedString(hResource, IDS_OPEN_MRU).c_str());
            AddToolTip(IDC_EXCLUDEDIRMRU, TranslatedString(hResource, IDS_OPEN_MRU).c_str());
            AddToolTip(IDC_PATTERNMRU, TranslatedString(hResource, IDS_OPEN_MRU).c_str());
            AddToolTip(IDC_REPLACETEXT, LPSTR_TEXTCALLBACK);

            SetWindowSubclass(GetDlgItem(*this, IDC_SEARCHTEXT), SearchEditWndProc, SearchEditSubclassID, reinterpret_cast<DWORD_PTR>(this));
            SetWindowSubclass(GetDlgItem(*this, IDC_EXCLUDEDIRSPATTERN), ExcludeDirEditWndProc, SearchEditSubclassID, reinterpret_cast<DWORD_PTR>(this));
            SetWindowSubclass(GetDlgItem(*this, IDC_PATTERN), FileNameMatchEditWndProc, SearchEditSubclassID, reinterpret_cast<DWORD_PTR>(this));

            if (m_searchPath.empty())
            {
                if (bPortable)
                    m_searchPath = g_iniFile.GetValue(L"global", L"searchpath", L"");
                else
                    m_searchPath = std::wstring(m_regSearchPath);
            }
            else
            {
                // expand a possible 'short' path
                DWORD ret = 0;
                ret       = ::GetLongPathName(m_searchPath.c_str(), nullptr, 0);
                if (ret)
                {
                    auto pathBuf = std::make_unique<wchar_t[]>(ret + 2LL);
                    ret          = ::GetLongPathName(m_searchPath.c_str(), pathBuf.get(), ret + 1);
                    m_searchPath = std::wstring(pathBuf.get(), ret);
                }
            }

            if (m_patternRegex.empty() && !m_patternRegexC)
            {
                if (bPortable)
                {
                    m_patternRegex      = g_iniFile.GetValue(L"global", L"pattern", L"");
                    m_bUseRegexForPaths = !!_wtoi(g_iniFile.GetValue(L"global", L"UseFileMatchRegex", L""));
                }
                else
                {
                    m_patternRegex      = std::wstring(m_regPattern);
                    m_bUseRegexForPaths = !!static_cast<DWORD>(m_regUseRegexForPaths);
                }
            }
            if (m_excludeDirsPatternRegex.empty() && !m_excludeDirsPatternRegexC)
            {
                if (bPortable)
                    m_excludeDirsPatternRegex = g_iniFile.GetValue(L"global", L"ExcludeDirsPattern", L"");
                else
                    m_excludeDirsPatternRegex = std::wstring(m_regExcludeDirsPattern);
            }
            // initialize the controls
            SetDlgItemText(hwndDlg, IDC_SEARCHPATH, m_searchPath.c_str());
            SetDlgItemText(hwndDlg, IDC_SEARCHTEXT, m_searchString.c_str());
            SetDlgItemText(hwndDlg, IDC_EXCLUDEDIRSPATTERN, m_excludeDirsPatternRegex.c_str());
            SetDlgItemText(hwndDlg, IDC_PATTERN, m_patternRegex.c_str());
            SetDlgItemText(hwndDlg, IDC_REPLACETEXT, m_replaceString.c_str());

            // the path edit control should work as a drop target for files and folders
            HWND hSearchPath = GetDlgItem(hwndDlg, IDC_SEARCHPATH);
            m_pDropTarget    = new CFileDropTarget(hSearchPath);
            RegisterDragDrop(hSearchPath, m_pDropTarget);
            // create the supported formats:
            FORMATETC ftEtc = {0};
            ftEtc.cfFormat  = CF_TEXT;
            ftEtc.dwAspect  = DVASPECT_CONTENT;
            ftEtc.lindex    = -1;
            ftEtc.tymed     = TYMED_HGLOBAL;
            m_pDropTarget->AddSuportedFormat(ftEtc);
            ftEtc.cfFormat = CF_HDROP;
            m_pDropTarget->AddSuportedFormat(ftEtc);
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
                    InsertMenu(hSysMenu, menuItemsCount - 2, MF_STRING | MF_BYPOSITION, ID_CLONE, TranslatedString(hResource, IDS_CLONE).c_str());
                    InsertMenu(hSysMenu, menuItemsCount - 2, MF_SEPARATOR | MF_BYPOSITION, NULL, nullptr);
                }
                else
                {
                    AppendMenu(hSysMenu, MF_SEPARATOR, NULL, nullptr);
                    AppendMenu(hSysMenu, MF_STRING, ID_CLONE, TranslatedString(hResource, IDS_CLONE).c_str());
                    AppendMenu(hSysMenu, MF_STRING, ID_ABOUTBOX, TranslatedString(hResource, IDS_ABOUT).c_str());
                }
            }

            wchar_t buf[MAX_PATH] = {0};
            if (m_bSizeC && (m_lSize != static_cast<uint64_t>(-1)))
            {
                swprintf_s(buf, _countof(buf), L"%I64u", m_lSize);
                SetDlgItemText(hwndDlg, IDC_SIZEEDIT, buf);
            }
            else
            {
                uint64_t s = _wtoll(std::wstring(m_regSize).c_str());
                if (bPortable)
                    s = _wtoi(g_iniFile.GetValue(L"global", L"size", L"2000"));
                swprintf_s(buf, _countof(buf), L"%I64u", s);
                SetDlgItemText(hwndDlg, IDC_SIZEEDIT, buf);
            }

            SendDlgItemMessage(hwndDlg, IDC_SIZECOMBO, CB_INSERTSTRING, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(static_cast<LPCWSTR>(TranslatedString(hResource, IDS_LESSTHAN).c_str())));
            SendDlgItemMessage(hwndDlg, IDC_SIZECOMBO, CB_INSERTSTRING, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(static_cast<LPCWSTR>(TranslatedString(hResource, IDS_EQUALTO).c_str())));
            SendDlgItemMessage(hwndDlg, IDC_SIZECOMBO, CB_INSERTSTRING, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(static_cast<LPCWSTR>(TranslatedString(hResource, IDS_GREATERTHAN).c_str())));
            if (!m_bIncludeSubfoldersC)
                m_bIncludeSubfolders = bPortable ? !!_wtoi(g_iniFile.GetValue(L"global", L"IncludeSubfolders", L"1")) : !!static_cast<DWORD>(m_regIncludeSubfolders);
            if (!m_bIncludeSymLinksC)
                m_bIncludeSymLinks = bPortable ? !!_wtoi(g_iniFile.GetValue(L"global", L"IncludeSymLinks", L"0")) : !!static_cast<DWORD>(m_regIncludeSymLinks);
            if (!m_bIncludeSystemC)
                m_bIncludeSystem = bPortable ? !!_wtoi(g_iniFile.GetValue(L"global", L"IncludeSystem", L"1")) : !!static_cast<DWORD>(m_regIncludeSystem);
            if (!m_bIncludeHiddenC)
                m_bIncludeHidden = bPortable ? !!_wtoi(g_iniFile.GetValue(L"global", L"IncludeHidden", L"0")) : !!static_cast<DWORD>(m_regIncludeHidden);
            if (!m_bIncludeBinaryC)
                m_bIncludeBinary = bPortable ? !!_wtoi(g_iniFile.GetValue(L"global", L"IncludeBinary", L"0")) : !!static_cast<DWORD>(m_regIncludeBinary);
            if (!m_bCaseSensitiveC)
                m_bCaseSensitive = bPortable ? !!_wtoi(g_iniFile.GetValue(L"global", L"CaseSensitive", L"0")) : !!static_cast<DWORD>(m_regCaseSensitive);
            if (!m_bDotMatchesNewlineC)
                m_bDotMatchesNewline = bPortable ? !!_wtoi(g_iniFile.GetValue(L"global", L"DotMatchesNewline", L"0")) : !!static_cast<DWORD>(m_regDotMatchesNewline);
            if (!m_bCreateBackupC)
                m_bCreateBackup = bPortable ? !!_wtoi(g_iniFile.GetValue(L"global", L"CreateBackup", L"0")) : !!static_cast<DWORD>(m_regCreateBackup);
            if (!m_bKeepFileDateC)
                m_bKeepFileDate = bPortable ? !!_wtoi(g_iniFile.GetValue(L"global", L"KeepFileDate", L"0")) : !!static_cast<DWORD>(m_regKeepFileDate);
            if (!m_bWholeWordsC)
                m_bWholeWords = bPortable ? !!_wtoi(g_iniFile.GetValue(L"global", L"WholeWords", L"0")) : !!static_cast<DWORD>(m_regWholeWords);
            if (!m_bUTF8C)
            {
                m_bUTF8        = bPortable ? !!_wtoi(g_iniFile.GetValue(L"global", L"UTF8", L"0")) : !!static_cast<DWORD>(m_regUTF8);
                m_bForceBinary = bPortable ? !!_wtoi(g_iniFile.GetValue(L"global", L"Binary", L"0")) : !!static_cast<DWORD>(m_regBinary);
            }
            if (!m_bDotMatchesNewlineC)
                m_bDotMatchesNewline = bPortable ? !!_wtoi(g_iniFile.GetValue(L"global", L"DotMatchesNewline", L"0")) : !!static_cast<DWORD>(m_regDotMatchesNewline);
            if (!m_bSizeC)
            {
                m_bAllSize = bPortable ? !!_wtoi(g_iniFile.GetValue(L"global", L"AllSize", L"0")) : !!static_cast<DWORD>(m_regAllSize);
                m_sizeCmp  = bPortable ? _wtoi(g_iniFile.GetValue(L"global", L"SizeCombo", L"0")) : static_cast<int>(static_cast<DWORD>(m_regSizeCombo));
            }
            if (!m_bDateLimitC)
            {
                m_dateLimit            = bPortable ? _wtoi(g_iniFile.GetValue(L"global", L"DateLimit", L"0")) : static_cast<int>(static_cast<DWORD>(m_regDateLimit));
                m_date1.dwLowDateTime  = bPortable ? wcstoul(g_iniFile.GetValue(L"global", L"Date1Low", L"0"), nullptr, 10) : static_cast<DWORD>(m_regDate1Low);
                m_date1.dwHighDateTime = bPortable ? wcstoul(g_iniFile.GetValue(L"global", L"Date1High", L"0"), nullptr, 10) : static_cast<DWORD>(m_regDate1High);
                m_date2.dwLowDateTime  = bPortable ? wcstoul(g_iniFile.GetValue(L"global", L"Date2Low", L"0"), nullptr, 10) : static_cast<DWORD>(m_regDate2Low);
                m_date2.dwHighDateTime = bPortable ? wcstoul(g_iniFile.GetValue(L"global", L"Date2High", L"0"), nullptr, 10) : static_cast<DWORD>(m_regDate2High);
            }
            else if (m_date1.dwHighDateTime == 0 && m_date1.dwLowDateTime == 0)
            {
                // use the current date as default
                SYSTEMTIME st{};
                FILETIME   ft{};
                GetSystemTime(&st);
                SystemTimeToFileTime(&st, &ft);
                m_date1 = ft;
                m_date2 = ft;
            }

            SendDlgItemMessage(hwndDlg, IDC_SIZECOMBO, CB_SETCURSEL, m_sizeCmp, 0);

            SendDlgItemMessage(hwndDlg, IDC_INCLUDESUBFOLDERS, BM_SETCHECK, m_bIncludeSubfolders ? BST_CHECKED : BST_UNCHECKED, 0);
            SendDlgItemMessage(hwndDlg, IDC_INCLUDESYMLINK, BM_SETCHECK, m_bIncludeSymLinks ? BST_CHECKED : BST_UNCHECKED, 0);
            SendDlgItemMessage(hwndDlg, IDC_CREATEBACKUP, BM_SETCHECK, m_bCreateBackup ? BST_CHECKED : BST_UNCHECKED, 0);
            SendDlgItemMessage(hwndDlg, IDC_KEEPFILEDATECHECK, BM_SETCHECK, m_bKeepFileDate ? BST_CHECKED : BST_UNCHECKED, 0);
            SendDlgItemMessage(hwndDlg, IDC_UTF8, BM_SETCHECK, m_bUTF8 ? BST_CHECKED : BST_UNCHECKED, 0);
            SendDlgItemMessage(hwndDlg, IDC_BINARY, BM_SETCHECK, m_bForceBinary ? BST_CHECKED : BST_UNCHECKED, 0);
            SendDlgItemMessage(hwndDlg, IDC_INCLUDESYSTEM, BM_SETCHECK, m_bIncludeSystem ? BST_CHECKED : BST_UNCHECKED, 0);
            SendDlgItemMessage(hwndDlg, IDC_INCLUDEHIDDEN, BM_SETCHECK, m_bIncludeHidden ? BST_CHECKED : BST_UNCHECKED, 0);
            SendDlgItemMessage(hwndDlg, IDC_INCLUDEBINARY, BM_SETCHECK, m_bIncludeBinary ? BST_CHECKED : BST_UNCHECKED, 0);
            SendDlgItemMessage(hwndDlg, IDC_CASE_SENSITIVE, BM_SETCHECK, m_bCaseSensitive ? BST_CHECKED : BST_UNCHECKED, 0);
            SendDlgItemMessage(hwndDlg, IDC_DOTMATCHNEWLINE, BM_SETCHECK, m_bDotMatchesNewline ? BST_CHECKED : BST_UNCHECKED, 0);

            CheckRadioButton(hwndDlg, IDC_REGEXRADIO, IDC_TEXTRADIO, (bPortable ? _wtoi(g_iniFile.GetValue(L"global", L"UseRegex", L"0")) : static_cast<DWORD>(m_regUseRegex)) ? IDC_REGEXRADIO : IDC_TEXTRADIO);
            CheckRadioButton(hwndDlg, IDC_ALLSIZERADIO, IDC_SIZERADIO, m_bAllSize ? IDC_ALLSIZERADIO : IDC_SIZERADIO);
            CheckRadioButton(hwndDlg, IDC_FILEPATTERNREGEX, IDC_FILEPATTERNTEXT, m_bUseRegexForPaths ? IDC_FILEPATTERNREGEX : IDC_FILEPATTERNTEXT);
            SendDlgItemMessage(hwndDlg, IDC_WHOLEWORDS, BM_SETCHECK, m_bWholeWords ? BST_CHECKED : BST_UNCHECKED, 0);
            DialogEnableWindow(IDC_WHOLEWORDS, IsDlgButtonChecked(hwndDlg, IDC_TEXTRADIO));
            if (!m_searchString.empty() || m_bUseRegexC)
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
                    m_showContent = static_cast<DWORD>(m_regShowContent) != 0;
                }
            }
            CheckRadioButton(*this, IDC_RESULTFILES, IDC_RESULTCONTENT, m_showContent ? IDC_RESULTCONTENT : IDC_RESULTFILES);

            CheckRadioButton(hwndDlg, IDC_RADIO_DATE_ALL, IDC_RADIO_DATE_BETWEEN, m_dateLimit + IDC_RADIO_DATE_ALL);
            SYSTEMTIME sysTime{};
            auto       hTime1 = GetDlgItem(hwndDlg, IDC_DATEPICK1);
            FileTimeToSystemTime(&m_date1, &sysTime);
            DateTime_SetSystemtime(hTime1, GDT_VALID, &sysTime);
            auto hTime2 = GetDlgItem(hwndDlg, IDC_DATEPICK2);
            FileTimeToSystemTime(&m_date2, &sysTime);
            DateTime_SetSystemtime(hTime2, GDT_VALID, &sysTime);
            ShowWindow(GetDlgItem(*this, IDC_DATEPICK2), (m_dateLimit == IDC_RADIO_DATE_BETWEEN - IDC_RADIO_DATE_ALL) ? SW_SHOW : SW_HIDE);
            ShowWindow(GetDlgItem(*this, IDC_DATEPICK1), (m_dateLimit != 0) ? SW_SHOW : SW_HIDE);

            SetFocus(GetDlgItem(hwndDlg, IDC_SEARCHTEXT));

            AdjustControlSize(IDC_BINARY);
            AdjustControlSize(IDC_UTF8);

            AdjustControlSize(IDC_REGEXRADIO);
            AdjustControlSize(IDC_TEXTRADIO);
            AdjustControlSize(IDC_WHOLEWORDS);
            AdjustControlSize(IDC_CASE_SENSITIVE);
            AdjustControlSize(IDC_DOTMATCHNEWLINE);
            AdjustControlSize(IDC_CREATEBACKUP);
            AdjustControlSize(IDC_UTF8);
            AdjustControlSize(IDC_BINARY);
            AdjustControlSize(IDC_KEEPFILEDATECHECK);
            AdjustControlSize(IDC_ALLSIZERADIO);
            AdjustControlSize(IDC_RADIO_DATE_ALL);
            AdjustControlSize(IDC_SIZERADIO);
            AdjustControlSize(IDC_RADIO_DATE_NEWER);
            AdjustControlSize(IDC_INCLUDESYSTEM);
            AdjustControlSize(IDC_INCLUDEHIDDEN);
            AdjustControlSize(IDC_RADIO_DATE_OLDER);
            AdjustControlSize(IDC_INCLUDESUBFOLDERS);
            AdjustControlSize(IDC_INCLUDESYMLINK);
            AdjustControlSize(IDC_INCLUDEBINARY);
            AdjustControlSize(IDC_RADIO_DATE_BETWEEN);
            AdjustControlSize(IDC_FILEPATTERNREGEX);
            AdjustControlSize(IDC_FILEPATTERNTEXT);
            AdjustControlSize(IDC_RESULTFILES);
            AdjustControlSize(IDC_RESULTCONTENT);

            m_resizer.Init(hwndDlg);
            m_resizer.UseSizeGrip(!CTheme::Instance().IsDarkTheme());
            m_resizer.AddControl(hwndDlg, IDC_HELPLABEL, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_ABOUTLINK, RESIZER_TOPRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_GROUPSEARCHIN, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_PATHMRU, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_SEARCHPATH, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_NEWINSTANCE, RESIZER_TOPRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_SEARCHPATHMULTILINEEDIT, RESIZER_TOPRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_SEARCHPATHBROWSE, RESIZER_TOPRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_GROUPSEARCHFOR, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_REGEXRADIO, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_TEXTRADIO, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_WHOLEWORDS, RESIZER_TOPLEFT);
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
            m_resizer.AddControl(hwndDlg, IDC_KEEPFILEDATECHECK, RESIZER_TOPLEFT);
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
            m_resizer.AddControl(hwndDlg, IDC_INCLUDESYMLINK, RESIZER_TOPLEFT);
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

            WINDOWPLACEMENT wpl       = {0};
            DWORD           size      = sizeof(wpl);
            std::wstring    winPosKey = L"windowpos_" + GetMonitorSetupHash();
            if (bPortable)
            {
                std::wstring sPos = g_iniFile.GetValue(L"global", winPosKey.c_str(), L"");

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
                if (SHGetValue(HKEY_CURRENT_USER, L"Software\\grepWin", winPosKey.c_str(), REG_NONE, &wpl, &size) == ERROR_SUCCESS)
                    SetWindowPlacement(*this, &wpl);
                else
                    ShowWindow(*this, SW_SHOW);
            }
            InitResultList();

            bool doCheck = true;
            if (bPortable)
                doCheck = !!_wtoi(g_iniFile.GetValue(L"global", L"CheckForUpdates", L"1"));
            else
                doCheck = !!static_cast<DWORD>(CRegStdDWORD(L"Software\\grepWin\\CheckForUpdates", 1));
            if (doCheck)
            {
                m_updateCheckThread = std::move(std::thread([&]() { CheckForUpdates(); }));
                ShowUpdateAvailable();
            }

            if (hInitProtection)
                CloseHandle(hInitProtection);
            hInitProtection = nullptr;

            switch (m_executeImmediately)
            {
                case ExecuteAction::Search:
                    DoCommand(IDOK, 0);
                    break;
                case ExecuteAction::Replace:
                    DoCommand(IDC_REPLACE, 0);
                    break;
                case ExecuteAction::Capture:
                    DoCommand(IDC_CAPTURESEARCH, 0);
                    break;
                case ExecuteAction::None:
                default:
                    break;
            }
            std::locale::global(std::locale(""));
        }
            return FALSE;
        case WM_CLOSE:
        {
            if (m_updateCheckThread.joinable())
                m_updateCheckThread.join();
            if (m_dwThreadRunning)
                m_cancelled = true;
            else
            {
                SaveSettings();
                if (!m_bNoSaveSettings)
                {
                    m_autoCompleteFilePatterns.Save();
                    m_autoCompleteSearchPatterns.Save();
                    m_autoCompleteReplacePatterns.Save();
                    m_autoCompleteSearchPaths.Save();
                }
                EndDialog(*this, IDCANCEL);
            }
        }
        break;
        case WM_DESTROY:
            RemoveWindowSubclass(*this, SearchEditWndProc, SearchEditSubclassID);
            CTheme::Instance().RemoveRegisteredCallback(m_themeCallbackId);
            break;
        case WM_COMMAND:
            return DoCommand(LOWORD(wParam), HIWORD(wParam));
        case WM_CONTEXTMENU:
        {
            ShowContextMenu(reinterpret_cast<HWND>(wParam), GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        }
        break;
        case WM_NOTIFY:
        {
            if (reinterpret_cast<LPNMHDR>(lParam)->code == TTN_GETDISPINFO)
            {
                auto lpnmtdi           = reinterpret_cast<LPNMTTDISPINFOW>(lParam);
                auto buf               = GetDlgItemText(IDC_REPLACETEXT);
                m_toolTipReplaceString = ExpandString(buf.get());
                lpnmtdi->lpszText      = m_toolTipReplaceString.data();
            }
            switch (wParam)
            {
                case IDC_RESULTLIST:
                    DoListNotify(reinterpret_cast<LPNMITEMACTIVATE>(lParam));
                    break;
                case IDOK:
                    switch (reinterpret_cast<LPNMHDR>(lParam)->code)
                    {
                        case BCN_DROPDOWN:
                        {
                            const NMBCDROPDOWN* pDropDown = reinterpret_cast<NMBCDROPDOWN*>(lParam);
                            // Get screen coordinates of the button.
                            POINT               pt;
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
                        default:
                            break;
                    }
                    break;
                case IDC_UPDATELINK:
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
                        default:
                            break;
                    }
                    break;
                case IDC_ABOUTLINK:
                    switch (reinterpret_cast<LPNMHDR>(lParam)->code)
                    {
                        case NM_CLICK:
                        case NM_RETURN:
                        {
                            PNMLINK pNMLink = reinterpret_cast<PNMLINK>(lParam);
                            LITEM   item    = pNMLink->item;
                            if (item.iLink == 0)
                            {
                                CAboutDlg dlgAbout(*this);
                                dlgAbout.DoModal(hResource, IDD_ABOUT, *this);
                            }
                            break;
                        }
                        default:
                            break;
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
            MINMAXINFO* mmi       = reinterpret_cast<MINMAXINFO*>(lParam);
            mmi->ptMinTrackSize.x = m_resizer.GetDlgRectScreen()->right;
            mmi->ptMinTrackSize.y = m_resizer.GetDlgRectScreen()->bottom;
            return 0;
        }
        case WM_DPICHANGED:
        {
            const RECT* rect = reinterpret_cast<RECT*>(lParam);
            SetWindowPos(*this, nullptr, rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
            ::RedrawWindow(*this, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE | RDW_ERASE | RDW_INTERNALPAINT | RDW_ALLCHILDREN | RDW_UPDATENOW);
        }
        break;
        case WM_SETCURSOR:
        {
            if (m_dwThreadRunning && LOWORD(lParam) == 1)
            {
                SetCursor(LoadCursor(nullptr, IDC_APPSTARTING));
                return TRUE;
            }
            return FALSE;
        }
        case SEARCH_START:
        {
            m_totalItems    = 0;
            m_searchedItems = 0;
            m_totalMatches  = 0;
            m_selectedItems = 0;
            UpdateInfoLabel();
            // reset the sort indicator
            HDITEM hd         = {0};
            hd.mask           = HDI_FORMAT;
            HWND hListControl = GetDlgItem(*this, IDC_RESULTLIST);
            HWND hHeader      = ListView_GetHeader(hListControl);
            int  iCount       = Header_GetItemCount(hHeader);
            for (int i = 0; i < iCount; ++i)
            {
                Header_GetItem(hHeader, i, &hd);
                hd.fmt &= ~(HDF_SORTDOWN | HDF_SORTUP);
                Header_SetItem(hHeader, i, &hd);
            }

            SetTimer(*this, LABELUPDATETIMER, 200, nullptr);
        }
        break;
        case SEARCH_FOUND:
        {
            auto searchInfo = reinterpret_cast<CSearchInfo*>(lParam);
            m_totalMatches += static_cast<int>(searchInfo->matchCount);
            if ((wParam != 0) || (m_searchString.empty()) || searchInfo->readError || !searchInfo->exception.empty() || m_bNotSearch)
            {
                AddFoundEntry(searchInfo);
            }
        }
        break;
        case SEARCH_PROGRESS:
        {
            if (wParam)
                m_searchedItems++;
            m_totalItems++;
        }
        break;
        case SEARCH_END:
        {
            AddFoundEntry(nullptr, true);
            AutoSizeAllColumns();
            UpdateInfoLabel();
            ::SetDlgItemText(*this, IDOK, TranslatedString(hResource, IDS_SEARCH).c_str());
            DialogEnableWindow(IDC_RESULTFILES, true);
            DialogEnableWindow(IDC_RESULTCONTENT, true);
            ShowWindow(GetDlgItem(*this, IDC_PROGRESS), SW_HIDE);
            SendDlgItemMessage(*this, IDC_PROGRESS, PBM_SETMARQUEE, 0, 0);
            if (m_pTaskbarList)
                m_pTaskbarList->SetProgressState(*this, TBPF_NOPROGRESS);
            ShowWindow(GetDlgItem(*this, IDC_EXPORT), m_items.empty() ? SW_HIDE : SW_SHOW);
            KillTimer(*this, LABELUPDATETIMER);
        }
        break;
        case WM_TIMER:
        {
            if (wParam == LABELUPDATETIMER)
            {
                AddFoundEntry(nullptr, true);
                UpdateInfoLabel();
            }
        }
        break;
        case WM_HELP:
        {
            if (m_rtfDialog == nullptr)
            {
                m_rtfDialog = std::make_unique<CInfoRtfDialog>();
            }
            m_rtfDialog->ShowModeless(g_hInst, *this, "grepWin help", IDR_INFODLG, L"RTF", IDI_GREPWIN, 400, 600);
            // ensure that the dialog is not too big and always visible on the screen
            RECT dlgRect{};
            GetWindowRect(*this, &dlgRect);
            WINDOWPLACEMENT placement{};
            placement.length           = sizeof(WINDOWPLACEMENT);
            placement.showCmd          = SW_SHOW;
            placement.rcNormalPosition = dlgRect;
            auto quarterWidth          = (dlgRect.right - dlgRect.left) / 4;
            placement.rcNormalPosition.left += quarterWidth;
            placement.rcNormalPosition.right -= quarterWidth;
            SetWindowPlacement(*m_rtfDialog, &placement);
        }
        break;
        case WM_SYSCOMMAND:
        {
            switch (wParam & 0xFFFF)
            {
                case ID_ABOUTBOX:
                {
                    CAboutDlg dlgAbout(*this);
                    dlgAbout.DoModal(hResource, IDD_ABOUT, *this);
                }
                break;
                case ID_CLONE:
                {
                    CloneWindow();
                }
                break;
                default:
                    break;
            }
        }
        break;
        case WM_COPYDATA:
        {
            if (lParam)
            {
                PCOPYDATASTRUCT pCopyData = reinterpret_cast<PCOPYDATASTRUCT>(lParam);
                std::wstring    newPath   = std::wstring(static_cast<LPCTSTR>(pCopyData->lpData), pCopyData->cbData / sizeof(wchar_t));
                if (!newPath.empty())
                {
                    auto buf     = GetDlgItemText(IDC_SEARCHPATH);
                    m_searchPath = buf.get();

                    if (wParam == 1)
                        m_searchPath.clear();
                    else
                        m_searchPath += L"|";
                    m_searchPath += newPath;
                    SetDlgItemText(hwndDlg, IDC_SEARCHPATH, m_searchPath.c_str());
                    g_startTime = GetTickCount64();
                }
            }
            return TRUE;
        }
        case WM_EDITDBLCLICK:
        {
            switch (wParam)
            {
                case IDC_PATTERN:
                {
                    m_autoCompleteFilePatterns.SetOptions(ACO_UPDOWNKEYDROPSLIST | ACO_AUTOSUGGEST | ACO_NOPREFIXFILTERING);
                    ::SetFocus(GetDlgItem(*this, IDC_PATTERN));
                    SendDlgItemMessage(*this, IDC_PATTERN, WM_KEYDOWN, VK_DOWN, 0);
                }
                break;
                case IDC_EXCLUDEDIRSPATTERN:
                {
                    m_autoCompleteExcludeDirsPatterns.SetOptions(ACO_UPDOWNKEYDROPSLIST | ACO_AUTOSUGGEST | ACO_NOPREFIXFILTERING);
                    ::SetFocus(GetDlgItem(*this, IDC_EXCLUDEDIRSPATTERN));
                    SendDlgItemMessage(*this, IDC_EXCLUDEDIRSPATTERN, WM_KEYDOWN, VK_DOWN, 0);
                }
                break;
                case IDC_SEARCHTEXT:
                {
                    m_autoCompleteSearchPatterns.SetOptions(ACO_UPDOWNKEYDROPSLIST | ACO_AUTOSUGGEST | ACO_NOPREFIXFILTERING);
                    ::SetFocus(GetDlgItem(*this, IDC_SEARCHTEXT));
                    SendDlgItemMessage(*this, IDC_SEARCHTEXT, WM_KEYDOWN, VK_DOWN, 0);
                }
                break;
                case IDC_REPLACETEXT:
                {
                    m_autoCompleteReplacePatterns.SetOptions(ACO_UPDOWNKEYDROPSLIST | ACO_AUTOSUGGEST | ACO_NOPREFIXFILTERING);
                    ::SetFocus(GetDlgItem(*this, IDC_REPLACETEXT));
                    SendDlgItemMessage(*this, IDC_REPLACETEXT, WM_KEYDOWN, VK_DOWN, 0);
                }
                break;
                case IDC_SEARCHPATH:
                {
                    m_autoCompleteSearchPaths.SetOptions(ACO_UPDOWNKEYDROPSLIST | ACO_AUTOSUGGEST | ACO_NOPREFIXFILTERING);
                    ::SetFocus(GetDlgItem(*this, IDC_SEARCHPATH));
                    SendDlgItemMessage(*this, IDC_SEARCHPATH, WM_KEYDOWN, VK_DOWN, 0);
                }
                break;
                default:
                    break;
            }
            return TRUE;
        }
        case WM_GREPWIN_THREADEND:
        {
            if (m_endDialog)
                EndDialog(m_hwnd, IDOK);
        }
        break;
        case WM_BOOKMARK:
        {
            if (m_bookmarksDlg)
            {
                m_searchString            = m_bookmarksDlg->GetSelectedSearchString();
                m_replaceString           = m_bookmarksDlg->GetSelectedReplaceString();
                m_bUseRegex               = m_bookmarksDlg->GetSelectedUseRegex();

                m_bCaseSensitive          = m_bookmarksDlg->GetSelectedSearchCase();
                m_bDotMatchesNewline      = m_bookmarksDlg->GetSelectedDotMatchNewline();
                m_bCreateBackup           = m_bookmarksDlg->GetSelectedBackup();
                m_bKeepFileDate           = m_bookmarksDlg->GetSelectedKeepFileDate();
                m_bWholeWords             = m_bookmarksDlg->GetSelectedWholeWords();
                m_bUTF8                   = m_bookmarksDlg->GetSelectedTreatAsUtf8();
                m_bForceBinary            = m_bookmarksDlg->GetSelectedTreatAsBinary();
                m_bIncludeSystem          = m_bookmarksDlg->GetSelectedIncludeSystem();
                m_bIncludeSubfolders      = m_bookmarksDlg->GetSelectedIncludeFolder();
                m_bIncludeSymLinks        = m_bookmarksDlg->GetSelectedIncludeSymLinks();
                m_bIncludeHidden          = m_bookmarksDlg->GetSelectedIncludeHidden();
                m_bIncludeBinary          = m_bookmarksDlg->GetSelectedIncludeBinary();
                m_excludeDirsPatternRegex = m_bookmarksDlg->GetSelectedExcludeDirs();
                m_patternRegex            = m_bookmarksDlg->GetSelectedFileMatch();
                m_bUseRegexForPaths       = m_bookmarksDlg->GetSelectedFileMatchRegex();
                if (!m_bookmarksDlg->GetPath().empty())
                {
                    m_searchPath = m_bookmarksDlg->GetPath();
                    SetDlgItemText(*this, IDC_SEARCHPATH, m_searchPath.c_str());
                }

                SetDlgItemText(*this, IDC_SEARCHTEXT, m_searchString.c_str());
                SetDlgItemText(*this, IDC_REPLACETEXT, m_replaceString.c_str());
                CheckRadioButton(*this, IDC_REGEXRADIO, IDC_TEXTRADIO, m_bUseRegex ? IDC_REGEXRADIO : IDC_TEXTRADIO);
                DialogEnableWindow(IDC_TESTREGEX, !IsDlgButtonChecked(*this, IDC_TEXTRADIO));

                SendDlgItemMessage(*this, IDC_INCLUDESUBFOLDERS, BM_SETCHECK, m_bIncludeSubfolders ? BST_CHECKED : BST_UNCHECKED, 0);
                SendDlgItemMessage(*this, IDC_INCLUDESYMLINK, BM_SETCHECK, m_bIncludeSymLinks ? BST_CHECKED : BST_UNCHECKED, 0);
                SendDlgItemMessage(*this, IDC_CREATEBACKUP, BM_SETCHECK, m_bCreateBackup ? BST_CHECKED : BST_UNCHECKED, 0);
                SendDlgItemMessage(*this, IDC_KEEPFILEDATECHECK, BM_SETCHECK, m_bKeepFileDate ? BST_CHECKED : BST_UNCHECKED, 0);
                SendDlgItemMessage(*this, IDC_UTF8, BM_SETCHECK, m_bUTF8 ? BST_CHECKED : BST_UNCHECKED, 0);
                SendDlgItemMessage(*this, IDC_BINARY, BM_SETCHECK, m_bForceBinary ? BST_CHECKED : BST_UNCHECKED, 0);
                SendDlgItemMessage(*this, IDC_INCLUDESYSTEM, BM_SETCHECK, m_bIncludeSystem ? BST_CHECKED : BST_UNCHECKED, 0);
                SendDlgItemMessage(*this, IDC_INCLUDEHIDDEN, BM_SETCHECK, m_bIncludeHidden ? BST_CHECKED : BST_UNCHECKED, 0);
                SendDlgItemMessage(*this, IDC_INCLUDEBINARY, BM_SETCHECK, m_bIncludeBinary ? BST_CHECKED : BST_UNCHECKED, 0);
                SendDlgItemMessage(*this, IDC_CASE_SENSITIVE, BM_SETCHECK, m_bCaseSensitive ? BST_CHECKED : BST_UNCHECKED, 0);
                SendDlgItemMessage(*this, IDC_DOTMATCHNEWLINE, BM_SETCHECK, m_bDotMatchesNewline ? BST_CHECKED : BST_UNCHECKED, 0);
                SendDlgItemMessage(*this, IDC_WHOLEWORDS, BM_SETCHECK, m_bWholeWords ? BST_CHECKED : BST_UNCHECKED, 0);

                CheckRadioButton(*this, IDC_FILEPATTERNREGEX, IDC_FILEPATTERNTEXT, m_bUseRegexForPaths ? IDC_FILEPATTERNREGEX : IDC_FILEPATTERNTEXT);
                SetDlgItemText(*this, IDC_EXCLUDEDIRSPATTERN, m_excludeDirsPatternRegex.c_str());
                SetDlgItemText(*this, IDC_PATTERN, m_patternRegex.c_str());
                DialogEnableWindow(IDC_WHOLEWORDS, IsDlgButtonChecked(hwndDlg, IDC_TEXTRADIO));
                CheckRegex();
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
                m_cancelled = true;
            }
            else
            {
                ::SetFocus(GetDlgItem(*this, IDOK));
                if (!SaveSettings())
                    break;

                CStringUtils::rtrim(m_searchPath, L"\\/");
                SearchReplace(m_searchPath, L"/", L"\\");

                if (PathIsRelative(m_searchPath.c_str()))
                {
                    ShowEditBalloon(IDC_SEARCHPATH, TranslatedString(hResource, IDS_ERR_INVALID_PATH).c_str(), TranslatedString(hResource, IDS_ERR_RELATIVEPATH).c_str());
                    break;
                }
                std::vector<std::wstring> searchPaths;
                stringtok(searchPaths, m_searchPath, true);
                for (const auto& sp : searchPaths)
                {
                    if (!PathFileExists(sp.c_str()))
                    {
                        auto sErr = TranslatedString(hResource, IDS_ERR_PATHNOTEXIST);
                        sErr      = CStringUtils::Format(sErr.c_str(), sp.c_str());
                        ShowEditBalloon(IDC_SEARCHPATH, TranslatedString(hResource, IDS_ERR_INVALID_PATH).c_str(), sErr.c_str());
                        break;
                    }
                }

                if ((id == IDC_SEARCHINFOUNDFILES) && (!m_items.empty()))
                {
                    m_searchPath.clear();
                    for (const auto& item : m_items)
                    {
                        if (!m_searchPath.empty())
                            m_searchPath += L"|";
                        m_searchPath += item.filePath;
                    }
                }

                m_searchedItems = 0;
                m_totalItems    = 0;

                ShowWindow(GetDlgItem(*this, IDC_EXPORT), SW_HIDE);
                m_items.clear();
                m_listItems.clear();
                m_listItems.reserve(500000);
                m_backupAndTempFiles.clear();

                HWND hListControl = GetDlgItem(*this, IDC_RESULTLIST);
                ListView_SetItemCount(hListControl, 0);
                DialogEnableWindow(IDC_RESULTFILES, false);
                DialogEnableWindow(IDC_RESULTCONTENT, false);

                m_autoCompleteFilePatterns.AddEntry(m_patternRegex.c_str());
                m_autoCompleteExcludeDirsPatterns.AddEntry(m_excludeDirsPatternRegex.c_str());
                m_autoCompleteSearchPatterns.AddEntry(m_searchString.c_str());
                m_autoCompleteReplacePatterns.AddEntry(m_replaceString.c_str());
                m_autoCompleteSearchPaths.AddEntry(m_searchPath.c_str());

                if (!m_bNoSaveSettings)
                {
                    m_autoCompleteFilePatterns.Save();
                    m_autoCompleteSearchPatterns.Save();
                    m_autoCompleteReplacePatterns.Save();
                    m_autoCompleteSearchPaths.Save();
                }

                m_bReplace = id == IDC_REPLACE;

                if (m_bReplace && !m_bCreateBackup && (m_bConfirmationOnReplace || m_replaceString.empty()))
                {
                    auto noWarnIfNoBackup = bPortable ? !!_wtoi(g_iniFile.GetValue(L"settings", L"nowarnifnobackup", L"0")) : static_cast<DWORD>(CRegStdDWORD(L"Software\\grepWin\\nowarnifnobackup", FALSE));
                    if (!noWarnIfNoBackup)
                    {
                        auto msgText = CStringUtils::Format(static_cast<LPCWSTR>(TranslatedString(hResource, IDS_REPLACECONFIRM).c_str()),
                                                            m_searchString.c_str(),
                                                            m_replaceString.empty() ? static_cast<LPCWSTR>(TranslatedString(hResource, IDS_ANEMPTYSTRING).c_str()) : m_replaceString.c_str());
                        if (::MessageBox(*this, msgText.c_str(), L"grepWin", MB_ICONQUESTION | MB_YESNO) != IDYES)
                        {
                            break;
                        }
                    }
                }
                if (m_bReplace && m_bUTF8)
                {
                    auto utf8OptionText = GetDlgItemText(IDC_UTF8);
                    auto msgText        = CStringUtils::Format(TranslatedString(hResource, IDS_REPLACEUTF8).c_str(),
                                                               utf8OptionText.get());
                    if (::MessageBox(*this, msgText.c_str(), L"grepWin", MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) != IDYES)
                    {
                        break;
                    }
                }
                m_bConfirmationOnReplace = true;
                m_bNotSearch             = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                if (id == IDC_INVERSESEARCH)
                    m_bNotSearch = true;
                m_bCaptureSearch = false;
                if (id == IDC_CAPTURESEARCH)
                {
                    m_bCaptureSearch = true;
                    m_bNotSearch     = false;
                    m_bReplace       = false;
                }
                if (m_bReplace)
                {
                    m_replaceString = ExpandString(m_replaceString);
                }

                if (m_searchString.empty() || m_bNotSearch)
                {
                    // switch to file view
                    CheckRadioButton(*this, IDC_RESULTFILES, IDC_RESULTCONTENT, IDC_RESULTFILES);
                    m_showContent = false;
                    InitResultList();
                }

                m_dwThreadRunning = true;
                m_cancelled       = false;
                SetDlgItemText(*this, IDOK, TranslatedString(hResource, IDS_STOP).c_str());
                ShowWindow(GetDlgItem(*this, IDC_PROGRESS), SW_SHOW);
                SendDlgItemMessage(*this, IDC_PROGRESS, PBM_SETMARQUEE, 1, 0);
                if (m_pTaskbarList)
                    m_pTaskbarList->SetProgressState(*this, TBPF_INDETERMINATE);
                // now start the thread which does the searching
                DWORD  dwThreadId = 0;
                HANDLE hThread    = CreateThread(nullptr, // no security attribute
                                                 0,       // default stack size
                                                 SearchThreadEntry,
                                                 static_cast<LPVOID>(this), // thread parameter
                                                 0,                         // not suspended
                                                 &dwThreadId);              // returns thread ID
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
            bool escClose = !!static_cast<DWORD>(CRegStdDWORD(L"Software\\grepWin\\escclose", FALSE));
            if (bPortable)
                escClose = !!_wtoi(g_iniFile.GetValue(L"settings", L"escclose", L"0"));
            if (escClose)
            {
                if (m_dwThreadRunning)
                    m_cancelled = true;
                else
                {
                    SaveSettings();
                    if (!m_bNoSaveSettings)
                    {
                        m_autoCompleteFilePatterns.Save();
                        m_autoCompleteSearchPatterns.Save();
                        m_autoCompleteReplacePatterns.Save();
                        m_autoCompleteSearchPaths.Save();
                    }
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
        case IDC_NEWINSTANCE:
            CloneWindow();
            break;
        case IDC_SEARCHPATHMULTILINEEDIT:
        {
            auto paths = std::wstring(GetDlgItemText(IDC_SEARCHPATH).get());

            SearchReplace(paths, L"|", L"\r\n");
            CMultiLineEditDlg editDlg(*this);
            editDlg.SetString(paths);

            if (editDlg.DoModal(hResource, IDD_MULTILINEEDIT, *this) == IDOK)
            {
                std::wstring text = editDlg.GetSearchString();
                SearchReplace(text, L"\r\n", L"|");
                SetDlgItemText(*this, IDC_SEARCHPATH, text.c_str());
            }
            ::SetFocus(GetDlgItem(*this, IDC_SEARCHPATH));
        }
        break;
        case IDC_SEARCHPATHBROWSE:
        {
            CBrowseFolder browse;

            auto          path = GetDlgItemText(IDC_SEARCHPATH);
            if (!PathFileExists(path.get()))
            {
                auto ptr = wcsstr(path.get(), L"|");
                if (ptr)
                    *ptr = 0;
                else
                    path.get()[0] = 0;
            }
            if (wcsstr(path.get(), L"..") != nullptr)
            {
                ShowEditBalloon(IDC_SEARCHPATH, TranslatedString(hResource, IDS_ERR_INVALID_PATH).c_str(), TranslatedString(hResource, IDS_ERR_RELATIVEPATH).c_str());
                break;
            }

            std::vector<std::wstring> paths;
            browse.SetInfo(TranslatedString(hResource, IDS_SELECTPATHTOSEARCH).c_str());
            if (browse.Show(*this, paths, m_searchPath) == CBrowseFolder::RetVal::Ok)
            {
                std::wstring pathString;
                for (const auto& selPath : paths)
                {
                    if (pathString.empty())
                        pathString = selPath;
                    else
                    {
                        pathString += L"|";
                        pathString += selPath;
                    }
                }
                SetDlgItemText(*this, IDC_SEARCHPATH, pathString.c_str());
                m_searchPath = pathString;
            }
        }
        break;
        case IDC_SEARCHPATH:
        {
            if (msg == EN_CHANGE)
            {
                if (m_autoCompleteSearchPaths.GetOptions() & ACO_NOPREFIXFILTERING)
                    m_autoCompleteSearchPaths.SetOptions(ACO_UPDOWNKEYDROPSLIST | ACO_AUTOSUGGEST);
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
                DialogEnableWindow(IDC_INCLUDESYMLINK, bIsDir);
                DialogEnableWindow(IDC_INCLUDEBINARY, bIsDir && len > 0);
                DialogEnableWindow(IDC_PATTERN, bIsDir);
                DialogEnableWindow(IDC_EXCLUDEDIRSPATTERN, bIsDir || bIncludeSubfolders);
                DialogEnableWindow(IDC_FILEPATTERNREGEX, bIsDir);
                DialogEnableWindow(IDC_FILEPATTERNTEXT, bIsDir);

                // change the dialog title to "grepWin : search/path"
                wchar_t compactPath[100] = {0};
                PathCompactPathEx(compactPath, buf.get(), 40, 0);
                wchar_t titleBuf[MAX_PATH] = {0};
                swprintf_s(titleBuf, _countof(titleBuf), L"grepWin : %s", compactPath);
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
                if (m_autoCompleteSearchPatterns.GetOptions() & ACO_NOPREFIXFILTERING)
                    m_autoCompleteSearchPatterns.SetOptions(ACO_UPDOWNKEYDROPSLIST | ACO_AUTOSUGGEST);
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
        case IDC_FILEPATTERNREGEX:
        case IDC_FILEPATTERNTEXT:
        {
            CheckRegex();
            DialogEnableWindow(IDC_TESTREGEX, !IsDlgButtonChecked(*this, IDC_TEXTRADIO));
            DialogEnableWindow(IDC_WHOLEWORDS, IsDlgButtonChecked(*this, IDC_TEXTRADIO));
        }
        break;
        case IDC_ADDTOBOOKMARKS:
        {
            auto buf                  = GetDlgItemText(IDC_SEARCHTEXT);
            m_searchString            = buf.get();
            buf                       = GetDlgItemText(IDC_REPLACETEXT);
            m_replaceString           = buf.get();
            buf                       = GetDlgItemText(IDC_EXCLUDEDIRSPATTERN);
            m_excludeDirsPatternRegex = buf.get();
            buf                       = GetDlgItemText(IDC_PATTERN);
            m_patternRegex            = buf.get();
            bool     bUseRegex        = (IsDlgButtonChecked(*this, IDC_REGEXRADIO) == BST_CHECKED);

            CNameDlg nameDlg(*this);
            if (nameDlg.DoModal(hResource, IDD_NAME, *this) == IDOK)
            {
                // add the bookmark
                CBookmarks bks;
                Bookmark   bk;
                bk.Name              = nameDlg.GetName();
                bk.Path              = nameDlg.IncludePath() ? m_searchPath : L"";
                bk.Search            = m_searchString;
                bk.Replace           = m_replaceString;
                bk.UseRegex          = bUseRegex;
                bk.CaseSensitive     = (IsDlgButtonChecked(*this, IDC_CASE_SENSITIVE) == BST_CHECKED);
                bk.DotMatchesNewline = (IsDlgButtonChecked(*this, IDC_DOTMATCHNEWLINE) == BST_CHECKED);
                bk.Backup            = (IsDlgButtonChecked(*this, IDC_CREATEBACKUP) == BST_CHECKED);
                bk.KeepFileDate      = (IsDlgButtonChecked(*this, IDC_KEEPFILEDATECHECK) == BST_CHECKED);
                bk.Utf8              = (IsDlgButtonChecked(*this, IDC_UTF8) == BST_CHECKED);
                bk.IncludeSystem     = (IsDlgButtonChecked(*this, IDC_INCLUDESYSTEM) == BST_CHECKED);
                bk.IncludeFolder     = (IsDlgButtonChecked(*this, IDC_INCLUDESUBFOLDERS) == BST_CHECKED);
                bk.IncludeSymLinks   = (IsDlgButtonChecked(*this, IDC_INCLUDESYMLINK) == BST_CHECKED);
                bk.IncludeHidden     = (IsDlgButtonChecked(*this, IDC_INCLUDEHIDDEN) == BST_CHECKED);
                bk.IncludeBinary     = (IsDlgButtonChecked(*this, IDC_INCLUDEBINARY) == BST_CHECKED);
                bk.ExcludeDirs       = m_excludeDirsPatternRegex;
                bk.FileMatch         = m_patternRegex;
                bk.FileMatchRegex    = (IsDlgButtonChecked(*this, IDC_FILEPATTERNREGEX) == BST_CHECKED);
                bks.Load();
                bks.AddBookmark(bk);
                bks.Save();
            }
        }
        break;
        case IDC_BOOKMARKS:
        {
            if (!m_bookmarksDlg)
                m_bookmarksDlg = std::make_unique<CBookmarksDlg>(*this);
            else
                m_bookmarksDlg->InitBookmarks();
            m_bookmarksDlg->ShowModeless(hResource, IDD_BOOKMARKS, *this);
        }
        break;
        case IDC_RESULTFILES:
        case IDC_RESULTCONTENT:
        {
            InitResultList();
            FillResultList();
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
            auto         buf      = GetDlgItemText(static_cast<int>(uID));
            std::wstring ctrlText = buf.get();

            // replace all \r\n strings with real CRLFs
            try
            {
                int                    ft         = boost::regex::normal;
                boost::wregex          expression = (id == IDC_EDITMULTILINE1
                                                         ? boost::wregex(L"\\(\\?:\\\\n\\|\\\\r\\\\n\\|\\\\n\\\\r\\)", ft)
                                                         : boost::wregex(L"\\\\r\\\\n", ft));
                boost::match_flag_type rFlags     = boost::match_default | boost::format_all;
                ctrlText                          = regex_replace(ctrlText, expression, L"\\r\\n", rFlags);
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
                    int                    ft         = boost::regex::normal;
                    boost::wregex          expression = boost::wregex(L"\\r\\n", ft);
                    boost::match_flag_type rFlags     = boost::match_default | boost::format_all;
                    if (id == IDC_EDITMULTILINE1)
                        text = regex_replace(text, expression, L"\\(\\?:\\\\n|\\\\r\\\\n|\\\\n\\\\r\\)", rFlags);
                    else
                        text = regex_replace(text, expression, L"\\\\r\\\\n", rFlags);
                }
                catch (const std::exception&)
                {
                }

                SetDlgItemText(*this, static_cast<int>(uID), text.c_str());
            }
            ::SetFocus(GetDlgItem(*this, uID));
        }
        break;
        case IDC_PATHMRU:
        {
            m_autoCompleteSearchPaths.SetOptions(ACO_UPDOWNKEYDROPSLIST | ACO_AUTOSUGGEST | ACO_NOPREFIXFILTERING);
            ::SetFocus(GetDlgItem(*this, IDC_SEARCHPATH));
            SendDlgItemMessage(*this, IDC_SEARCHPATH, WM_KEYDOWN, VK_DOWN, 0);
        }
        break;
        case IDC_EXCLUDEDIRMRU:
        {
            m_autoCompleteExcludeDirsPatterns.SetOptions(ACO_UPDOWNKEYDROPSLIST | ACO_AUTOSUGGEST | ACO_NOPREFIXFILTERING);
            ::SetFocus(GetDlgItem(*this, IDC_EXCLUDEDIRSPATTERN));
            SendDlgItemMessage(*this, IDC_EXCLUDEDIRSPATTERN, WM_KEYDOWN, VK_DOWN, 0);
        }
        break;
        case IDC_PATTERNMRU:
        {
            m_autoCompleteFilePatterns.SetOptions(ACO_UPDOWNKEYDROPSLIST | ACO_AUTOSUGGEST | ACO_NOPREFIXFILTERING);
            ::SetFocus(GetDlgItem(*this, IDC_PATTERN));
            SendDlgItemMessage(*this, IDC_PATTERN, WM_KEYDOWN, VK_DOWN, 0);
        }
        break;
        case IDC_PATTERN:
        {
            if (msg == EN_CHANGE)
            {
                if (m_autoCompleteFilePatterns.GetOptions() & ACO_NOPREFIXFILTERING)
                    m_autoCompleteFilePatterns.SetOptions(ACO_UPDOWNKEYDROPSLIST | ACO_AUTOSUGGEST);
                CheckRegex();
            }
        }
        break;
        case IDC_EXCLUDEDIRSPATTERN:
        {
            if (msg == EN_CHANGE)
            {
                if (m_autoCompleteExcludeDirsPatterns.GetOptions() & ACO_NOPREFIXFILTERING)
                    m_autoCompleteExcludeDirsPatterns.SetOptions(ACO_UPDOWNKEYDROPSLIST | ACO_AUTOSUGGEST);
                CheckRegex();
            }
        }
        break;
        case IDC_REPLACETEXT:
        {
            if (msg == EN_CHANGE)
            {
                if (m_autoCompleteReplacePatterns.GetOptions() & ACO_NOPREFIXFILTERING)
                    m_autoCompleteReplacePatterns.SetOptions(ACO_UPDOWNKEYDROPSLIST | ACO_AUTOSUGGEST);
            }
        }
        break;
        case IDC_EXPORT:
        {
            PreserveChdir      keepCwd;
            IFileSaveDialogPtr pfd;

            HRESULT            hr = pfd.CreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER);
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
                auto exportpaths       = static_cast<DWORD>(CRegStdDWORD(L"Software\\grepWin\\export_paths")) != 0;
                auto exportlinenumbers = static_cast<DWORD>(CRegStdDWORD(L"Software\\grepWin\\export_linenumbers")) != 0;
                auto exportlinecontent = static_cast<DWORD>(CRegStdDWORD(L"Software\\grepWin\\export_linecontent")) != 0;
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

            bool onlyPaths = !includeMatchLineNumbers && !includeMatchLineTexts;
            if (!path.empty())
            {
                std::ofstream file;
                file.open(path);

                if (file.is_open())
                {
                    if (onlyPaths)
                    {
                        for (const auto& item : m_items)
                        {
                            file << CUnicodeUtils::StdGetUTF8(item.filePath) << std::endl;
                        }
                    }
                    else
                    {
                        constexpr char separator = '*';
                        for (const auto& item : m_items)
                        {
                            for (size_t i = 0; i < item.matchLinesNumbers.size(); ++i)
                            {
                                bool needSeparator = false;
                                if (includePaths)
                                {
                                    file << CUnicodeUtils::StdGetUTF8(item.filePath);
                                    needSeparator = true;
                                }
                                if (includeMatchLineNumbers)
                                {
                                    if (needSeparator)
                                        file << separator;
                                    file << CStringUtils::Format("%lu", item.matchLinesNumbers[i]);
                                    needSeparator = true;
                                }
                                if (includeMatchLineTexts)
                                {
                                    if (needSeparator)
                                        file << separator;
                                    auto line = item.matchLines[i];
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
                        // ReSharper disable CppEntityAssignedButNoRead
                        auto exportPaths       = CRegStdDWORD(L"Software\\grepWin\\export_paths");
                        auto exportLineNumbers = CRegStdDWORD(L"Software\\grepWin\\export_linenumbers");
                        auto exportLineContent = CRegStdDWORD(L"Software\\grepWin\\export_linecontent");
                        // ReSharper restore CppEntityAssignedButNoRead

                        exportPaths            = includePaths ? 1 : 0;
                        exportLineNumbers      = includeMatchLineNumbers ? 1 : 0;
                        exportLineContent      = includeMatchLineTexts ? 1 : 0;
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
        default:
            break;
    }
    return 1;
}

void CSearchDlg::SaveWndPosition()
{
    WINDOWPLACEMENT wpl = {0};
    wpl.length          = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(*this, &wpl);
    std::wstring winPosKey = L"windowpos_" + GetMonitorSetupHash();
    if (bPortable)
    {
        auto sPos = CStringUtils::Format(L"%d;%d;%d;%d;%d;%d;%d;%d;%d;%d",
                                         wpl.flags, wpl.showCmd,
                                         wpl.ptMinPosition.x, wpl.ptMinPosition.y,
                                         wpl.ptMaxPosition.x, wpl.ptMaxPosition.y,
                                         wpl.rcNormalPosition.left, wpl.rcNormalPosition.top, wpl.rcNormalPosition.right, wpl.rcNormalPosition.bottom);
        g_iniFile.SetValue(L"global", winPosKey.c_str(), sPos.c_str());
    }
    else
    {
        SHSetValue(HKEY_CURRENT_USER, L"Software\\grepWin", winPosKey.c_str(), REG_NONE, &wpl, sizeof(wpl));
    }
}

void CSearchDlg::UpdateInfoLabel()
{
    std::wstring sText;
    wchar_t      buf[1024] = {0};
    if (m_searchString.empty())
    {
        if (m_selectedItems)
            swprintf_s(buf, _countof(buf), TranslatedString(hResource, IDS_INFOLABELSELEMPTY).c_str(),
                       m_items.size(), m_totalItems - m_searchedItems, m_selectedItems);
        else
            swprintf_s(buf, _countof(buf), TranslatedString(hResource, IDS_INFOLABELEMPTY).c_str(),
                       m_items.size(), m_totalItems - m_searchedItems);
    }
    else
    {
        if (m_selectedItems)
            swprintf_s(buf, _countof(buf), TranslatedString(hResource, IDS_INFOLABELSEL).c_str(),
                       m_searchedItems, m_totalItems - m_searchedItems, m_totalMatches, m_items.size(), m_selectedItems);
        else
            swprintf_s(buf, _countof(buf), TranslatedString(hResource, IDS_INFOLABEL).c_str(),
                       m_searchedItems, m_totalItems - m_searchedItems, m_totalMatches, m_items.size());
    }
    sText = buf;

    SetDlgItemText(*this, IDC_SEARCHINFOLABEL, sText.c_str());
}

bool CSearchDlg::InitResultList()
{
    HWND  hListControl = GetDlgItem(*this, IDC_RESULTLIST);
    bool  filelist     = (IsDlgButtonChecked(*this, IDC_RESULTFILES) == BST_CHECKED);
    DWORD exStyle      = LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP | LVS_EX_FULLROWSELECT;
    ListView_SetItemCount(hListControl, 0);

    int c = Header_GetItemCount(ListView_GetHeader(hListControl)) - 1;
    while (c >= 0)
        ListView_DeleteColumn(hListControl, c--);

    ListView_SetExtendedListViewStyle(hListControl, exStyle);
    ListView_SetImageList(hListControl, reinterpret_cast<WPARAM>(static_cast<HIMAGELIST>(CSysImageList::GetInstance())), LVSIL_SMALL);

    std::wstring sName         = TranslatedString(hResource, IDS_NAME);
    std::wstring sSize         = TranslatedString(hResource, IDS_SIZE);
    std::wstring sLine         = TranslatedString(hResource, IDS_LINE);
    std::wstring sMatches      = TranslatedString(hResource, IDS_MATCHES);
    std::wstring sText         = TranslatedString(hResource, IDS_TEXT);
    std::wstring sPath         = TranslatedString(hResource, IDS_PATH);
    std::wstring sEncoding     = TranslatedString(hResource, IDS_ENCODING);
    std::wstring sDateModified = TranslatedString(hResource, IDS_DATEMODIFIED);
    std::wstring sExtension    = TranslatedString(hResource, IDS_FILEEXT);

    LVCOLUMN     lvc           = {0};
    lvc.mask                   = LVCF_TEXT | LVCF_FMT;
    lvc.fmt                    = LVCFMT_LEFT;
    lvc.cx                     = -1;
    lvc.pszText                = const_cast<LPWSTR>(static_cast<LPCWSTR>(sName.c_str()));
    ListView_InsertColumn(hListControl, 0, &lvc);
    lvc.pszText = filelist ? const_cast<LPWSTR>(static_cast<LPCWSTR>(sSize.c_str())) : const_cast<LPWSTR>(static_cast<LPCWSTR>(sLine.c_str()));
    lvc.fmt     = filelist ? LVCFMT_RIGHT : LVCFMT_LEFT;
    ListView_InsertColumn(hListControl, 1, &lvc);
    lvc.fmt     = LVCFMT_LEFT;
    lvc.pszText = filelist ? const_cast<LPWSTR>(static_cast<LPCWSTR>(sMatches.c_str())) : const_cast<LPWSTR>(static_cast<LPCWSTR>(sText.c_str()));
    ListView_InsertColumn(hListControl, 2, &lvc);
    lvc.pszText = const_cast<LPWSTR>(static_cast<LPCWSTR>(sPath.c_str()));
    ListView_InsertColumn(hListControl, 3, &lvc);
    if (filelist)
    {
        lvc.pszText = const_cast<LPWSTR>(static_cast<LPCWSTR>(sExtension.c_str()));
        ListView_InsertColumn(hListControl, 4, &lvc);
        lvc.pszText = const_cast<LPWSTR>(static_cast<LPCWSTR>(sEncoding.c_str()));
        ListView_InsertColumn(hListControl, 5, &lvc);
        lvc.pszText = const_cast<LPWSTR>(static_cast<LPCWSTR>(sDateModified.c_str()));
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

    m_selectedItems = 0;

    return true;
}

bool CSearchDlg::AddFoundEntry(CSearchInfo* pInfo, bool bOnlyListControl)
{
    if (!bOnlyListControl)
    {
        m_items.push_back(*pInfo);
        int index    = static_cast<int>(m_items.size() - 1);
        int subIndex = 0;
        for (const auto& lineNumber : pInfo->matchLinesNumbers)
        {
            UNREFERENCED_PARAMETER(lineNumber);
            m_listItems.push_back(std::make_tuple(index, subIndex));
            ++subIndex;
        }
    }
    else
    {
        HWND hListControl = GetDlgItem(*this, IDC_RESULTLIST);
        bool fileList     = (IsDlgButtonChecked(*this, IDC_RESULTFILES) == BST_CHECKED);
        auto count        = ListView_GetItemCount(hListControl);
        if (count != static_cast<int>(fileList ? m_items.size() : m_listItems.size()))
            ListView_SetItemCountEx(hListControl, fileList ? m_items.size() : m_listItems.size(), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
    }
    return true;
}

void CSearchDlg::FillResultList()
{
    ProfileTimer profile(L"FillResultList");
    SetCursor(LoadCursor(nullptr, IDC_APPSTARTING));
    // refresh cursor
    POINT pt;
    GetCursorPos(&pt);
    SetCursorPos(pt.x, pt.y);

    bool filelist     = (IsDlgButtonChecked(*this, IDC_RESULTFILES) == BST_CHECKED);
    HWND hListControl = GetDlgItem(*this, IDC_RESULTLIST);
    SendMessage(hListControl, WM_SETREDRAW, FALSE, 0);
    ListView_SetItemCountEx(hListControl, filelist ? m_items.size() : m_listItems.size(), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
    AutoSizeAllColumns();
    SendMessage(hListControl, WM_SETREDRAW, TRUE, 0);
    SetCursor(LoadCursor(nullptr, IDC_ARROW));
    // refresh cursor
    GetCursorPos(&pt);
    SetCursorPos(pt.x, pt.y);

    RedrawWindow(hListControl, nullptr, nullptr, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
}

void CSearchDlg::ShowContextMenu(HWND hWnd, int x, int y)
{
    HWND hListControl = GetDlgItem(*this, IDC_RESULTLIST);
    if (hWnd != GetDlgItem(*this, IDC_RESULTLIST))
        return;
    bool  fileList = (IsDlgButtonChecked(*this, IDC_RESULTFILES) == BST_CHECKED);
    // check if clicked on a header
    POINT pt       = {x, y};
    auto  hHeader  = ListView_GetHeader(hListControl);
    RECT  headerRc{};
    GetWindowRect(hHeader, &headerRc);
    if (PtInRect(&headerRc, pt))
    {
        int colCount   = Header_GetItemCount(hHeader);
        int clickedCol = -1;
        for (int i = 0; i < colCount; ++i)
        {
            RECT iRc{};
            Header_GetItemRect(hHeader, i, &iRc);
            MapWindowPoints(hHeader, nullptr, reinterpret_cast<LPPOINT>(&iRc), 2);
            if (PtInRect(&iRc, pt))
            {
                clickedCol = i;
                break;
            }
        }
        if (clickedCol >= 0)
        {
            HMENU hMenu = CreatePopupMenu();
            if (hMenu)
            {
                OnOutOfScope(DestroyMenu(hMenu));
                auto sCopyColumn    = TranslatedString(hResource, IDS_COPY_COLUMN);
                auto sCopyColumnSel = TranslatedString(hResource, IDS_COPY_COLUMN_SEL);
                AppendMenu(hMenu, MF_STRING, 1, sCopyColumn.c_str());
                if (ListView_GetSelectedCount(hListControl) > 0)
                    AppendMenu(hMenu, MF_STRING, 2, sCopyColumnSel.c_str());
                // Display the menu.
                auto cmdId = TrackPopupMenu(hMenu, TPM_RETURNCMD, pt.x, pt.y, 0, *this, nullptr);
                if (cmdId == 1 || cmdId == 2)
                {
                    int          iItem = -1;
                    std::wstring copyText;
                    auto         sReadError = TranslatedString(hResource, IDS_READERROR);
                    while ((iItem = ListView_GetNextItem(hListControl, iItem, cmdId == 1 ? LVNI_ALL : LVNI_SELECTED)) != (-1))
                    {
                        int selIndex = GetSelectedListIndex(fileList, iItem);
                        if ((selIndex < 0) || (selIndex >= static_cast<int>(m_items.size())))
                            continue;
                        if (!copyText.empty())
                            copyText += L"\r\n";
                        if (fileList)
                        {
                            const auto* pInfo = &m_items[selIndex];
                            switch (clickedCol)
                            {
                                case 0: // name of the file
                                    copyText += pInfo->filePath.substr(pInfo->filePath.find_last_of('\\') + 1);
                                    break;
                                case 1: // file size
                                    if (!pInfo->folder)
                                    {
                                        wchar_t buf[1024]{};
                                        StrFormatByteSizeW(pInfo->fileSize, buf, _countof(buf));
                                        copyText += buf;
                                    }
                                    break;
                                case 2: // match count or read error
                                    if (pInfo->readError)
                                        copyText += sReadError.c_str();
                                    else if (!pInfo->exception.empty())
                                        copyText += pInfo->exception.c_str();
                                    else
                                        copyText += std::to_wstring(pInfo->matchCount);
                                    break;
                                case 3: // path
                                    if (m_searchPath.find('|') != std::wstring::npos)
                                        copyText += pInfo->filePath.substr(0, pInfo->filePath.size() - pInfo->filePath.substr(pInfo->filePath.find_last_of('\\')).size());
                                    else
                                    {
                                        auto filePart = pInfo->filePath.substr(pInfo->filePath.find_last_of('\\'));
                                        auto len      = pInfo->filePath.size() - m_searchPath.size() - filePart.size();
                                        if (len > 0)
                                            --len;
                                        if (m_searchPath.size() < pInfo->filePath.size())
                                        {
                                            auto text = pInfo->filePath.substr(m_searchPath.size() + 1, len);
                                            if (text.empty())
                                                text = L"\\.";
                                            copyText += text;
                                        }
                                        else
                                            copyText += pInfo->filePath.c_str();
                                    }
                                    break;
                                case 4: // extension of the file
                                {
                                    if (!pInfo->folder)
                                    {
                                        auto dotPos = pInfo->filePath.find_last_of('.');
                                        if (dotPos != std::wstring::npos)
                                        {
                                            if (pInfo->filePath.find('\\', dotPos) == std::wstring::npos)
                                                copyText += pInfo->filePath.substr(dotPos + 1);
                                        }
                                    }
                                }
                                break;
                                case 5: // encoding
                                    copyText += CTextFile::GetEncodingString(pInfo->encoding);
                                    break;
                                case 6: // modification date
                                {
                                    wchar_t buf[1024]{};
                                    formatDate(buf, pInfo->modifiedTime, true);
                                    copyText += buf;
                                }
                                break;
                            }
                        }
                        else
                        {
                            auto        tup      = m_listItems[iItem];
                            auto        index    = std::get<0>(tup);
                            auto        subIndex = std::get<1>(tup);
                            const auto& item     = m_items[index];
                            const auto& pInfo    = &item;
                            switch (clickedCol)
                            {
                                case 0: // name of the file
                                    copyText += pInfo->filePath.substr(pInfo->filePath.find_last_of('\\') + 1);
                                    break;
                                case 1: // line number
                                    copyText += std::to_wstring(pInfo->matchLinesNumbers[subIndex]);
                                    break;
                                case 2: // line
                                {
                                    std::wstring line;
                                    if (pInfo->matchLines.size() > static_cast<size_t>(subIndex))
                                    {
                                        line = pInfo->matchLines[subIndex];
                                        std::ranges::replace(line, '\n', ' ');
                                        std::ranges::replace(line, '\r', ' ');
                                    }
                                    copyText += line;
                                }
                                break;
                                case 3: // path
                                    copyText += pInfo->filePath.substr(0, pInfo->filePath.size() - pInfo->filePath.substr(pInfo->filePath.find_last_of('\\') + 1).size() - 1);
                                    break;
                            }
                        }
                    }
                    WriteAsciiStringToClipboard(copyText.c_str(), *this);
                }
            }
            return;
        }
    }

    int nCount = ListView_GetItemCount(hListControl);
    if (nCount == 0)
        return;
    CShellContextMenu                        shellMenu;
    int                                      iItem = -1;
    std::unordered_map<size_t, std::wstring> pathMap;
    std::vector<LineData>                    lines;

    while ((iItem = ListView_GetNextItem(hListControl, iItem, LVNI_SELECTED)) != (-1))
    {
        int selIndex = GetSelectedListIndex(fileList, iItem);
        if ((selIndex < 0) || (selIndex >= static_cast<int>(m_items.size())))
            continue;
        const auto& info  = m_items[selIndex];
        pathMap[selIndex] = info.filePath;
        if (!fileList)
        {
            LineData data;
            auto     tup    = m_listItems[iItem];
            auto     subIdx = std::get<1>(tup);
            data.path       = info.filePath;
            LineDataLine dataLine;
            if (static_cast<int>(info.matchLinesNumbers.size()) > subIdx)
                dataLine.number = info.matchLinesNumbers[subIdx];
            if (static_cast<int>(info.matchLines.size()) > subIdx)
                dataLine.text = info.matchLines[subIdx];
            data.lines.push_back(dataLine);
            lines.push_back(data);
        }
    }

    if (pathMap.empty())
        return;

    std::vector<CSearchInfo> vPaths;
    for (const auto& idx : pathMap | std::views::keys)
    {
        vPaths.push_back(m_items[idx]);
    }
    shellMenu.SetObjects(vPaths, lines);

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
                        NMITEMACTIVATE itemActivate = {nullptr};
                        itemActivate.hdr.code       = NM_DBLCLK;
                        itemActivate.iItem          = iItem;
                        DoListNotify(&itemActivate);
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
                    m_bBlockUpdate = true;
                    SendMessage(hListControl, WM_SETREDRAW, FALSE, 0);
                    int nCount = ListView_GetItemCount(hListControl);
                    for (int i = 0; i < nCount; ++i)
                    {
                        ListView_SetItemState(hListControl, i, LVIS_SELECTED, LVIS_SELECTED);
                    }
                    SendMessage(hListControl, WM_SETREDRAW, TRUE, 0);
                    m_bBlockUpdate  = false;
                    m_selectedItems = ListView_GetSelectedCount(hListControl);
                    UpdateInfoLabel();
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
                m_autoCompleteFilePatterns.RemoveSelected();
                m_autoCompleteSearchPatterns.RemoveSelected();
                m_autoCompleteReplacePatterns.RemoveSelected();
                m_autoCompleteSearchPaths.RemoveSelected();
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
                    int  iItem    = -1;
                    bool fileList = (IsDlgButtonChecked(*this, IDC_RESULTFILES) == BST_CHECKED);
                    while ((iItem = ListView_GetNextItem(hListControl, iItem, LVNI_SELECTED)) != (-1))
                    {
                        int selIndex = GetSelectedListIndex(fileList, iItem);
                        if ((selIndex < 0) || (selIndex >= static_cast<int>(m_items.size())))
                            continue;
                        OpenFileAtListIndex(selIndex);
                    }
                }
            }
            break;
            default:
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
    else if (lpNMItemActivate->hdr.code == LVN_ODSTATECHANGED)
    {
        if (!m_bBlockUpdate)
        {
            HWND hListControl = lpNMItemActivate->hdr.hwndFrom;
            m_selectedItems   = ListView_GetSelectedCount(hListControl);
            UpdateInfoLabel();
        }
    }
    else if (lpNMItemActivate->hdr.code == LVN_ITEMCHANGED)
    {
        if ((lpNMItemActivate->uOldState & LVIS_SELECTED) || (lpNMItemActivate->uNewState & LVIS_SELECTED))
        {
            if (!m_bBlockUpdate)
            {
                HWND hListControl = lpNMItemActivate->hdr.hwndFrom;
                m_selectedItems   = ListView_GetSelectedCount(hListControl);
                UpdateInfoLabel();
            }
        }
    }
    else if (lpNMItemActivate->hdr.code == LVN_BEGINDRAG)
    {
        CDropFiles dropFiles; // class for creating DROPFILES struct

        HWND       hListControl = GetDlgItem(*this, IDC_RESULTLIST);
        int        nCount       = ListView_GetItemCount(hListControl);
        if (nCount == 0)
            return;

        int  iItem    = -1;
        bool fileList = (IsDlgButtonChecked(*this, IDC_RESULTFILES) == BST_CHECKED);
        while ((iItem = ListView_GetNextItem(hListControl, iItem, LVNI_SELECTED)) != (-1))
        {
            dropFiles.AddFile(m_items[GetSelectedListIndex(fileList, iItem)].filePath);
        }

        if (dropFiles.GetCount() > 0)
        {
            dropFiles.CreateStructure(hListControl);
        }
    }
    else if (lpNMItemActivate->hdr.code == LVN_COLUMNCLICK)
    {
        bool fileList = (IsDlgButtonChecked(*this, IDC_RESULTFILES) == BST_CHECKED);
        m_bAscending  = !m_bAscending;
        bool bDidSort = false;
        switch (lpNMItemActivate->iSubItem)
        {
            case 0:
                if (m_bAscending)
                    std::ranges::sort(m_items, CSearchInfo::NameCompareAsc);
                else
                    std::ranges::sort(m_items, CSearchInfo::NameCompareDesc);
                bDidSort = true;
                break;
            case 1:
                if (fileList)
                {
                    if (m_bAscending)
                        std::ranges::sort(m_items, CSearchInfo::SizeCompareAsc);
                    else
                        std::ranges::sort(m_items, CSearchInfo::SizeCompareDesc);
                    bDidSort = true;
                }
                break;
            case 2:
                if (fileList)
                {
                    if (m_bAscending)
                        std::ranges::sort(m_items, CSearchInfo::MatchesCompareAsc);
                    else
                        std::ranges::sort(m_items, CSearchInfo::MatchesCompareDesc);
                    bDidSort = true;
                }
                break;
            case 3:
                if (m_bAscending)
                    std::ranges::sort(m_items, CSearchInfo::PathCompareAsc);
                else
                    std::ranges::sort(m_items, CSearchInfo::PathCompareDesc);
                bDidSort = true;
                break;
            case 4:
                if (m_bAscending)
                    std::ranges::sort(m_items, CSearchInfo::ExtCompareAsc);
                else
                    std::ranges::sort(m_items, CSearchInfo::ExtCompareDesc);
                bDidSort = true;
                break;
            case 5:
                if (m_bAscending)
                    std::ranges::sort(m_items, CSearchInfo::EncodingCompareAsc);
                else
                    std::ranges::sort(m_items, CSearchInfo::EncodingCompareDesc);
                bDidSort = true;
                break;
            case 6:
                if (m_bAscending)
                    std::ranges::sort(m_items, CSearchInfo::ModifiedTimeCompareAsc);
                else
                    std::ranges::sort(m_items, CSearchInfo::ModifiedTimeCompareDesc);
                bDidSort = true;
                break;
            default:
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
                for (const auto& lineNumber : item.matchLinesNumbers)
                {
                    UNREFERENCED_PARAMETER(lineNumber);
                    m_listItems.push_back(std::make_tuple(index, subIndex));
                    ++subIndex;
                }
                ++index;
            }
        }

        HWND hListControl = GetDlgItem(*this, IDC_RESULTLIST);
        SendMessage(hListControl, WM_SETREDRAW, FALSE, 0);
        ListView_SetItemCountEx(hListControl, fileList ? m_items.size() : m_listItems.size(), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);

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
        RedrawWindow(hListControl, nullptr, nullptr, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
    }
    else if (lpNMItemActivate->hdr.code == LVN_GETINFOTIP)
    {
        NMLVGETINFOTIP* pInfoTip = reinterpret_cast<NMLVGETINFOTIP*>(lpNMItemActivate);

        // Which item number?
        size_t          itemId   = pInfoTip->iItem;
        int             iItem    = GetSelectedListIndex(static_cast<int>(itemId));
        pInfoTip->pszText[0]     = 0;
        if (static_cast<int>(m_items.size()) > iItem)
        {
            CSearchInfo  inf         = m_items[iItem];

            std::wstring matchString = inf.filePath + L"\n";
            if (!inf.exception.empty())
            {
                matchString += inf.exception;
                matchString += L"\n";
            }
            std::wstring sFormat = TranslatedString(hResource, IDS_CONTEXTLINE);
            for (size_t i = 0; i < min(inf.matchLines.size(), 5); ++i)
            {
                std::wstring matchText = inf.matchLines[i];
                CStringUtils::trim(matchText);
                matchString += CStringUtils::Format(sFormat.c_str(), inf.matchLinesNumbers[i], matchText.c_str());
            }
            if (inf.matchLines.size() > 5)
            {
                std::wstring sx  = TranslatedString(hResource, IDS_XMOREMATCHES);
                std::wstring ssx = CStringUtils::Format(sx.c_str(), static_cast<int>(inf.matchLines.size() - 5));
                matchString += ssx;
            }
            wcsncpy_s(pInfoTip->pszText, pInfoTip->cchTextMax, matchString.c_str(), pInfoTip->cchTextMax - 1LL);
        }
    }
    else if (lpNMItemActivate->hdr.code == LVN_GETDISPINFO)
    {
        static const std::wstring sBinary         = TranslatedString(hResource, IDS_BINARY);
        static const std::wstring sReadError      = TranslatedString(hResource, IDS_READERROR);
        static const std::wstring sRegexException = TranslatedString(hResource, IDS_REGEXEXCEPTION);

        NMLVDISPINFO*             pDispInfo       = reinterpret_cast<NMLVDISPINFO*>(lpNMItemActivate);
        LV_ITEM*                  pItem           = &(pDispInfo)->item;

        int                       iItem           = pItem->iItem;
        bool                      fileList        = (IsDlgButtonChecked(*this, IDC_RESULTFILES) == BST_CHECKED);

        if (fileList)
        {
            const auto& pInfo = &m_items[iItem];
            if (pItem->mask & LVIF_TEXT)
            {
                switch (pItem->iSubItem)
                {
                    case 0: // name of the file
                        wcsncpy_s(pItem->pszText, pItem->cchTextMax, pInfo->filePath.substr(pInfo->filePath.find_last_of('\\') + 1).c_str(), pItem->cchTextMax - 1LL);
                        break;
                    case 1: // file size
                        if (!pInfo->folder)
                            StrFormatByteSizeW(pInfo->fileSize, pItem->pszText, pItem->cchTextMax);
                        break;
                    case 2: // match count or read error
                        if (pInfo->readError)
                            wcsncpy_s(pItem->pszText, pItem->cchTextMax, sReadError.c_str(), pItem->cchTextMax - 1LL);
                        else if (!pInfo->exception.empty())
                            wcsncpy_s(pItem->pszText, pItem->cchTextMax, sRegexException.c_str(), pItem->cchTextMax - 1LL);
                        else
                            swprintf_s(pItem->pszText, pItem->cchTextMax, L"%lld", pInfo->matchCount);
                        break;
                    case 3: // path
                        if (m_searchPath.find('|') != std::wstring::npos)
                            wcsncpy_s(pItem->pszText, pItem->cchTextMax, pInfo->filePath.substr(0, pInfo->filePath.size() - pInfo->filePath.substr(pInfo->filePath.find_last_of('\\')).size()).c_str(), pItem->cchTextMax - 1LL);
                        else
                        {
                            auto filePart = pInfo->filePath.substr(pInfo->filePath.find_last_of('\\'));
                            auto len      = pInfo->filePath.size() - m_searchPath.size() - filePart.size();
                            if (len > 0)
                                --len;
                            if (m_searchPath.size() < pInfo->filePath.size())
                            {
                                wcsncpy_s(pItem->pszText, pItem->cchTextMax, pInfo->filePath.substr(m_searchPath.size() + 1, len).c_str(), pItem->cchTextMax - 1LL);
                                if (pItem->pszText[0] == 0)
                                    wcscpy_s(pItem->pszText, pItem->cchTextMax, L"\\.");
                            }
                            else
                                wcsncpy_s(pItem->pszText, pItem->cchTextMax, pInfo->filePath.c_str(), pItem->cchTextMax - 1LL);
                        }
                        break;
                    case 4: // extension of the file
                    {
                        pItem->pszText[0] = 0;
                        if (!pInfo->folder)
                        {
                            auto dotPos = pInfo->filePath.find_last_of('.');
                            if (dotPos != std::wstring::npos)
                            {
                                if (pInfo->filePath.find('\\', dotPos) == std::wstring::npos)
                                    wcsncpy_s(pItem->pszText, pItem->cchTextMax, pInfo->filePath.substr(dotPos + 1).c_str(), pItem->cchTextMax - 1LL);
                            }
                        }
                    }
                    break;
                    case 5: // encoding
                        wcsncpy_s(pItem->pszText, pItem->cchTextMax, CTextFile::GetEncodingString(pInfo->encoding).c_str(), pItem->cchTextMax - 1LL);
                        break;
                    case 6: // modification date
                        formatDate(pItem->pszText, pInfo->modifiedTime, true);
                        break;
                    default:
                        pItem->pszText[0] = 0;
                        break;
                }
            }
            if (pItem->mask & LVIF_IMAGE)
            {
                pItem->iImage = pInfo->folder ? CSysImageList::GetInstance().GetDirIconIndex() : CSysImageList::GetInstance().GetFileIconIndex(pInfo->filePath);
            }
        }
        else
        {
            auto        tup      = m_listItems[iItem];
            auto        index    = std::get<0>(tup);
            auto        subIndex = std::get<1>(tup);

            const auto& item     = m_items[index];
            const auto& pInfo    = &item;
            if (item.encoding == CTextFile::Binary)
            {
                if (pItem->mask & LVIF_TEXT)
                {
                    switch (pItem->iSubItem)
                    {
                        case 0: // name of the file
                            wcsncpy_s(pItem->pszText, pItem->cchTextMax, pInfo->filePath.substr(pInfo->filePath.find_last_of('\\') + 1).c_str(), pItem->cchTextMax - 1LL);
                            break;
                        case 1: // binary
                            wcsncpy_s(pItem->pszText, pItem->cchTextMax, sBinary.c_str(), pItem->cchTextMax);
                            break;
                        case 3: // path
                            wcsncpy_s(pItem->pszText, pItem->cchTextMax, pInfo->filePath.substr(0, pInfo->filePath.size() - pInfo->filePath.substr(pInfo->filePath.find_last_of('\\') + 1).size() - 1).c_str(), pItem->cchTextMax - 1LL);
                            break;
                        default:
                            pItem->pszText[0] = 0;
                            break;
                    }
                }
                if (pItem->mask & LVIF_IMAGE)
                {
                    pItem->iImage = pInfo->folder ? CSysImageList::GetInstance().GetDirIconIndex() : CSysImageList::GetInstance().GetFileIconIndex(pInfo->filePath);
                }
            }
            else
            {
                if (pItem->mask & LVIF_TEXT)
                {
                    switch (pItem->iSubItem)
                    {
                        case 0: // name of the file
                            wcsncpy_s(pItem->pszText, pItem->cchTextMax, pInfo->filePath.substr(pInfo->filePath.find_last_of('\\') + 1).c_str(), pItem->cchTextMax - 1LL);
                            break;
                        case 1: // line number
                            swprintf_s(pItem->pszText, pItem->cchTextMax, L"%ld", pInfo->matchLinesNumbers[subIndex]);
                            break;
                        case 2: // line
                        {
                            std::wstring line;
                            if (pInfo->matchLines.size() > static_cast<size_t>(subIndex))
                                line = pInfo->matchLines[subIndex];
                            std::ranges::replace(line, '\t', ' ');
                            std::ranges::replace(line, '\n', ' ');
                            std::ranges::replace(line, '\r', ' ');
                            wcsncpy_s(pItem->pszText, pItem->cchTextMax, line.c_str(), pItem->cchTextMax - 1LL);
                        }
                        break;
                        case 3: // path
                            wcsncpy_s(pItem->pszText, pItem->cchTextMax, pInfo->filePath.substr(0, pInfo->filePath.size() - pInfo->filePath.substr(pInfo->filePath.find_last_of('\\') + 1).size() - 1).c_str(), pItem->cchTextMax - 1LL);
                            break;
                        default:
                            pItem->pszText[0] = 0;
                            break;
                    }
                }

                if (pItem->mask & LVIF_IMAGE)
                {
                    pItem->iImage = pInfo->folder ? CSysImageList::GetInstance().GetDirIconIndex() : CSysImageList::GetInstance().GetFileIconIndex(pInfo->filePath);
                }
            }
        }
    }
}

void CSearchDlg::OpenFileAtListIndex(int listIndex)
{
    int          iItem  = GetSelectedListIndex(listIndex);
    CSearchInfo  inf    = m_items[iItem];
    size_t       dotPos = inf.filePath.rfind('.');
    std::wstring ext;
    if (dotPos != std::wstring::npos)
        ext = inf.filePath.substr(dotPos);

    CRegStdString regEditorCmd(L"Software\\grepWin\\editorcmd");
    std::wstring  cmd = regEditorCmd;
    if (bPortable)
        cmd = g_iniFile.GetValue(L"global", L"editorcmd", L"");
    if (!cmd.empty() && !inf.readError && inf.encoding != CTextFile::UnicodeType::Binary)
    {
        bool fileList = (IsDlgButtonChecked(*this, IDC_RESULTFILES) == BST_CHECKED);
        if (!fileList)
        {
            HWND    hListControl          = GetDlgItem(*this, IDC_RESULTLIST);
            wchar_t textLineBuf[MAX_PATH] = {0};
            LVITEM  lv                    = {0};
            lv.iItem                      = listIndex;
            lv.iSubItem                   = 1; // line number
            lv.mask                       = LVIF_TEXT;
            lv.pszText                    = textLineBuf;
            lv.cchTextMax                 = _countof(textLineBuf);
            if (ListView_GetItem(hListControl, &lv))
            {
                if (_wtol(textLineBuf) == 0)
                    wcscpy_s(textLineBuf, L"0");
                SearchReplace(cmd, L"%line%", textLineBuf);
            }
        }
        else
        {
            // use the first matching line in this file
            if (!inf.matchLinesNumbers.empty())
                SearchReplace(cmd, L"%line%", CStringUtils::Format(L"%lu", inf.matchLinesNumbers[0]));
            else
                SearchReplace(cmd, L"%line%", L"0");
        }

        SearchReplace(cmd, L"%path%", inf.filePath.c_str());

        STARTUPINFO         startupInfo{};
        PROCESS_INFORMATION processInfo{};
        startupInfo.cb = sizeof(STARTUPINFO);
        CreateProcess(nullptr, const_cast<wchar_t*>(cmd.c_str()), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startupInfo, &processInfo);
        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);
        return;
    }

    DWORD bufLen = 0;
    if (AssocQueryString(ASSOCF_INIT_DEFAULTTOSTAR, ASSOCSTR_DDECOMMAND, ext.c_str(), nullptr, nullptr, &bufLen) == S_OK)
    {
        if (bufLen)
        {
            // application requires DDE to open the file:
            // since we can't do this the easy way with CreateProcess, we use ShellExecute instead
            ShellExecute(*this, nullptr, inf.filePath.c_str(), nullptr, nullptr, SW_SHOW);
            return;
        }
    }
    bufLen = 0;
    AssocQueryString(ASSOCF_INIT_DEFAULTTOSTAR, ASSOCSTR_COMMAND, ext.c_str(), nullptr, nullptr, &bufLen);
    auto cmdBuf = std::make_unique<wchar_t[]>(bufLen + 1LL);
    AssocQueryString(ASSOCF_INIT_DEFAULTTOSTAR, ASSOCSTR_COMMAND, ext.c_str(), nullptr, cmdBuf.get(), &bufLen);
    std::wstring application = cmdBuf.get();
    // normalize application path
    DWORD        len         = ExpandEnvironmentStrings(application.c_str(), nullptr, 0);
    cmdBuf                   = std::make_unique<wchar_t[]>(len + 1LL);
    ExpandEnvironmentStrings(application.c_str(), cmdBuf.get(), len);
    application = cmdBuf.get();

    // resolve parameters
    if (application.find(L"%1") == std::wstring::npos)
        application += L" %1";

    bool         fileList = (IsDlgButtonChecked(*this, IDC_RESULTFILES) == BST_CHECKED);
    std::wstring lineNumberParamBefore;
    std::wstring lineNumberParam;
    wchar_t      textLineBuf[MAX_PATH] = {0};
    if (!fileList)
    {
        HWND   hListControl = GetDlgItem(*this, IDC_RESULTLIST);
        LVITEM lv           = {0};
        lv.iItem            = listIndex;
        lv.iSubItem         = 1; // line number
        lv.mask             = LVIF_TEXT;
        lv.pszText          = textLineBuf;
        lv.cchTextMax       = _countof(textLineBuf);
        if (!ListView_GetItem(hListControl, &lv))
        {
            textLineBuf[0] = '\0';
        }
    }
    else if (!inf.matchLinesNumbers.empty())
    {
        // use the first matching line in this file
        swprintf_s(textLineBuf, L"%ld", inf.matchLinesNumbers[0]);
    }
    if (textLineBuf[0] == 0)
        wcscpy_s(textLineBuf, L"0");
    std::wstring appname = application;
    std::ranges::transform(appname, appname.begin(), ::towlower);

    // now find out if the application which opens the file is known to us
    // and if it has a 'linenumber' switch to jump directly to a specific
    // line number.
    if (appname.find(L"notepad++.exe") != std::wstring::npos)
    {
        // notepad++
        lineNumberParam = CStringUtils::Format(L"-n%s", textLineBuf);
    }
    else if (appname.find(L"xemacs.exe") != std::wstring::npos)
    {
        // XEmacs
        lineNumberParam = CStringUtils::Format(L"+%s", textLineBuf);
    }
    else if (appname.find(L"uedit32.exe") != std::wstring::npos)
    {
        // UltraEdit
        lineNumberParam = CStringUtils::Format(L"-l%s", textLineBuf);
    }
    else if (appname.find(L"codewright.exe") != std::wstring::npos)
    {
        // CodeWright
        lineNumberParam = CStringUtils::Format(L"-G%s", textLineBuf);
    }
    else if (appname.find(L"notepad2.exe") != std::wstring::npos)
    {
        // Notepad2
        auto escapedSearch = m_searchString;
        SearchReplace(escapedSearch, L"\"", L"\\\"");
        lineNumberParamBefore = CStringUtils::Format(L"/g %s /mr \"%s\"", textLineBuf, escapedSearch.c_str());
    }
    else if ((appname.find(L"bowpad.exe") != std::wstring::npos) || (appname.find(L"bowpad64.exe") != std::wstring::npos))
    {
        // BowPad
        lineNumberParam = CStringUtils::Format(L"/line:%s", textLineBuf);
    }
    else if (appname.find(L"code.exe") != std::wstring::npos)
    {
        // Visual Studio Code
        lineNumberParamBefore = L"--goto";
        lineNumberParam       = CStringUtils::Format(L":%s", textLineBuf);
    }

    // replace "%1" with %1
    std::wstring           tag     = L"\"%1\"";
    std::wstring           repl    = L"%1";
    std::wstring::iterator itBegin = search(application.begin(), application.end(), tag.begin(), tag.end());
    if (itBegin != application.end())
    {
        std::wstring::iterator itEnd = itBegin + tag.size();
        application.replace(itBegin, itEnd, repl);
    }
    // replace %1 with "path/of/selected/file"
    tag = L"%1";
    if (application.find(L"rundll32.exe") == std::wstring::npos && application.find(L"--single-argument") == std::wstring::npos)
        repl = L"\"" + inf.filePath + L"\"";
    else
        repl = inf.filePath;
    if (!lineNumberParamBefore.empty())
    {
        repl = lineNumberParamBefore + L" " + repl;
    }
    itBegin = std::ranges::search(application, tag).begin();
    if (itBegin != application.end())
    {
        std::wstring::iterator itEnd = itBegin + tag.size();
        application.replace(itBegin, itEnd, repl);
    }
    if (!lineNumberParam.empty())
    {
        if (!lineNumberParam.starts_with(L":"))
        {
            application += L" ";
        }
        application += lineNumberParam;
    }

    STARTUPINFO         startupInfo{};
    PROCESS_INFORMATION processInfo{};
    startupInfo.cb = sizeof(STARTUPINFO);
    CreateProcess(nullptr, const_cast<wchar_t*>(application.c_str()), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startupInfo, &processInfo);
    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
}

bool grepWinIsRegexValid(const std::wstring& searchString)
{
    // check if the regex is valid
    bool bValid = true;
    try
    {
        boost::wregex expression = boost::wregex(searchString);
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
    auto buf                  = GetDlgItemText(IDC_SEARCHPATH);
    m_searchPath              = buf.get();

    buf                       = GetDlgItemText(IDC_SEARCHTEXT);
    m_searchString            = buf.get();

    buf                       = GetDlgItemText(IDC_REPLACETEXT);
    m_replaceString           = buf.get();

    buf                       = GetDlgItemText(IDC_EXCLUDEDIRSPATTERN);
    m_excludeDirsPatternRegex = buf.get();

    buf                       = GetDlgItemText(IDC_PATTERN);
    m_patternRegex            = buf.get();

    // split the pattern string into single patterns and
    // add them to an array
    auto   pBuf               = buf.get();
    size_t pos                = 0;
    m_patterns.clear();
    do
    {
        pos            = wcscspn(pBuf, L"|");
        std::wstring s = std::wstring(pBuf, pos);
        if (!s.empty())
        {
            std::ranges::transform(s, s.begin(), ::towlower);
            m_patterns.push_back(s);
            auto endPart = s.rbegin();
            if (*endPart == '*' && s.size() > 2)
            {
                ++endPart;
                if (*endPart == '.')
                {
                    m_patterns.push_back(s.substr(0, s.size() - 2));
                }
            }
        }
        pBuf += pos;
        pBuf++;
    } while (*pBuf && (*(pBuf - 1)));

    m_bUseRegex = (IsDlgButtonChecked(*this, IDC_REGEXRADIO) == BST_CHECKED);
    if (m_bUseRegex)
    {
        // check if the regex is valid before doing the search
        if (!grepWinIsRegexValid(m_searchString) && !m_searchString.empty())
        {
            return false;
        }
    }
    m_bUseRegexForPaths = (IsDlgButtonChecked(*this, IDC_FILEPATTERNREGEX) == BST_CHECKED);
    if (m_bUseRegexForPaths)
    {
        // check if the regex is valid before doing the search
        if (!grepWinIsRegexValid(m_patternRegex) && !m_patternRegex.empty())
        {
            return false;
        }
    }
    // check if the Exclude Dirs regex is valid before doing the search
    if (!grepWinIsRegexValid(m_excludeDirsPatternRegex) && !m_excludeDirsPatternRegex.empty())
    {
        return false;
    }

    m_bAllSize = (IsDlgButtonChecked(*this, IDC_ALLSIZERADIO) == BST_CHECKED);

    m_lSize    = 0;
    m_sizeCmp  = 0;
    if (!m_bAllSize)
    {
        buf     = GetDlgItemText(IDC_SIZEEDIT);
        m_lSize = _wtol(buf.get());
        m_lSize *= 1024;
        m_sizeCmp = static_cast<int>(SendDlgItemMessage(*this, IDC_SIZECOMBO, CB_GETCURSEL, 0, 0));
    }
    m_bIncludeSystem     = (IsDlgButtonChecked(*this, IDC_INCLUDESYSTEM) == BST_CHECKED);
    m_bIncludeHidden     = (IsDlgButtonChecked(*this, IDC_INCLUDEHIDDEN) == BST_CHECKED);
    m_bIncludeSubfolders = (IsDlgButtonChecked(*this, IDC_INCLUDESUBFOLDERS) == BST_CHECKED);
    m_bIncludeSymLinks   = (IsDlgButtonChecked(*this, IDC_INCLUDESYMLINK) == BST_CHECKED);
    m_bIncludeBinary     = (IsDlgButtonChecked(*this, IDC_INCLUDEBINARY) == BST_CHECKED);
    m_bCreateBackup      = (IsDlgButtonChecked(*this, IDC_CREATEBACKUP) == BST_CHECKED);
    m_bKeepFileDate      = (IsDlgButtonChecked(*this, IDC_KEEPFILEDATECHECK) == BST_CHECKED);
    m_bWholeWords        = (IsDlgButtonChecked(*this, IDC_WHOLEWORDS) == BST_CHECKED);
    m_bUTF8              = (IsDlgButtonChecked(*this, IDC_UTF8) == BST_CHECKED);
    m_bForceBinary       = (IsDlgButtonChecked(*this, IDC_BINARY) == BST_CHECKED);
    m_bCaseSensitive     = (IsDlgButtonChecked(*this, IDC_CASE_SENSITIVE) == BST_CHECKED);
    m_bDotMatchesNewline = (IsDlgButtonChecked(*this, IDC_DOTMATCHNEWLINE) == BST_CHECKED);

    m_dateLimit          = 0;
    if (IsDlgButtonChecked(*this, IDC_RADIO_DATE_ALL) == BST_CHECKED)
        m_dateLimit = 0;
    if (IsDlgButtonChecked(*this, IDC_RADIO_DATE_NEWER) == BST_CHECKED)
        m_dateLimit = IDC_RADIO_DATE_NEWER - IDC_RADIO_DATE_ALL;
    if (IsDlgButtonChecked(*this, IDC_RADIO_DATE_OLDER) == BST_CHECKED)
        m_dateLimit = IDC_RADIO_DATE_OLDER - IDC_RADIO_DATE_ALL;
    if (IsDlgButtonChecked(*this, IDC_RADIO_DATE_BETWEEN) == BST_CHECKED)
        m_dateLimit = IDC_RADIO_DATE_BETWEEN - IDC_RADIO_DATE_ALL;
    SYSTEMTIME sysTime = {0};
    DateTime_GetSystemtime(GetDlgItem(*this, IDC_DATEPICK1), &sysTime);
    SystemTimeToFileTime(&sysTime, &m_date1);
    DateTime_GetSystemtime(GetDlgItem(*this, IDC_DATEPICK2), &sysTime);
    SystemTimeToFileTime(&sysTime, &m_date2);
    m_showContent = IsDlgButtonChecked(*this, IDC_RESULTCONTENT) == BST_CHECKED;

    if (m_searchPath.empty())
        return false;

    if (m_bNoSaveSettings)
        return true;

    if (bPortable)
        g_iniFile.SetValue(L"global", L"searchpath", m_searchPath.c_str());
    else
        m_regSearchPath = m_searchPath;
    if (bPortable)
        g_iniFile.SetValue(L"global", L"UseRegex", m_bUseRegex ? L"1" : L"0");
    else
        m_regUseRegex = static_cast<DWORD>(m_bUseRegex);
    if (bPortable)
        g_iniFile.SetValue(L"global", L"UseFileMatchRegex", m_bUseRegexForPaths ? L"1" : L"0");
    else
        m_regUseRegexForPaths = static_cast<DWORD>(m_bUseRegexForPaths);

    if (bPortable)
        g_iniFile.SetValue(L"global", L"AllSize", m_bAllSize ? L"1" : L"0");
    else
        m_regAllSize = static_cast<DWORD>(m_bAllSize);

    if (bPortable)
        g_iniFile.SetValue(L"global", L"Size", CStringUtils::Format(L"%I64u", m_lSize / 1024).c_str());
    else
        m_regSize = CStringUtils::Format(L"%I64u", m_lSize / 1024).c_str();

    if (bPortable)
        g_iniFile.SetValue(L"global", L"SizeCombo", CStringUtils::Format(L"%d", m_sizeCmp).c_str());
    else
        m_regSizeCombo = m_sizeCmp;

    if (bPortable)
    {
        g_iniFile.SetValue(L"global", L"IncludeSystem", m_bIncludeSystem ? L"1" : L"0");
        g_iniFile.SetValue(L"global", L"IncludeHidden", m_bIncludeHidden ? L"1" : L"0");
        g_iniFile.SetValue(L"global", L"IncludeSubfolders", m_bIncludeSubfolders ? L"1" : L"0");
        g_iniFile.SetValue(L"global", L"IncludeSymLinks", m_bIncludeSymLinks ? L"1" : L"0");
        g_iniFile.SetValue(L"global", L"IncludeBinary", m_bIncludeBinary ? L"1" : L"0");
        g_iniFile.SetValue(L"global", L"CreateBackup", m_bCreateBackup ? L"1" : L"0");
        g_iniFile.SetValue(L"global", L"KeepFileDate", m_bKeepFileDate ? L"1" : L"0");
        g_iniFile.SetValue(L"global", L"WholeWords", m_bWholeWords ? L"1" : L"0");
        g_iniFile.SetValue(L"global", L"UTF8", m_bUTF8 ? L"1" : L"0");
        g_iniFile.SetValue(L"global", L"Binary", m_bForceBinary ? L"1" : L"0");
        g_iniFile.SetValue(L"global", L"CaseSensitive", m_bCaseSensitive ? L"1" : L"0");
        g_iniFile.SetValue(L"global", L"DotMatchesNewline", m_bDotMatchesNewline ? L"1" : L"0");
        g_iniFile.SetValue(L"global", L"pattern", m_patternRegex.c_str());
        g_iniFile.SetValue(L"global", L"ExcludeDirsPattern", m_excludeDirsPatternRegex.c_str());
        g_iniFile.SetValue(L"global", L"DateLimit", std::to_wstring(m_dateLimit).c_str());
        g_iniFile.SetValue(L"global", L"Date1Low", std::to_wstring(m_date1.dwLowDateTime).c_str());
        g_iniFile.SetValue(L"global", L"Date1High", std::to_wstring(m_date1.dwHighDateTime).c_str());
        g_iniFile.SetValue(L"global", L"Date2Low", std::to_wstring(m_date2.dwLowDateTime).c_str());
        g_iniFile.SetValue(L"global", L"Date2High", std::to_wstring(m_date2.dwHighDateTime).c_str());
        if (!m_showContentSet)
            g_iniFile.SetValue(L"global", L"showcontent", m_showContent ? L"1" : L"0");
    }
    else
    {
        m_regIncludeSystem      = static_cast<DWORD>(m_bIncludeSystem);
        m_regIncludeHidden      = static_cast<DWORD>(m_bIncludeHidden);
        m_regIncludeSubfolders  = static_cast<DWORD>(m_bIncludeSubfolders);
        m_regIncludeSymLinks    = static_cast<DWORD>(m_bIncludeSymLinks);
        m_regIncludeBinary      = static_cast<DWORD>(m_bIncludeBinary);
        m_regCreateBackup       = static_cast<DWORD>(m_bCreateBackup);
        m_regKeepFileDate       = static_cast<DWORD>(m_bKeepFileDate);
        m_regWholeWords         = static_cast<DWORD>(m_bWholeWords);
        m_regUTF8               = static_cast<DWORD>(m_bUTF8);
        m_regBinary             = static_cast<DWORD>(m_bForceBinary);
        m_regCaseSensitive      = static_cast<DWORD>(m_bCaseSensitive);
        m_regDotMatchesNewline  = static_cast<DWORD>(m_bDotMatchesNewline);
        m_regPattern            = m_patternRegex;
        m_regExcludeDirsPattern = m_excludeDirsPatternRegex;
        m_regDateLimit          = m_dateLimit;
        m_regDate1Low           = m_date1.dwLowDateTime;
        m_regDate1High          = m_date1.dwHighDateTime;
        m_regDate2Low           = m_date2.dwLowDateTime;
        m_regDate2High          = m_date2.dwHighDateTime;
        if (!m_showContentSet)
            m_regShowContent = m_showContent;
    }

    SaveWndPosition();

    return true;
}

bool grepWinMatchI(const std::wstring& theRegex, const wchar_t* pText)
{
    try
    {
        boost::wregex  expression = boost::wregex(theRegex, boost::regex::normal | boost::regbase::icase);
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
    ProfileTimer              profile(L"SearchThread");

    auto                      pathBuf        = std::make_unique<wchar_t[]>(MAX_PATH_NEW);

    // split the path string into single paths and
    // add them to an array
    const auto*               pBufSearchPath = m_searchPath.c_str();
    size_t                    pos            = 0;
    std::vector<std::wstring> pathVector;
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
            CStringUtils::rtrim(s, L"\\/ ");
            pathVector.push_back(s);
        }
        pBufSearchPath += pos;
        pBufSearchPath++;
    } while (*pBufSearchPath && (*(pBufSearchPath - 1)));

    if (!m_bUseRegex && !m_replaceString.empty())
    {
        // escape all characters in the replace string
        std::wstring sRepl;
        for (const auto& c : m_replaceString)
        {
            switch (c)
            {
                case '$':
                // case '\\':
                case '(':
                case ')':
                case '?':
                case ':':
                    sRepl += L"\\";
                    break;
                default:
                    break;
            }
            sRepl += c;
        }
        m_replaceString = sRepl;
    }

    SendMessage(*this, SEARCH_START, 0, 0);

    std::wstring searchStringutf16;

    for (auto c : m_searchString)
    {
        searchStringutf16 += c;
        if (std::iswalpha(c) && ((c & 0xFF00) == 0))
            searchStringutf16 += L"\\x00";
        else
        {
            searchStringutf16 = m_searchString;
            break;
        }
    }

    // use a thread pool:
    // use 2 threads less than processors are available,
    // because we already have two threads in use:
    // the UI thread and this one.
    ThreadPool tp(max(std::thread::hardware_concurrency() - 2, 1));

    for (const auto& cSearchPath : pathVector)
    {
        std::wstring searchPath = cSearchPath;
        size_t       endPos     = searchPath.find_last_not_of(L" \\");
        if (std::wstring::npos != endPos)
        {
            searchPath = searchPath.substr(0, endPos + 1);
            if (searchPath[searchPath.length() - 1] == ':')
                searchPath += L"\\";
        }
        std::wstring searchRoot = searchPath;
        if (!searchPath.empty())
        {
            bool bAlwaysSearch = false;
            if (!PathIsDirectory(searchPath.c_str()))
            {
                bAlwaysSearch = true;
                searchRoot    = searchRoot.substr(0, searchRoot.find_last_of('\\'));
            }
            bool         bIsDirectory = false;
            CDirFileEnum fileEnumerator(searchPath.c_str());
            if (!m_bIncludeSymLinks)
                fileEnumerator.SetAttributesToIgnore(FILE_ATTRIBUTE_REPARSE_POINT);
            bool         bRecurse = m_bIncludeSubfolders;
            std::wstring sPath;
            while ((fileEnumerator.NextFile(sPath, &bIsDirectory, bRecurse)) && ((!m_cancelled) || (bAlwaysSearch)))
            {
                if (bAlwaysSearch && _wcsicmp(searchPath.c_str(), sPath.c_str()))
                    bAlwaysSearch = false;
                if (m_backupAndTempFiles.contains(sPath))
                    continue;
                wcscpy_s(pathBuf.get(), MAX_PATH_NEW, sPath.c_str());
                if (!bIsDirectory)
                {
                    bool     bSearch      = false;
                    uint64_t fullFileSize = 0;
                    FILETIME ft           = {0};
                    if (bAlwaysSearch)
                    {
                        const WIN32_FIND_DATA* pFindData = fileEnumerator.GetFileInfo();
                        wcscpy_s(pathBuf.get(), MAX_PATH_NEW, searchPath.c_str());
                        fullFileSize = (static_cast<uint64_t>(pFindData->nFileSizeHigh) << 32) | pFindData->nFileSizeLow;
                        ft           = pFindData->ftLastWriteTime;
                    }
                    else
                    {
                        const WIN32_FIND_DATA* pFindData = fileEnumerator.GetFileInfo();
                        bSearch                          = ((m_bIncludeHidden) || ((pFindData->dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) == 0));
                        bSearch                          = bSearch && ((m_bIncludeSystem) || ((pFindData->dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) == 0));
                        fullFileSize                     = (static_cast<uint64_t>(pFindData->nFileSizeHigh) << 32) | pFindData->nFileSizeLow;
                        ft                               = pFindData->ftLastWriteTime;
                        if (!m_bAllSize && bSearch)
                        {
                            switch (m_sizeCmp)
                            {
                                case 0: // less than
                                    bSearch = bSearch && (fullFileSize < m_lSize);
                                    break;
                                case 1: // equal
                                    bSearch = bSearch && (fullFileSize == m_lSize);
                                    break;
                                case 2: // greater than
                                    bSearch = bSearch && (fullFileSize > m_lSize);
                                    break;
                                default:
                                    break;
                            }
                        }
                        if (bSearch)
                        {
                            switch (m_dateLimit + IDC_RADIO_DATE_ALL)
                            {
                                default:
                                case IDC_RADIO_DATE_ALL:
                                    break;
                                case IDC_RADIO_DATE_NEWER:
                                    bSearch = CompareFileTime(&ft, &m_date1) >= 0;
                                    break;
                                case IDC_RADIO_DATE_OLDER:
                                    bSearch = CompareFileTime(&ft, &m_date1) <= 0;
                                    break;
                                case IDC_RADIO_DATE_BETWEEN:
                                    bSearch = CompareFileTime(&ft, &m_date1) >= 0;
                                    bSearch = bSearch && (CompareFileTime(&ft, &m_date2) <= 0);
                                    break;
                            }
                        }
                    }
                    bRecurse      = ((m_bIncludeSubfolders) && (bSearch));
                    bool bPattern = MatchPath(pathBuf.get());

                    if ((bSearch && bPattern) || (bAlwaysSearch))
                    {
                        CSearchInfo sInfo(pathBuf.get());
                        sInfo.fileSize     = fullFileSize;
                        sInfo.modifiedTime = ft;
                        if (m_searchString.empty())
                        {
                            SendMessage(*this, SEARCH_FOUND, 1, reinterpret_cast<LPARAM>(&sInfo));
                            SendMessage(*this, SEARCH_PROGRESS, 1, 0);
                        }
                        else
                        {
                            auto searchFn = [=]() {
                                SearchFile(sInfo, searchRoot, bAlwaysSearch, m_bIncludeBinary, m_bUseRegex, m_bCaseSensitive, m_bDotMatchesNewline, m_searchString, searchStringutf16, m_cancelled);
                            };
                            tp.enqueueWait(searchFn);
                            // SearchFile(std::move(sinfo), searchRoot, bAlwaysSearch, m_bIncludeBinary, m_bUseRegex, m_bCaseSensitive, m_bDotMatchesNewline, m_searchString, SearchStringutf16, &m_Cancelled);
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
                    std::wstring relPath             = pathBuf.get();
                    relPath                          = relPath.substr(searchPath.size());
                    if (!relPath.empty())
                    {
                        if (relPath[0] == '\\')
                            relPath = relPath.substr(1);
                    }
                    bool bExcludeDir = bSearch && !m_excludeDirsPatternRegex.empty() &&
                                       (grepWinMatchI(m_excludeDirsPatternRegex, pFindData->cFileName) ||
                                        grepWinMatchI(m_excludeDirsPatternRegex, pathBuf.get()) ||
                                        grepWinMatchI(m_excludeDirsPatternRegex, relPath.c_str()));
                    bSearch  = bSearch && !bExcludeDir;
                    bRecurse = ((bIsDirectory) && (m_bIncludeSubfolders) && (bSearch));
                    if (m_searchString.empty() && m_replaceString.empty())
                    {
                        // if there's no search and replace string, include folders in the 'matched' list if they
                        // match the specified file pattern
                        if (MatchPath(pathBuf.get()))
                        {
                            if (bSearch)
                            {
                                switch (m_dateLimit + IDC_RADIO_DATE_ALL)
                                {
                                    default:
                                    case IDC_RADIO_DATE_ALL:
                                        break;
                                    case IDC_RADIO_DATE_NEWER:
                                        bSearch = CompareFileTime(&pFindData->ftLastWriteTime, &m_date1) >= 0;
                                        break;
                                    case IDC_RADIO_DATE_OLDER:
                                        bSearch = CompareFileTime(&pFindData->ftLastWriteTime, &m_date1) <= 0;
                                        break;
                                    case IDC_RADIO_DATE_BETWEEN:
                                        bSearch = CompareFileTime(&pFindData->ftLastWriteTime, &m_date1) >= 0;
                                        bSearch = bSearch && (CompareFileTime(&pFindData->ftLastWriteTime, &m_date2) <= 0);
                                        break;
                                }
                            }
                            if (!m_bAllSize && bSearch)
                            {
                                // assume a 'file'-size of zero for dirs
                                switch (m_sizeCmp)
                                {
                                    case 0: // less than
                                        bSearch = bSearch && (0 < m_lSize);
                                        break;
                                    case 1: // equal
                                        bSearch = bSearch && (0 == m_lSize);
                                        break;
                                    case 2: // greater than
                                        bSearch = bSearch && (0 > m_lSize);
                                        break;
                                    default:
                                        break;
                                }
                            }
                            if (bSearch)
                            {
                                CSearchInfo sInfo(pathBuf.get());
                                sInfo.modifiedTime = pFindData->ftLastWriteTime;
                                sInfo.folder       = true;
                                SendMessage(*this, SEARCH_FOUND, 1, reinterpret_cast<LPARAM>(&sInfo));
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
    m_dwThreadRunning = false;

    // refresh cursor
    POINT pt;
    GetCursorPos(&pt);
    SetCursorPos(pt.x, pt.y);

    PostMessage(m_hwnd, WM_GREPWIN_THREADEND, 0, 0);

    return 0L;
}

void CSearchDlg::SetSearchPath(const std::wstring& path)
{
    m_searchPath = path;
    SearchReplace(m_searchPath, L"/", L"\\");
}

void CSearchDlg::SetFileMask(const std::wstring& mask, bool reg)
{
    m_patternRegex      = mask;
    m_bUseRegexForPaths = reg;
    m_patternRegexC     = true;
}

void CSearchDlg::SetDirExcludeRegexMask(const std::wstring& mask)
{
    m_excludeDirsPatternRegex  = mask;
    m_excludeDirsPatternRegexC = true;
}

void CSearchDlg::SetUseRegex(bool reg)
{
    m_bUseRegex  = reg;
    m_bUseRegexC = true;
}

void CSearchDlg::SetPreset(const std::wstring& preset)
{
    CBookmarks bookmarks;
    bookmarks.Load();
    auto bk = bookmarks.GetBookmark(preset);
    if (bk.Name == preset)
    {
        auto removeQuotes = [](std::wstring& str) {
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
        m_bKeepFileDate           = bk.KeepFileDate;
        m_bWholeWords             = bk.WholeWords;
        m_bUTF8                   = bk.Utf8;
        m_bForceBinary            = bk.Binary;
        m_bIncludeSystem          = bk.IncludeSystem;
        m_bIncludeSubfolders      = bk.IncludeFolder;
        m_bIncludeSymLinks        = bk.IncludeSymLinks;
        m_bIncludeHidden          = bk.IncludeHidden;
        m_bIncludeBinary          = bk.IncludeBinary;
        m_excludeDirsPatternRegex = bk.ExcludeDirs;
        m_patternRegex            = bk.FileMatch;
        m_bUseRegexForPaths       = bk.FileMatchRegex;
        if (!bk.Path.empty())
            m_searchPath = bk.Path;

        m_bIncludeSystemC          = true;
        m_bIncludeHiddenC          = true;
        m_bIncludeSubfoldersC      = true;
        m_bIncludeSymLinksC        = true;
        m_bIncludeBinaryC          = true;
        m_bCreateBackupC           = true;
        m_bCreateBackupInFoldersC  = true;
        m_bKeepFileDateC           = true;
        m_bWholeWordsC             = true;
        m_bUTF8C                   = true;
        m_bCaseSensitiveC          = true;
        m_bDotMatchesNewlineC      = true;
        m_patternRegexC            = true;
        m_excludeDirsPatternRegexC = true;

        removeQuotes(m_searchString);
        removeQuotes(m_replaceString);
        removeQuotes(m_excludeDirsPatternRegex);
        removeQuotes(m_patternRegex);
    }
}

void CSearchDlg::SetCaseSensitive(bool bSet)
{
    m_bCaseSensitiveC = true;
    m_bCaseSensitive  = bSet;
}

void CSearchDlg::SetMatchesNewline(bool bSet)
{
    m_bDotMatchesNewlineC = true;
    m_bDotMatchesNewline  = bSet;
}

void CSearchDlg::SetCreateBackups(bool bSet)
{
    m_bCreateBackupC         = true;
    m_bCreateBackup          = bSet;
    m_bConfirmationOnReplace = false;
}

void CSearchDlg::SetCreateBackupsInFolders(bool bSet)
{
    m_bCreateBackupInFoldersC = true;
    m_bCreateBackupInFolders  = bSet;
    SetCreateBackups(bSet);
}

void CSearchDlg::SetKeepFileDate(bool bSet)
{
    m_bKeepFileDateC = true;
    m_bKeepFileDate  = bSet;
}

void CSearchDlg::SetWholeWords(bool bSet)
{
    m_bWholeWordsC = true;
    m_bWholeWords  = bSet;
}

void CSearchDlg::SetUTF8(bool bSet)
{
    m_bUTF8C       = true;
    m_bUTF8        = bSet;
    m_bForceBinary = false;
}

void CSearchDlg::SetBinary(bool bSet)
{
    m_bUTF8C       = true;
    m_bForceBinary = bSet;
    m_bUTF8        = false;
}

void CSearchDlg::SetSize(uint64_t size, int cmp)
{
    m_bSizeC   = true;
    m_lSize    = size;
    m_sizeCmp  = cmp;
    m_bAllSize = (size == static_cast<uint64_t>(-1));
}

void CSearchDlg::SetIncludeSystem(bool bSet)
{
    m_bIncludeSystemC = true;
    m_bIncludeSystem  = bSet;
}

void CSearchDlg::SetIncludeHidden(bool bSet)
{
    m_bIncludeHiddenC = true;
    m_bIncludeHidden  = bSet;
}

void CSearchDlg::SetIncludeSubfolders(bool bSet)
{
    m_bIncludeSubfoldersC = true;
    m_bIncludeSubfolders  = bSet;
}

void CSearchDlg::SetIncludeSymLinks(bool bSet)
{
    m_bIncludeSymLinksC = true;
    m_bIncludeSymLinks  = bSet;
}

void CSearchDlg::SetIncludeBinary(bool bSet)
{
    m_bIncludeBinaryC = true;
    m_bIncludeBinary  = bSet;
}

void CSearchDlg::SetDateLimit(int dateLimit, FILETIME t1, FILETIME t2)
{
    m_bDateLimitC = true;
    m_dateLimit   = dateLimit;
    m_date1       = t1;
    m_date2       = t2;
}

bool CSearchDlg::MatchPath(LPCTSTR pathBuf) const
{
    bool        bPattern = false;
    // find start of pathname
    const auto* pName    = wcsrchr(pathBuf, '\\');
    if (pName == nullptr)
        pName = pathBuf;
    else
        pName++; // skip the last '\\' char
    if (m_bUseRegexForPaths)
    {
        if (m_patterns.empty())
            bPattern = true;
        else
        {
            if (grepWinMatchI(m_patternRegex, pName))
                bPattern = true;
            // for a regex check, also test with the full path
            else if (grepWinMatchI(m_patternRegex, pathBuf))
                bPattern = true;
        }
    }
    else
    {
        if (!m_patterns.empty())
        {
            if (m_patterns[0].size() && (m_patterns[0][0] == '-'))
                bPattern = true;

            std::wstring fName = pName;
            std::ranges::transform(fName, fName.begin(), ::towlower);

            for (const auto& pattern : m_patterns)
            {
                if (!pattern.empty() && pattern.at(0) == '-')
                    bPattern = bPattern && !wcswildcmp(&(pattern)[1], fName.c_str());
                else
                    bPattern = bPattern || wcswildcmp(pattern.c_str(), fName.c_str());
            }
        }
        else
            bPattern = true;
    }
    return bPattern;
}

void CSearchDlg::SearchFile(CSearchInfo sInfo, const std::wstring& searchRoot, bool bSearchAlways, bool bIncludeBinary, bool bUseRegex, bool bCaseSensitive, bool bDotMatchesNewline, const std::wstring& searchString, const std::wstring& searchStringUtf16Le, std::atomic_bool& bCancelled)
{
    int          nFound            = 0;
    // we keep it simple:
    // files bigger than 30MB are considered binary. Binary files are searched
    // as if they're ANSI text files.
    std::wstring localSearchString = searchString;

    SearchReplace(localSearchString, L"${filepath}", sInfo.filePath);
    std::wstring fileNameFull = sInfo.filePath.substr(sInfo.filePath.find_last_of('\\') + 1);
    auto         dotPos       = fileNameFull.find_last_of('.');
    if (dotPos != std::string::npos)
    {
        std::wstring filename = fileNameFull.substr(0, dotPos);
        SearchReplace(localSearchString, L"${filename}", filename);
        if (fileNameFull.size() > dotPos)
        {
            std::wstring fileExt = fileNameFull.substr(dotPos + 1);
            SearchReplace(localSearchString, L"${fileext}", fileExt);
        }
    }
    if (!bUseRegex)
    {
        using namespace std::string_literals;
        for (const auto& c : {L"\\"s, L"^"s, L"$"s, L"."s, L"|"s, L"?"s, L"*"s, L"+"s, L"("s, L")"s, L"["s, L"{"s})
        {
            SearchReplace(localSearchString, c, L"\\" + c);
        }
        if (m_bWholeWords)
            localSearchString = L"\\b" + localSearchString + L"\\b";
    }

    CTextFile              textFile;
    CTextFile::UnicodeType type        = CTextFile::AutoType;
    bool                   bLoadResult = false;
    if (m_bForceBinary)
    {
        type = CTextFile::Binary;
    }
    else
    {
        ProfileTimer profile((L"file load and parse: " + sInfo.filePath).c_str());
        auto         nNullCount = bPortable ? _wtoi(g_iniFile.GetValue(L"settings", L"nullbytes", L"0")) : static_cast<int>(static_cast<DWORD>(CRegStdDWORD(L"Software\\grepWin\\nullbytes", 0)));
        if (nNullCount > 0)
        {
            constexpr __int64 oneMB = 1024 * 1024;
            auto              megs  = sInfo.fileSize / oneMB;
            textFile.SetNullbyteCountForBinary(nNullCount * (static_cast<int>(megs) + 1));
        }
        bLoadResult = textFile.Load(sInfo.filePath.c_str(), type, m_bUTF8, bCancelled);
    }
    sInfo.encoding = type;
    if ((bLoadResult) && ((type != CTextFile::Binary) || (bIncludeBinary) || bSearchAlways))
    {
        sInfo.readError = false;
        std::wstring::const_iterator start, end;
        start = textFile.GetFileString().begin();
        end   = textFile.GetFileString().end();
        boost::match_results<std::wstring::const_iterator> what;
        try
        {
            int ft = boost::regex::normal;
            if (!bCaseSensitive)
                ft |= boost::regbase::icase;
            boost::wregex                                      expression = boost::wregex(localSearchString, ft);
            boost::match_results<std::wstring::const_iterator> whatC;
            boost::match_flag_type                             flags = boost::match_default | boost::format_all;
            if (!bDotMatchesNewline)
                flags |= boost::match_not_dot_newline;
            long prevLineStart = 0;
            long prevLineEnd   = 0;
            while (!bCancelled && regex_search(start, end, whatC, expression, flags))
            {
                if (whatC[0].matched)
                {
                    nFound++;
                    if (m_bNotSearch)
                        break;
                    long lineStart = textFile.LineFromPosition(static_cast<long>(whatC[0].first - textFile.GetFileString().begin()));
                    long lineEnd   = textFile.LineFromPosition(static_cast<long>(whatC[0].second - textFile.GetFileString().begin()));
                    if ((lineStart != prevLineStart) || (lineEnd != prevLineEnd))
                    {
                        for (long l = lineStart; l <= lineEnd; ++l)
                        {
                            auto sLine = textFile.GetLineString(l);
                            if (m_bCaptureSearch)
                            {
                                auto out = whatC.format(m_replaceString, flags);
                                sInfo.matchLines.push_back(std::move(out));
                            }
                            else
                                sInfo.matchLines.push_back(std::move(sLine));
                            sInfo.matchLinesNumbers.push_back(l);
                        }
                    }
                    ++sInfo.matchCount;
                    prevLineStart = lineStart;
                    prevLineEnd   = lineEnd;
                }
                // update search position:
                if (start == whatC[0].second)
                {
                    if (start == end)
                        break;
                    ++start;
                }
                else
                    start = whatC[0].second;
                // update flags:
                flags |= boost::match_prev_avail;
                flags |= boost::match_not_bob;
            }
            if (type == CTextFile::Binary)
            {
                boost::wregex expressionUtf16 = boost::wregex(searchStringUtf16Le, ft);
                start                         = textFile.GetFileString().begin();
                end                           = textFile.GetFileString().end();

                while (!bCancelled && regex_search(start, end, whatC, expressionUtf16, flags))
                {
                    if (whatC[0].matched)
                    {
                        nFound++;
                        if (m_bNotSearch)
                            break;
                        long lineStart = textFile.LineFromPosition(static_cast<long>(whatC[0].first - textFile.GetFileString().begin()));
                        long lineEnd   = textFile.LineFromPosition(static_cast<long>(whatC[0].second - textFile.GetFileString().begin()));
                        if (m_bCaptureSearch)
                        {
                            auto out = whatC.format(m_replaceString, flags);
                            sInfo.matchLines.push_back(out);
                            sInfo.matchLinesNumbers.push_back(lineStart);
                        }
                        else
                        {
                            if ((lineStart != prevLineStart) || (lineEnd != prevLineEnd))
                            {
                                for (long l = lineStart; l <= lineEnd; ++l)
                                {
                                    sInfo.matchLines.push_back(textFile.GetLineString(l));
                                    sInfo.matchLinesNumbers.push_back(l);
                                }
                            }
                        }
                        ++sInfo.matchCount;
                        prevLineStart = lineStart;
                        prevLineEnd   = lineEnd;
                    }
                    // update search position:
                    if (start == whatC[0].second)
                    {
                        if (start == end)
                            break;
                        ++start;
                    }
                    else
                        start = whatC[0].second;
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
                replaceFmt.SetReplacePair(L"${filepath}", sInfo.filePath);
                std::wstring fileNameFullW = sInfo.filePath.substr(sInfo.filePath.find_last_of('\\') + 1);
                auto         dotPosW       = fileNameFullW.find_last_of('.');
                if (dotPosW != std::string::npos)
                {
                    std::wstring filename = fileNameFullW.substr(0, dotPosW);
                    replaceFmt.SetReplacePair(L"${filename}", filename);
                    if (fileNameFullW.size() > dotPosW)
                    {
                        std::wstring fileExt = fileNameFullW.substr(dotPosW + 1);
                        replaceFmt.SetReplacePair(L"${fileext}", fileExt);
                    }
                }
                std::wstring replaced = regex_replace(textFile.GetFileString(), expression, replaceFmt, flags);
                if (replaced.compare(textFile.GetFileString()))
                {
                    textFile.SetFileContent(replaced);
                    if (m_bCreateBackup)
                    {
                        std::wstring backupFile     = sInfo.filePath + L".bak";
                        auto         backupInFolder = bPortable
                                                          ? (_wtoi(g_iniFile.GetValue(L"settings", L"backupinfolder", L"0")) != 0)
                                                          : (static_cast<DWORD>(m_regBackupInFolder) != 0);
                        if (backupInFolder)
                        {
                            std::wstring backupFolder = searchRoot + L"\\grepWin_backup\\";
                            backupFolder += sInfo.filePath.substr(searchRoot.size() + 1);
                            backupFolder = CPathUtils::GetParentDirectory(backupFolder);
                            CPathUtils::CreateRecursiveDirectory(backupFolder);
                            backupFile = backupFolder + L"\\" + CPathUtils::GetFileName(sInfo.filePath);
                        }
                        CopyFile(sInfo.filePath.c_str(), backupFile.c_str(), FALSE);
                        m_backupAndTempFiles.insert(backupFile);
                    }
                    if (!textFile.Save(sInfo.filePath.c_str(), m_bKeepFileDate))
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
                            FILETIME creationTime{};
                            FILETIME lastAccessTime{};
                            FILETIME lastWriteTime{};
                            if (m_bKeepFileDate)
                            {
                                CAutoFile hFile = CreateFile(sInfo.filePath.c_str(), GENERIC_READ, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
                                if (hFile)
                                {
                                    GetFileTime(hFile, &creationTime, &lastAccessTime, &lastWriteTime);
                                }
                            }

                            DWORD origAttributes = GetFileAttributes(sInfo.filePath.c_str());
                            DWORD newAttributes  = origAttributes & (~(FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM));
                            SetFileAttributes(sInfo.filePath.c_str(), newAttributes);
                            bool bRet = textFile.Save(sInfo.filePath.c_str(), false);
                            // restore the attributes
                            SetFileAttributes(sInfo.filePath.c_str(), origAttributes);
                            if (m_bKeepFileDate)
                            {
                                bool success = false;
                                int  retries = 5;
                                while (!success && retries >= 0)
                                {
                                    {
                                        CAutoFile hFile = CreateFile(sInfo.filePath.c_str(), GENERIC_WRITE, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
                                        if (hFile)
                                        {
                                            success = !!SetFileTime(hFile, &creationTime, &lastAccessTime, &lastWriteTime);
                                        }
                                    }
                                    --retries;
                                    if (!success)
                                        Sleep(50);
                                }
                                assert(success);
                            }
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
        catch (const std::exception& ex)
        {
            sInfo.exception = CUnicodeUtils::StdGetUnicode(ex.what());
            SendMessage(*this, SEARCH_FOUND, 0, reinterpret_cast<LPARAM>(&sInfo));
            SendMessage(*this, SEARCH_PROGRESS, 1, 0);
        }
    }
    else
    {
        if (type == CTextFile::AutoType)
        {
            sInfo.readError = true;
            SendMessage(*this, SEARCH_FOUND, 0, reinterpret_cast<LPARAM>(&sInfo));
            SendMessage(*this, SEARCH_PROGRESS, 1, 0);
            return;
        }

        // file is either too big or binary.
        // in any case, use the search function that uses a file iterator
        // instead of a string iterator to reduce the memory consumption

        if ((type != CTextFile::Binary) || bIncludeBinary || bSearchAlways || m_bForceBinary)
        {
            sInfo.encoding        = type;
            std::string filePath  = CUnicodeUtils::StdGetANSI(sInfo.filePath);
            std::string searchFor = (type == CTextFile::Ansi) ? CUnicodeUtils::StdGetANSI(searchString) : CUnicodeUtils::StdGetUTF8(searchString);

            if (!bUseRegex)
            {
                searchFor = "\\Q";
                searchFor += CUnicodeUtils::StdGetUTF8(searchString);
                searchFor += "\\E";
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
                boost::regex       expression = boost::regex(searchFor, ft);
                std::vector<DWORD> matchLinesNumbers;
                bool               bFound = false;
                {
                    boost::spirit::classic::file_iterator<>                       start(filePath.c_str());
                    boost::spirit::classic::file_iterator<>                       fBeg = start;
                    boost::spirit::classic::file_iterator<>                       end  = start.make_end();
                    boost::match_results<boost::spirit::classic::file_iterator<>> whatC;
                    while (boost::regex_search(start, end, whatC, expression, flags) && !bCancelled)
                    {
                        nFound++;
                        if (m_bNotSearch)
                            break;
                        matchLinesNumbers.push_back(static_cast<DWORD>(whatC[0].first - fBeg));
                        ++sInfo.matchCount;
                        // update search position:
                        start = whatC[0].second;
                        // update flags:
                        flags |= boost::match_prev_avail;
                        flags |= boost::match_not_bob;
                        bFound = true;
                        if (bCancelled)
                            break;
                    }
                }
                if (type == CTextFile::Binary && !m_bReplace)
                {
                    boost::wregex                                                        expressionUtf16Le = boost::wregex(searchString, ft);
                    boost::spirit::classic::file_iterator<wchar_t>                       start(filePath.c_str());
                    boost::spirit::classic::file_iterator<wchar_t>                       fBeg = start;
                    boost::spirit::classic::file_iterator<wchar_t>                       end  = start.make_end();
                    boost::match_results<boost::spirit::classic::file_iterator<wchar_t>> whatC;
                    while (boost::regex_search(start, end, whatC, expressionUtf16Le, flags))
                    {
                        nFound++;
                        if (m_bNotSearch)
                            break;
                        matchLinesNumbers.push_back(static_cast<DWORD>(whatC[0].first - fBeg));
                        ++sInfo.matchCount;
                        // update search position:
                        start = whatC[0].second;
                        // update flags:
                        flags |= boost::match_prev_avail;
                        flags |= boost::match_not_bob;
                        bFound = true;
                        if (bCancelled)
                            break;
                    }
                }

                if (bFound && !bCancelled)
                {
                    if (!bLoadResult && (type != CTextFile::Binary) && !m_bNotSearch)
                    {
                        linePositions.clear();
                        // open the file and set up a vector of all lines
                        CAutoFile hFile;
                        int       retryCounter = 0;
                        do
                        {
                            if (retryCounter)
                                Sleep(20 + retryCounter * 50);
                            hFile = CreateFile(sInfo.filePath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                               nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
                            retryCounter++;
                        } while (!hFile && retryCounter < 5);
                        if (hFile)
                        {
                            auto   fBuf      = std::make_unique<char[]>(4096);
                            DWORD  bytesRead = 0;
                            size_t pos       = 0;
                            while (ReadFile(hFile, fBuf.get(), 4096, &bytesRead, nullptr))
                            {
                                if (bytesRead == 0)
                                    break;
                                for (DWORD br = 0; br < bytesRead; ++br)
                                {
                                    if (fBuf[br] == '\r')
                                    {
                                        ++br;
                                        ++pos;
                                        if (br < bytesRead)
                                        {
                                            if (fBuf[br] == '\n')
                                            {
                                                // crlf lineending
                                                auto lp            = linePositions.size();
                                                linePositions[pos] = static_cast<DWORD>(lp);
                                            }
                                            else
                                            {
                                                // cr lineending
                                                auto lp                = linePositions.size();
                                                linePositions[pos - 1] = static_cast<DWORD>(lp);
                                            }
                                        }
                                        else
                                            break;
                                    }
                                    else if (fBuf[br] == '\n')
                                    {
                                        // lf lineending
                                        auto lp            = linePositions.size();
                                        linePositions[pos] = static_cast<DWORD>(lp);
                                    }
                                    ++pos;
                                }
                            }
                            for (size_t mp = 0; mp < matchLinesNumbers.size(); ++mp)
                            {
                                auto fp = linePositions.lower_bound(matchLinesNumbers[mp]);
                                if (fp != linePositions.end())
                                    matchLinesNumbers[mp] = fp->second;
                            }
                        }
                    }
                    sInfo.matchLinesNumbers = matchLinesNumbers;

                    if (m_bReplace)
                    {
                        std::wstring backupFile = sInfo.filePath + L".bak";
                        if (m_bCreateBackup)
                        {
                            if (static_cast<DWORD>(m_regBackupInFolder))
                            {
                                std::wstring backupFolder = searchRoot + L"\\grepWin_backup\\";
                                backupFolder += sInfo.filePath.substr(searchRoot.size() + 1);
                                backupFolder = CPathUtils::GetParentDirectory(backupFolder);
                                CPathUtils::CreateRecursiveDirectory(backupFolder);
                                backupFile = backupFolder + L"\\" + CPathUtils::GetFileName(sInfo.filePath);
                            }
                            CopyFile(sInfo.filePath.c_str(), backupFile.c_str(), FALSE);
                            m_backupAndTempFiles.insert(backupFile);
                        }

                        flags &= ~boost::match_prev_avail;
                        flags &= ~boost::match_not_bob;
                        RegexReplaceFormatterA replaceFmt(CUnicodeUtils::StdGetUTF8(m_replaceString));
                        replaceFmt.SetReplacePair("${filepath}", CUnicodeUtils::StdGetUTF8(sInfo.filePath));
                        std::string fileNameFullA = CUnicodeUtils::StdGetUTF8(sInfo.filePath.substr(sInfo.filePath.find_last_of('\\') + 1));
                        auto        dotPosA       = fileNameFullA.find_last_of('.');
                        if (dotPosA != std::string::npos)
                        {
                            std::string filename = fileNameFullA.substr(0, dotPosA);
                            replaceFmt.SetReplacePair("${filename}", filename);
                            if (fileNameFull.size() > dotPosA)
                            {
                                std::string fileExt = fileNameFullA.substr(dotPosA + 1);
                                replaceFmt.SetReplacePair("${fileext}", fileExt);
                            }
                        }

                        FILETIME creationTime{};
                        FILETIME lastAccessTime{};
                        FILETIME lastWriteTime{};
                        if (m_bKeepFileDate)
                        {
                            CAutoFile hFile = CreateFile(sInfo.filePath.c_str(), GENERIC_READ, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
                            if (hFile)
                            {
                                GetFileTime(hFile, &creationTime, &lastAccessTime, &lastWriteTime);
                            }
                            else
                                assert(false);
                        }

                        std::string filePathOut = m_bCreateBackup ? filePath : filePath + ".grepwinreplaced";
                        if (!m_bCreateBackup)
                            m_backupAndTempFiles.insert(sInfo.filePath + L".grepwinreplaced");
                        boost::iostreams::mapped_file_source replaceInFile(m_bCreateBackup ? CUnicodeUtils::StdGetANSI(backupFile).c_str() : filePath.c_str());
                        std::ofstream                        os(filePathOut.c_str(), std::ios::out | std::ios::trunc | std::ios::binary);
                        std::ostream_iterator<char, char>    out(os);
                        regex_replace(out, replaceInFile.begin(), replaceInFile.end(), expression, replaceFmt, flags);
                        os.close();
                        replaceInFile.close();
                        if (!m_bCreateBackup)
                            MoveFileExA(filePathOut.c_str(), filePath.c_str(), MOVEFILE_REPLACE_EXISTING);
                        if (m_bKeepFileDate)
                        {
                            bool success = false;
                            int  retries = 5;
                            while (!success && retries >= 0)
                            {
                                {
                                    CAutoFile hFile = CreateFile(sInfo.filePath.c_str(), GENERIC_WRITE, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
                                    if (hFile)
                                    {
                                        success = !!SetFileTime(hFile, &creationTime, &lastAccessTime, &lastWriteTime);
                                    }
                                }
                                --retries;
                                if (!success)
                                    Sleep(50);
                            }
                            assert(success);
                        }
                    }
                }
            }
            catch (const std::exception& ex)
            {
                sInfo.exception = CUnicodeUtils::StdGetUnicode(ex.what());
                SendMessage(*this, SEARCH_FOUND, 0, reinterpret_cast<LPARAM>(&sInfo));
                SendMessage(*this, SEARCH_PROGRESS, 1, 0);
            }
            catch (...)
            {
                SendMessage(*this, SEARCH_PROGRESS, 0, 0);
                return;
            }
        }
    }
    if (m_bNotSearch)
    {
        if (nFound == 0)
            SendMessage(*this, SEARCH_FOUND, nFound, reinterpret_cast<LPARAM>(&sInfo));
        SendMessage(*this, SEARCH_PROGRESS, (nFound >= 0), 0);
        return;
    }

    if (nFound >= 0)
        SendMessage(*this, SEARCH_FOUND, nFound, reinterpret_cast<LPARAM>(&sInfo));
    SendMessage(*this, SEARCH_PROGRESS, (nFound >= 0), 0);
}

DWORD WINAPI SearchThreadEntry(LPVOID lpParam)
{
    CSearchDlg* pThis = static_cast<CSearchDlg*>(lpParam);
    if (pThis)
        return pThis->SearchThread();
    return 0L;
}

void CSearchDlg::formatDate(wchar_t dateNative[], const FILETIME& fileTime, bool forceShortFmt) const
{
    dateNative[0] = '\0';

    // Convert UTC to local time
    SYSTEMTIME systemTime;
    FileTimeToSystemTime(&fileTime, &systemTime);

    static TIME_ZONE_INFORMATION timeZone = {-1};
    if (timeZone.Bias == -1)
        GetTimeZoneInformation(&timeZone);

    SYSTEMTIME localSystime;
    SystemTimeToTzSpecificLocalTime(&timeZone, &systemTime, &localSystime);

    wchar_t timeBuf[GREPWIN_DATEBUFFER] = {0};
    wchar_t dateBuf[GREPWIN_DATEBUFFER] = {0};

    LCID    locale                      = MAKELCID(MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), SORT_DEFAULT);

    /// reusing this instance is vital for \ref formatDate performance

    DWORD   flags                       = forceShortFmt ? DATE_SHORTDATE : DATE_LONGDATE;

    GetDateFormat(locale, flags, &localSystime, nullptr, dateBuf, GREPWIN_DATEBUFFER);
    GetTimeFormat(locale, 0, &localSystime, nullptr, timeBuf, GREPWIN_DATEBUFFER);
    wcsncat_s(dateNative, GREPWIN_DATEBUFFER, dateBuf, GREPWIN_DATEBUFFER);
    wcsncat_s(dateNative, GREPWIN_DATEBUFFER, L" ", GREPWIN_DATEBUFFER);
    wcsncat_s(dateNative, GREPWIN_DATEBUFFER, timeBuf, GREPWIN_DATEBUFFER);
}

int CSearchDlg::CheckRegex()
{
    m_isRegexValid                 = true;
    m_isExcludeDirsRegexValid      = true;
    m_isFileNameMatchingRegexValid = true;

    auto buf                       = GetDlgItemText(IDC_SEARCHTEXT);
    int  len                       = static_cast<int>(wcslen(buf.get()));
    int  lenSearchText             = len;
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
                bValid         = false;
                m_isRegexValid = false;
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
                RedrawWindow(GetDlgItem(*this, IDC_SEARCHTEXT), nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE);
            }
            else
            {
                SetDlgItemText(*this, IDC_REGEXOKLABEL, TranslatedString(hResource, IDS_REGEXINVALID).c_str());
                DialogEnableWindow(IDOK, false);
                DialogEnableWindow(IDC_REPLACE, false);
                DialogEnableWindow(IDC_CREATEBACKUP, false);
                RedrawWindow(GetDlgItem(*this, IDC_SEARCHTEXT), nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE);
                lenSearchText = 0;
            }
        }
        else
        {
            SetDlgItemText(*this, IDC_REGEXOKLABEL, L"");
            DialogEnableWindow(IDOK, true);
            DialogEnableWindow(IDC_REPLACE, false);
            DialogEnableWindow(IDC_CREATEBACKUP, false);
            RedrawWindow(GetDlgItem(*this, IDC_SEARCHTEXT), nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE);
        }
    }
    else
    {
        SetDlgItemText(*this, IDC_REGEXOKLABEL, L"");
        DialogEnableWindow(IDOK, true);
        DialogEnableWindow(IDC_REPLACE, len > 0);
        DialogEnableWindow(IDC_CREATEBACKUP, len > 0);
        RedrawWindow(GetDlgItem(*this, IDC_SEARCHTEXT), nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE);
    }

    {
        buf = GetDlgItemText(IDC_EXCLUDEDIRSPATTERN);
        len = static_cast<int>(wcslen(buf.get()));
        if (len)
        {
            try
            {
                std::wstring  sRegex     = buf.get();
                boost::wregex expression = boost::wregex(sRegex);
            }
            catch (const std::exception&)
            {
                m_isExcludeDirsRegexValid = false;
            }
        }
        RedrawWindow(GetDlgItem(*this, IDC_EXCLUDEDIRSPATTERN), nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE);
    }
    if (IsDlgButtonChecked(*this, IDC_FILEPATTERNREGEX) == BST_CHECKED)
    {
        buf = GetDlgItemText(IDC_PATTERN);
        len = static_cast<int>(wcslen(buf.get()));
        if (len)
        {
            try
            {
                std::wstring  sRegex     = buf.get();
                boost::wregex expression = boost::wregex(sRegex);
            }
            catch (const std::exception&)
            {
                m_isFileNameMatchingRegexValid = false;
            }
        }
    }
    RedrawWindow(GetDlgItem(*this, IDC_PATTERN), nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE);

    return lenSearchText;
}

void CSearchDlg::AutoSizeAllColumns()
{
    HWND             hListControl          = GetDlgItem(*this, IDC_RESULTLIST);
    auto             headerCtrl            = ListView_GetHeader(hListControl);
    int              nItemCount            = ListView_GetItemCount(hListControl);
    wchar_t          textBuf[MAX_PATH * 4] = {0};
    std::vector<int> colWidths;
    if (headerCtrl)
    {
        int  maxCol   = Header_GetItemCount(headerCtrl) - 1;
        int  imgWidth = 0;
        auto hImgList = ListView_GetImageList(hListControl, LVSIL_SMALL);
        if ((hImgList) && (ImageList_GetImageCount(hImgList)))
        {
            IMAGEINFO imgInfo;
            ImageList_GetImageInfo(hImgList, 0, &imgInfo);
            imgWidth = (imgInfo.rcImage.right - imgInfo.rcImage.left) + CDPIAware::Instance().Scale(*this, 3); // 3 pixels between icon and text
        }
        for (int col = 0; col <= maxCol; col++)
        {
            HDITEM hdi     = {0};
            hdi.mask       = HDI_TEXT;
            hdi.pszText    = textBuf;
            hdi.cchTextMax = _countof(textBuf);
            Header_GetItem(headerCtrl, col, &hdi);
            int cx  = ListView_GetStringWidth(hListControl, hdi.pszText) + 20; // 20 pixels for col separator and margin

            int inc = max(1, nItemCount / 1000);
            for (int index = 0; index < nItemCount; index = index + inc)
            {
                // get the width of the string and add 14 pixels for the column separator and margins
                ListView_GetItemText(hListControl, index, col, textBuf, _countof(textBuf));
                int lineWidth = ListView_GetStringWidth(hListControl, textBuf) + CDPIAware::Instance().Scale(*this, 14);
                // add the image size
                if (col == 0)
                    lineWidth += imgWidth;
                if (cx < lineWidth)
                    cx = lineWidth;
            }
            colWidths.push_back(cx);
        }
    }
    bool fileList = (IsDlgButtonChecked(*this, IDC_RESULTFILES) == BST_CHECKED);
    if (!fileList)
    {
        RECT rc{};
        ListView_GetItemRect(hListControl, 0, &rc, LVIR_BOUNDS);
        auto itemWidth = rc.right - rc.left;
        if (nItemCount > 0)
        {
            GetClientRect(hListControl, &rc);
            if (itemWidth < rc.right - rc.left)
            {
                itemWidth = rc.right - rc.left;
            }
        }
        auto totalWidth = std::accumulate(colWidths.begin(), colWidths.end(), 0);
        totalWidth -= colWidths[colWidths.size() - 2];
        auto textWidth = itemWidth - totalWidth;
        if (textWidth > 0)
            colWidths[colWidths.size() - 2] = textWidth;
        else
        {
            colWidths[colWidths.size() - 1] = 100;
            totalWidth                      = std::accumulate(colWidths.begin(), colWidths.end(), 0);
            totalWidth -= colWidths[colWidths.size() - 2];
            textWidth = itemWidth - totalWidth;
            if (textWidth > 0)
                colWidths[colWidths.size() - 2] = textWidth;
        }
    }
    int col = 0;
    for (const auto& colWidth : colWidths)
    {
        ListView_SetColumnWidth(hListControl, col, colWidth);
        ++col;
    }
}

int CSearchDlg::GetSelectedListIndex(int index)
{
    bool fileList = (IsDlgButtonChecked(*this, IDC_RESULTFILES) == BST_CHECKED);
    return GetSelectedListIndex(fileList, index);
}

int CSearchDlg::GetSelectedListIndex(bool fileList, int index) const
{
    if (fileList)
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
    bool doCheck         = true;
    if (bPortable)
        doCheck = !!_wtoi(g_iniFile.GetValue(L"global", L"CheckForUpdates", L"1"));
    else
        doCheck = !!static_cast<DWORD>(CRegStdDWORD(L"Software\\grepWin\\CheckForUpdates", 1));
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
            last = _wtoll(static_cast<std::wstring>(CRegStdString(L"Software\\grepWin\\CheckForUpdatesLast", L"0")).c_str());
        }
        double days = std::difftime(now, last) / (60LL * 60LL * 24LL);
        if ((days >= 7.0) || force)
        {
            std::wstring tempFile  = CTempFiles::Instance().GetTempFilePath(true);

            std::wstring sCheckURL = L"https://raw.githubusercontent.com/stefankueng/grepWin/main/version.txt";
            HRESULT      res       = URLDownloadToFile(nullptr, sCheckURL.c_str(), tempFile.c_str(), 0, nullptr);
            if (res == S_OK)
            {
                if (bPortable)
                {
                    g_iniFile.SetValue(L"global", L"CheckForUpdatesLast", std::to_wstring(now).c_str());
                }
                else
                {
                    // ReSharper disable once CppEntityAssignedButNoRead
                    auto regLast = CRegStdString(L"Software\\grepWin\\CheckForUpdatesLast", L"0");
                    regLast      = std::to_wstring(now);
                }
                std::ifstream file;
                file.open(tempFile.c_str());
                if (file.good())
                {
                    char line[1024];
                    file.getline(line, sizeof(line));
                    auto verLine    = CUnicodeUtils::StdGetUnicode(line);
                    bNewerAvailable = IsVersionNewer(verLine);
                    file.getline(line, sizeof(line));
                    auto updateUrl = CUnicodeUtils::StdGetUnicode(line);
                    if (bNewerAvailable)
                    {
                        if (bPortable)
                        {
                            g_iniFile.SetValue(L"global", L"CheckForUpdatesVersion", verLine.c_str());
                            g_iniFile.SetValue(L"global", L"CheckForUpdatesUrl", updateUrl.c_str());
                        }
                        else
                        {
                            // ReSharper disable once CppEntityAssignedButNoRead
                            auto regVersion   = CRegStdString(L"Software\\grepWin\\CheckForUpdatesVersion", L"");
                            regVersion        = verLine;
                            // ReSharper disable once CppEntityAssignedButNoRead
                            auto regUpdateUrl = CRegStdString(L"Software\\grepWin\\CheckForUpdatesUrl", L"");
                            regUpdateUrl      = updateUrl;
                        }
                        ShowUpdateAvailable();
                    }
                }
                file.close();
                DeleteFile(tempFile.c_str());
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

bool CSearchDlg::IsVersionNewer(const std::wstring& sVer) const
{
    int            major = 0;
    int            minor = 0;
    int            micro = 0;
    int            build = 0;

    const wchar_t* pLine = sVer.c_str();

    major                = _wtoi(pLine);
    pLine                = wcschr(pLine, '.');
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

bool CSearchDlg::CloneWindow()
{
    if (!SaveSettings())
        return false;
    if (bPortable)
    {
        FILE* pFile = nullptr;
        _wfopen_s(&pFile, g_iniPath.c_str(), L"wb");
        g_iniFile.SaveFile(pFile);
        fclose(pFile);
    }

    std::wstring arguments;
    arguments += CStringUtils::Format(L" /searchpath:\"%s\"", m_searchPath.c_str());
    arguments += CStringUtils::Format(L" /searchfor:\"%s\"", m_searchString.c_str());
    arguments += CStringUtils::Format(L" /replacewith:\"%s\"", m_replaceString.c_str());
    arguments += L" /new";
    auto             file = CPathUtils::GetModulePath();

    SHELLEXECUTEINFO sei  = {0};
    sei.cbSize            = sizeof(SHELLEXECUTEINFO);
    sei.lpVerb            = TEXT("open");
    sei.lpFile            = file.c_str();
    sei.lpParameters      = arguments.c_str();
    sei.nShow             = SW_SHOWNORMAL;
    ShellExecuteEx(&sei);
    return true;
}

std::wstring CSearchDlg::ExpandString(const std::wstring& replaceString) const
{
    // ${now,formatString}
    wchar_t buf[4096] = {};
    GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, nullptr, nullptr, buf, _countof(buf));
    std::wstring dateStr = buf;
    GetTimeFormat(LOCALE_USER_DEFAULT, 0, nullptr, nullptr, buf, _countof(buf));
    dateStr += L" - ";
    dateStr += buf;
    std::time_t                                        now        = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    int                                                ft         = boost::regex::normal;
    boost::wregex                                      expression = boost::wregex(L"\\$\\{now\\s*,?([^}]*)\\}", ft);
    boost::match_results<std::wstring::const_iterator> whatC;
    boost::match_flag_type                             flags        = boost::match_default | boost::format_all;
    auto                                               resultString = replaceString;
    try
    {
        while (regex_search(resultString.cbegin(), resultString.cend(), whatC, expression, flags))
        {
            if (whatC[0].matched)
            {
                auto         fullMatch = whatC.str();

                std::wstring formatStr;
                if (whatC.size() > 1)
                {
                    formatStr = whatC[1].str();
                    if (!formatStr.empty())
                    {
                        std::wstring formattedDateStr(4096, '\0');
                        struct tm    locTime;
                        _localtime64_s(&locTime, &now);
                        std::wcsftime(&formattedDateStr[0], formattedDateStr.size(), formatStr.c_str(), &locTime);
                        SearchReplace(resultString, fullMatch, formattedDateStr);
                    }
                }
                else
                {
                    SearchReplace(resultString, fullMatch, dateStr);
                }
            }
        }
    }
    catch (const std::exception&)
    {
    }
    return resultString;
}