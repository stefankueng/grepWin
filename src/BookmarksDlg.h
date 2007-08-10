#pragma once
#include "basedialog.h"
#include "DlgResizer.h"
#include "Bookmarks.h"
#include <string>

using namespace std;

/**
 * bookmarks dialog.
 */
class CBookmarksDlg : public CDialog
{
public:
	CBookmarksDlg(HWND hParent);
	~CBookmarksDlg(void);

	wstring					GetName() {return m_name;}

protected:
	LRESULT CALLBACK		DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT					DoCommand(int id, int msg);
	void					InitBookmarks();
private:
	HWND					m_hParent;
	wstring					m_name;
	CBookmarks				m_bookmarks;

	CDlgResizer				m_resizer;
};
