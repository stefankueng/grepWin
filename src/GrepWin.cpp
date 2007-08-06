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

	HACCEL hAccelTable;

	CCmdLineParser parser(lpCmdLine);

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_GREPWIN));

	CSearchDlg searchDlg(NULL);

	int ret = searchDlg.DoModal(hInstance, IDD_SEARCHDLG, NULL);

	::OleUninitialize();
	return ret;
}



