// grepWin - regex search and replace for Windows

// Copyright (C) 2007, 2009 - Stefan Kueng

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
#pragma once
#include <string>
#include "AeroGlass.h"

/**
 * A base window class.
 * Provides separate window message handlers for every window object based on
 * this class.
 */
class CDialog
{
public:
    INT_PTR DoModal(HINSTANCE hInstance, int resID, HWND hWndParent);
    INT_PTR DoModal(HINSTANCE hInstance, int resID, HWND hWndParent, UINT idAccel);
    HWND    Create(HINSTANCE hInstance, int resID, HWND hWndParent);
    BOOL    EndDialog(HWND hDlg, INT_PTR nResult);
    void    AddToolTip(UINT ctrlID, LPTSTR text);
    bool    IsCursorOverWindowBorder();
    /**
     * Wrapper around the CWnd::EnableWindow() method, but
     * makes sure that a control that has the focus is not disabled
     * before the focus is passed on to the next control.
     */
    bool    DialogEnableWindow(UINT nID, bool bEnable);
    void    OnCompositionChanged();
    void    ExtendFrameIntoClientArea(UINT leftControl, UINT topControl, UINT rightControl, UINT botomControl);

    virtual LRESULT CALLBACK DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) = 0;
    virtual bool PreTranslateMessage(MSG* pMsg);

    operator HWND() {return m_hwnd;}
protected:
    HINSTANCE       hResource;
    HWND            m_hwnd;
    CDwmApiImpl     m_Dwm;
    MARGINS         m_margins;

    void InitDialog(HWND hwndDlg, UINT iconID);

    // the real message handler
    static INT_PTR CALLBACK stDlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

    // returns a pointer the dialog (stored as the WindowLong)
    inline static CDialog * GetObjectFromWindow(HWND hWnd)
    {
        return (CDialog *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    }
private:
    bool        m_bPseudoModal;
    bool        m_bPseudoEnded;
    INT_PTR     m_iPseudoRet;
    HWND        m_hToolTips;
};

