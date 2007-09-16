#include "stdafx.h"
#include "BaseDialog.h"


INT_PTR CDialog::DoModal(HINSTANCE hInstance, int resID, HWND hWndParent)
{
	m_bPseudoModal = false;
	hResource = hInstance;
	return DialogBoxParam(hInstance, MAKEINTRESOURCE(resID), hWndParent, &CDialog::stDlgFunc, (LPARAM)this);
}

INT_PTR CDialog::DoModal(HINSTANCE hInstance, int resID, HWND hWndParent, UINT idAccel)
{
	m_bPseudoModal = true;
	hResource = hInstance;
	m_hwnd = CreateDialogParam(hInstance, MAKEINTRESOURCE(resID), hWndParent, &CDialog::stDlgFunc, (LPARAM)this);

	// deactivate the parent window
	if (hWndParent)
		::EnableWindow(hWndParent, FALSE);

	ShowWindow(m_hwnd, SW_SHOW);

	// Main message loop:
	MSG msg;
	HACCEL hAccelTable = LoadAccelerators(hResource, MAKEINTRESOURCE(idAccel));
	BOOL bRet = TRUE;
	while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0)
	{ 
		if (bRet == -1)
		{
			// handle the error and possibly exit
			break;
		}
		else
		{
			if (!TranslateAccelerator(m_hwnd, hAccelTable, &msg) && 
				!IsDialogMessage(m_hwnd, &msg)) 
			{ 
				TranslateMessage(&msg); 
				DispatchMessage(&msg); 
			}
		}
	} 
	// re-enable the parent window
	if (hWndParent)
		::EnableWindow(hWndParent, TRUE);
	DestroyWindow(m_hwnd);
	return msg.wParam;
}

BOOL CDialog::EndDialog(HWND hDlg, INT_PTR nResult)
{
	if (m_bPseudoModal)
		::PostQuitMessage(nResult);
	return ::EndDialog(hDlg, nResult);
}

HWND CDialog::Create(HINSTANCE hInstance, int resID, HWND hWndParent)
{
	m_bPseudoModal = true;
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

void CDialog::AddToolTip(UINT ctrlID, LPTSTR text)
{
	TOOLINFO tt;
	tt.cbSize = sizeof(TOOLINFO);
	tt.uFlags = TTF_IDISHWND|TTF_CENTERTIP|TTF_SUBCLASS;
	tt.hwnd = GetDlgItem(*this, ctrlID);
	tt.uId = (UINT)GetDlgItem(*this, ctrlID);
	tt.lpszText = text;

	SendMessage (m_hToolTips, TTM_ADDTOOL, 0, (LPARAM) &tt);
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
		// create the tooltip control
		pWnd->m_hToolTips = CreateWindowEx(NULL,
			TOOLTIPS_CLASS, NULL,
			WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
			CW_USEDEFAULT, CW_USEDEFAULT,
			CW_USEDEFAULT, CW_USEDEFAULT,
			hwndDlg,
			NULL, pWnd->hResource,
			NULL);

		SetWindowPos(pWnd->m_hToolTips, HWND_TOPMOST,0, 0, 0, 0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
		SendMessage(pWnd->m_hToolTips, TTM_SETMAXTIPWIDTH, 0, 600);  
		SendMessage(pWnd->m_hToolTips, TTM_ACTIVATE, TRUE, 0);
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
