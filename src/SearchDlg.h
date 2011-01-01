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
#pragma once
#include "basedialog.h"
#include "SearchInfo.h"
#include "DlgResizer.h"
#include "FileDropTarget.h"
#include "AutoComplete.h"
#include "Registry.h"
#include "hyperlink.h"
#include "AeroControls.h"
#include <string>
#include <vector>

using namespace std;

#define SEARCH_FOUND        (WM_APP+1)
#define SEARCH_START        (WM_APP+2)
#define SEARCH_PROGRESS     (WM_APP+3)
#define SEARCH_END          (WM_APP+4)

#define ID_ABOUTBOX         0x0010

/**
 * search dialog.
 */
class CSearchDlg : public CDialog
{
public:
    CSearchDlg(HWND hParent);
    ~CSearchDlg(void);

    DWORD                   SearchThread();
    void                    SetSearchPath(const wstring& path) {m_searchpath = path;}
    void                    SetSearchString(const wstring& search) {m_searchString = search;}
    void                    SetFileMask(const wstring& mask, bool reg) {m_patternregex = mask; m_bUseRegexForPaths = reg;}
    void                    SetExcludeFileMask(const wstring& mask) {m_excludedirspatternregex = mask;}
    void                    SetReplaceWith(const wstring& replace) {m_replaceString = replace;}

    void                    SetCaseSensitive(bool bSet) {m_bCaseSensitiveC = true; m_bCaseSensitive = bSet;}
    void                    SetMatchesNewline(bool bSet) {m_bDotMatchesNewlineC = true; m_bDotMatchesNewline = bSet;}
    void                    SetCreateBackups(bool bSet) {m_bCreateBackupC = true; m_bCreateBackup = bSet;}
    void                    SetUTF8(bool bSet) {m_bUTF8C = true;m_bUTF8 = bSet;}
    void                    SetSize(DWORD size, int cmp) {m_bSizeC = true; m_lSize = size; m_sizeCmp = cmp; m_bAllSize = (size == (DWORD)-1);}
    void                    SetIncludeSystem(bool bSet) {m_bIncludeSystemC = true; m_bIncludeSystem = bSet;}
    void                    SetIncludeHidden(bool bSet) {m_bIncludeHiddenC = true; m_bIncludeHidden = bSet;}
    void                    SetIncludeSubfolders(bool bSet) {m_bIncludeSubfoldersC = true; m_bIncludeSubfolders = bSet;}
    void                    SetIncludeBinary(bool bSet) {m_bIncludeBinaryC = true; m_bIncludeBinary = bSet;}

    void                    SetExecute(bool execute) {m_bExecuteImmediately = execute;}
protected:
    LRESULT CALLBACK        DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT                 DoCommand(int id, int msg);
    bool                    PreTranslateMessage(MSG* pMsg);

    int                     SearchFile(CSearchInfo& sinfo, bool bSearchAlways, bool bIncludeBinary, bool bUseRegex, bool bCaseSensitive, bool bDotMatchesNewline, const wstring& searchString);

    bool                    InitResultList();
    void                    FillResultList();
    bool                    AddFoundEntry(CSearchInfo * pInfo, bool bOnlyListControl = false);
    void                    ShowContextMenu(int x, int y);
    void                    DoListNotify(LPNMITEMACTIVATE lpNMItemActivate);
    void                    UpdateInfoLabel();
    void                    UpdateSearchButton();
    bool                    SaveSettings();
    void                    SaveWndPosition();
    void                    formatDate(TCHAR date_native[], const FILETIME& filetime, bool force_short_fmt);
    int                     CheckRegex();
    bool                    MatchPath(LPCTSTR pathbuf);
    void                    AutoSizeAllColumns();
    int                     GetSelectedListIndex(int index);

private:
    static bool             NameCompareAsc(const CSearchInfo Entry1, const CSearchInfo Entry2);
    static bool             SizeCompareAsc(const CSearchInfo Entry1, const CSearchInfo Entry2);
    static bool             MatchesCompareAsc(const CSearchInfo Entry1, const CSearchInfo Entry2);
    static bool             PathCompareAsc(const CSearchInfo Entry1, const CSearchInfo Entry2);
    static bool             EncodingCompareAsc(const CSearchInfo Entry1, const CSearchInfo Entry2);
    static bool             ModifiedTimeCompareAsc(const CSearchInfo Entry1, const CSearchInfo Entry2);

