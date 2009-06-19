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
#include "ShellContextMenu.h"
#include "shellapi.h"

#define MIN_ID 3
#define MAX_ID 10000

IContextMenu2 * g_IContext2 = NULL;
IContextMenu3 * g_IContext3 = NULL;
WNDPROC g_OldWndProc = NULL;

CShellContextMenu::CShellContextMenu()
{
	m_psfFolder = NULL;
	m_pidlArray = NULL;
	m_Menu = NULL;
}

CShellContextMenu::~CShellContextMenu()
{
	// free all allocated data
	if (m_psfFolder && bDelete)
		m_psfFolder->Release ();
	m_psfFolder = NULL;
	FreePIDLArray (m_pidlArray);
	m_pidlArray = NULL;

	if (m_Menu)
		DestroyMenu(m_Menu);
}


// this functions determines which version of IContextMenu is available for those objects (always the highest one)
// and returns that interface
BOOL CShellContextMenu::GetContextMenu(void ** ppContextMenu, int & iMenuType)
{
	*ppContextMenu = NULL;
	LPCONTEXTMENU icm1 = NULL;
	
	if (m_psfFolder == NULL)
		return FALSE;

	// first we retrieve the normal IContextMenu interface (every object should have it)
	m_psfFolder->GetUIObjectOf(NULL, nItems, (LPCITEMIDLIST *) m_pidlArray, IID_IContextMenu, NULL, (void**) &icm1);

	if (icm1)
	{	// since we got an IContextMenu interface we can now obtain the higher version interfaces via that
		if (icm1->QueryInterface(IID_IContextMenu3, ppContextMenu) == NOERROR)
			iMenuType = 3;
		else if (icm1->QueryInterface(IID_IContextMenu2, ppContextMenu) == NOERROR)
			iMenuType = 2;

		if (*ppContextMenu) 
			icm1->Release(); // we can now release version 1 interface, cause we got a higher one
		else 
		{	
			// since no higher versions were found
			// redirect ppContextMenu to version 1 interface
			iMenuType = 1;
			*ppContextMenu = icm1;	
		}
	}
	else
		return FALSE;
	
	return TRUE;
}


LRESULT CALLBACK CShellContextMenu::HookWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{ 
	case WM_MENUCHAR:	// only supported by IContextMenu3
		if (g_IContext3)
		{
			LRESULT lResult = 0;
			g_IContext3->HandleMenuMsg2 (message, wParam, lParam, &lResult);
			return (lResult);
		}
		break;

	case WM_DRAWITEM:
	case WM_MEASUREITEM:
		if (wParam) 
			break; // if wParam != 0 then the message is not menu-related
  
	case WM_INITMENU:
	case WM_INITMENUPOPUP:
		if (g_IContext3)
		{
			LRESULT lResult = 0;
			g_IContext3->HandleMenuMsg2 (message, wParam, lParam, &lResult);
		}
		else
			g_IContext2->HandleMenuMsg (message, wParam, lParam);

		return TRUE;
		break;

	default:
		break;
	}

	// call original WndProc of window to prevent undefined behavior of window
	return ::CallWindowProc (g_OldWndProc , hWnd, message, wParam, lParam);
}


