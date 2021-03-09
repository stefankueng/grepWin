// grepWin - regex search and replace for Windows

// Copyright (C) 2007-2021 - Stefan Kueng

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
#include "BaseDialog.h"
#include "SearchInfo.h"
#include "BookmarksDlg.h"
#include "DlgResizer.h"
#include "FileDropTarget.h"
#include "AutoComplete.h"
#include "Registry.h"
#include "EditDoubleClick.h"
#include "StringUtils.h"
#include <string>
#include <vector>
#include <set>
#include <thread>

#include <wrl.h>
using namespace Microsoft::WRL;

#define SEARCH_FOUND         (WM_APP + 1)
#define SEARCH_START         (WM_APP + 2)
#define SEARCH_PROGRESS      (WM_APP + 3)
#define SEARCH_END           (WM_APP + 4)
#define WM_GREPWIN_THREADEND (WM_APP + 5)

#define ID_ABOUTBOX 0x0010
#define ID_CLONE    0x0011

enum class ExecuteAction
{
    None,
    Search,
    Replace,
    Capture
};

/**
 * search dialog.
 */
class CSearchDlg : public CDialog
{
public:
    CSearchDlg(HWND hParent);
    ~CSearchDlg();

    DWORD SearchThread();
    void  SetSearchPath(const std::wstring& path);
    void  SetSearchString(const std::wstring& search) { m_searchString = search; }
    void  SetFileMask(const std::wstring& mask, bool reg);
    void  SetDirExcludeRegexMask(const std::wstring& mask) { m_excludeDirsPatternRegex = mask; }
    void  SetReplaceWith(const std::wstring& replace) { m_replaceString = replace; }
    void  SetUseRegex(bool reg) { m_bUseRegex = reg; }
    void  SetPreset(const std::wstring& preset);
    void  SetCaseSensitive(bool bSet);
    void  SetMatchesNewline(bool bSet);
    void  SetCreateBackups(bool bSet);
    void  SetCreateBackupsInFolders(bool bSet);
    void  SetUTF8(bool bSet);
    void  SetBinary(bool bSet);
    void  SetSize(uint64_t size, int cmp);
    void  SetIncludeSystem(bool bSet);
    void  SetIncludeHidden(bool bSet);
    void  SetIncludeSubfolders(bool bSet);
    void  SetIncludeBinary(bool bSet);
    void  SetDateLimit(int dateLimit, FILETIME t1, FILETIME t2);
    void  SetNoSaveSettings(bool noSave) { m_bNoSaveSettings = noSave; }

    void SetExecute(ExecuteAction execute) { m_executeImmediately = execute; }
    void SetEndDialog() { m_endDialog = true; }
    void SetShowContent()
    {
        m_showContent    = true;
        m_showContentSet = true;
    }

protected:
    LRESULT CALLBACK DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) override;
    LRESULT          DoCommand(int id, int msg);
    bool             PreTranslateMessage(MSG* pMsg) override;

    void SearchFile(CSearchInfo sInfo, const std::wstring& searchRoot, bool bSearchAlways, bool bIncludeBinary, bool bUseRegex, bool bCaseSensitive, bool bDotMatchesNewline, const std::wstring& searchString, const std::wstring& searchStringUtf16Le, volatile LONG* bCancelled);

    bool        InitResultList();
    void        FillResultList();
    bool        AddFoundEntry(CSearchInfo* pInfo, bool bOnlyListControl = false);
    void        ShowContextMenu(int x, int y);
    void        DoListNotify(LPNMITEMACTIVATE lpNMItemActivate);
    void        OpenFileAtListIndex(int listIndex);
    void        UpdateInfoLabel();
    bool        SaveSettings();
    void        SaveWndPosition();
    void        formatDate(wchar_t dateNative[], const FILETIME& fileTime, bool forceShortFmt) const;
    int         CheckRegex();
    bool        MatchPath(LPCTSTR pathBuf);
    void        AutoSizeAllColumns();
    int         GetSelectedListIndex(int index);
    static bool FailedShowMessage(HRESULT hr);
    void        CheckForUpdates(bool force = false);
    void        ShowUpdateAvailable();
    bool        IsVersionNewer(const std::wstring& sVer) const;
    bool        CloneWindow();

private:
    static bool NameCompareAsc(const CSearchInfo& entry1, const CSearchInfo& entry2);
    static bool SizeCompareAsc(const CSearchInfo& entry1, const CSearchInfo& entry2);
    static bool MatchesCompareAsc(const CSearchInfo& entry1, const CSearchInfo& entry2);
    static bool PathCompareAsc(const CSearchInfo& entry1, const CSearchInfo& entry2);
    static bool EncodingCompareAsc(const CSearchInfo& entry1, const CSearchInfo& entry2);
    static bool ModifiedTimeCompareAsc(const CSearchInfo& entry1, const CSearchInfo& entry2);
    static bool ExtCompareAsc(const CSearchInfo& entry1, const CSearchInfo& entry2);

    static bool NameCompareDesc(const CSearchInfo& entry1, const CSearchInfo& entry2);
    static bool SizeCompareDesc(const CSearchInfo& entry1, const CSearchInfo& entry2);
    static bool MatchesCompareDesc(const CSearchInfo& entry1, const CSearchInfo& entry2);
    static bool PathCompareDesc(const CSearchInfo& entry1, const CSearchInfo& entry2);
    static bool EncodingCompareDesc(const CSearchInfo& entry1, const CSearchInfo& entry2);
    static bool ModifiedTimeCompareDesc(const CSearchInfo& entry1, const CSearchInfo& entry2);
    static bool ExtCompareDesc(const CSearchInfo& entry1, const CSearchInfo& entry2);

