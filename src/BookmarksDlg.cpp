#include "StdAfx.h"
#include "Resource.h"
#include "BookmarksDlg.h"
#include <string>

#include <boost/regex.hpp>
using namespace boost;
using namespace std;


CBookmarksDlg::CBookmarksDlg(HWND hParent)
{
	m_hParent = hParent;
}

CBookmarksDlg::~CBookmarksDlg(void)
{
}

LRESULT CBookmarksDlg::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (uMsg)
	{
	case WM_INITDIALOG:
		{
			InitDialog(hwndDlg, IDI_GREPWIN);
			// initialize the controls
			InitBookmarks();

			m_resizer.Init(hwndDlg);
		}
		return TRUE;
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
	case WM_CONTEXTMENU:
		{
			long x = GET_X_LPARAM(lParam);
			long y = GET_Y_LPARAM(lParam);
			HWND hListControl = GetDlgItem(*this, IDC_BOOKMARKS);
			if (HWND(wParam) == hListControl)
			{
				int nCount = ListView_GetItemCount(hListControl);
				if (nCount == 0)
					break;
				int iItem = ListView_GetSelectionMark(hListControl);
				if (iItem < 0)
					break;

				POINT pt = {x,y};
				if ((x==-1)&&(y==-1))
				{
					RECT rc;
					ListView_GetItemRect(hListControl, iItem, &rc, LVIR_LABEL);
					pt.x = (rc.right-rc.left)/2;
					pt.y = (rc.bottom-rc.top)/2;
					ClientToScreen(hListControl, &pt);
				}
				HMENU hMenu = LoadMenu(hResource, MAKEINTRESOURCE(IDC_BKPOPUP));
				HMENU hPopup = GetSubMenu(hMenu, 0);
				TrackPopupMenu(hPopup, TPM_LEFTALIGN|TPM_RIGHTBUTTON, x, y, 0, *this, NULL);
			}
		}
		break;
	default:
		return FALSE;
	}
	return FALSE;
}

LRESULT CBookmarksDlg::DoCommand(int id, int /*msg*/)
{
	switch (id)
	{
	case IDOK:
		{
			m_bookmarks.Save();
			int iItem = ListView_GetSelectionMark(GetDlgItem(*this, IDC_BOOKMARKS));
			if (iItem >= 0)
			{
				TCHAR buf[MAX_PATH*4] = {0};
				LVITEM lv = {0};
				lv.mask = LVIF_TEXT;
				lv.pszText = buf;
				lv.cchTextMax = MAX_PATH*4;
				ListView_GetItem(GetDlgItem(*this, IDC_BOOKMARKS), &lv);
				m_searchString = m_bookmarks.GetValue(buf, _T("searchString"), _T(""));
				m_replaceString = m_bookmarks.GetValue(buf, _T("replaceString"), _T(""));
			}
		}
		// fall through
	case IDCANCEL:
		EndDialog(*this, id);
		break;
	case ID_REMOVEBOOKMARK:
		{
			int iItem = ListView_GetSelectionMark(GetDlgItem(*this, IDC_BOOKMARKS));
			if (iItem >= 0)
			{
				TCHAR buf[MAX_PATH*4] = {0};
				LVITEM lv = {0};
				lv.mask = LVIF_TEXT;
				lv.pszText = buf;
				lv.cchTextMax = MAX_PATH*4;
				ListView_GetItem(GetDlgItem(*this, IDC_BOOKMARKS), &lv);
				m_bookmarks.RemoveBookmark(buf);
				ListView_DeleteItem(GetDlgItem(*this, IDC_BOOKMARKS), iItem);
			}
		}
		break;
	}
	return 1;
}

void CBookmarksDlg::InitBookmarks()
{
	HWND hListControl = GetDlgItem(*this, IDC_BOOKMARKS);
	DWORD exStyle = LVS_EX_DOUBLEBUFFER;
	ListView_DeleteAllItems(hListControl);

	int c = Header_GetItemCount(ListView_GetHeader(hListControl))-1;
	while (c>=0)
		ListView_DeleteColumn(hListControl, c--);

	ListView_SetExtendedListViewStyle(hListControl, exStyle);
	LVCOLUMN lvc = {0};
	lvc.mask = LVCF_TEXT;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = -1;
	lvc.pszText = _T("Name");
	ListView_InsertColumn(hListControl, 0, &lvc);
	lvc.pszText = _T("Search string");
	ListView_InsertColumn(hListControl, 1, &lvc);
	lvc.pszText = _T("Replace string");
	ListView_InsertColumn(hListControl, 2, &lvc);


	m_bookmarks.Load();
	CSimpleIni::TNamesDepend sections;
	m_bookmarks.GetAllSections(sections);
	for (CSimpleIni::TNamesDepend::iterator it = sections.begin(); it != sections.end(); ++it)
	{
		wstring searchString = m_bookmarks.GetValue(*it, _T("searchString"), _T(""));
		wstring replaceString = m_bookmarks.GetValue(*it, _T("replaceString"), _T(""));

		LVITEM lv = {0};
		lv.mask = LVIF_TEXT;
		TCHAR * pBuf = new TCHAR[_tcslen(*it)+1];
		_tcscpy_s(pBuf, _tcslen(*it)+1, *it);
		lv.pszText = pBuf;
		lv.iItem = ListView_GetItemCount(hListControl);
		int ret = ListView_InsertItem(hListControl, &lv);
		delete [] pBuf;
		if (ret >= 0)
		{
			lv.iItem = ret;
			lv.iSubItem = 1;
			pBuf = new TCHAR[searchString.size()+1];
			lv.pszText = pBuf;
			_tcscpy_s(lv.pszText, searchString.size()+1, searchString.c_str());
			ListView_SetItem(hListControl, &lv);
			delete [] pBuf;
			lv.iSubItem = 2;
			pBuf = new TCHAR[replaceString.size()+1];
			lv.pszText = pBuf;
			_tcscpy_s(lv.pszText, replaceString.size()+1, replaceString.c_str());
			ListView_SetItem(hListControl, &lv);
			delete [] pBuf;
		}
	}

	ListView_SetColumnWidth(hListControl, 0, LVSCW_AUTOSIZE_USEHEADER);
	ListView_SetColumnWidth(hListControl, 1, LVSCW_AUTOSIZE_USEHEADER);
	ListView_SetColumnWidth(hListControl, 2, LVSCW_AUTOSIZE_USEHEADER);
}
