#include "StdAfx.h"
#include "Resource.h"
#include "RegexTestDlg.h"
#include <string>

#include <boost/regex.hpp>
using namespace boost;
using namespace std;


CRegexTestDlg::CRegexTestDlg(HWND hParent)
{
	m_hParent = hParent;
}

CRegexTestDlg::~CRegexTestDlg(void)
{
}

LRESULT CRegexTestDlg::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (uMsg)
	{
	case WM_INITDIALOG:
		{
			InitDialog(hwndDlg, IDI_GREPWIN);
			// initialize the controls
			SetDlgItemText(hwndDlg, IDC_SEARCHTEXT, m_searchText.c_str());
			SetDlgItemText(hwndDlg, IDC_REPLACETEXT, m_replaceText.c_str());

			SetFocus(GetDlgItem(hwndDlg, IDC_SEARCHTEXT));

			m_resizer.Init(hwndDlg);

		}
		return FALSE;
	case WM_COMMAND:
		return DoCommand(LOWORD(wParam), HIWORD(wParam));
	case WM_SIZE:
		{
			m_resizer.DoResize(LOWORD(lParam), HIWORD(lParam));
		}
		break;
	case WM_GETMINMAXINFO:
		{
			MINMAXINFO * mmi = (MINMAXINFO*)lParam;
			mmi->ptMinTrackSize.x = m_resizer.GetDlgRect()->right;
			mmi->ptMinTrackSize.y = m_resizer.GetDlgRect()->bottom;
			return 0;
		}
		break;
	case WM_TIMER:
		{
			if (wParam == ID_REGEXTIMER)
			{
				KillTimer(*this, ID_REGEXTIMER);
				DoRegex();
			}
		}
		break;
	default:
		return FALSE;
	}
	return FALSE;
}

LRESULT CRegexTestDlg::DoCommand(int id, int msg)
{
	switch (id)
	{
	case IDOK:
		{
			TCHAR buf[MAX_PATH*4] = {0};
			GetDlgItemText(*this, IDC_SEARCHTEXT, buf, MAX_PATH*4);
			m_searchText = buf;
			GetDlgItemText(*this, IDC_REPLACETEXT, buf, MAX_PATH*4);
			m_replaceText = buf;
		}
		// fall through
	case IDCANCEL:
		EndDialog(*this, id);
		break;
	case IDC_TEXTCONTENT:
		{
			if (msg == EN_CHANGE)
			{
				TCHAR * buf = new TCHAR[10*1024*1024];
				GetDlgItemText(*this, IDC_TEXTCONTENT, buf, 10*1024*1024);
				m_textContent = wstring(buf);
				delete [] buf;

				SetTimer(*this, ID_REGEXTIMER, 300, NULL);
			}
		}
		break;
	case IDC_REPLACETEXT:
	case IDC_SEARCHTEXT:
		{
			if (msg == EN_CHANGE)
			{
				TCHAR buf[MAX_PATH*4] = {0};
				GetDlgItemText(*this, IDC_SEARCHTEXT, buf, MAX_PATH*4);
				m_searchText = buf;
				GetDlgItemText(*this, IDC_REPLACETEXT, buf, MAX_PATH*4);
				m_replaceText = buf;

				SetTimer(*this, ID_REGEXTIMER, 300, NULL);
			}
		}
		break;
	}
	return 1;
}

void CRegexTestDlg::SetStrings(const wstring& search, const wstring& replace)
{
	m_replaceText = replace;
	m_searchText = search;
}

void CRegexTestDlg::DoRegex()
{
	if (m_textContent.empty())
	{
		SetDlgItemText(*this, IDC_REGEXMATCH, _T("no text to test with available"));
		SetDlgItemText(*this, IDC_REGEXREPLACED, _T("no text to test with available"));
	}
	else if (m_searchText.empty())
	{
		SetDlgItemText(*this, IDC_REGEXMATCH, _T("search string is empty"));
		SetDlgItemText(*this, IDC_REGEXREPLACED, _T("search string is empty"));
	}
	else if (m_replaceText.empty())
	{
		SetDlgItemText(*this, IDC_REGEXREPLACED, _T("no text to replace with"));
	}

	if (!m_textContent.empty())
	{
		wstring searchresult;
		wstring replaceresult;
		if (!m_searchText.empty())
		{
			wstring::const_iterator start, end;
			start = m_textContent.begin();
			end = m_textContent.end();
			match_results<wstring::const_iterator> what;
			try
			{
				wregex expression = wregex(m_searchText);
				match_results<wstring::const_iterator> whatc;
				match_flag_type flags = match_default;
				while (regex_search(start, end, whatc, expression, flags))   
				{
					if (!searchresult.empty())
						searchresult = searchresult + _T("\r\n----------------------------\r\n");
					wstring c(whatc[0].first, whatc[0].second);
					searchresult = searchresult + c;
					if (!m_searchText.empty())
					{
						match_flag_type rflags = match_default|format_all;
						wstring replaced = regex_replace(c, expression, m_replaceText, rflags);
						if (!replaceresult.empty())
							replaceresult = replaceresult + _T("\r\n----------------------------\r\n");
						replaceresult = replaceresult + replaced;
					}
					// update search position:
					if (start == whatc[0].second)
					{
						if (start == end)
							break;
						start++;
					}
					else
						start = whatc[0].second;
					// update flags:
					flags |= match_prev_avail;
					flags |= match_not_bob;
				}
			}
			catch (const exception&)
			{

			}
			if (searchresult.empty())
				SetDlgItemText(*this, IDC_REGEXMATCH, _T("no match"));
			else
				SetDlgItemText(*this, IDC_REGEXMATCH, searchresult.c_str());
		}
		if (!searchresult.empty())
			SetDlgItemText(*this, IDC_REGEXMATCH, searchresult.c_str());
		if (!replaceresult.empty())
			SetDlgItemText(*this, IDC_REGEXREPLACED, replaceresult.c_str());
	}
}