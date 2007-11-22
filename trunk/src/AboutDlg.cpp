#include "StdAfx.h"
#include "Resource.h"
#include "AboutDlg.h"
#include "version.h"
#include <string>


CAboutDlg::CAboutDlg(HWND hParent)
{
	m_hParent = hParent;
}

CAboutDlg::~CAboutDlg(void)
{
}

LRESULT CAboutDlg::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (uMsg)
	{
	case WM_INITDIALOG:
		{
			InitDialog(hwndDlg, IDI_GREPWIN);
			TCHAR buf[MAX_PATH] = {0};
			_stprintf_s(buf, MAX_PATH, _T("grepWin version %ld.%ld.%ld.%ld"), CM_VERMAJOR, CM_VERMINOR, CM_VERMICRO, CM_VERBUILD);
			SetDlgItemText(*this, IDC_VERSIONINFO, buf);
			SetDlgItemText(*this, IDC_DATE, _T(CM_VERDATE));
			m_link.ConvertStaticToHyperlink(hwndDlg, IDC_WEBLINK, _T("http://tools.tortoisesvn.net"));
		}
		return TRUE;
	case WM_COMMAND:
		return DoCommand(LOWORD(wParam), HIWORD(wParam));
	default:
		return FALSE;
	}
	return FALSE;
}

LRESULT CAboutDlg::DoCommand(int id, int /*msg*/)
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

