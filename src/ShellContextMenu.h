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
