#pragma once
#include "basedialog.h"
#include "DlgResizer.h"
#include "Bookmarks.h"
#include "hyperlink.h"
#include <string>

using namespace std;

/**
 * bookmarks dialog.
 */
class CAboutDlg : public CDialog
{
public:
	CAboutDlg(HWND hParent);
	~CAboutDlg(void);

protected:
	LRESULT CALLBACK		DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT					DoCommand(int id, int msg);
private:
	HWND					m_hParent;
	CHyperLink				m_link;
};
