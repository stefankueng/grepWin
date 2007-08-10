#include "StdAfx.h"
#include "Resource.h"
#include "NameDlg.h"
#include <string>

#include <boost/regex.hpp>
using namespace boost;
using namespace std;


CNameDlg::CNameDlg(HWND hParent)
{
	m_hParent = hParent;
}

CNameDlg::~CNameDlg(void)
{
}

LRESULT CNameDlg::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (uMsg)
	{
	case WM_INITDIALOG:
		{
			InitDialog(hwndDlg, IDI_GREPWIN);
			// initialize the controls

			SetFocus(GetDlgItem(hwndDlg, IDC_NAME));

			m_resizer.Init(hwndDlg);

		}
		return FALSE;
	case WM_COMMAND:
		return DoCommand(LOWORD(wParam), HIWORD(wParam));
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
	default:
		return FALSE;
	}
	return FALSE;
}

LRESULT CNameDlg::DoCommand(int id, int msg)
{
	switch (id)
	{
	case IDOK:
		{
			TCHAR buf[MAX_PATH] = {0};
			GetDlgItemText(*this, IDC_NAME, buf, MAX_PATH);
			m_name = buf;
		}
		// fall through
	case IDCANCEL:
		EndDialog(*this, id);
		break;
	}
	return 1;
}

