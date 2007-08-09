#pragma once
#include "basedialog.h"
#include "SearchInfo.h"
#include "DlgResizer.h"
#include <string>
#include <vector>

using namespace std;

#define SEARCH_FOUND		(WM_APP+1)
#define SEARCH_START		(WM_APP+2)
#define SEARCH_PROGRESS		(WM_APP+3)
#define SEARCH_END			(WM_APP+4)


/**
 * search dialog.
 */
class CSearchDlg : public CDialog
{
public:
	CSearchDlg(HWND hParent);
	~CSearchDlg(void);

	DWORD					SearchThread();

protected:
	LRESULT CALLBACK		DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT					DoCommand(int id, int msg);

	int						SearchFile(CSearchInfo& sinfo, bool bUseRegex, const wstring& searchString);

	bool					InitResultList();
	bool					AddFoundEntry(CSearchInfo * pInfo);
	void					ShowContextMenu(int x, int y);
	void					DoListNotify(LPNMITEMACTIVATE lpNMItemActivate);
	void					UpdateInfoLabel();
private:
	HWND					m_hParent;
	volatile LONG			m_dwThreadRunning;
	volatile LONG			m_Cancelled;
	wstring					m_searchpath;
	wstring					m_searchString;
	wstring					m_replaceString;
	bool					m_bUseRegex;
	bool					m_bAllSize;
	DWORD					m_lSize;
	int						m_sizeCmp;
	bool					m_bIncludeSystem;
	bool					m_bIncludeHidden;
	bool					m_bIncludeSubfolders;

	HANDLE					m_hSearchThread;

	vector<CSearchInfo>		m_items;
	int						m_totalitems;
	int						m_searchedItems;

	CDlgResizer				m_resizer;
};
