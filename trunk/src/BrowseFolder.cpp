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
#include "StdAfx.h"
#include <windowsx.h>
#include "BrowseFolder.h"
#include "maxpath.h"

BOOL CBrowseFolder::m_bCheck = FALSE;
BOOL CBrowseFolder::m_bCheck2 = FALSE;
WNDPROC CBrowseFolder::CBProc = NULL;
HWND CBrowseFolder::checkbox = NULL;
HWND CBrowseFolder::checkbox2 = NULL;
HWND CBrowseFolder::ListView = NULL;
TCHAR CBrowseFolder::m_CheckText[200];		
TCHAR CBrowseFolder::m_CheckText2[200];		
std::wstring CBrowseFolder::m_sDefaultPath;
bool CBrowseFolder::m_DisableCheckbox2WhenCheckbox1IsChecked = false;


CBrowseFolder::CBrowseFolder(void)
:	m_style(0),
	m_root(NULL)
{
	memset(m_displayName, 0, sizeof(m_displayName));
	memset(m_title, 0, sizeof(m_title));
	memset(m_CheckText, 0, sizeof(m_CheckText));
}

CBrowseFolder::~CBrowseFolder(void)
{
}

//show the dialog
CBrowseFolder::retVal CBrowseFolder::Show(HWND parent, LPTSTR path, size_t pathlen, LPCTSTR szDefaultPath /* = NULL */)
{
	std::wstring temp;
	temp = path;
	std::wstring sDefault;
	if (szDefaultPath)
		sDefault = szDefaultPath;
	CBrowseFolder::retVal ret = Show(parent, temp, sDefault);
	_tcscpy_s(path, pathlen, temp.c_str());
	return ret;
}
CBrowseFolder::retVal CBrowseFolder::Show(HWND parent, std::wstring& path, const std::wstring& sDefaultPath /* = std::wstring() */)
{
	retVal ret = OK;		//assume OK
	m_sDefaultPath = sDefaultPath;
	LPITEMIDLIST itemIDList;

	BROWSEINFO browseInfo;

	browseInfo.hwndOwner		= parent;
	browseInfo.pidlRoot			= m_root;
	browseInfo.pszDisplayName	= m_displayName;
	browseInfo.lpszTitle		= m_title;
	browseInfo.ulFlags			= m_style;
	browseInfo.lpfn				= NULL;
	browseInfo.lParam			= (LPARAM)this;
	
	if ((_tcslen(m_CheckText) > 0)||(!m_sDefaultPath.empty()))
	{
		browseInfo.lpfn = BrowseCallBackProc;
	}
	
	itemIDList = SHBrowseForFolder(&browseInfo);

	//is the dialog cancelled?
	if (!itemIDList)
		ret = CANCEL;

	if (ret != CANCEL) 
	{
		TCHAR buf[MAX_PATH_NEW] = {0};
		if (!SHGetPathFromIDList(itemIDList, buf))
			ret = NOPATH;

		path = buf;
	
		LPMALLOC	shellMalloc;
		HRESULT		hr;

		hr = SHGetMalloc(&shellMalloc);

		if (SUCCEEDED(hr)) 
		{
			//free memory
			shellMalloc->Free(itemIDList);
			//release interface
			shellMalloc->Release();
		}
	}
	return ret;
}

void CBrowseFolder::SetInfo(LPCTSTR title)
{
	if (title)
		_tcscpy_s(m_title, 200, title);
}

void CBrowseFolder::SetCheckBoxText(LPCTSTR checktext)
{
	if (checktext)
		_tcscpy_s(m_CheckText, 200, checktext);
}

void CBrowseFolder::SetCheckBoxText2(LPCTSTR checktext)
{
	if (checktext)
		_tcscpy_s(m_CheckText2, 200, checktext);
}

void CBrowseFolder::SetFont(HWND hwnd,LPTSTR FontName,int FontSize)
{

	HFONT hf;
	LOGFONT lf={0};
	HDC hdc=GetDC(hwnd);

	GetObject(GetWindowFont(hwnd),sizeof(lf),&lf);
	lf.lfWeight = FW_REGULAR;
	lf.lfHeight = (LONG)FontSize;
	lstrcpy( lf.lfFaceName, FontName );
	hf=CreateFontIndirect(&lf);
	SetBkMode(hdc,OPAQUE);
	SendMessage(hwnd,WM_SETFONT,(WPARAM)hf,TRUE);
	ReleaseDC(hwnd,hdc);

}