UINT CShellContextMenu::ShowContextMenu(HWND hWnd, POINT pt)
{
	int iMenuType = 0;	// to know which version of IContextMenu is supported
	LPCONTEXTMENU pContextMenu;	// common pointer to IContextMenu and higher version interface
   
	if (!GetContextMenu ((void**)&pContextMenu, iMenuType))	
		return 0;

	if (!m_Menu)
	{
		DestroyMenu(m_Menu);
		m_Menu = CreatePopupMenu();
	}

	if (m_strVector.size() == 1)
	{
		::InsertMenu(m_Menu, 1, MF_BYPOSITION | MF_STRING, 1, _T("Open Containing Folder"));
		::InsertMenu(m_Menu, 2, MF_SEPARATOR|MF_BYPOSITION, 0, NULL);
	}
	// lets fill the our popup menu  
	pContextMenu->QueryContextMenu(m_Menu, GetMenuItemCount(m_Menu), MIN_ID, MAX_ID, CMF_NORMAL | CMF_EXPLORE);
 
	// subclass window to handle menu related messages in CShellContextMenu 
	if (iMenuType > 1)	// only subclass if its version 2 or 3
	{
		g_OldWndProc = (WNDPROC)SetWindowLong (hWnd, GWL_WNDPROC, (DWORD)HookWndProc);
		if (iMenuType == 2)
			g_IContext2 = (LPCONTEXTMENU2)pContextMenu;
		else	// version 3
			g_IContext3 = (LPCONTEXTMENU3)pContextMenu;
	}
	else
		g_OldWndProc = NULL;

	UINT idCommand = TrackPopupMenu(m_Menu, TPM_RETURNCMD | TPM_LEFTALIGN, pt.x, pt.y, 0, hWnd, NULL);

	if (g_OldWndProc) // un-subclass
		SetWindowLong(hWnd, GWL_WNDPROC, (DWORD) g_OldWndProc);

	if (idCommand >= MIN_ID && idCommand <= MAX_ID)	// see if returned idCommand belongs to shell menu entries
	{
		InvokeCommand(pContextMenu, idCommand - MIN_ID);	// execute related command
		idCommand = 0;
	}
	else
	{
		switch (idCommand)
		{
		case 1:
			{
				// This is the command line for explorer which tells it to select the given file  
				wstring sFolder = _T( "/Select," ) + m_strVector[0];

				// Prepare shell execution params  
				SHELLEXECUTEINFO shExecInfo   = { 0 };
				shExecInfo.cbSize             = sizeof(shExecInfo);
				shExecInfo.lpFile             = _T("explorer.exe");
				shExecInfo.lpParameters       = sFolder.c_str();
				shExecInfo.nShow              = SW_SHOWNORMAL;
				shExecInfo.lpVerb             = _T("open"); // Context menu item      
				shExecInfo.fMask              = SEE_MASK_INVOKEIDLIST | SEE_MASK_FLAG_DDEWAIT | SEE_MASK_FLAG_NO_UI;      

				// Select file in explorer  
				ShellExecuteEx(&shExecInfo);
			}
			break;
		}
	}
	
	pContextMenu->Release();
	g_IContext2 = NULL;
	g_IContext3 = NULL;

	return (idCommand);
}


void CShellContextMenu::InvokeCommand(LPCONTEXTMENU pContextMenu, UINT idCommand)
{
	CMINVOKECOMMANDINFO cmi = {0};
	cmi.cbSize = sizeof (CMINVOKECOMMANDINFO);
	cmi.lpVerb = (LPSTR) MAKEINTRESOURCE (idCommand);
	cmi.nShow = SW_SHOWNORMAL;
	
	pContextMenu->InvokeCommand (&cmi);
}

