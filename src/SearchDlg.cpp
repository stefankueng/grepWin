// grepWin - regex search and replace for Windows

// Copyright (C) 2007-2011 - Stefan Kueng

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
#include "AboutDlg.h"
#include "InfoDlg.h"
#include "DropFiles.h"
#include <string>
#include <iostream>
#include <fstream>
#include <iterator>
#include <algorithm>
#include <Commdlg.h>

#include <boost/regex.hpp>
#include <boost/spirit/include/classic_file_iterator.hpp>
using namespace std;

#define GREPWIN_DATEBUFFER 100

DWORD WINAPI SearchThreadEntry(LPVOID lpParam);

UINT CSearchDlg::GREPWIN_STARTUPMSG = RegisterWindowMessage(_T("grepWin_StartupMessage"));

CSearchDlg::CSearchDlg(HWND hParent) : m_searchedItems(0)
    , m_totalitems(0)
    , m_dwThreadRunning(FALSE)
    , m_Cancelled(FALSE)
    , m_bAscending(true)
    , m_pDropTarget(NULL)
    , m_hParent(hParent)
    , m_bExecuteImmediately(false)
    , m_bUseRegexForPaths(false)
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
    , m_bUTF8(false)
    , m_bUTF8C(false)
    , m_bCaseSensitive(false)
    , m_bCaseSensitiveC(false)
    , m_bDotMatchesNewline(false)
    , m_bDotMatchesNewlineC(false)
    , m_bSizeC(false)
    , m_regUseRegex(_T("Software\\grepWin\\UseRegex"), 1)
    , m_regAllSize(_T("Software\\grepWin\\AllSize"))
    , m_regSize(_T("Software\\grepWin\\Size"), 2000)
    , m_regSizeCombo(_T("Software\\grepWin\\SizeCombo"), 0)
    , m_regIncludeSystem(_T("Software\\grepWin\\IncludeSystem"))
    , m_regIncludeHidden(_T("Software\\grepWin\\IncludeHidden"))
    , m_regIncludeSubfolders(_T("Software\\grepWin\\IncludeSubfolders"), 1)
    , m_regIncludeBinary(_T("Software\\grepWin\\IncludeBinary"), 1)
    , m_regCreateBackup(_T("Software\\grepWin\\CreateBackup"))
    , m_regUTF8(_T("Software\\grepWin\\UTF8"))
    , m_regCaseSensitive(_T("Software\\grepWin\\CaseSensitive"))
    , m_regDotMatchesNewline(_T("Software\\grepWin\\DotMatchesNewline"))
    , m_regUseRegexForPaths(_T("Software\\grepWin\\UseFileMatchRegex"))
    , m_regPattern(_T("Software\\grepWin\\pattern"))
    , m_regExcludeDirsPattern(_T("Software\\grepWin\\ExcludeDirsPattern"))
    , m_regSearchPath(_T("Software\\grepWin\\searchpath"))
{
    m_startTime = GetTickCount();
}

CSearchDlg::~CSearchDlg(void)
{
    if (m_pDropTarget)
        delete m_pDropTarget;
}

