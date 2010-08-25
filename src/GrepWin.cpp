// grepWin - regex search and replace for Windows

// Copyright (C) 2007-2008, 2010 - Stefan Kueng

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
#include "GrepWin.h"
#include "SearchDlg.h"
#include "AboutDlg.h"
#include "CmdLineParser.h"


// Global Variables:
HINSTANCE hInst;								// current instance

// Forward declarations of functions included in this code module:

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

	bool bQuit = false;
	HWND hWnd = FindWindow(NULL, _T("grepWin"));		// try finding the running instance of this app
	if (hWnd)
	{
		UINT GREPWIN_STARTUPMSG = RegisterWindowMessage(_T("grepWin_StartupMessage"));
		if (SendMessage(hWnd, GREPWIN_STARTUPMSG, 0, 0))				// send the new path
		{
			wstring spath = parser.GetVal(_T("searchpath"));
			COPYDATASTRUCT CopyData = {0};
			CopyData.lpData = (LPVOID)spath.c_str();
			CopyData.cbData = spath.size()*sizeof(wchar_t);
			SendMessage(hWnd, WM_COPYDATA, 0, (LPARAM)&CopyData);
			SetForegroundWindow(hWnd);									//set the window to front
			bQuit = true;
		}
	}

	int ret = 0;
	if (!bQuit)
	{
		if (parser.HasKey(_T("about"))||parser.HasKey(_T("?"))||parser.HasKey(_T("help")))
		{
			CAboutDlg aboutDlg(NULL);
			ret= aboutDlg.DoModal(hInstance, IDD_ABOUT, NULL, NULL);
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
			if (parser.HasKey(_T("execute")))
				searchDlg.SetExecute(true);
			ret = searchDlg.DoModal(hInstance, IDD_SEARCHDLG, NULL, IDR_SEARCHDLG);
		}
	}

	::CoUninitialize();
	::OleUninitialize();
	FreeLibrary(hRichEdt);
	return ret;
}



