#pragma once
#include "basedialog.h"
#include "DlgResizer.h"
#include <string>
#include <vector>

using namespace std;

#define ID_REGEXTIMER		100

/**
 * regex test dialog.
 */
class CRegexTestDlg : public CDialog
{
public:
	CRegexTestDlg(HWND hParent);
	~CRegexTestDlg(void);

	void					SetStrings(const wstring& search, const wstring& replace);
	wstring					GetSearchString() {return m_searchText;}
	wstring					GetReplaceString() {return m_replaceText;}

protected:
	LRESULT CALLBACK		DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT					DoCommand(int id, int msg);
	void					DoRegex();

private:
	HWND					m_hParent;
	wstring					m_searchText;
	wstring					m_replaceText;
	wstring					m_textContent;

	CDlgResizer				m_resizer;
};