void CShellContextMenu::SetObjects(const vector<wstring>& strVector)
{
	// free all allocated data
	if (m_psfFolder && bDelete)
		m_psfFolder->Release();
	m_psfFolder = NULL;
	FreePIDLArray(m_pidlArray);
	m_pidlArray = NULL;
	
	// get IShellFolder interface of Desktop (root of shell namespace)
	IShellFolder * psfDesktop = NULL;
	SHGetDesktopFolder(&psfDesktop);	// needed to obtain full qualified pidl

	// ParseDisplayName creates a PIDL from a file system path relative to the IShellFolder interface
	// but since we use the Desktop as our interface and the Desktop is the namespace root
	// that means that it's a fully qualified PIDL, which is what we need
	LPITEMIDLIST pidl = NULL;
	
	psfDesktop->ParseDisplayName(NULL, 0, (LPWSTR)strVector[0].c_str(), NULL, &pidl, NULL);

	// now we need the parent IShellFolder interface of pidl, and the relative PIDL to that interface
	LPITEMIDLIST pidlItem = NULL;	// relative pidl
	SHBindToParent(pidl, IID_IShellFolder, (void **) &m_psfFolder, NULL);
	free (pidlItem);
	// get interface to IMalloc (need to free the PIDLs allocated by the shell functions)
	LPMALLOC lpMalloc = NULL;
	SHGetMalloc(&lpMalloc);
	lpMalloc->Free(pidl);

	// now we have the IShellFolder interface to the parent folder specified in the first element in strArray
	// since we assume that all objects are in the same folder (as it's stated in the MSDN)
	// we now have the IShellFolder interface to every objects parent folder
	
	IShellFolder * psfFolder = NULL;
	nItems = strVector.size();
	for (int i = 0; i < nItems; i++)
	{
		psfDesktop->ParseDisplayName(NULL, 0, (LPWSTR)strVector[i].c_str(), NULL, &pidl, NULL);
		m_pidlArray = (LPITEMIDLIST *)realloc (m_pidlArray, (i + 1) * sizeof (LPITEMIDLIST));
		// get relative pidl via SHBindToParent
		if (SHBindToParent(pidl, IID_IShellFolder, (void **) &psfFolder, (LPCITEMIDLIST *) &pidlItem) == S_OK)
		{
			m_pidlArray[i] = CopyPIDL(pidlItem);	// copy relative pidl to pidlArray
			lpMalloc->Free(pidl);		// free pidl allocated by ParseDisplayName
			psfFolder->Release();
		}
		else
		{
			m_pidlArray[i] = NULL;
			lpMalloc->Free(pidl);		// free pidl allocated by ParseDisplayName
		}
	}
	lpMalloc->Release ();
	psfDesktop->Release ();

	m_strVector = strVector;
	bDelete = TRUE;	// indicates that m_psfFolder should be deleted by CShellContextMenu
}

void CShellContextMenu::FreePIDLArray(LPITEMIDLIST *pidlArray)
{
	if (!pidlArray)
		return;

	int iSize = _msize(pidlArray) / sizeof(LPITEMIDLIST);

	for (int i = 0; i < iSize; i++)
		free(pidlArray[i]);
	free(pidlArray);
}


LPITEMIDLIST CShellContextMenu::CopyPIDL(LPCITEMIDLIST pidl, int cb)
{
	if (cb == -1)
		cb = GetPIDLSize(pidl); // Calculate size of list.

    LPITEMIDLIST pidlRet = (LPITEMIDLIST)calloc (cb + sizeof(USHORT), sizeof(BYTE));
    if (pidlRet)
		CopyMemory(pidlRet, pidl, cb);

    return pidlRet;
}


UINT CShellContextMenu::GetPIDLSize(LPCITEMIDLIST pidl)
{  
	if (!pidl) 
		return 0;
	int nSize = 0;
	LPITEMIDLIST pidlTemp = (LPITEMIDLIST)pidl;
	while (pidlTemp->mkid.cb)
	{
		nSize += pidlTemp->mkid.cb;
		pidlTemp = (LPITEMIDLIST)(((LPBYTE)pidlTemp) + pidlTemp->mkid.cb);
	}
	return nSize;
}

HMENU CShellContextMenu::GetMenu()
{
	if (!m_Menu)
	{
		m_Menu = CreatePopupMenu();
	}
	return m_Menu;
}

LPBYTE CShellContextMenu::GetPIDLPos(LPCITEMIDLIST pidl, int nPos)
{
	if (!pidl)
		return 0;
	int nCount = 0;
	
	BYTE * pCur = (BYTE *)pidl;
	while (((LPCITEMIDLIST)pCur)->mkid.cb)
	{
		if (nCount == nPos)
			return pCur;
		nCount++;
		pCur += ((LPCITEMIDLIST)pCur)->mkid.cb;
	}
	if (nCount == nPos) 
		return pCur;
	return NULL;
}


int CShellContextMenu::GetPIDLCount(LPCITEMIDLIST pidl)
{
	if (!pidl)
		return 0;

	int nCount = 0;
	BYTE*  pCur = (BYTE *)pidl;
	while (((LPCITEMIDLIST)pCur)->mkid.cb)
	{
		nCount++;
		pCur += ((LPCITEMIDLIST)pCur)->mkid.cb;
	}
	return nCount;
}