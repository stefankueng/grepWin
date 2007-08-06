#include "StdAfx.h"
#include "Resource.h"
#include "SearchDlg.h"
#include "Registry.h"
#include <string>
#include <Commdlg.h>

using namespace std;

CSearchDlg::CSearchDlg(HWND hParent)
{
	m_hParent = hParent;
}

CSearchDlg::~CSearchDlg(void)
{
}

LRESULT CSearchDlg::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (uMsg)
	{
	case WM_INITDIALOG:
		{
			InitDialog(hwndDlg, IDI_GREPWIN);
			// initialize the controls
		}
		return TRUE;
	case WM_COMMAND:
		return DoCommand(LOWORD(wParam));
	default:
		return FALSE;
	}
}

LRESULT CSearchDlg::DoCommand(int id)
{
	switch (id)
	{
	case IDOK:
		// fall through
	case IDCANCEL:
		EndDialog(*this, id);
		break;
	}
	return 1;
}

