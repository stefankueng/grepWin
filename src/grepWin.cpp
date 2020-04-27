// grepWin - regex search and replace for Windows

// Copyright (C) 2007-2008, 2010-2020 - Stefan Kueng

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
#include "AboutDlg.h"
#include "CmdLineParser.h"
#include "Registry.h"
#include "Language.h"
#include "StringUtils.h"
#include "PathUtils.h"
#pragma warning(push)
#pragma warning(disable : 4458) // declaration of 'xxx' hides class member
#include <gdiplus.h>
#pragma warning(pop)

// Global Variables:
HINSTANCE  g_hInst; // current instance
bool       bPortable = false;
CSimpleIni g_iniFile;

ULONGLONG g_startTime = GetTickCount64();

static std::wstring SanitizeSearchPaths(const std::wstring& searchpath)
{
    std::vector<std::wstring> container;
    stringtok(container, searchpath, true);
    std::wstring sResult;
    for (auto path : container)
    {
        if (!sResult.empty())
            sResult += L"|";
        size_t endpos = path.find_last_not_of(L" \"\t");
        if (std::wstring::npos != endpos)
        {
            path = path.substr(0, endpos + 1);
        }
        size_t startpos = path.find_first_not_of(L" \"\t");
        if ((startpos > 0) && (std::wstring::npos != startpos))
        {
            path = path.substr(startpos);
        }
        sResult += CPathUtils::GetLongPathname(path);
    }
    return sResult;
}