LRESULT CSearchDlg::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    if (uMsg == GREPWIN_STARTUPMSG)
    {
        if ((GetTickCount() - 1000) < m_startTime)
        {
            m_startTime = GetTickCount();
            return TRUE;
        }
        return FALSE;
    }
    switch (uMsg)
    {
    case WM_INITDIALOG:
        {
            AddToolTip(IDC_PATTERN, _T("only files that match this pattern are searched.\r\nExample: *.cpp;*.h"));
            AddToolTip(IDC_EXCLUDEDIRSPATTERN, _T("you can exclude directories, e.g. CVS and images.\r\nExample: ^(CVS|images)$\r\nNote, '.svn' folders are 'hidden' on Windows, so they usually are not scanned."));
            AddToolTip(IDC_SEARCHPATH, _T("the path(s) which is searched recursively.\r\nSeparate paths with the | symbol.\r\nExample: c:\\temp|d:\\logs"));
            AddToolTip(IDC_DOTMATCHNEWLINE, _T("\\n is matched by '.'"));
            AddToolTip(IDC_SEARCHTEXT, _T("a regular expression used for searching. Press F1 for more info."));

            if (m_searchpath.empty())
            {
                m_searchpath = wstring(m_regSearchPath);
            }
            // expand a possible 'short' path
            DWORD ret = 0;
            ret = ::GetLongPathName(m_searchpath.c_str(), NULL, 0);
            if (ret)
            {
                TCHAR * pathbuf = new TCHAR[ret+2];
                ret = ::GetLongPathName(m_searchpath.c_str(), pathbuf, ret+1);
                m_searchpath = wstring(pathbuf, ret);
                delete pathbuf;
            }

            if (m_patternregex.size() == 0)
            {
                m_patternregex = wstring(m_regPattern);
                m_bUseRegexForPaths = !!DWORD(m_regUseRegexForPaths);
            }
            if (m_excludedirspatternregex.size() == 0)
                m_excludedirspatternregex = wstring(m_regExcludeDirsPattern);
            // initialize the controls
            SetDlgItemText(hwndDlg, IDC_SEARCHPATH, m_searchpath.c_str());
            SetDlgItemText(hwndDlg, IDC_SEARCHTEXT, m_searchString.c_str());
            SetDlgItemText(hwndDlg, IDC_EXCLUDEDIRSPATTERN, m_excludedirspatternregex.c_str());
            SetDlgItemText(hwndDlg, IDC_PATTERN, m_patternregex.c_str());
            SetDlgItemText(hwndDlg, IDC_REPLACETEXT, m_replaceString.c_str());

            // the path edit control should work as a drop target for files and folders
            HWND hSearchPath = GetDlgItem(hwndDlg, IDC_SEARCHPATH);
            m_pDropTarget = new CFileDropTarget(hSearchPath);
            RegisterDragDrop(hSearchPath, m_pDropTarget);
            // create the supported formats:
            FORMATETC ftetc={0};
            ftetc.cfFormat = CF_TEXT;
            ftetc.dwAspect = DVASPECT_CONTENT;
            ftetc.lindex = -1;
            ftetc.tymed = TYMED_HGLOBAL;
            m_pDropTarget->AddSuportedFormat(ftetc);
            ftetc.cfFormat=CF_HDROP;
            m_pDropTarget->AddSuportedFormat(ftetc);
            SHAutoComplete(GetDlgItem(*this, IDC_SEARCHPATH), SHACF_FILESYSTEM|SHACF_AUTOSUGGEST_FORCE_ON);

            m_AutoCompleteFilePatterns.Load(_T("Software\\grepWin\\History"), _T("FilePattern"));
            m_AutoCompleteFilePatterns.Init(GetDlgItem(hwndDlg, IDC_PATTERN));
            m_AutoCompleteExcludeDirsPatterns.Load(_T("Software\\grepWin\\History"), _T("ExcludeDirsPattern"));
            m_AutoCompleteExcludeDirsPatterns.Init(GetDlgItem(hwndDlg, IDC_EXCLUDEDIRSPATTERN));
            m_AutoCompleteSearchPatterns.Load(_T("Software\\grepWin\\History"), _T("SearchPattern"));
            m_AutoCompleteSearchPatterns.Init(GetDlgItem(hwndDlg, IDC_SEARCHTEXT));
            m_AutoCompleteReplacePatterns.Load(_T("Software\\grepWin\\History"), _T("ReplacePattern"));
            m_AutoCompleteReplacePatterns.Init(GetDlgItem(hwndDlg, IDC_REPLACETEXT));
            m_AutoCompleteSearchPaths.Load(_T("Software\\grepWin\\History"), _T("SearchPaths"));
            m_AutoCompleteSearchPaths.Init(GetDlgItem(hwndDlg, IDC_SEARCHPATH));

            // add an "About" entry to the system menu
            HMENU hSysMenu = GetSystemMenu(hwndDlg, FALSE);
            if (hSysMenu)
            {
                int menuItemsCount = GetMenuItemCount(hSysMenu);
                if (menuItemsCount > 2)
                {
                    InsertMenu(hSysMenu, menuItemsCount - 2, MF_STRING | MF_BYPOSITION, ID_ABOUTBOX, _T("&About grepWin..."));
                    InsertMenu(hSysMenu, menuItemsCount - 2, MF_SEPARATOR | MF_BYPOSITION, NULL, NULL);
                }
                else
                {
                    AppendMenu(hSysMenu, MF_SEPARATOR, NULL, NULL);
                    AppendMenu(hSysMenu, MF_STRING, ID_ABOUTBOX, _T("&About grepWin..."));
                }
            }

            TCHAR buf[MAX_PATH] = {0};
            if (m_bSizeC)
            {
                _stprintf_s(buf, MAX_PATH, _T("%ld"), m_lSize);
                SetDlgItemText(hwndDlg, IDC_SIZEEDIT, buf);
            }
            else
            {
                if (DWORD(m_regSize) > 0)
                {
                    _stprintf_s(buf, MAX_PATH, _T("%ld"), DWORD(m_regSize));
                    SetDlgItemText(hwndDlg, IDC_SIZEEDIT, buf);
                }
                else
                    SetDlgItemText(hwndDlg, IDC_SIZEEDIT, _T("2000"));
            }

            SendDlgItemMessage(hwndDlg, IDC_SIZECOMBO, CB_INSERTSTRING, (WPARAM)-1, (LPARAM)_T("less than"));
            SendDlgItemMessage(hwndDlg, IDC_SIZECOMBO, CB_INSERTSTRING, (WPARAM)-1, (LPARAM)_T("equal to"));
            SendDlgItemMessage(hwndDlg, IDC_SIZECOMBO, CB_INSERTSTRING, (WPARAM)-1, (LPARAM)_T("greater than"));
            if (!m_bIncludeSubfoldersC)
                m_bIncludeSubfolders = !!DWORD(m_regIncludeSubfolders);
            if (!m_bIncludeSystemC)
                m_bIncludeSystem = !!DWORD(m_regIncludeSystem);
            if (!m_bIncludeHiddenC)
                m_bIncludeHidden = !!DWORD(m_regIncludeHidden);
            if (!m_bIncludeBinaryC)
                m_bIncludeBinaryC = !!DWORD(m_regIncludeBinary);
            if (!m_bCaseSensitiveC)
                m_bCaseSensitive = !!DWORD(m_regCaseSensitive);
            if (!m_bDotMatchesNewlineC)
                m_bDotMatchesNewline = !!DWORD(m_regDotMatchesNewline);
            if (!m_bCreateBackupC)
                m_bCreateBackup = !!DWORD(m_regCreateBackup);
            if (!m_bUTF8C)
                m_bUTF8 = !!DWORD(m_regUTF8);
            if (!m_bDotMatchesNewlineC)
                m_bDotMatchesNewline = !!DWORD(m_regDotMatchesNewline);
            if (!m_bSizeC)
            {
                m_bAllSize = !!DWORD(m_regAllSize);
                m_sizeCmp = (int)DWORD(m_regSizeCombo);
            }
            SendDlgItemMessage(hwndDlg, IDC_SIZECOMBO, CB_SETCURSEL, m_sizeCmp, 0);

            SendDlgItemMessage(hwndDlg, IDC_INCLUDESUBFOLDERS, BM_SETCHECK, m_bIncludeSubfolders ? BST_CHECKED : BST_UNCHECKED, 0);
            SendDlgItemMessage(hwndDlg, IDC_CREATEBACKUP, BM_SETCHECK, m_bCreateBackup ? BST_CHECKED : BST_UNCHECKED, 0);
            SendDlgItemMessage(hwndDlg, IDC_UTF8, BM_SETCHECK, m_bUTF8 ? BST_CHECKED : BST_UNCHECKED, 0);
            SendDlgItemMessage(hwndDlg, IDC_INCLUDESYSTEM, BM_SETCHECK, m_bIncludeSystem ? BST_CHECKED : BST_UNCHECKED, 0);
            SendDlgItemMessage(hwndDlg, IDC_INCLUDEHIDDEN, BM_SETCHECK, m_bIncludeHidden ? BST_CHECKED : BST_UNCHECKED, 0);
            SendDlgItemMessage(hwndDlg, IDC_INCLUDEBINARY, BM_SETCHECK, m_bIncludeBinary ? BST_CHECKED : BST_UNCHECKED, 0);
            SendDlgItemMessage(hwndDlg, IDC_CASE_SENSITIVE, BM_SETCHECK, m_bCaseSensitive ? BST_CHECKED : BST_UNCHECKED, 0);
            SendDlgItemMessage(hwndDlg, IDC_DOTMATCHNEWLINE, BM_SETCHECK, m_bDotMatchesNewline ? BST_CHECKED : BST_UNCHECKED, 0);

            CheckRadioButton(hwndDlg, IDC_REGEXRADIO, IDC_TEXTRADIO, DWORD(m_regUseRegex) ? IDC_REGEXRADIO : IDC_TEXTRADIO);
            CheckRadioButton(hwndDlg, IDC_ALLSIZERADIO, IDC_SIZERADIO, m_bAllSize ? IDC_ALLSIZERADIO : IDC_SIZERADIO);
            CheckRadioButton(hwndDlg, IDC_FILEPATTERNREGEX, IDC_FILEPATTERNTEXT, m_bUseRegexForPaths ? IDC_FILEPATTERNREGEX : IDC_FILEPATTERNTEXT);

            DialogEnableWindow(IDC_ADDTOBOOKMARKS, FALSE);
            DialogEnableWindow(IDC_EXCLUDEDIRSPATTERN, !!m_bIncludeSubfolders);

            bool bText = (IsDlgButtonChecked(*this, IDC_TEXTRADIO) == BST_CHECKED);
            DialogEnableWindow(IDC_REPLACETEXT, !bText);
            DialogEnableWindow(IDC_REPLACE, !bText);
            DialogEnableWindow(IDC_CREATEBACKUP, !bText);
            ::SetDlgItemText(*this, IDOK, _T("&Search"));
            CheckRadioButton(*this, IDC_RESULTFILES, IDC_RESULTCONTENT, IDC_RESULTFILES);

            SetFocus(GetDlgItem(hwndDlg, IDC_SEARCHTEXT));

            m_link.ConvertStaticToHyperlink(hwndDlg, IDC_ABOUTLINK, _T(""));

            m_resizer.Init(hwndDlg);
            m_resizer.AddControl(hwndDlg, IDC_ABOUTLINK, RESIZER_TOPRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_GROUPSEARCHIN, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_SEARCHPATH, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_SEARCHPATHBROWSE, RESIZER_TOPRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_GROUPSEARCHFOR, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_REGEXRADIO, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_TEXTRADIO, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_SEARCHFORLABEL, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_SEARCHTEXT, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_HELPBUTTON, RESIZER_TOPRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_REPLACEWITHLABEL, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_REPLACETEXT, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_CASE_SENSITIVE, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_DOTMATCHNEWLINE, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_REGEXOKLABEL, RESIZER_TOPRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_CREATEBACKUP, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_UTF8, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_TESTREGEX, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_ADDTOBOOKMARKS, RESIZER_TOPRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_BOOKMARKS, RESIZER_TOPRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_GROUPLIMITSEARCH, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_ALLSIZERADIO, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_SIZERADIO, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_SIZECOMBO, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_SIZEEDIT, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_KBTEXT, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_INCLUDESYSTEM, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_INCLUDEHIDDEN, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_INCLUDESUBFOLDERS, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_INCLUDEBINARY, RESIZER_TOPLEFT);

            m_resizer.AddControl(hwndDlg, IDC_PATTERNLABEL, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_PATTERN, RESIZER_TOPLEFTRIGHT);

            m_resizer.AddControl(hwndDlg, IDC_EXCLUDE_DIRS_PATTERNLABEL, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_EXCLUDEDIRSPATTERN, RESIZER_TOPLEFTRIGHT);

            m_resizer.AddControl(hwndDlg, IDC_FILEPATTERNREGEX, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_FILEPATTERNTEXT, RESIZER_TOPLEFT);

            m_resizer.AddControl(hwndDlg, IDC_REPLACE, RESIZER_TOPRIGHT);
            m_resizer.AddControl(hwndDlg, IDOK, RESIZER_TOPRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_GROUPSEARCHRESULTS, RESIZER_TOPLEFTBOTTOMRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_RESULTLIST, RESIZER_TOPLEFTBOTTOMRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_RESULTFILES, RESIZER_BOTTOMRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_RESULTCONTENT, RESIZER_BOTTOMRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_SEARCHINFOLABEL, RESIZER_BOTTOMLEFTRIGHT);

            InitDialog(hwndDlg, IDI_GREPWIN);

            WINDOWPLACEMENT wpl = {0};
            DWORD size = sizeof(wpl);
            if (SHGetValue(HKEY_CURRENT_USER, _T("Software\\grepWin"), _T("windowpos"), REG_NONE, &wpl, &size) == ERROR_SUCCESS)
                SetWindowPlacement(*this, &wpl);
            else
                ShowWindow(*this, SW_SHOW);

            ExtendFrameIntoClientArea(0, IDC_GROUPSEARCHIN, 0, 0);
            m_aerocontrols.SubclassControl(GetDlgItem(*this, IDC_ABOUTLINK));
            if (m_Dwm.IsDwmCompositionEnabled())
                m_resizer.ShowSizeGrip(false);

            if (m_bExecuteImmediately)
            {
                DoCommand(IDOK, 0);
            }
        }
        return FALSE;
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
            if (wParam == IDC_RESULTLIST)
            {
                DoListNotify((LPNMITEMACTIVATE)lParam);
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
            MINMAXINFO * mmi = (MINMAXINFO*)lParam;
            mmi->ptMinTrackSize.x = m_resizer.GetDlgRect()->right;
            mmi->ptMinTrackSize.y = m_resizer.GetDlgRect()->bottom;
            return 0;
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
            m_totalitems = 0;
            m_searchedItems = 0;
            m_totalmatches = 0;
            UpdateInfoLabel();
        }
        break;
    case SEARCH_FOUND:
        if ((wParam != 0)||(m_searchString.empty())||((CSearchInfo*)lParam)->readerror)
            AddFoundEntry((CSearchInfo*)lParam);
        m_totalmatches += ((CSearchInfo*)lParam)->matchlinesnumbers.size();
        UpdateInfoLabel();
        break;
    case SEARCH_PROGRESS:
        {
            if (wParam)
                m_searchedItems++;
            m_totalitems++;
            UpdateInfoLabel();
        }
        break;
    case SEARCH_END:
        {
            AutoSizeAllColumns();
            UpdateInfoLabel();
            ::SetDlgItemText(*this, IDOK, _T("&Search"));
            DialogEnableWindow(IDC_RESULTFILES, true);
            DialogEnableWindow(IDC_RESULTCONTENT, true);
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
                wstring newpath = wstring((LPCTSTR)pCopyData->lpData, pCopyData->cbData/sizeof(wchar_t));
                if (!newpath.empty())
                {
                    m_searchpath += _T("|");
                    m_searchpath += newpath;
                    SetDlgItemText(hwndDlg, IDC_SEARCHPATH, m_searchpath.c_str());
                }
            }
            return TRUE;
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

                m_searchedItems = 0;
                m_totalitems = 0;

                m_items.clear();
                InitResultList();
                DialogEnableWindow(IDC_RESULTFILES, false);
                DialogEnableWindow(IDC_RESULTCONTENT, false);

                m_AutoCompleteFilePatterns.AddEntry(m_patternregex.c_str());
                m_AutoCompleteExcludeDirsPatterns.AddEntry(m_excludedirspatternregex.c_str());
                m_AutoCompleteSearchPatterns.AddEntry(m_searchString.c_str());
                m_AutoCompleteReplacePatterns.AddEntry(m_replaceString.c_str());
                m_AutoCompleteSearchPaths.AddEntry(m_searchpath.c_str());

                m_bReplace = id == IDC_REPLACE;

                if (m_bReplace)
                {
                    TCHAR * msgtext = new TCHAR[m_searchString.size() + m_replaceString.size() + MAX_PATH * 4];
                    _stprintf_s(msgtext, m_searchString.size() + m_replaceString.size() + MAX_PATH * 4,
                        _T("Are you sure you want to replace\n%s\nwith\n%s\nwithout creating backups?"),
                        m_searchString.c_str(),
                        m_replaceString.empty() ? _T("an empty string") : m_replaceString.c_str());
                    if (!m_bCreateBackup)
                    {
                        if (::MessageBox(*this, msgtext, _T("grepWin"), MB_ICONQUESTION | MB_YESNO) != IDYES)
                        {
                            delete [] msgtext;
                            break;
                        }
                    }
                    delete [] msgtext;
                }

                InterlockedExchange(&m_dwThreadRunning, TRUE);
                InterlockedExchange(&m_Cancelled, FALSE);
                SetDlgItemText(*this, IDOK, _T("S&top"));

                // now start the thread which does the searching
                DWORD dwThreadId = 0;
                m_hSearchThread = CreateThread(
                    NULL,              // no security attribute
                    0,                 // default stack size
                    SearchThreadEntry,
                    (LPVOID)this,      // thread parameter
                    0,                 // not suspended
                    &dwThreadId);      // returns thread ID
            }
        }
        break;
    case IDCANCEL:
        if (m_dwThreadRunning)
            InterlockedExchange(&m_Cancelled, TRUE);
        else
        {
            SaveSettings();
            m_AutoCompleteFilePatterns.Save();
            m_AutoCompleteSearchPatterns.Save();
            m_AutoCompleteReplacePatterns.Save();
            m_AutoCompleteSearchPaths.Save();
            EndDialog(*this, id);
        }
        break;
    case IDC_TESTREGEX:
        {
            // get all the information we need from the dialog
            TCHAR buf[MAX_PATH*4] = {0};
            GetDlgItemText(*this, IDC_SEARCHTEXT, buf, MAX_PATH*4);
            m_searchString = buf;
            GetDlgItemText(*this, IDC_REPLACETEXT, buf, MAX_PATH*4);
            m_replaceString = buf;

            SaveSettings();
            CRegexTestDlg dlg(*this);
            dlg.bCaseSensitive = m_bCaseSensitive;
            dlg.bDotMatchesNewline = m_bDotMatchesNewline;
            dlg.SetStrings(m_searchString, m_replaceString);
            if (dlg.DoModal(hResource, IDD_REGEXTEST, *this) == IDOK)
            {
                m_searchString = dlg.GetSearchString();
                m_replaceString = dlg.GetReplaceString();
                SetDlgItemText(*this, IDC_SEARCHTEXT, m_searchString.c_str());
                SetDlgItemText(*this, IDC_REPLACETEXT, m_replaceString.c_str());
            }
        }
        break;
    case IDC_SEARCHPATHBROWSE:
        {
            TCHAR path[MAX_PATH_NEW] = {0};
            CBrowseFolder browse;

            GetDlgItemText(*this, IDC_SEARCHPATH, path, MAX_PATH*4);
            browse.SetInfo(_T("Select path to search"));
            if (browse.Show(*this, path, MAX_PATH_NEW, m_searchpath.c_str()) == CBrowseFolder::OK)
            {
                SetDlgItemText(*this, IDC_SEARCHPATH, path);
                m_searchpath = path;
            }
        }
        break;
    case IDC_SEARCHPATH:
        {
            if (msg == EN_CHANGE)
            {
                TCHAR buf[MAX_PATH*4] = {0};
                GetDlgItemText(*this, IDC_SEARCHTEXT, buf, MAX_PATH*4);
                int len = _tcslen(buf);
                GetDlgItemText(*this, IDC_SEARCHPATH, buf, MAX_PATH*4);
                bool bIsDir = !!PathIsDirectory(buf);
                if ((!bIsDir) && _tcschr(buf, '|'))
                    bIsDir = true;  // assume directories in case of multiple paths
                DialogEnableWindow(IDC_ALLSIZERADIO, bIsDir);
                DialogEnableWindow(IDC_SIZERADIO, bIsDir);
                DialogEnableWindow(IDC_SIZECOMBO, bIsDir);
                DialogEnableWindow(IDC_SIZEEDIT, bIsDir);
                DialogEnableWindow(IDC_INCLUDESYSTEM, bIsDir);
                DialogEnableWindow(IDC_INCLUDEHIDDEN, bIsDir);
                DialogEnableWindow(IDC_INCLUDESUBFOLDERS, bIsDir);
                DialogEnableWindow(IDC_INCLUDEBINARY, bIsDir && len > 0);
                DialogEnableWindow(IDC_PATTERN, bIsDir);
                DialogEnableWindow(IDC_EXCLUDEDIRSPATTERN, bIsDir);
                DialogEnableWindow(IDC_FILEPATTERNREGEX, bIsDir);
                DialogEnableWindow(IDC_FILEPATTERNTEXT, bIsDir);

                // change the dialog title to "grepWin : search/path"
                TCHAR compactPath[100] = {0};
                PathCompactPathEx(compactPath, buf, 40, 0);
                TCHAR titleBuf[MAX_PATH] = {0};
                _stprintf_s(titleBuf, _countof(titleBuf), _T("grepWin : %s"), compactPath);
                SetWindowText(*this, titleBuf);
            }
        }
        break;
    case IDC_INCLUDESUBFOLDERS:
        {
            if (msg == BN_CLICKED)
            {
                bool bIncludeSubfolders = (IsDlgButtonChecked(*this, IDC_INCLUDESUBFOLDERS) == BST_CHECKED);
                DialogEnableWindow(IDC_EXCLUDEDIRSPATTERN, bIncludeSubfolders);
            }
        }
        break;
    case IDC_SEARCHTEXT:
        {
            if (msg == EN_CHANGE)
            {
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
                TCHAR buf[10] = {0};
                GetDlgItemText(*this, IDC_SIZEEDIT, buf, 10);
                if (_tcslen(buf))
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
            bool bText = (IsDlgButtonChecked(*this, IDC_TEXTRADIO) == BST_CHECKED);
            DialogEnableWindow(IDC_REPLACETEXT, !bText);
            DialogEnableWindow(IDC_REPLACE, !bText);
            DialogEnableWindow(IDC_CREATEBACKUP, !bText);
            CheckRegex();
        }
        break;
    case IDC_ADDTOBOOKMARKS:
        {
            TCHAR buf[MAX_PATH*4] = {0};
            GetDlgItemText(*this, IDC_SEARCHTEXT, buf, MAX_PATH*4);
            m_searchString = buf;
            GetDlgItemText(*this, IDC_REPLACETEXT, buf, MAX_PATH*4);
            m_replaceString = buf;
            bool bUseRegex = (IsDlgButtonChecked(*this, IDC_REGEXRADIO) == BST_CHECKED);

            CNameDlg nameDlg(*this);
            if (nameDlg.DoModal(hResource, IDD_NAME, *this) == IDOK)
            {
                // add the bookmark
                CBookmarks bks;
                bks.Load();
                bks.AddBookmark(nameDlg.GetName(), m_searchString, m_replaceString, bUseRegex);
                bks.Save();
            }
        }
        break;
    case IDC_BOOKMARKS:
        {
            CBookmarksDlg dlg(*this);
            if (dlg.DoModal(hResource, IDD_BOOKMARKS, *this, IDC_GREPWIN) == IDOK)
            {
                m_searchString = dlg.GetSelectedSearchString();
                m_replaceString = dlg.GetSelectedReplaceString();
                m_bUseRegex = dlg.GetSelectedUseRegex();
                SetDlgItemText(*this, IDC_SEARCHTEXT, m_searchString.c_str());
                SetDlgItemText(*this, IDC_REPLACETEXT, m_replaceString.c_str());
                CheckRadioButton(*this, IDC_REGEXRADIO, IDC_TEXTRADIO, m_bUseRegex ? IDC_REGEXRADIO : IDC_TEXTRADIO);
            }
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
    case IDC_HELPBUTTON:
        {
            CInfoDlg::ShowDialog(IDR_INFODLG, hResource);
        }
        break;
    }
    return 1;
}