    static bool             NameCompareDesc(const CSearchInfo Entry1, const CSearchInfo Entry2);
    static bool             SizeCompareDesc(const CSearchInfo Entry1, const CSearchInfo Entry2);
    static bool             MatchesCompareDesc(const CSearchInfo Entry1, const CSearchInfo Entry2);
    static bool             PathCompareDesc(const CSearchInfo Entry1, const CSearchInfo Entry2);
    static bool             EncodingCompareDesc(const CSearchInfo Entry1, const CSearchInfo Entry2);
    static bool             ModifiedTimeCompareDesc(const CSearchInfo Entry1, const CSearchInfo Entry2);

private:
    HWND                    m_hParent;
    volatile LONG           m_dwThreadRunning;
    volatile LONG           m_Cancelled;
    wstring                 m_searchpath;
    wstring                 m_searchString;
    wstring                 m_replaceString;
    vector<wstring>         m_patterns;
    wstring                 m_patternregex;
    wstring                 m_excludedirspatternregex;
    bool                    m_bUseRegex;
    bool                    m_bUseRegexForPaths;
    bool                    m_bAllSize;
    DWORD                   m_lSize;
    int                     m_sizeCmp;
    bool                    m_bIncludeSystem;
    bool                    m_bIncludeSystemC;
    bool                    m_bIncludeHidden;
    bool                    m_bIncludeHiddenC;
    bool                    m_bIncludeSubfolders;
    bool                    m_bIncludeSubfoldersC;
    bool                    m_bIncludeBinary;
    bool                    m_bIncludeBinaryC;
    bool                    m_bCreateBackup;
    bool                    m_bCreateBackupC;
    bool                    m_bUTF8;
    bool                    m_bUTF8C;
    bool                    m_bCaseSensitive;
    bool                    m_bCaseSensitiveC;
    bool                    m_bDotMatchesNewline;
    bool                    m_bDotMatchesNewlineC;
    bool                    m_bSizeC;
    bool                    m_bExecuteImmediately;

    bool                    m_bReplace;
    HANDLE                  m_hSearchThread;

    vector<CSearchInfo>     m_items;
    int                     m_totalitems;
    int                     m_searchedItems;
    int                     m_totalmatches;
    bool                    m_bAscending;
    wstring                 m_resultString;

    CDlgResizer             m_resizer;
    CHyperLink              m_link;

    CFileDropTarget *       m_pDropTarget;
    AeroControlBase         m_aerocontrols;

    DWORD                   m_startTime;
    static UINT             GREPWIN_STARTUPMSG;

    CAutoComplete           m_AutoCompleteFilePatterns;
    CAutoComplete           m_AutoCompleteExcludeDirsPatterns;
    CAutoComplete           m_AutoCompleteSearchPatterns;
    CAutoComplete           m_AutoCompleteReplacePatterns;
    CAutoComplete           m_AutoCompleteSearchPaths;
    CRegStdWORD             m_regUseRegex;
    CRegStdWORD             m_regAllSize;
    CRegStdWORD             m_regSize;
    CRegStdWORD             m_regSizeCombo;
    CRegStdWORD             m_regIncludeSystem;
    CRegStdWORD             m_regIncludeHidden;
    CRegStdWORD             m_regIncludeSubfolders;
    CRegStdWORD             m_regIncludeBinary;
    CRegStdWORD             m_regCreateBackup;
    CRegStdWORD             m_regUTF8;
    CRegStdWORD             m_regCaseSensitive;
    CRegStdWORD             m_regDotMatchesNewline;
    CRegStdWORD             m_regUseRegexForPaths;
    CRegStdString           m_regPattern;
    CRegStdString           m_regExcludeDirsPattern;
    CRegStdString           m_regSearchPath;
};