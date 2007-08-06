#include "stdafx.h"
#include "BaseDialog.h"


INT_PTR CDialog::DoModal(HINSTANCE hInstance, int resID, HWND hWndParent)
{
	hResource = hInstance;
	return DialogBoxParam(hInstance, MAKEINTRESOURCE(resID), hWndParent, &CDialog::stDlgFunc, (LPARAM)this);
}

HWND CDialog::Create(HINSTANCE hInstance, int resID, HWND hWndParent)
{
	hResource = hInstance;
    m_hwnd = CreateDialogParam(hInstance, MAKEINTRESOURCE(resID), hWndParent, &CDialog::stDlgFunc, (LPARAM)this);
    return m_hwnd;
}

void CDialog::InitDialog(HWND hwndDlg, UINT iconID)
{
	HWND hwndOwner; 
	RECT rc, rcDlg, rcOwner;

	hwndOwner = ::GetParent(hwndDlg);
	if (hwndOwner == NULL)
		hwndOwner = ::GetDesktopWindow();

	GetWindowRect(hwndOwner, &rcOwner); 
	GetWindowRect(hwndDlg, &rcDlg); 
	CopyRect(&rc, &rcOwner); 

	OffsetRect(&rcDlg, -rcDlg.left, -rcDlg.top); 
	OffsetRect(&rc, -rc.left, -rc.top); 
	OffsetRect(&rc, -rcDlg.right, -rcDlg.bottom); 

	SetWindowPos(hwndDlg, HWND_TOP, rcOwner.left + (rc.right / 2), rcOwner.top + (rc.bottom / 2), 0, 0,	SWP_NOSIZE); 
	HICON hIcon = (HICON)::LoadImage(hResource, MAKEINTRESOURCE(iconID), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE|LR_SHARED);
	::SendMessage(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
	::SendMessage(hwndDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
}

INT_PTR CALLBACK CDialog::stDlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CDialog* pWnd;
	if (uMsg == WM_INITDIALOG)
	{
		// get the pointer to the window from lpCreateParams
		SetWindowLongPtr(hwndDlg, GWLP_USERDATA, lParam);
		pWnd = (CDialog*)lParam;
		pWnd->m_hwnd = hwndDlg;
	}
	// get the pointer to the window
	pWnd = GetObjectFromWindow(hwndDlg);

	// if we have the pointer, go to the message handler of the window
	// else, use DefWindowProc
	if (pWnd)
	{
		LRESULT lRes = pWnd->DlgFunc(hwndDlg, uMsg, wParam, lParam);
		SetWindowLongPtr(hwndDlg, DWL_MSGRESULT, lRes);
		return lRes;
	}
	else
		return DefWindowProc(hwndDlg, uMsg, wParam, lParam);
}