void CSearchDlg::SaveWndPosition()
{
    WINDOWPLACEMENT wpl = {0};
    wpl.length = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(*this, &wpl);
    SHSetValue(HKEY_CURRENT_USER, _T("Software\\grepWin"), _T("windowpos"), REG_NONE, &wpl, sizeof(wpl));
}

void CSearchDlg::UpdateInfoLabel()
{
    TCHAR buf[1024] = {0};
    _stprintf_s(buf, 1024, _T("Searched %ld files, skipped %ld files. Found %ld matches in %ld files."),
        m_searchedItems, m_totalitems-m_searchedItems, m_totalmatches, m_items.size());
    SetDlgItemText(*this, IDC_SEARCHINFOLABEL, buf);
}

bool CSearchDlg::InitResultList()
{
    HWND hListControl = GetDlgItem(*this, IDC_RESULTLIST);
    bool filelist = (IsDlgButtonChecked(*this, IDC_RESULTFILES) == BST_CHECKED);
    DWORD exStyle = LVS_EX_DOUBLEBUFFER|LVS_EX_INFOTIP;
    ListView_DeleteAllItems(hListControl);

    int c = Header_GetItemCount(ListView_GetHeader(hListControl))-1;
    while (c>=0)
        ListView_DeleteColumn(hListControl, c--);

    ListView_SetExtendedListViewStyle(hListControl, exStyle);
    ListView_SetImageList(hListControl, (WPARAM)(HIMAGELIST)CSysImageList::GetInstance(), LVSIL_SMALL);
    LVCOLUMN lvc = {0};
    lvc.mask = LVCF_TEXT;
    lvc.fmt = LVCFMT_LEFT;
    lvc.cx = -1;
    lvc.pszText = _T("Name");
    ListView_InsertColumn(hListControl, 0, &lvc);
    lvc.pszText = filelist ? _T("Size") : _T("Line");
    ListView_InsertColumn(hListControl, 1, &lvc);
    lvc.pszText = filelist ? _T("Matches") : _T("Text");
    ListView_InsertColumn(hListControl, 2, &lvc);
    if (filelist)
    {
        lvc.pszText = _T("Path");
        ListView_InsertColumn(hListControl, 3, &lvc);
        lvc.pszText = _T("Encoding");
        ListView_InsertColumn(hListControl, 4, &lvc);
        lvc.pszText = _T("Date modified");
        ListView_InsertColumn(hListControl, 5, &lvc);
    }

    ListView_SetColumnWidth(hListControl, 0, 300);
    ListView_SetColumnWidth(hListControl, 1, 50);
    ListView_SetColumnWidth(hListControl, 2, LVSCW_AUTOSIZE_USEHEADER);
    ListView_SetColumnWidth(hListControl, 3, LVSCW_AUTOSIZE_USEHEADER);
    ListView_SetColumnWidth(hListControl, 4, LVSCW_AUTOSIZE_USEHEADER);
    ListView_SetColumnWidth(hListControl, 5, LVSCW_AUTOSIZE_USEHEADER);

    return true;
}

