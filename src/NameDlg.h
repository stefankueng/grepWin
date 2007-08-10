#pragma once
#include "basedialog.h"
#include "DlgResizer.h"
#include <string>

using namespace std;

/**
 * name dialog.
 */
class CNameDlg : public CDialog
{
public:
	CNameDlg(HWND hParent);
	~CNameDlg(void);

	wstring					GetName() {return m_name;}

protected:
	LRESULT CALLBACK		DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT					DoCommand(int id, int msg);

private:
	HWND					m_hParent;
	wstring					m_name;

	CDlgResizer				m_resizer;
};
