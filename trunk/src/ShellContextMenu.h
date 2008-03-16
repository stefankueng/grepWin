// grepWin - regex search and replace for Windows

// Copyright (C) 2007-2008 - Stefan Kueng

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
#include <vector>
#include <shobjidl.h>
#include <shlobj.h>

#pragma comment(lib, "shell32.lib")

using namespace std;

class CShellContextMenu  
{
public:
	HMENU GetMenu();
	void SetObjects(IShellFolder * psfFolder, LPITEMIDLIST pidlItem);
	void SetObjects(IShellFolder * psfFolder, LPITEMIDLIST * pidlArray, int nItemCount);
	void SetObjects(LPITEMIDLIST pidl);
	void SetObjects(LPCTSTR strObject);
	void SetObjects(const vector<wstring>& strVector);
	UINT ShowContextMenu(HWND hWnd, POINT pt);
	CShellContextMenu();
	virtual ~CShellContextMenu();

private:
	int nItems;
	BOOL bDelete;
	HMENU m_Menu;
	IShellFolder * m_psfFolder;
	LPITEMIDLIST * m_pidlArray;	
	
	void InvokeCommand(LPCONTEXTMENU pContextMenu, UINT idCommand);
	BOOL GetContextMenu(void ** ppContextMenu, int & iMenuType);
	static LRESULT CALLBACK HookWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	void FreePIDLArray(LPITEMIDLIST * pidlArray);
	LPITEMIDLIST CopyPIDL(LPCITEMIDLIST pidl, int cb = -1);
	UINT GetPIDLSize(LPCITEMIDLIST pidl);
	LPBYTE GetPIDLPos(LPCITEMIDLIST pidl, int nPos);
	int GetPIDLCount(LPCITEMIDLIST pidl);
};