bool CSearchDlg::AddFoundEntry(CSearchInfo * pInfo, bool bOnlyListControl)
{
    bool filelist = (IsDlgButtonChecked(*this, IDC_RESULTFILES) == BST_CHECKED);
    HWND hListControl = GetDlgItem(*this, IDC_RESULTLIST);
    LVITEM lv = {0};
    lv.iItem = ListView_GetItemCount(hListControl);
    LPARAM nEntryCount = filelist ? lv.iItem : m_items.size();
    lv.lParam = nEntryCount;
    int ret = 0;
    if (filelist)
    {
        TCHAR * pBuf = new TCHAR[pInfo->filepath.size()+1];
        wstring name = pInfo->filepath.substr(pInfo->filepath.find_last_of('\\')+1);
        _tcscpy_s(pBuf, pInfo->filepath.size()+1, name.c_str());
        lv.pszText = pBuf;
        lv.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
        lv.iImage = pInfo->folder ? CSysImageList::GetInstance().GetDirIconIndex() : CSysImageList::GetInstance().GetFileIconIndex(pInfo->filepath);
        ret = ListView_InsertItem(hListControl, &lv);
        delete [] pBuf;

        lv.mask = LVIF_TEXT;
        lv.iItem = ret;
        lv.iSubItem = 1;
        TCHAR sb[MAX_PATH_NEW] = {0};
        if (!pInfo->folder)
            StrFormatByteSizeW(pInfo->filesize, sb, 20);
        lv.pszText = sb;
        ListView_SetItem(hListControl, &lv);
        lv.iSubItem = 2;
        if (pInfo->readerror)
            _tcscpy_s(sb, MAX_PATH_NEW, _T("read error"));
        else
            _stprintf_s(sb, MAX_PATH_NEW, _T("%ld"), pInfo->matchlinesnumbers.size());
        ListView_SetItem(hListControl, &lv);
        lv.iSubItem = 3;
        _tcscpy_s(sb, MAX_PATH_NEW, pInfo->filepath.substr(0, pInfo->filepath.size()-name.size()-1).c_str());
        ListView_SetItem(hListControl, &lv);
        lv.iSubItem = 4;
        switch (pInfo->encoding)
        {
        case CTextFile::ANSI:
            _tcscpy_s(sb, MAX_PATH*4, _T("ANSI"));
            break;
        case CTextFile::UNICODE_LE:
            _tcscpy_s(sb, MAX_PATH*4, _T("UNICODE"));
            break;
        case CTextFile::UTF8:
            _tcscpy_s(sb, MAX_PATH*4, _T("UTF8"));
            break;
        case CTextFile::BINARY:
            _tcscpy_s(sb, MAX_PATH*4, _T("BINARY"));
            break;
        default:
            _tcscpy_s(sb, MAX_PATH*4, _T(""));
            break;
        }
        ListView_SetItem(hListControl, &lv);
        lv.iSubItem = 5;
        formatDate(sb, pInfo->modifiedtime, true);
        ListView_SetItem(hListControl, &lv);
    }
    else
    {
        // file contents
        if (pInfo->encoding == CTextFile::BINARY)
        {
            lv.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
            TCHAR * pBuf = new TCHAR[pInfo->filepath.size()+1];
            wstring name = pInfo->filepath.substr(pInfo->filepath.find_last_of('\\')+1);
            _tcscpy_s(pBuf, pInfo->filepath.size()+1, name.c_str());
            lv.pszText = pBuf;
            lv.iImage = pInfo->folder ? CSysImageList::GetInstance().GetDirIconIndex() : CSysImageList::GetInstance().GetFileIconIndex(pInfo->filepath);
            ret = ListView_InsertItem(hListControl, &lv);
            delete [] pBuf;
            if (ret >= 0)
            {
                lv.mask = LVIF_TEXT;
                lv.iItem = ret;

                lv.iSubItem = 1;
                lv.pszText = _T("binary");
                ListView_SetItem(hListControl, &lv);
            }
        }
        else
        {
            for (size_t subIndex = 0; subIndex < pInfo->matchlinesnumbers.size(); ++subIndex)
            {
                lv.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
                TCHAR * pBuf = new TCHAR[pInfo->filepath.size()+1];
                wstring name = pInfo->filepath.substr(pInfo->filepath.find_last_of('\\')+1);
                _tcscpy_s(pBuf, pInfo->filepath.size()+1, name.c_str());
                lv.pszText = pBuf;
                lv.iImage = pInfo->folder ? CSysImageList::GetInstance().GetDirIconIndex() : CSysImageList::GetInstance().GetFileIconIndex(pInfo->filepath);
                lv.iItem = ListView_GetItemCount(hListControl);
                lv.iSubItem = 0;
                lv.lParam = nEntryCount;
                ret = ListView_InsertItem(hListControl, &lv);
                delete [] pBuf;
                if (ret >= 0)
                {
                    lv.mask = LVIF_TEXT;
                    lv.iItem = ret;

                    lv.iSubItem = 1;
                    TCHAR sb[MAX_PATH_NEW] = {0};
                    _stprintf_s(sb, MAX_PATH_NEW, _T("%ld"), pInfo->matchlinesnumbers[subIndex]);
                    lv.pszText = sb;
                    ListView_SetItem(hListControl, &lv);

                    lv.iSubItem = 2;
                    wstring line = pInfo->matchlines[subIndex];
                    std::replace(line.begin(), line.end(), '\t', ' ');
                    std::replace(line.begin(), line.end(), '\n', ' ');
                    std::replace(line.begin(), line.end(), '\r', ' ');
                    lv.pszText = (LPWSTR)line.c_str();
                    ListView_SetItem(hListControl, &lv);
                }
            }
        }
    }
    if ((ret != -1)&&(!bOnlyListControl))
    {
        m_items.push_back(*pInfo);
    }

    return (ret != -1);
}

void CSearchDlg::FillResultList()
{
    bool filelist = (IsDlgButtonChecked(*this, IDC_RESULTFILES) == BST_CHECKED);
    HWND hListControl = GetDlgItem(*this, IDC_RESULTLIST);

    ListView_DeleteAllItems(hListControl);

    int index = 0;
    int fileIndex = 0;
    for (vector<CSearchInfo>::const_iterator pInfo = m_items.begin(); pInfo != m_items.end(); ++pInfo)
    {
        if (filelist)
        {
            LVITEM lv = {0};
            lv.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
            TCHAR * pBuf = new TCHAR[pInfo->filepath.size()+1];
            wstring name = pInfo->filepath.substr(pInfo->filepath.find_last_of('\\')+1);
            _tcscpy_s(pBuf, pInfo->filepath.size()+1, name.c_str());
            lv.pszText = pBuf;
            lv.iImage = pInfo->folder ? CSysImageList::GetInstance().GetDirIconIndex() : CSysImageList::GetInstance().GetFileIconIndex(pInfo->filepath);
            lv.iItem = index;
            lv.lParam = fileIndex;
            int ret = ListView_InsertItem(hListControl, &lv);
            delete [] pBuf;
            if (ret >= 0)
            {
                lv.mask = LVIF_TEXT;
                lv.iItem = ret;

                lv.iSubItem = 1;
                TCHAR sb[MAX_PATH_NEW] = {0};
                if (!pInfo->folder)
                    StrFormatByteSizeW(pInfo->filesize, sb, 20);
                lv.pszText = sb;
                ListView_SetItem(hListControl, &lv);

                lv.iSubItem = 2;
                if (pInfo->readerror)
                    _tcscpy_s(sb, MAX_PATH_NEW, _T("read error"));
                else
                    _stprintf_s(sb, MAX_PATH_NEW, _T("%ld"), pInfo->matchlinesnumbers.size());
                ListView_SetItem(hListControl, &lv);

                lv.iSubItem = 3;
                _tcscpy_s(sb, MAX_PATH_NEW, pInfo->filepath.substr(0, pInfo->filepath.size()-name.size()-1).c_str());
                ListView_SetItem(hListControl, &lv);

                lv.iSubItem = 4;
                switch (pInfo->encoding)
                {
                case CTextFile::ANSI:
                    _tcscpy_s(sb, MAX_PATH*4, _T("ANSI"));
                    break;
                case CTextFile::UNICODE_LE:
                    _tcscpy_s(sb, MAX_PATH*4, _T("UNICODE"));
                    break;
                case CTextFile::UTF8:
                    _tcscpy_s(sb, MAX_PATH*4, _T("UTF8"));
                    break;
                case CTextFile::BINARY:
                    _tcscpy_s(sb, MAX_PATH*4, _T("BINARY"));
                    break;
                default:
                    _tcscpy_s(sb, MAX_PATH*4, _T(""));
                    break;
                }
                ListView_SetItem(hListControl, &lv);

                lv.iSubItem = 5;
                formatDate(sb, pInfo->modifiedtime, true);
                ListView_SetItem(hListControl, &lv);
                index++;
            }
        }
        else
        {
            // file contents
            if (pInfo->encoding == CTextFile::BINARY)
            {
                LVITEM lv = {0};
                lv.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
                TCHAR * pBuf = new TCHAR[pInfo->filepath.size()+1];
                wstring name = pInfo->filepath.substr(pInfo->filepath.find_last_of('\\')+1);
                _tcscpy_s(pBuf, pInfo->filepath.size()+1, name.c_str());
                lv.pszText = pBuf;
                lv.iImage = pInfo->folder ? CSysImageList::GetInstance().GetDirIconIndex() : CSysImageList::GetInstance().GetFileIconIndex(pInfo->filepath);
                lv.iItem = index++;
                lv.lParam = fileIndex;
                int ret = ListView_InsertItem(hListControl, &lv);
                delete [] pBuf;
                if (ret >= 0)
                {
                    lv.mask = LVIF_TEXT;
                    lv.iItem = ret;

                    lv.iSubItem = 1;
                    lv.pszText = _T("binary");
                    ListView_SetItem(hListControl, &lv);
                }
            }
            else
            {
                for (size_t subIndex = 0; subIndex < pInfo->matchlinesnumbers.size(); ++subIndex)
                {
                    LVITEM lv = {0};
                    lv.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
                    TCHAR * pBuf = new TCHAR[pInfo->filepath.size()+1];
                    wstring name = pInfo->filepath.substr(pInfo->filepath.find_last_of('\\')+1);
                    _tcscpy_s(pBuf, pInfo->filepath.size()+1, name.c_str());
                    lv.pszText = pBuf;
                    lv.iImage = pInfo->folder ? CSysImageList::GetInstance().GetDirIconIndex() : CSysImageList::GetInstance().GetFileIconIndex(pInfo->filepath);
                    lv.iItem = index;
                    lv.lParam = fileIndex;
                    int ret = ListView_InsertItem(hListControl, &lv);
                    delete [] pBuf;
                    if (ret >= 0)
                    {
                        lv.mask = LVIF_TEXT;
                        lv.iItem = ret;

                        lv.iSubItem = 1;
                        TCHAR sb[MAX_PATH_NEW] = {0};
                        _stprintf_s(sb, MAX_PATH_NEW, _T("%ld"), pInfo->matchlinesnumbers[subIndex]);
                        lv.pszText = sb;
                        ListView_SetItem(hListControl, &lv);

                        lv.iSubItem = 2;
                        wstring line = pInfo->matchlines[subIndex];
                        std::replace(line.begin(), line.end(), '\t', ' ');
                        std::replace(line.begin(), line.end(), '\n', ' ');
                        std::replace(line.begin(), line.end(), '\r', ' ');
                        lv.pszText = (LPWSTR)line.c_str();
                        ListView_SetItem(hListControl, &lv);
                        index++;
                    }
                }
            }
        }
        fileIndex++;
    }
    AutoSizeAllColumns();
}

