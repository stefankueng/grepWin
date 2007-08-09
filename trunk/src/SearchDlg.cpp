#include "StdAfx.h"
#include "Resource.h"
#include "SearchDlg.h"
#include "Registry.h"
#include "DirFileEnum.h"
#include "TextFile.h"
#include "SearchInfo.h"
#include "UnicodeUtils.h"
#include "BrowseFolder.h"
#include "SysImageList.h"
#include <string>
#include <iostream>
#include <fstream>
#include <iterator>
#include <algorithm>
#include <Commdlg.h>

#include <boost/regex.hpp>
#include <boost/spirit/iterator/file_iterator.hpp>
using namespace boost;
using namespace std;

DWORD WINAPI SearchThreadEntry(LPVOID lpParam);


CSearchDlg::CSearchDlg(HWND hParent)
{
	m_hParent = hParent;
}

CSearchDlg::~CSearchDlg(void)
{
}

LRESULT CSearchDlg::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (uMsg)
	{
	case WM_INITDIALOG:
		{
			InitDialog(hwndDlg, IDI_GREPWIN);
			// initialize the controls
			SetDlgItemText(hwndDlg, IDC_SEARCHPATH, m_searchpath.c_str());
			CheckRadioButton(hwndDlg, IDC_REGEXRADIO, IDC_TEXTRADIO, IDC_REGEXRADIO);
			CheckRadioButton(hwndDlg, IDC_ALLSIZERADIO, IDC_SIZERADIO, IDC_ALLSIZERADIO);
			SendDlgItemMessage(hwndDlg, IDC_SIZECOMBO, CB_INSERTSTRING, (WPARAM)-1, (LPARAM)_T("less than"));
			SendDlgItemMessage(hwndDlg, IDC_SIZECOMBO, CB_INSERTSTRING, (WPARAM)-1, (LPARAM)_T("equal to"));
			SendDlgItemMessage(hwndDlg, IDC_SIZECOMBO, CB_INSERTSTRING, (WPARAM)-1, (LPARAM)_T("greater than"));
			SendDlgItemMessage(hwndDlg, IDC_SIZECOMBO, CB_SETCURSEL, 0, 0);
			SendDlgItemMessage(hwndDlg, IDC_INCLUDESUBFOLDERS, BM_SETCHECK, BST_CHECKED, 0);
		}
		return TRUE;
	case WM_COMMAND:
		return DoCommand(LOWORD(wParam), HIWORD(wParam));
	case SEARCH_FOUND:
		if (wParam)
		{
			AddFoundEntry((CSearchInfo*)lParam);
		}
		break;
	case SEARCH_END:
		{
			HWND hListControl = GetDlgItem(*this, IDC_RESULTLIST);
			ListView_SetColumnWidth(hListControl, 0, LVSCW_AUTOSIZE_USEHEADER);
			ListView_SetColumnWidth(hListControl, 1, LVSCW_AUTOSIZE_USEHEADER);
			ListView_SetColumnWidth(hListControl, 2, LVSCW_AUTOSIZE_USEHEADER);
		}
		break;
	default:
		return FALSE;
	}
	return FALSE;
}

LRESULT CSearchDlg::DoCommand(int id, int msg)
{
	switch (id)
	{
	case IDOK:
		{
			// get all the information we need from the dialog
			TCHAR buf[MAX_PATH*4] = {0};
			GetDlgItemText(*this, IDC_SEARCHPATH, buf, MAX_PATH*4);
			m_searchpath = buf;
			GetDlgItemText(*this, IDC_SEARCHTEXT, buf, MAX_PATH*4);
			m_searchString = buf;

			m_bUseRegex = (IsDlgButtonChecked(*this, IDC_REGEXRADIO) == BST_CHECKED);
			m_bAllSize = (IsDlgButtonChecked(*this, IDC_ALLSIZERADIO) == BST_CHECKED);
			if (!m_bAllSize)
			{
				GetDlgItemText(*this, IDC_SIZEEDIT, buf, MAX_PATH*4);
				m_lSize = _tstol(buf);
				m_sizeCmp = SendDlgItemMessage(*this, IDC_SIZECOMBO, CB_GETCURSEL, 0, 0);
			}
			m_bIncludeSystem = (IsDlgButtonChecked(*this, IDC_INCLUDESYSTEM) == BST_CHECKED);
			m_bIncludeHidden = (IsDlgButtonChecked(*this, IDC_INCLUDEHIDDEN) == BST_CHECKED);
			m_bIncludeSubfolders = (IsDlgButtonChecked(*this, IDC_INCLUDESUBFOLDERS) == BST_CHECKED);
			
			InitResultList();

			// now start the thread which does the searching
			DWORD dwThreadId = 0;
			m_hSearchThread = CreateThread( 
				NULL,              // no security attribute 
				0,                 // default stack size 
				SearchThreadEntry, 
				(LPVOID)this,      // thread parameter 
				0,                 // not suspended 
				&dwThreadId);      // returns thread ID 

		}
		break;
	case IDCANCEL:
		EndDialog(*this, id);
		break;
	case IDC_SEARCHPATHBROWSE:
		{
			TCHAR path[MAX_PATH*4] = {0};
			CBrowseFolder browse;

			GetDlgItemText(*this, IDC_SEARCHPATH, path, MAX_PATH*4);
			browse.SetInfo(_T("Select path to search"));
			if (browse.Show(*this, path, MAX_PATH*4, m_searchpath.c_str()) == CBrowseFolder::OK)
			{
				SetDlgItemText(*this, IDC_SEARCHPATH, path);
				m_searchpath = path;
			}
		}
		break;
	case IDC_SEARCHTEXT:
		{
			if (msg == EN_CHANGE)
			{
				if (IsDlgButtonChecked(*this, IDC_REGEXRADIO) == BST_CHECKED)
				{
					// check if the regex is valid
					TCHAR buf[MAX_PATH*4] = {0};
					GetDlgItemText(*this, IDC_SEARCHTEXT, buf, MAX_PATH*4);
					bool bValid = true;
					if (_tcslen(buf))
					{
						try
						{
							wregex expression = wregex(buf);
						}
						catch (const exception&)
						{
							bValid = false;
						}
					}
					if (bValid)
						SetDlgItemText(*this, IDC_REGEXOKLABEL, _T("regex ok"));
					else
						SetDlgItemText(*this, IDC_REGEXOKLABEL, _T("invalid regex!"));
				}
				else
				{
					SetDlgItemText(*this, IDC_REGEXOKLABEL, _T(""));
				}
			}
		}
		break;
	}
	return 1;
}

