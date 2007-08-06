#pragma once
#include "basedialog.h"

/**
 * search dialog.
 */
class CSearchDlg : public CDialog
{
public:
	CSearchDlg(HWND hParent);
	~CSearchDlg(void);

protected:
	LRESULT CALLBACK		DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT					DoCommand(int id);

private:
	HWND					m_hParent;
};