void CSearchDlg::ShowContextMenu(int x, int y)
{
    HWND hListControl = GetDlgItem(*this, IDC_RESULTLIST);
    int nCount = ListView_GetItemCount(hListControl);
    if (nCount == 0)
        return;
    CShellContextMenu shellMenu;
    int iItem = -1;
    vector<wstring> paths;
    while ((iItem = ListView_GetNextItem(hListControl, iItem, LVNI_SELECTED)) != (-1))
        paths.push_back(m_items[GetSelectedListIndex(iItem)].filepath);

    if (paths.size() == 0)
        return;

    shellMenu.SetObjects(paths);

    POINT pt = {x,y};
    if ((x==-1)&&(y==-1))
    {
        RECT rc;
        ListView_GetItemRect(hListControl, ListView_GetSelectionMark(hListControl), &rc, LVIR_LABEL);
        pt.x = (rc.right-rc.left)/2;
        pt.y = (rc.bottom-rc.top)/2;
        ClientToScreen(hListControl, &pt);
    }
    shellMenu.ShowContextMenu(hListControl, pt);
}

bool CSearchDlg::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN)
    {
        HWND hListControl = GetDlgItem(*this, IDC_RESULTLIST);
        switch (pMsg->wParam)
        {
        case VK_RETURN:
            {
                if (GetFocus() == hListControl)
                {
                    int selItem = ListView_GetSelectionMark(hListControl);
                    selItem = GetSelectedListIndex(selItem);
                    if ((selItem >= 0)&&(selItem < (int)m_items.size()))
                    {
                        SHELLEXECUTEINFO shExInfo = {0};
                        shExInfo.cbSize = sizeof(SHELLEXECUTEINFO);
                        shExInfo.fMask = SEE_MASK_UNICODE;
                        shExInfo.hwnd = *this;
                        shExInfo.lpFile = m_items[selItem].filepath.c_str();
                        shExInfo.nShow = SW_SHOW;
                        ShellExecuteEx(&shExInfo);
                        return true;
                    }
                }
            }
            break;
        case 'A':
            {
                if ((GetFocus() == hListControl)&&(GetKeyState(VK_CONTROL)&0x8000))
                {
                    // select all entries
                    int nCount = ListView_GetItemCount(hListControl);
                    for (int i=0; i<nCount; ++i)
                    {
                        ListView_SetItemState(hListControl, i, LVIS_SELECTED, LVIS_SELECTED);
                    }
                    return true;
                }
            }
            break;
        case 'C':
            {
                if ((GetFocus() == hListControl)&&(GetKeyState(VK_CONTROL)&0x8000))
                {
                    // copy all selected entries to the clipboard
                    std::wstring clipBoardText;
                    HWND hHeader = ListView_GetHeader(hListControl);
                    int columns = Header_GetItemCount(hHeader);
                    WCHAR buf[MAX_PATH] = {0};
                    for (int i = 0; i < columns; ++i)
                    {
                        HD_ITEM hdi = {0};
                        hdi.mask = HDI_TEXT;
                        hdi.pszText = buf;
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
            wstring verb = _T("open");
            int iItem = GetSelectedListIndex(lpNMItemActivate->iItem);
            CSearchInfo inf = m_items[iItem];
            size_t dotPos = inf.filepath.rfind('.');
            wstring ext;
            if (dotPos != wstring::npos)
                ext = inf.filepath.substr(dotPos);

            // some scripting languages (e.g. python) will execute the document
            // instead of open it. Try to identify these cases.
            DWORD buflen = 0;
            AssocQueryString(ASSOCF_INIT_DEFAULTTOSTAR, ASSOCSTR_COMMAND, ext.c_str(), _T("open"), NULL, &buflen);
            TCHAR * cmdbuf = new TCHAR[buflen + 1];
            AssocQueryString(ASSOCF_INIT_DEFAULTTOSTAR, ASSOCSTR_COMMAND, ext.c_str(), _T("open"), cmdbuf, &buflen);
            wstring application = cmdbuf;
            delete [] cmdbuf;
            if (   (application.find(_T("%2")) != wstring::npos)
                || (application.find(_T("%*")) != wstring::npos))
            {
                verb = _T("edit");
            }
            // normalize application path
            DWORD len = ExpandEnvironmentStrings (application.c_str(), NULL, 0);
            cmdbuf = new TCHAR[len+1];
            ExpandEnvironmentStrings (application.c_str(), cmdbuf, len);
            application = cmdbuf;
            delete [] cmdbuf;

            // resolve parameters
            if (application.find(_T("%1")) == wstring::npos)
                application += _T(" %1");


            bool filelist = (IsDlgButtonChecked(*this, IDC_RESULTFILES) == BST_CHECKED);
            wstring linenumberparam;
            if (!filelist)
            {
                HWND hListControl = GetDlgItem(*this, IDC_RESULTLIST);
                TCHAR textlinebuf[MAX_PATH] = {0};
                LVITEM lv = {0};
                lv.iItem = lpNMItemActivate->iItem;
                lv.iSubItem = 1;    // line number
                lv.mask = LVIF_TEXT;
                lv.pszText = textlinebuf;
                lv.cchTextMax = MAX_PATH;
                if (ListView_GetItem(hListControl, &lv))
                {
                    wstring appname = application;
                    std::transform(appname.begin(), appname.end(), appname.begin(), std::tolower);

                    // now find out if the application which opens the file is known to us
                    // and if it has a 'linenumber' switch to jump directly to a specific
                    // line number.
                    if (appname.find(_T("notepad++.exe")) != wstring::npos)
                    {
                        // notepad++
                        TCHAR buf[MAX_PATH] = {0};
                        _stprintf_s(buf, MAX_PATH, _T("-n%s"), textlinebuf);
                        linenumberparam = buf;
                    }
                    else if (appname.find(_T("xemacs.exe")) != wstring::npos)
                    {
                        // XEmacs
                        TCHAR buf[MAX_PATH] = {0};
                        _stprintf_s(buf, MAX_PATH, _T("+%s"), textlinebuf);
                        linenumberparam = buf;
                    }
                    else if (appname.find(_T("uedit32.exe")) != wstring::npos)
                    {
                        // UltraEdit
                        TCHAR buf[MAX_PATH] = {0};
                        _stprintf_s(buf, MAX_PATH, _T("-l%s"), textlinebuf);
                        linenumberparam = buf;
                    }
                    else if (appname.find(_T("codewright.exe")) != wstring::npos)
                    {
                        // CodeWright
                        TCHAR buf[MAX_PATH] = {0};
                        _stprintf_s(buf, MAX_PATH, _T("-G%s"), textlinebuf);
                        linenumberparam = buf;
                    }
                    else if (appname.find(_T("notepad2.exe")) != wstring::npos)
                    {
                        // Notepad2
                        TCHAR buf[MAX_PATH] = {0};
                        _stprintf_s(buf, MAX_PATH, _T("/G %s"), textlinebuf);
                        linenumberparam = buf;
                    }
                }
            }

            // replace "%1" with %1
            wstring tag = _T("\"%1\"");
            wstring repl = _T("%1");
            wstring::iterator it_begin = search(application.begin(), application.end(), tag.begin(), tag.end());
            if (it_begin != application.end())
            {
                wstring::iterator it_end= it_begin + tag.size();
                application.replace(it_begin, it_end, repl);
            }
            // replace %1 with "path/of/selected/file"
            tag = _T("%1");
            repl = _T("\"") + inf.filepath + _T("\"");
            it_begin = search(application.begin(), application.end(), tag.begin(), tag.end());
            if (it_begin != application.end())
            {
                wstring::iterator it_end= it_begin + tag.size();
                application.replace(it_begin, it_end, repl);
            }
            if (linenumberparam.size())
            {
                application += _T(" ");
                application += linenumberparam;
            }

            STARTUPINFO startupInfo;
            PROCESS_INFORMATION processInfo;
            memset(&startupInfo, 0, sizeof(STARTUPINFO));
            startupInfo.cb = sizeof(STARTUPINFO);

            memset(&processInfo, 0, sizeof(PROCESS_INFORMATION));
            CreateProcess(NULL, const_cast<TCHAR*>(application.c_str()), NULL, NULL, FALSE, 0, 0, NULL, &startupInfo, &processInfo);
            CloseHandle(processInfo.hThread);
            CloseHandle(processInfo.hProcess);
        }
    }
    if (lpNMItemActivate->hdr.code == LVN_BEGINDRAG)
    {
        CDropFiles dropFiles; // class for creating DROPFILES struct

        HWND hListControl = GetDlgItem(*this, IDC_RESULTLIST);
        int nCount = ListView_GetItemCount(hListControl);
        if (nCount == 0)
            return;

        int iItem = -1;
        while ((iItem = ListView_GetNextItem(hListControl, iItem, LVNI_SELECTED)) != (-1))
        {
            dropFiles.AddFile(m_items[GetSelectedListIndex(iItem)].filepath);
        }

        if ( dropFiles.GetCount()>0 )
        {
            dropFiles.CreateStructure(hListControl);
        }
    }
    if (lpNMItemActivate->hdr.code == LVN_COLUMNCLICK)
    {
        bool filelist = (IsDlgButtonChecked(*this, IDC_RESULTFILES) == BST_CHECKED);
        if (!filelist)
            return;     // no sorting for the result view
        m_bAscending = !m_bAscending;
        switch (lpNMItemActivate->iSubItem)
        {
        case 0:
            if (m_bAscending)
                sort(m_items.begin(), m_items.end(), NameCompareAsc);
            else
                sort(m_items.begin(), m_items.end(), NameCompareDesc);
            break;
        case 1:
            if (m_bAscending)
                sort(m_items.begin(), m_items.end(), SizeCompareAsc);
            else
                sort(m_items.begin(), m_items.end(), SizeCompareDesc);
            break;
        case 2:
            if (m_bAscending)
                sort(m_items.begin(), m_items.end(), MatchesCompareAsc);
            else
                sort(m_items.begin(), m_items.end(), MatchesCompareDesc);
            break;
        case 3:
            if (m_bAscending)
                sort(m_items.begin(), m_items.end(), PathCompareAsc);
            else
                sort(m_items.begin(), m_items.end(), PathCompareDesc);
            break;
        case 4:
            if (m_bAscending)
                sort(m_items.begin(), m_items.end(), EncodingCompareAsc);
            else
                sort(m_items.begin(), m_items.end(), EncodingCompareDesc);
            break;
        case 5:
            if (m_bAscending)
                sort(m_items.begin(), m_items.end(), ModifiedTimeCompareAsc);
            else
                sort(m_items.begin(), m_items.end(), ModifiedTimeCompareDesc);
            break;
        }

        HWND hListControl = GetDlgItem(*this, IDC_RESULTLIST);
        ListView_DeleteAllItems(hListControl);
        for (vector<CSearchInfo>::iterator it = m_items.begin(); it != m_items.end(); ++it)
        {
            AddFoundEntry(&(*it), true);
        }

        AutoSizeAllColumns();
        HDITEM hd = {0};
        hd.mask = HDI_FORMAT;
        HWND hHeader = ListView_GetHeader(hListControl);
        int iCount = Header_GetItemCount(hHeader);
        for (int i=0; i<iCount; ++i)
        {
            Header_GetItem(hHeader, i, &hd);
            hd.fmt &= ~(HDF_SORTDOWN | HDF_SORTUP);
            Header_SetItem(hHeader, i, &hd);
        }
        Header_GetItem(hHeader, lpNMItemActivate->iSubItem, &hd);
        hd.fmt |= (m_bAscending ? HDF_SORTUP : HDF_SORTDOWN);
        Header_SetItem(hHeader, lpNMItemActivate->iSubItem, &hd);

    }
    if (lpNMItemActivate->hdr.code == LVN_GETINFOTIP)
    {
        NMLVGETINFOTIP *pInfoTip = reinterpret_cast<NMLVGETINFOTIP*>(lpNMItemActivate);

        // Which item number?
        size_t itemid = pInfoTip->iItem;
        int iItem = GetSelectedListIndex(itemid);
        pInfoTip->pszText[0] = 0;
        if ((int)m_items.size() > iItem)
        {
            CSearchInfo inf = m_items[iItem];
            lstrcpyn(pInfoTip->pszText,inf.filepath.c_str(), pInfoTip->cchTextMax);
        }
    }
}

bool grepWin_is_regex_valid(const wstring& m_searchString)
{
    // check if the regex is valid
    bool bValid = true;
    try
    {
        boost::wregex expression = boost::wregex(m_searchString);
    }
    catch (const exception&)
    {
        bValid = false;
    }
    return bValid;
}

bool CSearchDlg::SaveSettings()
{
    // get all the information we need from the dialog
    TCHAR buf[MAX_PATH*4] = {0};

    GetDlgItemText(*this, IDC_SEARCHPATH, buf, MAX_PATH*4);
    m_searchpath = buf;

    GetDlgItemText(*this, IDC_SEARCHTEXT, buf, MAX_PATH*4);
    m_searchString = buf;

    GetDlgItemText(*this, IDC_REPLACETEXT, buf, MAX_PATH*4);
    m_replaceString = buf;

    GetDlgItemText(*this, IDC_EXCLUDEDIRSPATTERN, buf, MAX_PATH*4);
    m_excludedirspatternregex = buf;

    GetDlgItemText(*this, IDC_PATTERN, buf, MAX_PATH*4);
    m_patternregex = buf;

    // split the pattern string into single patterns and
    // add them to an array
    TCHAR * pBuf = buf;
    size_t pos = 0;
    m_patterns.clear();
    do
    {
        pos = _tcscspn(pBuf, _T(",; "));
        wstring s = wstring(pBuf, pos);
        if (!s.empty())
        {
            std::transform(s.begin(), s.end(), s.begin(), (int(*)(int)) std::tolower);
            m_patterns.push_back(s);
        }
        pBuf += pos;
        pBuf++;
    } while(*pBuf && (*(pBuf-1)));

    if (m_searchpath.empty())
        return false;
    m_regSearchPath = m_searchpath;
    m_bUseRegex = (IsDlgButtonChecked(*this, IDC_REGEXRADIO) == BST_CHECKED);
    m_regUseRegex = (DWORD)m_bUseRegex;
    if (m_bUseRegex)
    {
        // check if the regex is valid before doing the search
        if( !grepWin_is_regex_valid(m_searchString) && !m_searchString.empty() )
        {
            return false;
        }
    }
    m_bUseRegexForPaths = (IsDlgButtonChecked(*this, IDC_FILEPATTERNREGEX) == BST_CHECKED);
    m_regUseRegexForPaths = (DWORD)m_bUseRegexForPaths;
    if (m_bUseRegexForPaths)
    {
        // check if the regex is valid before doing the search
        if( !grepWin_is_regex_valid(m_patternregex) && !m_patternregex.empty() )
        {
            return false;
        }
    }

    // check if the Exclude Dirs regex is valid before doing the search
    if( !grepWin_is_regex_valid(m_excludedirspatternregex) && !m_excludedirspatternregex.empty() )
    {
        return false;
    }

    m_bAllSize = (IsDlgButtonChecked(*this, IDC_ALLSIZERADIO) == BST_CHECKED);
    m_regAllSize = (DWORD)m_bAllSize;
    m_lSize = 0;
    m_sizeCmp = 0;
    if (!m_bAllSize)
    {
        GetDlgItemText(*this, IDC_SIZEEDIT, buf, MAX_PATH*4);
        m_lSize = _tstol(buf);
        m_regSize = m_lSize;
        m_lSize *= 1024;
        m_sizeCmp = SendDlgItemMessage(*this, IDC_SIZECOMBO, CB_GETCURSEL, 0, 0);
        m_regSizeCombo = m_sizeCmp;
    }
    m_bIncludeSystem = (IsDlgButtonChecked(*this, IDC_INCLUDESYSTEM) == BST_CHECKED);
    m_bIncludeHidden = (IsDlgButtonChecked(*this, IDC_INCLUDEHIDDEN) == BST_CHECKED);
    m_bIncludeSubfolders = (IsDlgButtonChecked(*this, IDC_INCLUDESUBFOLDERS) == BST_CHECKED);
    m_bIncludeBinary = (IsDlgButtonChecked(*this, IDC_INCLUDEBINARY) == BST_CHECKED);
    m_bCreateBackup = (IsDlgButtonChecked(*this, IDC_CREATEBACKUP) == BST_CHECKED);
    m_bUTF8 = (IsDlgButtonChecked(*this, IDC_UTF8) == BST_CHECKED);
    m_bCaseSensitive = (IsDlgButtonChecked(*this, IDC_CASE_SENSITIVE) == BST_CHECKED);
    m_bDotMatchesNewline = (IsDlgButtonChecked(*this, IDC_DOTMATCHNEWLINE) == BST_CHECKED);

    m_regIncludeSystem = (DWORD)m_bIncludeSystem;
    m_regIncludeHidden = (DWORD)m_bIncludeHidden;
    m_regIncludeSubfolders = (DWORD)m_bIncludeSubfolders;
    m_regIncludeBinary = (DWORD)m_bIncludeBinary;
    m_regCreateBackup = (DWORD)m_bCreateBackup;
    m_regUTF8 = (DWORD)m_bUTF8;
    m_regCaseSensitive = (DWORD)m_bCaseSensitive;
    m_regDotMatchesNewline = (DWORD)m_bDotMatchesNewline;
    m_regPattern = m_patternregex;
    m_regExcludeDirsPattern = m_excludedirspatternregex;

    SaveWndPosition();

    return true;
}

bool CSearchDlg::NameCompareAsc(const CSearchInfo Entry1, const CSearchInfo Entry2)
{
    wstring name1 = Entry1.filepath.substr(Entry1.filepath.find_last_of('\\')+1);
    wstring name2 = Entry2.filepath.substr(Entry2.filepath.find_last_of('\\')+1);
    return name1.compare(name2) < 0;
}

bool CSearchDlg::SizeCompareAsc(const CSearchInfo Entry1, const CSearchInfo Entry2)
{
    return Entry1.filesize < Entry2.filesize;
}

bool CSearchDlg::MatchesCompareAsc(const CSearchInfo Entry1, const CSearchInfo Entry2)
{
    return Entry1.matchlinesnumbers.size() < Entry2.matchlinesnumbers.size();
}

bool CSearchDlg::PathCompareAsc(const CSearchInfo Entry1, const CSearchInfo Entry2)
{
    wstring name1 = Entry1.filepath.substr(Entry1.filepath.find_last_of('\\')+1);
    wstring name2 = Entry2.filepath.substr(Entry2.filepath.find_last_of('\\')+1);
    wstring path1 = Entry1.filepath.substr(0, Entry1.filepath.size()-name1.size()-1);
    wstring path2 = Entry2.filepath.substr(0, Entry2.filepath.size()-name2.size()-1);
    int cmp = path1.compare(path2);
    if (cmp != 0)
        return cmp < 0;
    return name1.compare(name2) < 0;
}

bool CSearchDlg::EncodingCompareAsc(const CSearchInfo Entry1, const CSearchInfo Entry2)
{
    return Entry1.encoding < Entry2.encoding;
}

bool CSearchDlg::ModifiedTimeCompareAsc(const CSearchInfo Entry1, const CSearchInfo Entry2)
{
    return CompareFileTime(&Entry1.modifiedtime, &Entry2.modifiedtime) < 0;
}

bool CSearchDlg::NameCompareDesc(const CSearchInfo Entry1, const CSearchInfo Entry2)
{
    wstring name1 = Entry1.filepath.substr(Entry1.filepath.find_last_of('\\')+1);
    wstring name2 = Entry2.filepath.substr(Entry2.filepath.find_last_of('\\')+1);
    return name1.compare(name2) > 0;
}

bool CSearchDlg::SizeCompareDesc(const CSearchInfo Entry1, const CSearchInfo Entry2)
{
    return Entry1.filesize > Entry2.filesize;
}

bool CSearchDlg::MatchesCompareDesc(const CSearchInfo Entry1, const CSearchInfo Entry2)
{
    return Entry1.matchlinesnumbers.size() > Entry2.matchlinesnumbers.size();
}

bool CSearchDlg::PathCompareDesc(const CSearchInfo Entry1, const CSearchInfo Entry2)
{
    wstring name1 = Entry1.filepath.substr(Entry1.filepath.find_last_of('\\')+1);
    wstring name2 = Entry2.filepath.substr(Entry2.filepath.find_last_of('\\')+1);
    wstring path1 = Entry1.filepath.substr(0, Entry1.filepath.size()-name1.size()-1);
    wstring path2 = Entry2.filepath.substr(0, Entry2.filepath.size()-name2.size()-1);
    int cmp = path1.compare(path2);
    if (cmp != 0)
        return cmp > 0;
    return name1.compare(name2) > 0;
}

bool CSearchDlg::EncodingCompareDesc(const CSearchInfo Entry1, const CSearchInfo Entry2)
{
    return Entry1.encoding > Entry2.encoding;
}

bool CSearchDlg::ModifiedTimeCompareDesc(const CSearchInfo Entry1, const CSearchInfo Entry2)
{
    return CompareFileTime(&Entry1.modifiedtime, &Entry2.modifiedtime) > 0;
}

bool grepWin_match_i(const wstring& the_regex, const TCHAR *pText)
{
    try
    {
        boost::wregex expression = boost::wregex(the_regex, boost::regex::normal|boost::regbase::icase);
        boost::wcmatch whatc;
        if (boost::regex_match(pText, whatc, expression))
        {
            return true;
        }
    }
    catch (const exception&)
    {

    }
    return false;
}

DWORD CSearchDlg::SearchThread()
{
    TCHAR pathbuf[MAX_PATH_NEW] = {0};

    // split the path string into single paths and
    // add them to an array
    const TCHAR * pBuf = m_searchpath.c_str();
    size_t pos = 0;
    vector<wstring> pathvector;
    do
    {
        pos = _tcscspn(pBuf, _T("|"));
        wstring s = wstring(pBuf, pos);
        if (!s.empty())
        {

            TCHAR * pBuf2 = new TCHAR[s.size()+1];
            TCHAR * pBuf = new TCHAR[s.size()+1];
            TCHAR * prettypath = pBuf;
            _tcscpy_s(pBuf2, s.size()+1, s.c_str());
            // skip a possible UNC path start
            if (*pBuf2 == '\\')
            {
                for (int i=0; i<2 && (*pBuf2); ++i)
                    *pBuf++ = *pBuf2++;
            }
            while(*pBuf2)
            {
                *pBuf = *pBuf2;
                if (*pBuf2 == '\\')
                {
                    do
                    {
                        pBuf2++;
                    } while (*pBuf2 == '\\');
                }
                else
                    pBuf2++;
                pBuf++;
            }
            *pBuf = 0;
            s = prettypath;
            pathvector.push_back(s);
        }
        pBuf += pos;
        pBuf++;
    } while(*pBuf && (*(pBuf-1)));

    for (vector<wstring>::const_iterator it = pathvector.begin(); it != pathvector.end(); ++it)
    {
        wstring searchpath = *it;
        if (!searchpath.empty())
        {
            bool bAlwaysSearch = false;
            if (!PathIsDirectory(searchpath.c_str()))
                bAlwaysSearch = true;
            bool bIsDirectory = false;
            CDirFileEnum fileEnumerator(searchpath.c_str());
            bool bRecurse = m_bIncludeSubfolders;

            SendMessage(*this, SEARCH_START, 0, 0);
            while (((fileEnumerator.NextFile(pathbuf, bRecurse, &bIsDirectory))&&(!m_Cancelled))||(bAlwaysSearch))
            {
                if (!bIsDirectory)
                {
                    bool bSearch = false;
                    DWORD nFileSizeLow = 0;
                    FILETIME ft = {0};
                    if (bAlwaysSearch)
                    {
                        _tcscpy_s(pathbuf, MAX_PATH_NEW, searchpath.c_str());
                        HANDLE hFile = CreateFile(searchpath.c_str(), FILE_READ_EA, FILE_SHARE_DELETE|FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                        if (hFile != INVALID_HANDLE_VALUE)
                        {
                            BY_HANDLE_FILE_INFORMATION bhfi = {0};
                            GetFileInformationByHandle(hFile, &bhfi);
                            nFileSizeLow = bhfi.nFileSizeLow;
                            ft = bhfi.ftLastWriteTime;
                            CloseHandle(hFile);
                        }
                    }
                    else
                    {
                        const WIN32_FIND_DATA * pFindData = fileEnumerator.GetFileInfo();
                        bSearch = ((m_bIncludeHidden)||((pFindData->dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) == 0));
                        bSearch = bSearch && ((m_bIncludeSystem)||((pFindData->dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) == 0));
                        nFileSizeLow = pFindData->nFileSizeLow;
                        ft = pFindData->ftLastWriteTime;
                        if (!m_bAllSize)
                        {
                            switch (m_sizeCmp)
                            {
                            case 0: // less than
                                bSearch = bSearch && (pFindData->nFileSizeLow < m_lSize);
                                break;
                            case 1: // equal
                                bSearch = bSearch && (pFindData->nFileSizeLow == m_lSize);
                                break;
                            case 2: // greater than
                                bSearch = bSearch && (pFindData->nFileSizeLow > m_lSize);
                                break;
                            }
                        }
                    }
                    bRecurse = ((m_bIncludeSubfolders)&&(bSearch));
                    bool bPattern = MatchPath(pathbuf);

                    int nFound = -1;
                    if ((bSearch && bPattern)||(bAlwaysSearch))
                    {
                        CSearchInfo sinfo(pathbuf);
                        sinfo.filesize = nFileSizeLow;
                        sinfo.modifiedtime = ft;
                        if (m_searchString.empty())
                        {
                            SendMessage(*this, SEARCH_FOUND, 1, (LPARAM)&sinfo);
                            nFound = 1;
                        }
                        else
                        {
                            nFound = SearchFile(sinfo, bAlwaysSearch, m_bIncludeBinary, m_bUseRegex, m_bCaseSensitive, m_bDotMatchesNewline, m_searchString);
                            if (nFound >= 0)
                                SendMessage(*this, SEARCH_FOUND, nFound, (LPARAM)&sinfo);
                        }
                    }
                    SendMessage(*this, SEARCH_PROGRESS, ((bSearch && bPattern)||bAlwaysSearch) && (nFound >= 0), 0);
                }
                else
                {
                    const WIN32_FIND_DATA * pFindData = fileEnumerator.GetFileInfo();
                    bool bSearch = ((m_bIncludeHidden)||((pFindData->dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) == 0));
                    bSearch = bSearch && ((m_bIncludeSystem)||((pFindData->dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) == 0));
                    bool bExcludeDir = bSearch && m_excludedirspatternregex.size() && grepWin_match_i(m_excludedirspatternregex, pFindData->cFileName);
                    bSearch = bSearch && !bExcludeDir;
                    bRecurse = ((bIsDirectory)&&(m_bIncludeSubfolders)&&(bSearch));
                    if (m_searchString.empty() && m_replaceString.empty())
                    {
                        // if there's no search and replace string, include folders in the 'matched' list if they
                        // match the specified file pattern
                        if (MatchPath(pathbuf))
                        {
                            CSearchInfo sinfo(pathbuf);
                            sinfo.modifiedtime = pFindData->ftLastWriteTime;
                            sinfo.folder = true;
                            SendMessage(*this, SEARCH_FOUND, 1, (LPARAM)&sinfo);
                        }
                    }
                }
                bAlwaysSearch = false;
            }
        }
    }
    SendMessage(*this, SEARCH_END, 0, 0);
    InterlockedExchange(&m_dwThreadRunning, FALSE);

    // refresh cursor
    POINT pt;
    GetCursorPos(&pt);
    SetCursorPos(pt.x, pt.y);

    return 0L;
}

bool CSearchDlg::MatchPath(LPCTSTR pathbuf)
{
    bool bPattern = false;
    // find start of pathname
    const TCHAR * pName = _tcsrchr(pathbuf, '\\');
    if (pName == NULL)
        pName = pathbuf;
    else
        pName++;    // skip the last '\\' char
    if (m_bUseRegexForPaths)
    {
        if ( (m_patterns.size()==0) || grepWin_match_i(m_patternregex, pName) )
        {
            bPattern = true;
        }
    }
    else
    {
        if (m_patterns.size())
        {
            wstring fname = pName;
            std::transform(fname.begin(), fname.end(), fname.begin(), (int(*)(int)) std::tolower);

            for (vector<wstring>::const_iterator it = m_patterns.begin(); it != m_patterns.end(); ++it)
                bPattern = bPattern || wcswildcmp(it->c_str(), fname.c_str());
        }
        else
            bPattern = true;
    }
    return bPattern;
}

int CSearchDlg::SearchFile(CSearchInfo& sinfo, bool bSearchAlways, bool bIncludeBinary, bool bUseRegex, bool bCaseSensitive, bool bDotMatchesNewline, const wstring& searchString)
{
    int nFound = 0;
    // we keep it simple:
    // files bigger than 30MB are considered binary. Binary files are searched
    // as if they're ANSI text files.
    wstring localSearchString = searchString;
    if (!bUseRegex)
        localSearchString = _T("\\Q") + searchString + _T("\\E");
    if ((sinfo.filesize < 30*1024*1024)||(m_bReplace))
    {
        CTextFile textfile;

        CTextFile::UnicodeType type = CTextFile::AUTOTYPE;
        if (textfile.Load(sinfo.filepath.c_str(), type, m_bUTF8))
        {
            sinfo.encoding = type;
            sinfo.readerror = false;
            if ((type != CTextFile::BINARY)||(bIncludeBinary)||bSearchAlways)
            {
                wstring::const_iterator start, end;
                start = textfile.GetFileString().begin();
                end = textfile.GetFileString().end();
                boost::match_results<wstring::const_iterator> what;
                try
                {
                    int ft = boost::regex::normal;
                    if (!bCaseSensitive)
                        ft |= boost::regbase::icase;
                    boost::wregex expression = boost::wregex(localSearchString, ft);
                    boost::match_results<wstring::const_iterator> whatc;
                    boost::match_flag_type flags = boost::match_default;
                    if (!bDotMatchesNewline)
                        flags |= boost::match_not_dot_newline;
                    while (regex_search(start, end, whatc, expression, flags))
                    {
                        if (whatc[0].matched)
                        {
                            nFound++;
                            long linestart = textfile.LineFromPosition(whatc[0].first-textfile.GetFileString().begin());
                            long lineend = textfile.LineFromPosition(whatc[0].second-textfile.GetFileString().begin());
                            for (long l = linestart; l <= lineend; ++l)
                            {
                                sinfo.matchlines.push_back(textfile.GetLineString(l));
                                sinfo.matchlinesnumbers.push_back(l);
                            }
                        }
                        // update search position:
                        if (start == whatc[0].second)
                        {
                            if (start == end)
                                break;
                            start++;
                        }
                        else
                            start = whatc[0].second;
                        // update flags:
                        flags |= boost::match_prev_avail;
                        flags |= boost::match_not_bob;
                    }
                    if ((m_bReplace)&&(nFound))
                    {
                        boost::match_flag_type flags = boost::match_default | boost::format_all;
                        if (!bDotMatchesNewline)
                            flags |= boost::match_not_dot_newline;
                        wstring replaced = regex_replace(textfile.GetFileString(), expression, m_replaceString, flags);
                        if (replaced.compare(textfile.GetFileString()))
                        {
                            textfile.SetFileContent(replaced);
                            if (m_bCreateBackup)
                            {
                                wstring backupfile = sinfo.filepath + _T(".bak");
                                CopyFile(sinfo.filepath.c_str(), backupfile.c_str(), FALSE);
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
                                    DWORD newAttributes = origAttributes & (~(FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_SYSTEM));
                                    SetFileAttributes(sinfo.filepath.c_str(), newAttributes);
                                    bool bRet = textfile.Save(sinfo.filepath.c_str());
                                    // restore the attributes
                                    SetFileAttributes(sinfo.filepath.c_str(), origAttributes);
                                    if (!bRet)
                                        return -1;
                                }
                            }
                        }
                    }
                }
                catch (const exception&)
                {
                    return -1;
                }
            }
        }
        else
        {
            sinfo.readerror = true;
            return 0;
        }
    }
    else
    {
        // assume binary file
        if (bIncludeBinary||bSearchAlways)
        {
            sinfo.encoding = CTextFile::BINARY;
            string filepath = CUnicodeUtils::StdGetANSI(sinfo.filepath);
            string searchfor = CUnicodeUtils::StdGetUTF8(searchString);

            if (!bUseRegex)
            {
                searchfor = "\\Q";
                searchfor += CUnicodeUtils::StdGetUTF8(searchString);
                searchfor += "\\E";
            }
            boost::spirit::classic::file_iterator<> start(filepath.c_str());
            boost::spirit::classic::file_iterator<> fbeg = start;
            boost::spirit::classic::file_iterator<> end = start.make_end();

            boost::match_results<string::const_iterator> what;
            boost::match_flag_type flags = boost::match_default;
            if (!bDotMatchesNewline)
                flags |= boost::match_not_dot_newline;
            try
            {
                boost::regex expression = boost::regex(searchfor);
                boost::match_results<boost::spirit::classic::file_iterator<>> whatc;
                while (boost::regex_search(start, end, whatc, expression, flags))
                {
                    nFound++;
                    sinfo.matchlinesnumbers.push_back(whatc[0].first-fbeg);
                    // update search position:
                    start = whatc[0].second;
                    // update flags:
                    flags |= boost::match_prev_avail;
                    flags |= boost::match_not_bob;
                }
            }
            catch (const exception&)
            {
                return -1;
            }
        }
    }

    return nFound;
}

DWORD WINAPI SearchThreadEntry(LPVOID lpParam)
{
    CSearchDlg * pThis = (CSearchDlg*)lpParam;
    if (pThis)
        return pThis->SearchThread();
    return 0L;
}

void CSearchDlg::formatDate(TCHAR date_native[], const FILETIME& filetime, bool force_short_fmt)
{
    date_native[0] = '\0';

    // Convert UTC to local time
    SYSTEMTIME systemtime;
    FileTimeToSystemTime(&filetime,&systemtime);

    static TIME_ZONE_INFORMATION timeZone = {-1};
    if (timeZone.Bias == -1)
        GetTimeZoneInformation (&timeZone);

    SYSTEMTIME localsystime;
    SystemTimeToTzSpecificLocalTime(&timeZone, &systemtime,&localsystime);

    TCHAR timebuf[GREPWIN_DATEBUFFER] = {0};
    TCHAR datebuf[GREPWIN_DATEBUFFER] = {0};

    LCID locale = MAKELCID(MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), SORT_DEFAULT);

    /// reusing this instance is vital for \ref formatDate performance

    DWORD flags = force_short_fmt ? DATE_SHORTDATE : DATE_LONGDATE;

    GetDateFormat(locale, flags, &localsystime, NULL, datebuf, GREPWIN_DATEBUFFER);
    GetTimeFormat(locale, 0, &localsystime, NULL, timebuf, GREPWIN_DATEBUFFER);
    _tcsncat_s(date_native, GREPWIN_DATEBUFFER, datebuf, GREPWIN_DATEBUFFER);
    _tcsncat_s(date_native, GREPWIN_DATEBUFFER, _T(" "), GREPWIN_DATEBUFFER);
    _tcsncat_s(date_native, GREPWIN_DATEBUFFER, timebuf, GREPWIN_DATEBUFFER);
}

int CSearchDlg::CheckRegex()
{
    TCHAR buf[MAX_PATH*4] = {0};
    GetDlgItemText(*this, IDC_SEARCHTEXT, buf, MAX_PATH*4);
    int len = _tcslen(buf);
    if (IsDlgButtonChecked(*this, IDC_REGEXRADIO) == BST_CHECKED)
    {
        // check if the regex is valid
        bool bValid = true;
        if (len)
        {
            try
            {
                boost::wregex expression = boost::wregex(buf);
            }
            catch (const exception&)
            {
                bValid = false;
            }
        }
        if (len)
        {
            if (bValid)
            {
                SetDlgItemText(*this, IDC_REGEXOKLABEL, _T("regex ok"));
                DialogEnableWindow(IDOK, true);
                DialogEnableWindow(IDC_REPLACE, true);
            }
            else
            {
                SetDlgItemText(*this, IDC_REGEXOKLABEL, _T("invalid regex!"));
                DialogEnableWindow(IDOK, false);
                DialogEnableWindow(IDC_REPLACE, false);
            }
        }
        else
        {
            SetDlgItemText(*this, IDC_REGEXOKLABEL, _T(""));
            DialogEnableWindow(IDOK, true);
            DialogEnableWindow(IDC_REPLACE, true);
        }
    }
    else
    {
        SetDlgItemText(*this, IDC_REGEXOKLABEL, _T(""));
        DialogEnableWindow(IDOK, true);
        DialogEnableWindow(IDC_REPLACE, true);
    }

    return len;
}

void CSearchDlg::AutoSizeAllColumns()
{
    HWND hListControl = GetDlgItem(*this, IDC_RESULTLIST);
    ListView_SetColumnWidth(hListControl, 0, LVSCW_AUTOSIZE_USEHEADER);
    ListView_SetColumnWidth(hListControl, 1, LVSCW_AUTOSIZE_USEHEADER);
    ListView_SetColumnWidth(hListControl, 2, LVSCW_AUTOSIZE_USEHEADER);
    ListView_SetColumnWidth(hListControl, 3, LVSCW_AUTOSIZE_USEHEADER);
}

int CSearchDlg::GetSelectedListIndex(int index)
{
    HWND hListControl = GetDlgItem(*this, IDC_RESULTLIST);
    int iItem = -1;
    LVITEM lv = {0};
    lv.iItem = index;
    lv.mask = LVIF_PARAM;
    if (ListView_GetItem(hListControl, &lv))
        iItem = lv.lParam;

    return iItem;
}