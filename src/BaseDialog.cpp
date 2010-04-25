// grepWin - regex search and replace for Windows

// Copyright (C) 2007, 2009-2010 - Stefan Kueng

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
#include "commctrl.h"

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
                if (!TranslateAccelerator(m_hwnd, hAccelTable, &msg) &&
                    !IsDialogMessage(m_hwnd, &msg)
                    )
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

    SetWindowPos(hwndDlg, HWND_TOP, rcOwner.left + (rc.right / 2), rcOwner.top + (rc.bottom / 2), 0, 0, SWP_NOSIZE|SWP_SHOWWINDOW);
    HICON hIcon = (HICON)::LoadImage(hResource, MAKEINTRESOURCE(iconID), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE|LR_SHARED);
    ::SendMessage(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
    ::SendMessage(hwndDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    m_Dwm.Initialize();
    m_margins.cxLeftWidth = 0;
    m_margins.cxRightWidth = 0;
    m_margins.cyBottomHeight = 0;
    m_margins.cyTopHeight = 0;
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
    static bool bInDlgProc = false;
    if (bInDlgProc)
        return FALSE;

    CDialog* pWnd;
    switch (uMsg)
    {
    case WM_INITDIALOG:
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
        break;
    }
    // get the pointer to the window
    pWnd = GetObjectFromWindow(hwndDlg);

    // if we have the pointer, go to the message handler of the window
    if (pWnd)
    {
        LRESULT lRes = pWnd->DlgFunc(hwndDlg, uMsg, wParam, lParam);
        switch (uMsg)
        {
        case WM_DWMCOMPOSITIONCHANGED:
            pWnd->OnCompositionChanged();
            break;
        case WM_ERASEBKGND:
            {
                if (pWnd->m_Dwm.IsDwmCompositionEnabled())
                {
                    bInDlgProc = true;
                    DefDlgProc(hwndDlg, uMsg, wParam, lParam);
                    bInDlgProc = false;
                    HDC hdc = (HDC)wParam;
                    // draw the frame margins in black
                    RECT rc;
                    GetClientRect(hwndDlg, &rc);
                    if (pWnd->m_margins.cxLeftWidth < 0)
                    {
                        SetBkColor(hdc, RGB(0,0,0));
                        ::ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &rc, NULL, 0, NULL);
                    }
                    else
                    {
                        SetBkColor(hdc, RGB(0,0,0));
                        RECT rect;
                        rect.left = rc.left;
                        rect.top = rc.top;
                        rect.right =  rc.left + pWnd->m_margins.cxLeftWidth;
                        rect.bottom = rc.bottom;
                        ::ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &rect, NULL, 0, NULL);

                        rect.left = rc.left;
                        rect.top = rc.top;
                        rect.right =  rc.right;
                        rect.bottom = rc.top + pWnd->m_margins.cyTopHeight;
                        ::ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &rect, NULL, 0, NULL);

                        rect.left = rc.right - pWnd->m_margins.cxRightWidth;
                        rect.top = rc.top;
                        rect.right =  rc.right;
                        rect.bottom = rc.bottom;
                        ::ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &rect, NULL, 0, NULL);

                        rect.left = rc.left;
                        rect.top = rc.bottom - pWnd->m_margins.cyBottomHeight;
                        rect.right =  rc.right;
                        rect.bottom = rc.bottom;
                        ::ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &rect, NULL, 0, NULL);
                    }
                    lRes = TRUE;
                }
            }
            break;
        case WM_NCHITTEST:
            {
                if (pWnd->m_Dwm.IsDwmCompositionEnabled())
                {
                    POINTS pts = MAKEPOINTS(lParam);
                    POINT pt;
                    pt.x = pts.x;
                    pt.y = pts.y;
                    RECT rc;
                    GetClientRect(hwndDlg, &rc);
                    MapWindowPoints(hwndDlg, NULL, (LPPOINT)&rc, 2);

                    if (pWnd->m_margins.cxLeftWidth < 0)
                    {

                        lRes = PtInRect(&rc, pt) ? HTCAPTION : FALSE;
                    }
                    else
                    {
                        RECT m = rc;
                        m.left += pWnd->m_margins.cxLeftWidth;
                        m.top += pWnd->m_margins.cyTopHeight;
                        m.right -= pWnd->m_margins.cxRightWidth;
                        m.bottom -= pWnd->m_margins.cyBottomHeight;
                        lRes = (PtInRect(&rc, pt) && !PtInRect(&m, pt)) ? HTCAPTION : FALSE;
                    }
                }
            }
            break;
        default:
            break;
        }
        SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, lRes);
        return lRes;
    }
    else
        return 0;
}