bool CSearchDlg::InitResultList()
{
	HWND hListControl = GetDlgItem(*this, IDC_RESULTLIST);
	DWORD exStyle = LVS_EX_DOUBLEBUFFER;
	ListView_DeleteAllItems(hListControl);

	int c = Header_GetItemCount(ListView_GetHeader(hListControl))-1;
	while (c>=0)
		ListView_DeleteColumn(hListControl, c--);

	ListView_SetExtendedListViewStyle(hListControl, exStyle);
	ListView_SetImageList(hListControl, (WPARAM)(HIMAGELIST)CSysImageList::GetInstance(), LVSIL_SMALL);
	LVCOLUMN lvc = {0};
	lvc.mask = LVCF_TEXT;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = -1;
	lvc.pszText = _T("Name");
	ListView_InsertColumn(hListControl, 0, &lvc);
	lvc.pszText = _T("Size");
	ListView_InsertColumn(hListControl, 1, &lvc);
	lvc.pszText = _T("Matches");
	ListView_InsertColumn(hListControl, 2, &lvc);
	lvc.pszText = _T("Path");
	ListView_InsertColumn(hListControl, 3, &lvc);

	ListView_SetColumnWidth(hListControl, 0, LVSCW_AUTOSIZE_USEHEADER);
	ListView_SetColumnWidth(hListControl, 1, LVSCW_AUTOSIZE_USEHEADER);
	ListView_SetColumnWidth(hListControl, 2, LVSCW_AUTOSIZE_USEHEADER);
	ListView_SetColumnWidth(hListControl, 3, LVSCW_AUTOSIZE_USEHEADER);

	return true;
}

bool CSearchDlg::AddFoundEntry(CSearchInfo * pInfo)
{
	HWND hListControl = GetDlgItem(*this, IDC_RESULTLIST);
	LVITEM lv = {0};
	lv.mask = LVIF_TEXT | LVIF_IMAGE;
	TCHAR * pBuf = new TCHAR[pInfo->filepath.size()+1];
	wstring name = pInfo->filepath.substr(pInfo->filepath.find_last_of('\\')+1);
	_tcscpy_s(pBuf, pInfo->filepath.size()+1, name.c_str());
	lv.pszText = pBuf;
	lv.iImage = CSysImageList::GetInstance().GetFileIconIndex(pInfo->filepath);
	int ret = ListView_InsertItem(hListControl, &lv);
	delete [] pBuf;
	if (ret >= 0)
	{
		lv.iItem = ret;
		lv.iSubItem = 1;
		TCHAR sb[MAX_PATH*4] = {0};
		StrFormatByteSizeW(pInfo->filesize, sb, 20);
		lv.pszText = sb;
		ListView_SetItem(hListControl, &lv);
		lv.iSubItem = 2;
		_stprintf_s(sb, MAX_PATH*4, _T("%ld"), pInfo->matchstarts.size());
		ListView_SetItem(hListControl, &lv);
		lv.iSubItem = 3;
		_tcscpy_s(sb, MAX_PATH*4, pInfo->filepath.substr(0, pInfo->filepath.size()-name.size()).c_str()-1);
		ListView_SetItem(hListControl, &lv);
	}
	return (ret != -1);
}