private:
    HWND          m_hParent;
    volatile LONG m_dwThreadRunning;
    volatile LONG m_cancelled;

    CBookmarksDlg*        m_pBookmarksDlg;
    ComPtr<ITaskbarList3> m_pTaskbarList;

    std::wstring                      m_searchPath;
    std::wstring                      m_searchString;
    std::wstring                      m_replaceString;
    std::vector<std::wstring>         m_patterns;
    std::wstring                      m_patternRegex;
    std::wstring                      m_excludeDirsPatternRegex;
    bool                              m_bUseRegex;
    bool                              m_bUseRegexForPaths;
    bool                              m_bAllSize;
    uint64_t                          m_lSize;
    int                               m_sizeCmp;
    bool                              m_bIncludeSystem;
    bool                              m_bIncludeSystemC;
    bool                              m_bIncludeHidden;
    bool                              m_bIncludeHiddenC;
    bool                              m_bIncludeSubfolders;
    bool                              m_bIncludeSubfoldersC;
    bool                              m_bIncludeBinary;
    bool                              m_bIncludeBinaryC;
    bool                              m_bCreateBackup;
    bool                              m_bCreateBackupC;
    bool                              m_bCreateBackupInFolders;
    bool                              m_bCreateBackupInFoldersC;
    bool                              m_bUTF8;
    bool                              m_bUTF8C;
    bool                              m_bForceBinary;
    bool                              m_bCaseSensitive;
    bool                              m_bCaseSensitiveC;
    bool                              m_bDotMatchesNewline;
    bool                              m_bDotMatchesNewlineC;
    bool                              m_bNotSearch;
    bool                              m_bCaptureSearch;
    bool                              m_bSizeC;
    bool                              m_endDialog;
    ExecuteAction                     m_executeImmediately;
    int                               m_dateLimit;
    bool                              m_bDateLimitC;
    FILETIME                          m_date1;
    FILETIME                          m_date2;
    bool                              m_bNoSaveSettings;
    bool                              m_bReplace;
    bool                              m_bConfirmationOnReplace;
    bool                              m_showContent;
    bool                              m_showContentSet;
    std::vector<CSearchInfo>          m_items;
    std::vector<std::tuple<int, int>> m_listItems;
    std::set<std::wstring>            m_backupAndTempFiles;
    int                               m_totalItems;
    int                               m_searchedItems;
    int                               m_totalMatches;
    bool                              m_bAscending;
    std::wstring                      m_resultString;

    CDlgResizer m_resizer;
    int         m_themeCallbackId;

    CFileDropTarget* m_pDropTarget;

    static UINT m_grepwinStartupmsg;

    std::thread m_updateCheckThread;

    CAutoComplete m_autoCompleteFilePatterns;
    CAutoComplete m_autoCompleteExcludeDirsPatterns;
    CAutoComplete m_autoCompleteSearchPatterns;
    CAutoComplete m_autoCompleteReplacePatterns;
    CAutoComplete m_autoCompleteSearchPaths;

    CEditDoubleClick m_editFilePatterns;
    CEditDoubleClick m_editExcludeDirsPatterns;
    CEditDoubleClick m_editSearchPatterns;
    CEditDoubleClick m_editReplacePatterns;
    CEditDoubleClick m_editSearchPaths;

    CRegStdDWORD  m_regUseRegex;
    CRegStdDWORD  m_regAllSize;
    CRegStdString m_regSize;
    CRegStdDWORD  m_regSizeCombo;
    CRegStdDWORD  m_regIncludeSystem;
    CRegStdDWORD  m_regIncludeHidden;
    CRegStdDWORD  m_regIncludeSubfolders;
    CRegStdDWORD  m_regIncludeBinary;
    CRegStdDWORD  m_regCreateBackup;
    CRegStdDWORD  m_regUTF8;
    CRegStdDWORD  m_regBinary;
    CRegStdDWORD  m_regCaseSensitive;
    CRegStdDWORD  m_regDotMatchesNewline;
    CRegStdDWORD  m_regUseRegexForPaths;
    CRegStdString m_regPattern;
    CRegStdString m_regExcludeDirsPattern;
    CRegStdString m_regSearchPath;
    CRegStdString m_regEditorCmd;
    CRegStdDWORD  m_regBackupInFolder;
    CRegStdDWORD  m_regDateLimit;
    CRegStdDWORD  m_regDate1Low;
    CRegStdDWORD  m_regDate1High;
    CRegStdDWORD  m_regDate2Low;
    CRegStdDWORD  m_regDate2High;
    CRegStdDWORD  m_regShowContent;
};