bool CDialog::PreTranslateMessage(MSG* /*pMsg*/)
{
    return false;
}

bool CDialog::IsCursorOverWindowBorder()
{
    RECT wrc, crc;
    GetWindowRect(*this, &wrc);
    GetClientRect(*this, &crc);
    MapWindowPoints(*this, NULL, (LPPOINT)&crc, 2);
    DWORD pos = GetMessagePos();
    POINT pt;
    pt.x = GET_X_LPARAM(pos);
    pt.y = GET_Y_LPARAM(pos);
    return (PtInRect(&wrc, pt) && !PtInRect(&crc, pt));
}

/**
 * Wrapper around the CWnd::EnableWindow() method, but
 * makes sure that a control that has the focus is not disabled
 * before the focus is passed on to the next control.
 */
bool CDialog::DialogEnableWindow(UINT nID, bool bEnable)
{
    HWND hwndDlgItem = GetDlgItem(*this, nID);
    if (hwndDlgItem == NULL)
        return false;
    if (bEnable)
        return !!EnableWindow(hwndDlgItem, bEnable);
    if (GetFocus() == hwndDlgItem)
    {
        SendMessage(*this, WM_NEXTDLGCTL, 0, false);
    }
    return !!EnableWindow(hwndDlgItem, bEnable);
}

void CDialog::OnCompositionChanged()
{
    if (m_Dwm.IsDwmCompositionEnabled())
    {
        m_Dwm.DwmExtendFrameIntoClientArea(*this, &m_margins);
    }
}

void CDialog::ExtendFrameIntoClientArea(UINT leftControl, UINT topControl, UINT rightControl, UINT botomControl)
{
    RECT rc, rc2;
    GetWindowRect(*this, &rc);
    GetClientRect(*this, &rc2);
    MapWindowPoints(*this, NULL, (LPPOINT)&rc2, 2);

    RECT rccontrol;
    if (leftControl && leftControl != -1)
    {
        HWND hw = GetDlgItem(*this, leftControl);
        if (hw == NULL)
            return;
        ::GetWindowRect(hw, &rccontrol);
        m_margins.cxLeftWidth = rccontrol.left - rc.left;
        m_margins.cxLeftWidth -= (rc2.left-rc.left);
    }
    else
        m_margins.cxLeftWidth = 0;

    if (topControl && topControl != -1)
    {
        HWND hw = GetDlgItem(*this, topControl);
        if (hw == NULL)
            return;
        ::GetWindowRect(hw, &rccontrol);
        m_margins.cyTopHeight = rccontrol.top - rc.top;
        m_margins.cyTopHeight -= (rc2.top-rc.top);
    }
    else
        m_margins.cyTopHeight = 0;

    if (rightControl && rightControl != -1)
    {
        HWND hw = GetDlgItem(*this, rightControl);
        if (hw == NULL)
            return;
        ::GetWindowRect(hw, &rccontrol);
        m_margins.cxRightWidth = rc.right - rccontrol.right;
        m_margins.cxRightWidth -= (rc.right-rc2.right);
    }
    else
        m_margins.cxRightWidth = 0;

    if (botomControl && botomControl != -1)
    {
        HWND hw = GetDlgItem(*this, botomControl);
        if (hw == NULL)
            return;
        ::GetWindowRect(hw, &rccontrol);
        m_margins.cyBottomHeight = rc.bottom - rccontrol.bottom;
        m_margins.cyBottomHeight -= (rc.bottom-rc2.bottom);
    }
    else
        m_margins.cyBottomHeight = 0;

    if ((m_margins.cxLeftWidth == 0)&&
        (m_margins.cyTopHeight == 0)&&
        (m_margins.cxRightWidth == 0)&&
        (m_margins.cyBottomHeight == 0))
    {
        m_margins.cxLeftWidth = -1;
        m_margins.cyTopHeight = -1;
        m_margins.cxRightWidth = -1;
        m_margins.cyBottomHeight = -1;
    }
    if (m_Dwm.IsDwmCompositionEnabled())
    {
        m_Dwm.DwmExtendFrameIntoClientArea(*this, &m_margins);
    }
}