int CBrowseFolder::BrowseCallBackProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM /*lpData*/)
{
	RECT ListViewRect,Dialog;
	//Initialization callback message
	if (uMsg == BFFM_INITIALIZED)
	{
		if (_tcslen(m_CheckText) > 0)
		{
			bool bSecondCheckbox = (_tcslen(m_CheckText2)!=0);
			//Rectangles for getting the positions
			checkbox = CreateWindowEx(	0,
				_T("BUTTON"),
				m_CheckText,
				WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN|BS_AUTOCHECKBOX,
				0,100,100,50,
				hwnd,
				0,
				NULL,
				NULL);
			if (checkbox == NULL)
				return 0;

			if (bSecondCheckbox)
			{
				//Rectangles for getting the positions
				checkbox2 = CreateWindowEx(	0,
					_T("BUTTON"),
					m_CheckText2,
					WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN|BS_AUTOCHECKBOX,
					0,100,100,50,
					hwnd,
					0,
					NULL,
					NULL);
				if (checkbox2 == NULL)
					return 0;
			}

			ListView = FindWindowEx(hwnd,NULL,_T("SysTreeView32"),NULL);
			if (ListView == NULL)
				ListView = FindWindowEx(hwnd,NULL,_T("SHBrowseForFolder ShellNameSpace Control"),NULL);

			if (ListView == NULL)
				return 0;

			//Gets the dimensions of the windows
			GetWindowRect(hwnd,&Dialog);
			GetWindowRect(ListView,&ListViewRect);
			POINT pt;
			pt.x = ListViewRect.left;
			pt.y = ListViewRect.top;
			ScreenToClient(hwnd, &pt);
			ListViewRect.top = pt.y;
			ListViewRect.left = pt.x;
			pt.x = ListViewRect.right;
			pt.y = ListViewRect.bottom;
			ScreenToClient(hwnd, &pt);
			ListViewRect.bottom = pt.y;
			ListViewRect.right = pt.x;
			//Sets the listview controls dimensions
			SetWindowPos(ListView,0,ListViewRect.left,
				bSecondCheckbox ? ListViewRect.top+40 : ListViewRect.top+20,
				(ListViewRect.right-ListViewRect.left),
				bSecondCheckbox ? (ListViewRect.bottom - ListViewRect.top)-40 : (ListViewRect.bottom - ListViewRect.top)-20,
				SWP_NOZORDER);
			//Sets the window positions of checkbox and dialog controls
			SetWindowPos(checkbox,HWND_BOTTOM,ListViewRect.left,
				ListViewRect.top,
				(ListViewRect.right-ListViewRect.left),
				14,
				SWP_NOZORDER);
			if (bSecondCheckbox)
			{
				SetWindowPos(checkbox2,HWND_BOTTOM,ListViewRect.left,
					ListViewRect.top+20,
					(ListViewRect.right-ListViewRect.left),
					14,
					SWP_NOZORDER);
			}
			HWND label = FindWindowEx(hwnd, NULL, _T("STATIC"), NULL);
			if (label)
			{
				HFONT hFont = (HFONT)::SendMessage(label, WM_GETFONT, 0, 0);
				LOGFONT lf = {0};
				GetObject(hFont, sizeof(lf), &lf);
				HFONT hf2 = CreateFontIndirect(&lf);
				::SendMessage(checkbox, WM_SETFONT, (WPARAM)hf2, TRUE);
				if (bSecondCheckbox)
					::SendMessage(checkbox2, WM_SETFONT, (WPARAM)hf2, TRUE);
			}
			else
			{
				//Sets the fonts of static controls
				SetFont(checkbox,_T("MS Sans Serif"),12);
				if (bSecondCheckbox)
					SetFont(checkbox2,_T("MS Sans Serif"),12);
			}

			// Subclass the checkbox control. 
			CBProc = (WNDPROC) SetWindowLongPtr(checkbox,GWLP_WNDPROC, (LONG_PTR) CheckBoxSubclassProc); 
			//Sets the checkbox to checked position
			SendMessage(checkbox,BM_SETCHECK,(WPARAM)m_bCheck,0);
			if (bSecondCheckbox)
			{
				CBProc = (WNDPROC) SetWindowLongPtr(checkbox2,GWLP_WNDPROC, (LONG_PTR) CheckBoxSubclassProc2); 
				SendMessage(checkbox2,BM_SETCHECK,(WPARAM)m_bCheck,0);
			}
			// send a resize message to the resized listview control. Otherwise it won't show
			// up properly until the user resizes the window!
			SendMessage(ListView, WM_SIZE, SIZE_RESTORED, MAKELONG(ListViewRect.right-ListViewRect.left, bSecondCheckbox ? (ListViewRect.bottom - ListViewRect.top)-40 : (ListViewRect.bottom - ListViewRect.top)-20));
		}
		
		// now set the default directory
		SendMessage(hwnd, BFFM_SETSELECTION, TRUE, (LPARAM)m_sDefaultPath.c_str());
	}
	if (uMsg == BFFM_SELCHANGED)
	{
		// Set the status window to the currently selected path.
		TCHAR szDir[MAX_PATH_NEW];
		if (SHGetPathFromIDList((LPITEMIDLIST)lParam, szDir))
		{
			SendMessage(hwnd,BFFM_SETSTATUSTEXT, 0, (LPARAM)szDir);
		}
	}
	
	return 0;
}

LRESULT CBrowseFolder::CheckBoxSubclassProc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	if (uMsg == WM_LBUTTONUP)
	{
		m_bCheck = (SendMessage(hwnd,BM_GETCHECK,0,0)==BST_UNCHECKED);
		if (m_bCheck && m_DisableCheckbox2WhenCheckbox1IsChecked)
		{
			::EnableWindow(checkbox2, !m_bCheck);
		}
		else
			::EnableWindow(checkbox2, true);
	}

	return CallWindowProc(CBProc, hwnd, uMsg, 
		wParam, lParam); 
} 

LRESULT CBrowseFolder::CheckBoxSubclassProc2(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	if (uMsg == WM_LBUTTONUP)
	{
		m_bCheck2 = (SendMessage(hwnd,BM_GETCHECK,0,0)==BST_UNCHECKED);
	}

	return CallWindowProc(CBProc, hwnd, uMsg, 
		wParam, lParam); 
} 
