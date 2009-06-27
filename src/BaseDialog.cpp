// grepWin - regex search and replace for Windows

// Copyright (C) 2007-2009 - Stefan Kueng

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
	m_bPseudoEnded = false;
	hResource = hInstance;
	m_hwnd = CreateDialogParam(hInstance, MAKEINTRESOURCE(resID), hWndParent, &CDialog::stDlgFunc, (LPARAM)this);

	// deactivate the parent window
	if (hWndParent)
		::EnableWindow(hWndParent, FALSE);

	ShowWindow(m_hwnd, SW_SHOW);

	// Main message loop:
	MSG msg = {0};
	HACCEL hAccelTable = LoadAccelerators(hResource, MAKEINTRESOURCE(idAccel));
	BOOL bRet = TRUE;
	while (!m_bPseudoEnded && ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0))
	{ 
		if (bRet == -1)
		{
			// handle the error and possibly exit
			break;
		}
		else
		{
			if (!PreTranslateMessage(&msg))
			{
				if (!IsDialogMessage(m_hwnd, &msg) && 
					!TranslateAccelerator(m_hwnd, hAccelTable, &msg)) 
				{ 
					TranslateMessage(&msg); 
					DispatchMessage(&msg); 
				}
			}
		}
	} 
	if (msg.message == WM_QUIT)
		PostQuitMessage((int)msg.wParam);
	// re-enable the parent window
	if (hWndParent)
		::EnableWindow(hWndParent, TRUE);
	DestroyWindow(m_hwnd);
	if (m_bPseudoModal)
		return m_iPseudoRet;
	return msg.wParam;
}

BOOL CDialog::EndDialog(HWND hDlg, INT_PTR nResult)
{
	if (m_bPseudoModal)
	{
		m_bPseudoEnded = true;
		m_iPseudoRet = nResult;
	}
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
	WINDOWPLACEMENT placement;
	placement.length = sizeof(WINDOWPLACEMENT);

	hwndOwner = ::GetParent(hwndDlg);
	GetWindowPlacement(hwndOwner, &placement);
	if ((hwndOwner == NULL)||(placement.showCmd == SW_SHOWMINIMIZED)||(placement.showCmd == SW_SHOWMINNOACTIVE))
		hwndOwner = ::GetDesktopWindow();

	GetWindowRect(hwndOwner, &rcOwner); 
	GetWindowRect(hwndDlg, &rcDlg); 
	CopyRect(&rc, &rcOwner); 

	OffsetRect(&rcDlg, -rcDlg.left, -rcDlg.top); 
	OffsetRect(&rc, -rc.left, -rc.top); 
	OffsetRect(&rc, -rcDlg.right, -rcDlg.bottom); 

	SetWindowPos(hwndDlg, HWND_TOP, rcOwner.left + (rc.right / 2), rcOwner.top + (rc.bottom / 2), 0, 0,	SWP_NOSIZE|SWP_SHOWWINDOW); 
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

bool CDialog::PreTranslateMessage(MSG* /*pMsg*/)
{
	return false;
}