DWORD CSearchDlg::SearchThread()
{
	TCHAR pathbuf[MAX_PATH*4] = {0};
	bool bIsDirectory = false;
	CDirFileEnum fileEnumerator(m_searchpath.c_str());
	bool bRecurse = m_bIncludeSubfolders;

	while (fileEnumerator.NextFile(pathbuf, bRecurse, &bIsDirectory))
	{
		if (!bIsDirectory)
		{
			const WIN32_FIND_DATA * pFindData = fileEnumerator.GetFileInfo();
			bool bSearch = ((m_bIncludeHidden)||((pFindData->dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) == 0));
			bSearch = bSearch && ((m_bIncludeSystem)||((pFindData->dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) == 0));
			if (!m_bAllSize)
			{
				switch (m_sizeCmp)
				{
				case 0:	// less than
					bSearch = bSearch && (pFindData->nFileSizeLow < m_lSize);
					break;
				case 1:	// equal
					bSearch = bSearch && (pFindData->nFileSizeLow == m_lSize);
					break;
				case 2:	// greater than
					bSearch = bSearch && (pFindData->nFileSizeLow > m_lSize);
					break;
				}
			}
			bRecurse = ((bIsDirectory)&&(m_bIncludeSubfolders)&&(bSearch));

			if (bSearch)
			{
				CSearchInfo sinfo(pathbuf);
				sinfo.filesize = pFindData->nFileSizeLow;
				int nFound = SearchFile(sinfo, m_bUseRegex, m_searchString);
				SendMessage(*this, SEARCH_FOUND, nFound, (LPARAM)&sinfo);
			}
		}
		else
		{
			const WIN32_FIND_DATA * pFindData = fileEnumerator.GetFileInfo();
			bool bSearch = ((m_bIncludeHidden)||((pFindData->dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) == 0));
			bSearch = bSearch && ((m_bIncludeSystem)||((pFindData->dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) == 0));
			bRecurse = ((bIsDirectory)&&(m_bIncludeSubfolders)&&(bSearch));
		}
	}
	SendMessage(*this, SEARCH_END, 0, 0);
	return 0L;
}

int CSearchDlg::SearchFile(CSearchInfo& sinfo, bool bUseRegex, const wstring& searchString)
{
	int nFound = 0;
	// we keep it simple:
	// files bigger than 10MB are considered binary. Binary files are searched
	// as if they're ANSI text files.
	if (sinfo.filesize < 10*1024*1024)
	{
		CTextFile textfile;
		if (textfile.Load(sinfo.filepath.c_str()))
		{
			if (bUseRegex)
			{
				wstring::const_iterator start, end;
				start = textfile.GetFileString().begin();
				end = textfile.GetFileString().end();
				match_results<wstring::const_iterator> what;
				match_flag_type flags = match_default;
				try
				{
					wregex expression = wregex(searchString);
					match_results<wstring::const_iterator> whatc;
					while (regex_search(start, end, whatc, expression, flags))   
					{
						nFound++;
						sinfo.matchstarts.push_back(whatc[0].first-textfile.GetFileString().begin());
						sinfo.matchends.push_back(whatc[0].second-textfile.GetFileString().begin());
						// update search position:
						start = whatc[0].second;      
						// update flags:
						flags |= match_prev_avail;
						flags |= match_not_bob;
					}
				}
				catch (const exception&)
				{

				}
			}
			else
			{
				wstring::size_type foundpos = textfile.GetFileString().find(searchString);
				while (foundpos != wstring::npos)
				{
					nFound++;
					sinfo.matchstarts.push_back(foundpos);
					sinfo.matchends.push_back(foundpos+searchString.size());

					foundpos = textfile.GetFileString().find(searchString, foundpos+1);
				}
			}
		}
	}
	else
	{
		// assume binary file

		string filepath = CUnicodeUtils::StdGetANSI(sinfo.filepath);
		string searchfor = CUnicodeUtils::StdGetUTF8(searchString);

		if (bUseRegex)
		{
			spirit::file_iterator<> start(filepath.c_str());
			spirit::file_iterator<> fbeg = start;
			spirit::file_iterator<> end = start.make_end();

			match_results<string::const_iterator> what;
			match_flag_type flags = match_default;
			try
			{
				regex expression = regex(searchfor);
				match_results<spirit::file_iterator<>> whatc;
				while (regex_search(start, end, whatc, expression, flags))   
				{
					nFound++;
					sinfo.matchstarts.push_back(whatc[0].first-fbeg);
					sinfo.matchends.push_back(whatc[0].second-fbeg);
					// update search position:
					start = whatc[0].second;
					// update flags:
					flags |= match_prev_avail;
					flags |= match_not_bob;
				}
			}
			catch (const exception&)
			{

			}
		}
		else
		{
			ifstream file (filepath.c_str(), ios::in|ios::binary|ios::ate);
			if (file.is_open())
			{
				file.seekg (0, ios::beg);

				istream_iterator<string> start(file);
				istream_iterator<string> end;

				start = find(start, end, searchfor);
				while (start != end)
				{
					nFound++;
					sinfo.matchstarts.push_back(file.tellg());
					sinfo.matchends.push_back((DWORD)file.tellg()+searchfor.size());
					++start;
					start = find(start, end, searchfor);
				}
			}
		}
	}

	return nFound;
}

DWORD WINAPI SearchThreadEntry(LPVOID lpParam)
{
	CSearchDlg * pThis = (CSearchDlg*)lpParam;
	if (pThis)
		return pThis->SearchThread();
	return 0L;
}