static void RegisterContextMenu(bool bAdd)
{
    if (bAdd)
    {
        std::wstring sIconPath = CStringUtils::Format(L"%s,-%d", CPathUtils::GetLongPathname(CPathUtils::GetModulePath()).c_str(), IDI_GREPWIN);
        std::wstring sExePath  = CStringUtils::Format(L"%s /searchpath:\"%%1\"", CPathUtils::GetLongPathname(CPathUtils::GetModulePath()).c_str());
        SHSetValue(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\grepWin", NULL, REG_SZ, L"search with grepWin", sizeof(L"search with grepWin") + 2);
        SHSetValue(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\grepWin", L"Icon", REG_SZ, sIconPath.c_str(), DWORD((sIconPath.size() + 1) * sizeof(WCHAR)));
        SHSetValue(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\grepWin\\Command", NULL, REG_SZ, sExePath.c_str(), DWORD((sExePath.size() + 1) * sizeof(WCHAR)));
        SHSetValue(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\Background\\shell\\grepWin", NULL, REG_SZ, L"search with grepWin", sizeof(L"search with grepWin") + 2);
        SHSetValue(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\Background\\shell\\grepWin", L"Icon", REG_SZ, sIconPath.c_str(), DWORD((sIconPath.size() + 1) * sizeof(WCHAR)));

        SHSetValue(HKEY_CURRENT_USER, L"Software\\Classes\\Folder\\shell\\grepWin", NULL, REG_SZ, L"search with grepWin", sizeof(L"search with grepWin") + 2);
        SHSetValue(HKEY_CURRENT_USER, L"Software\\Classes\\Folder\\shell\\grepWin", L"Icon", REG_SZ, sIconPath.c_str(), DWORD((sIconPath.size() + 1) * sizeof(WCHAR)));
        SHSetValue(HKEY_CURRENT_USER, L"Software\\Classes\\Folder\\shell\\grepWin\\Command", NULL, REG_SZ, sExePath.c_str(), DWORD((sExePath.size() + 1) * sizeof(WCHAR)));
        SHSetValue(HKEY_CURRENT_USER, L"Software\\Classes\\Drive\\shell\\grepWin", NULL, REG_SZ, L"search with grepWin", sizeof(L"search with grepWin") + 2);
        SHSetValue(HKEY_CURRENT_USER, L"Software\\Classes\\Drive\\shell\\grepWin", L"Icon", REG_SZ, sIconPath.c_str(), DWORD((sIconPath.size() + 1) * sizeof(WCHAR)));
        SHSetValue(HKEY_CURRENT_USER, L"Software\\Classes\\Drive\\shell\\grepWin\\Command", NULL, REG_SZ, sExePath.c_str(), DWORD((sExePath.size() + 1) * sizeof(WCHAR)));
        SHSetValue(HKEY_CURRENT_USER, L"Software\\Classes\\*\\shell\\grepWin", NULL, REG_SZ, L"search with grepWin", sizeof(L"search with grepWin") + 2);
        SHSetValue(HKEY_CURRENT_USER, L"Software\\Classes\\*\\shell\\grepWin", L"Icon", REG_SZ, sIconPath.c_str(), DWORD((sIconPath.size() + 1) * sizeof(WCHAR)));
        SHSetValue(HKEY_CURRENT_USER, L"Software\\Classes\\*\\shell\\grepWin\\Command", NULL, REG_SZ, sExePath.c_str(), DWORD((sExePath.size() + 1) * sizeof(WCHAR)));
        SHSetValue(HKEY_CURRENT_USER, L"Software\\Classes\\*\\shell\\grepWin", L"MultiSelectModel", REG_SZ, L"Player", sizeof(L"Player") + 2);

        sExePath = CStringUtils::Format(L"%s /searchpath:\"%%V\"", CPathUtils::GetLongPathname(CPathUtils::GetModulePath()).c_str());
        SHSetValue(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\Background\\shell\\grepWin\\Command", NULL, REG_SZ, sExePath.c_str(), DWORD((sExePath.size() + 1) * sizeof(WCHAR)));
    }
    else
    {
        SHDeleteKey(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\grepWin");
        SHDeleteKey(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\Background\\shell\\grepWin");
        SHDeleteKey(HKEY_CURRENT_USER, L"Software\\Classes\\Folder\\shell\\grepWin");
        SHDeleteKey(HKEY_CURRENT_USER, L"Software\\Classes\\Drive\\shell\\grepWin");
        SHDeleteKey(HKEY_CURRENT_USER, L"Software\\Classes\\*\\shell\\grepWin");
    }
}

BOOL CALLBACK windowenumerator(__in HWND hwnd, __in LPARAM lParam)
{
    HWND* pWnd          = (HWND*)lParam;
    WCHAR buf[MAX_PATH] = {0};
    GetWindowText(hwnd, buf, _countof(buf));
    if (_wcsnicmp(buf, L"grepwin :", 9) == 0)
    {
        *pWnd = hwnd;
    }
    return TRUE;
}

int APIENTRY _tWinMain(HINSTANCE hInstance,
                       HINSTANCE hPrevInstance,
                       LPTSTR    lpCmdLine,
                       int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    // uncomment the following lines for low-memory tests.
    // note: process needs to run elevated for this to work.
    //
    //auto job = CreateJobObject(NULL, NULL);
    //JOBOBJECT_EXTENDED_LIMIT_INFORMATION joblimit = { 0 };
    //joblimit.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_WORKINGSET;
    //joblimit.JobMemoryLimit = 30 * 1024 * 1024;
    //joblimit.ProcessMemoryLimit = 30 * 1024 * 1024;
    //joblimit.PeakProcessMemoryUsed = 30 * 1024 * 1024;
    //joblimit.BasicLimitInformation.MaximumWorkingSetSize = 30 * 1024 * 1024;
    //joblimit.BasicLimitInformation.MinimumWorkingSetSize = 30 * 1024;
    //SetInformationJobObject(job, JobObjectExtendedLimitInformation, &joblimit, sizeof(joblimit));
    //AssignProcessToJobObject(job, GetCurrentProcess());

    SetDllDirectory(L"");
    // if multiple items are selected in explorer and grepWin is started for all of them,
    // explorer starts multiple grepWin instances at once. In case there's already a grepWin instance
    // running, sleep for a while to give that instance time to fully initialize
    std::unique_ptr<std::remove_pointer_t<HANDLE>, decltype(&::CloseHandle)> hReloadProtection(
        ::CreateMutex(NULL, FALSE, L"{6473AA76-0EAE-4C96-8C99-AFDFEFFE42B5}"), CloseHandle);

    bool   alreadyRunning    = false;
    if ((!hReloadProtection) || (GetLastError() == ERROR_ALREADY_EXISTS))
    {
        // An instance of grepWin is already running
        alreadyRunning = true;
    }

    g_hInst = hInstance;
    ::OleInitialize(NULL);
    ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    // we need some of the common controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC  = ICC_LINK_CLASS | ICC_LISTVIEW_CLASSES | ICC_PAGESCROLLER_CLASS | ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES | ICC_TAB_CLASSES | ICC_TREEVIEW_CLASSES | ICC_UPDOWN_CLASS | ICC_USEREX_CLASSES | ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icex);

    std::unique_ptr<std::remove_pointer_t<HMODULE>, decltype(&::FreeLibrary)> hRichEdt(LoadLibrary(_T("Riched20.dll")), FreeLibrary);

    CCmdLineParser parser(lpCmdLine);

    if (parser.HasKey(L"register"))
    {
        RegisterContextMenu(true);
        return FALSE;
    }
    if ((parser.HasKey(L"unregister")) || (parser.HasKey(L"deregister")))
    {
        RegisterContextMenu(false);
        return FALSE;
    }

    bool bQuit   = false;
    HWND hWnd    = NULL;
    int  timeout = 20;
    do
    {
        EnumWindows(windowenumerator, (LPARAM)&hWnd);
        if (alreadyRunning && (hWnd == NULL))
            Sleep(100);
        timeout--;
    } while ((hWnd == NULL) && alreadyRunning && timeout);

    auto modulename = CPathUtils::GetFileName(CPathUtils::GetModulePath(0));
    bPortable       = ((_tcsstr(modulename.c_str(), _T("portable"))) || (parser.HasKey(_T("portable"))));

    std::wstring iniPath = CPathUtils::GetModuleDir(0);
    iniPath += L"\\grepwin.ini";
    if (parser.HasVal(L"inipath"))
        iniPath = parser.GetVal(L"inipath");

    if (bPortable)
    {
        g_iniFile.SetUnicode();
        g_iniFile.LoadFile(iniPath.c_str());
    }

    if (hWnd)
    {
        bool bOnlyOne = !!DWORD(CRegStdDWORD(_T("Software\\grepWin\\onlyone"), 0));
        if (bPortable)
            bOnlyOne = !!_wtoi(g_iniFile.GetValue(L"global", L"onlyone", L"0"));
        UINT GREPWIN_STARTUPMSG = RegisterWindowMessage(_T("grepWin_StartupMessage"));
        if (SendMessage(hWnd, GREPWIN_STARTUPMSG, 0, 0)) // send the new path
        {
            std::wstring spath = parser.GetVal(_T("searchpath"));
            SearchReplace(spath, L"/", L"\\");
            spath                   = SanitizeSearchPaths(spath);
            COPYDATASTRUCT CopyData = {0};
            CopyData.lpData         = (LPVOID)spath.c_str();
            CopyData.cbData         = (DWORD)spath.size() * sizeof(wchar_t);
            SendMessage(hWnd, WM_COPYDATA, 0, (LPARAM)&CopyData);
            SetForegroundWindow(hWnd); //set the window to front
            bQuit = true;
        }
        else if (bOnlyOne)
        {
            std::wstring spath = parser.HasVal(L"searchpath") ? parser.GetVal(_T("searchpath")) : L"";
            SearchReplace(spath, L"/", L"\\");
            spath                   = SanitizeSearchPaths(spath);
            COPYDATASTRUCT CopyData = {0};
            CopyData.lpData         = (LPVOID)spath.c_str();
            CopyData.cbData         = (DWORD)spath.size() * sizeof(wchar_t);
            SendMessage(hWnd, WM_COPYDATA, 1, (LPARAM)&CopyData);
            SetForegroundWindow(hWnd); //set the window to front
            bQuit = true;
        }
    }

    int ret = 0;
    if (!bQuit)
    {
        Gdiplus::GdiplusStartupInput gdiplusStartupInput;
        ULONG_PTR                    gdiplusToken;
        // Initialize GDI+.
        Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

        CLanguage::Instance().LoadFile(bPortable ? g_iniFile.GetValue(L"global", L"languagefile", L"") : std::wstring(CRegStdString(L"Software\\grepWin\\languagefile")));
        if (parser.HasKey(_T("about")) || parser.HasKey(_T("?")) || parser.HasKey(_T("help")))
        {
            CAboutDlg aboutDlg(NULL);
            ret = (int)aboutDlg.DoModal(hInstance, IDD_ABOUT, NULL, NULL);
        }
        else
        {
            CSearchDlg searchDlg(NULL);
            if (parser.HasVal(_T("searchpath")))
            {
                std::wstring spath = parser.GetVal(L"searchpath");
                spath              = SanitizeSearchPaths(spath);
                searchDlg.SetSearchPath(spath);
            }
            if (parser.HasVal(_T("searchfor")))
                searchDlg.SetSearchString(parser.GetVal(_T("searchfor")));
            if (parser.HasVal(_T("filemaskregex")))
                searchDlg.SetFileMask(parser.GetVal(_T("filemaskregex")), true);
            if (parser.HasVal(_T("filemask")))
                searchDlg.SetFileMask(parser.GetVal(_T("filemask")), false);
            if (parser.HasVal(_T("filemaskexclude")))
                searchDlg.SetExcludeFileMask(parser.GetVal(_T("filemaskexclude")));
            if (parser.HasVal(_T("replacewith")))
                searchDlg.SetReplaceWith(parser.GetVal(_T("replacewith")));

            if (parser.HasVal(_T("i")))
                searchDlg.SetCaseSensitive(_tcsicmp(parser.GetVal(_T("i")), _T("yes")) != 0);
            if (parser.HasVal(_T("n")))
                searchDlg.SetMatchesNewline(_tcsicmp(parser.GetVal(_T("n")), _T("yes")) == 0);
            if (parser.HasVal(_T("k")))
                searchDlg.SetCreateBackups(_tcsicmp(parser.GetVal(_T("k")), _T("yes")) == 0);
            if (parser.HasVal(_T("utf8")))
                searchDlg.SetUTF8(_tcsicmp(parser.GetVal(_T("utf8")), _T("yes")) == 0);
            if (parser.HasVal(_T("size")))
            {
                int cmp = 0;
                if (parser.HasVal(_T("sizecmp")))
                    cmp = parser.GetLongVal(_T("sizecmp"));
                searchDlg.SetSize(parser.GetLongVal(_T("size")), cmp);
            }
            if (parser.HasVal(_T("s")))
                searchDlg.SetIncludeSystem(_tcsicmp(parser.GetVal(_T("s")), _T("yes")) == 0);
            if (parser.HasVal(_T("h")))
                searchDlg.SetIncludeHidden(_tcsicmp(parser.GetVal(_T("h")), _T("yes")) == 0);
            if (parser.HasVal(_T("u")))
                searchDlg.SetIncludeSubfolders(_tcsicmp(parser.GetVal(_T("u")), _T("yes")) == 0);
            if (parser.HasVal(_T("b")))
                searchDlg.SetIncludeBinary(_tcsicmp(parser.GetVal(_T("b")), _T("yes")) == 0);
            if (parser.HasVal(_T("regex")))
                searchDlg.SetUseRegex(_tcsicmp(parser.GetVal(_T("regex")), _T("yes")) == 0);
            else if (parser.HasVal(_T("searchfor")))
                searchDlg.SetUseRegex(true);

            if (parser.HasKey(L"execute") || parser.HasKey(L"executesearch"))
                searchDlg.SetExecute(ExecuteAction::Search);
            if (parser.HasKey(L"executereplace"))
                searchDlg.SetExecute(ExecuteAction::Replace);
            if (parser.HasKey(L"closedialog"))
                searchDlg.SetEndDialog();
            if (parser.HasKey(L"content"))
                searchDlg.SetShowContent();
            if (parser.HasVal(L"datelimit") && parser.HasVal(L"date1"))
            {
                FILETIME date1  = {0};
                FILETIME date2  = {0};
                int      year   = 0;
                int      month  = 0;
                int      day    = 0;
                auto     sDate1 = parser.GetVal(L"date1");
                swscanf_s(sDate1, L"%4d:%2d:%2d", &year, &month, &day);
                SYSTEMTIME sysTime = {0};
                sysTime.wYear      = (WORD)year;
                sysTime.wMonth     = (WORD)month;
                sysTime.wDay       = (WORD)day;
                SystemTimeToFileTime(&sysTime, &date1);
                if (parser.HasVal(L"date2"))
                {
                    auto sDate2 = parser.GetVal(L"date2");
                    swscanf_s(sDate2, L"%4d:%2d:%2d", &year, &month, &day);
                    sysTime.wYear  = (WORD)year;
                    sysTime.wMonth = (WORD)month;
                    sysTime.wDay   = (WORD)day;
                    SystemTimeToFileTime(&sysTime, &date2);
                }
                searchDlg.SetDateLimit(parser.GetLongVal(L"datelimit"), date1, date2);
            }

            if (!parser.HasVal(L"searchpath"))
            {
                auto cmdLineSize = wcslen(lpCmdLine);
                auto cmdLinePath = std::make_unique<wchar_t[]>(cmdLineSize + 1);
                wcscpy_s(cmdLinePath.get(), cmdLineSize + 1, lpCmdLine);
                PathUnquoteSpaces(cmdLinePath.get());
                if (PathFileExists(cmdLinePath.get()))
                {
                    std::wstring spath = cmdLinePath.get();
                    spath = SanitizeSearchPaths(spath);
                    searchDlg.SetSearchPath(spath);
                }
            }

            ret = (int)searchDlg.DoModal(hInstance, IDD_SEARCHDLG, NULL, IDR_SEARCHDLG);
        }
        if (bPortable)
        {
            FILE* pFile = NULL;
            _tfopen_s(&pFile, iniPath.c_str(), _T("wb"));
            g_iniFile.SaveFile(pFile);
            fclose(pFile);
        }
        Gdiplus::GdiplusShutdown(gdiplusToken);
    }

    ::CoUninitialize();
    ::OleUninitialize();
    return ret;
}
