// CommitMonitor - simple checker for new commits in svn repositories

// Copyright (C) 2009 - Stefan Kueng

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
#include "AeroGlass.h"
#include "registry.h"
#include <map>
#include <gdiplus.h>

class AeroControlBase
{
public:
    AeroControlBase();
    virtual ~AeroControlBase();

    bool SubclassControl(HWND hControl);

private:
    static LRESULT CALLBACK SubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uidSubclass, DWORD_PTR dwRefData);
    LRESULT StaticWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT ButtonWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT ProgressbarWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    void DrawFocusRect(LPRECT prcFocus, HDC hdcPaint);
    void DrawRect(LPRECT prc, HDC hdcPaint, Gdiplus::DashStyle dashStyle, Gdiplus::Color clr, Gdiplus::REAL width);
    void FillRect(LPRECT prc, HDC hdcPaint, Gdiplus::Color clr);
    int GetStateFromBtnState(LONG_PTR dwStyle, BOOL bHot, BOOL bFocus, LRESULT dwCheckState, int iPartId, BOOL bHasMouseCapture);
    void PaintControl(HWND hWnd, HDC hdc, RECT* prc, bool bDrawBorder);
    void ScreenToClient(HWND hWnd, LPRECT lprc);
    void DrawSolidWndRectOnParent(HWND hWnd,Gdiplus:: Color clr);
    void DrawEditBorder(HWND hWnd);
    BOOL GetEditBorderColor(HWND hWnd, COLORREF *pClr);
    void GetRoundRectPath(Gdiplus::GraphicsPath *pPath, Gdiplus::Rect r, int dia);

    CDwmApiImpl                 m_dwm;
    CUxThemeAeroImpl            m_theme;
    std::map<HWND, UINT_PTR>    subclassedControls;
    ULONG_PTR                   gdiplusToken;
};

