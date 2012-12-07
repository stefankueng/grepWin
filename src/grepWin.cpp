// grepWin - regex search and replace for Windows

// Copyright (C) 2007-2008, 2010-2012 - Stefan Kueng

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

// Global Variables:
HINSTANCE hInst;            // current instance


BOOL CALLBACK windowenumerator(__in  HWND hwnd,__in  LPARAM lParam)
{
    HWND * pWnd = (HWND*)lParam;
    WCHAR buf[MAX_PATH] = {0};
    GetWindowText(hwnd, buf, _countof(buf));
    if (_wcsnicmp(buf, L"grepwin :", 9) == 0)
    {
        *pWnd = hwnd;
        return FALSE;
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

    SetDllDirectory(L"");
    ::OleInitialize(NULL);
    ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    // we need some of the common controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LINK_CLASS|ICC_LISTVIEW_CLASSES|ICC_PAGESCROLLER_CLASS
        |ICC_PROGRESS_CLASS|ICC_STANDARD_CLASSES|ICC_TAB_CLASSES|ICC_TREEVIEW_CLASSES
        |ICC_UPDOWN_CLASS|ICC_USEREX_CLASSES|ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icex);

    HMODULE hRichEdt = LoadLibrary(_T("Riched20.dll"));

    CCmdLineParser parser(lpCmdLine);

    // if multiple items are selected in explorer and grepWin is started for all of them,
    // explorer starts multiple grepWin instances at once. In case there's already a grepWin instance
    // running, sleep for a while to give that instance time to fully initialize
    HANDLE hReloadProtection = ::CreateMutex(NULL, FALSE, L"{6473AA76-0EAE-4C96-8C99-AFDFEFFE42B5}");
    bool alreadyRunning = false;
    if ((!hReloadProtection) || (GetLastError() == ERROR_ALREADY_EXISTS))
    {
        // An instance of grepWin is already running
        alreadyRunning = true;
    }

    bool bQuit = false;
    HWND hWnd = NULL;
    int timeout = 20;
    do 
    {
        EnumWindows(windowenumerator, (LPARAM)&hWnd);
        if (alreadyRunning && (hWnd == NULL))
            Sleep(100);
        timeout--;
    } while ((hWnd == NULL) && alreadyRunning && timeout);

    if (hWnd)
    {
        UINT GREPWIN_STARTUPMSG = RegisterWindowMessage(_T("grepWin_StartupMessage"));
        if (SendMessage(hWnd, GREPWIN_STARTUPMSG, 0, 0))                // send the new path
        {
            std::wstring spath = parser.GetVal(_T("searchpath"));
            COPYDATASTRUCT CopyData = {0};
            CopyData.lpData = (LPVOID)spath.c_str();
            CopyData.cbData = (DWORD)spath.size()*sizeof(wchar_t);
            SendMessage(hWnd, WM_COPYDATA, 0, (LPARAM)&CopyData);
            SetForegroundWindow(hWnd);                                  //set the window to front
            bQuit = true;
        }
        else if (DWORD(CRegStdDWORD(_T("Software\\grepWin\\onlyone"), 0)))
        {
            std::wstring spath = parser.GetVal(_T("searchpath"));
            COPYDATASTRUCT CopyData = {0};
            CopyData.lpData = (LPVOID)spath.c_str();
            CopyData.cbData = (DWORD)spath.size()*sizeof(wchar_t);
            SendMessage(hWnd, WM_COPYDATA, 1, (LPARAM)&CopyData);
            SetForegroundWindow(hWnd);                                  //set the window to front
            bQuit = true;
        }
    }

    int ret = 0;
    if (!bQuit)
    {
        if (parser.HasKey(_T("about"))||parser.HasKey(_T("?"))||parser.HasKey(_T("help")))
        {
            CAboutDlg aboutDlg(NULL);
            ret= (int)aboutDlg.DoModal(hInstance, IDD_ABOUT, NULL, NULL);
        }
        else
        {
            CSearchDlg searchDlg(NULL);
            if (parser.HasVal(_T("searchpath")))
                searchDlg.SetSearchPath(parser.GetVal(_T("searchpath")));
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
                searchDlg.SetCaseSensitive(_tcsicmp(parser.GetVal(_T("i")), _T("yes"))!=0);
            if (parser.HasVal(_T("n")))
                searchDlg.SetMatchesNewline(_tcsicmp(parser.GetVal(_T("n")), _T("yes"))==0);
            if (parser.HasVal(_T("k")))
                searchDlg.SetCreateBackups(_tcsicmp(parser.GetVal(_T("k")), _T("yes"))==0);
            if (parser.HasVal(_T("utf8")))
                searchDlg.SetUTF8(_tcsicmp(parser.GetVal(_T("utf8")), _T("yes"))==0);
            if (parser.HasVal(_T("size")))
            {
                int cmp = 0;
                if (parser.HasVal(_T("sizecmp")))
                    cmp = parser.GetLongVal(_T("sizecmp"));
                searchDlg.SetSize(parser.GetLongVal(_T("size")), cmp);
            }
            if (parser.HasVal(_T("s")))
                searchDlg.SetIncludeSystem(_tcsicmp(parser.GetVal(_T("s")), _T("yes"))==0);
            if (parser.HasVal(_T("h")))
                searchDlg.SetIncludeHidden(_tcsicmp(parser.GetVal(_T("h")), _T("yes"))==0);
            if (parser.HasVal(_T("u")))
                searchDlg.SetIncludeSubfolders(_tcsicmp(parser.GetVal(_T("u")), _T("yes"))==0);
            if (parser.HasVal(_T("b")))
                searchDlg.SetIncludeBinary(_tcsicmp(parser.GetVal(_T("b")), _T("yes"))==0);

            if (parser.HasKey(_T("execute")))
                searchDlg.SetExecute(true);
            ret = (int)searchDlg.DoModal(hInstance, IDD_SEARCHDLG, NULL, IDR_SEARCHDLG);
        }
    }

    ::CoUninitialize();
    ::OleUninitialize();
    FreeLibrary(hRichEdt);
    CloseHandle(hReloadProtection);
    return ret;
}
