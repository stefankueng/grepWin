#include "StdAfx.h"
#include "DlgResizer.h"

CDlgResizer::CDlgResizer(void)
{
	m_controls.clear();
}

CDlgResizer::~CDlgResizer(void)
{
	m_controls.clear();
}


void CDlgResizer::Init(HWND hWndDlg)
{
	m_hDlg = hWndDlg;
	GetClientRect(hWndDlg, &m_dlgRect);

	m_sizeGrip.cx = GetSystemMetrics(SM_CXVSCROLL);
	m_sizeGrip.cy = GetSystemMetrics(SM_CYHSCROLL);

	RECT rect = { 0 , 0, m_sizeGrip.cx, m_sizeGrip.cy };

	m_wndGrip = ::CreateWindowEx(0, _T("SCROLLBAR"), 
		(LPCTSTR)NULL, 
		WS_CHILD | WS_CLIPSIBLINGS | SBS_SIZEGRIP,
		rect.left, rect.top, 
		rect.right-rect.left,
		rect.bottom-rect.top,
		m_hDlg,
		(HMENU)0,
		NULL,
		NULL);

	if (m_wndGrip)
	{
		// set a triangular window region
		HRGN rgnGrip, rgn;
		rgn = ::CreateRectRgn(0,0,1,1);
		rgnGrip = ::CreateRectRgnIndirect(&rect);

		for (int y=0; y<m_sizeGrip.cy; y++)
		{
			::SetRectRgn(rgn, 0, y, m_sizeGrip.cx-y, y+1);
			::CombineRgn(rgnGrip, rgnGrip, rgn, RGN_DIFF); 
		}
		::SetWindowRgn(m_wndGrip, rgnGrip, FALSE);

		// update pos
		UpdateGripPos();
		ShowSizeGrip();
	}
}

void CDlgResizer::AddControl(HWND hWndDlg, UINT ctrlId, UINT resizeType)
{
	ResizeCtrls ctrlInfo;

	ctrlInfo.hWnd = GetDlgItem(hWndDlg, ctrlId);
	ctrlInfo.resizeType = resizeType;
	
	GetClientRect(ctrlInfo.hWnd, &ctrlInfo.origSize);
	MapWindowPoints(ctrlInfo.hWnd, hWndDlg, (LPPOINT)&ctrlInfo.origSize, 2);

	m_controls.push_back(ctrlInfo);
}

void CDlgResizer::DoResize(int width, int height)
{
	UpdateGripPos();
	if (m_controls.size() == 0)
		return;

	HDWP hdwp = BeginDeferWindowPos(m_controls.size());
	for (size_t i=0; i<m_controls.size(); ++i)
	{
		RECT newpos = m_controls[i].origSize;
		switch (m_controls[i].resizeType)
		{
		case RESIZER_TOPLEFT:
			break;	// do nothing - the original position is fine
		case RESIZER_TOPRIGHT:
			newpos.left += (width - m_dlgRect.right);
			newpos.right += (width - m_dlgRect.right);
			break;
		case RESIZER_TOPLEFTRIGHT:
			newpos.right += (width - m_dlgRect.right);
			break;
		case RESIZER_TOPLEFTBOTTOMRIGHT:
			newpos.right += (width - m_dlgRect.right);
			newpos.bottom += (height - m_dlgRect.bottom);
			break;
		case RESIZER_BOTTOMLEFT:
			newpos.top += (height - m_dlgRect.bottom);
			newpos.bottom += (height - m_dlgRect.bottom);
			break;
		case RESIZER_BOTTOMRIGHT:
			newpos.top += (height - m_dlgRect.bottom);
			newpos.bottom += (height - m_dlgRect.bottom);
			newpos.left += (width - m_dlgRect.right);
			newpos.right += (width - m_dlgRect.right);
			break;
		case RESIZER_BOTTOMLEFTRIGHT:
			newpos.top += (height - m_dlgRect.bottom);
			newpos.bottom += (height - m_dlgRect.bottom);
			newpos.right += (width - m_dlgRect.right);
			break;
		}
		hdwp = DeferWindowPos(hdwp, m_controls[i].hWnd, NULL, newpos.left, newpos.top,
			newpos.right-newpos.left, newpos.bottom-newpos.top,
			SWP_NOZORDER|SWP_SHOWWINDOW);
	}
	EndDeferWindowPos(hdwp);
}

void CDlgResizer::UpdateGripPos()
{
	RECT rect;
	::GetClientRect(m_hDlg, &rect);

	rect.left = rect.right - m_sizeGrip.cx;
	rect.top = rect.bottom - m_sizeGrip.cy;

	// must stay below other children
	::SetWindowPos(m_wndGrip,HWND_BOTTOM, rect.left, rect.top, 0, 0,
		SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOREPOSITION);

	// maximized windows cannot be resized

	if ( ::IsZoomed(m_hDlg) )
	{
		::EnableWindow(m_wndGrip, FALSE);
		ShowSizeGrip(false);
	}
	else
	{
		::EnableWindow(m_wndGrip, TRUE);
		ShowSizeGrip(false);
	}
}

