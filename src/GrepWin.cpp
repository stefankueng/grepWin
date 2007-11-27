// GrepWin.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "GrepWin.h"
#include "SearchDlg.h"
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

	::OleInitialize(NULL);

	// we need some of the common controls
	INITCOMMONCONTROLSEX icex;
	icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
	icex.dwICC = ICC_LINK_CLASS|ICC_LISTVIEW_CLASSES|ICC_PAGESCROLLER_CLASS
		|ICC_PROGRESS_CLASS|ICC_STANDARD_CLASSES|ICC_TAB_CLASSES|ICC_TREEVIEW_CLASSES
		|ICC_UPDOWN_CLASS|ICC_USEREX_CLASSES|ICC_WIN95_CLASSES;
	InitCommonControlsEx(&icex);

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
		CSearchDlg searchDlg(NULL);
		if (parser.HasVal(_T("searchpath")))
			searchDlg.SetSearchPath(parser.GetVal(_T("searchpath")));
		ret = searchDlg.DoModal(hInstance, IDD_SEARCHDLG, NULL, IDC_GREPWIN);
	}

	::OleUninitialize();
	return ret;
}



