#include "StdAfx.h"
#include "Resource.h"
#include "SearchDlg.h"
#include "Registry.h"
#include "DirFileEnum.h"
#include "TextFile.h"
#include "SearchInfo.h"
#include "UnicodeUtils.h"
#include "StringUtils.h"
#include "BrowseFolder.h"
#include "SysImageList.h"
#include "ShellContextMenu.h"
#include "RegexTestDlg.h"
#include "NameDlg.h"
#include "BookmarksDlg.h"
#include "AboutDlg.h"
#include "InfoDlg.h"
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

UINT CSearchDlg::GREPWIN_STARTUPMSG = RegisterWindowMessage(_T("grepWin_StartupMessage"));

CSearchDlg::CSearchDlg(HWND hParent) : m_searchedItems(0)
	, m_totalitems(0)
	, m_dwThreadRunning(FALSE)
	, m_Cancelled(FALSE)
	, m_bAscending(true)
	, m_pDropTarget(NULL)
{
	m_hParent = hParent;
	m_startTime = GetTickCount();
}

CSearchDlg::~CSearchDlg(void)
{
	if (m_pDropTarget)
		delete m_pDropTarget;
}

LRESULT CSearchDlg::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	if (uMsg == GREPWIN_STARTUPMSG)
	{
		if ((GetTickCount() - 1000) < m_startTime)
		{
			m_startTime = GetTickCount();
			return TRUE;
		}
		return FALSE;
	}
	switch (uMsg)
	{
	case WM_INITDIALOG:
		{
			InitDialog(hwndDlg, IDI_GREPWIN);

			AddToolTip(IDC_PATTERN, _T("only files that matches this pattern are searched.\r\nExample: *.cpp;*.h"));
			AddToolTip(IDC_SEARCHPATH, _T("the path which is searched recursively"));

			// expand a possible 'short' path
			DWORD ret = 0;
			ret = ::GetLongPathName(m_searchpath.c_str(), NULL, 0);
			TCHAR * pathbuf = new TCHAR[ret+2];
			ret = ::GetLongPathName(m_searchpath.c_str(), pathbuf, ret+1);
			m_searchpath = wstring(pathbuf, ret);
			delete pathbuf;

			// initialize the controls
			SetDlgItemText(hwndDlg, IDC_SEARCHPATH, m_searchpath.c_str());

			// the path edit control should work as a drop target for files and folders
			HWND hSearchPath = GetDlgItem(hwndDlg, IDC_SEARCHPATH);
			m_pDropTarget = new CFileDropTarget(hSearchPath);
			RegisterDragDrop(hSearchPath, m_pDropTarget);
			// create the supported formats:
			FORMATETC ftetc={0}; 
			ftetc.cfFormat = CF_TEXT; 
			ftetc.dwAspect = DVASPECT_CONTENT; 
			ftetc.lindex = -1; 
			ftetc.tymed = TYMED_HGLOBAL; 
			m_pDropTarget->AddSuportedFormat(ftetc); 
			ftetc.cfFormat=CF_HDROP; 
			m_pDropTarget->AddSuportedFormat(ftetc);
			SHAutoComplete(GetDlgItem(*this, IDC_SEARCHPATH), SHACF_FILESYSTEM|SHACF_AUTOSUGGEST_FORCE_ON);

			m_AutoCompleteFilePatterns.Load(_T("Software\\grepWin\\History"), _T("FilePattern"));
			m_AutoCompleteFilePatterns.Init(GetDlgItem(hwndDlg, IDC_PATTERN));
			m_AutoCompleteSearchPatterns.Load(_T("Software\\grepWin\\History"), _T("SearchPattern"));
			m_AutoCompleteSearchPatterns.Init(GetDlgItem(hwndDlg, IDC_SEARCHTEXT));
			m_AutoCompleteReplacePatterns.Load(_T("Software\\grepWin\\History"), _T("ReplacePattern"));
			m_AutoCompleteReplacePatterns.Init(GetDlgItem(hwndDlg, IDC_REPLACETEXT));

			// add an "About" entry to the system menu
			HMENU hSysMenu = GetSystemMenu(hwndDlg, FALSE);
			if (hSysMenu)
			{
				AppendMenu(hSysMenu, MF_SEPARATOR, NULL, NULL);
				AppendMenu(hSysMenu, MF_STRING, ID_ABOUTBOX, _T("&About grepWin..."));
			}

			CheckRadioButton(hwndDlg, IDC_REGEXRADIO, IDC_TEXTRADIO, IDC_REGEXRADIO);
			CheckRadioButton(hwndDlg, IDC_ALLSIZERADIO, IDC_SIZERADIO, IDC_SIZERADIO);
			CheckRadioButton(hwndDlg, IDC_FILEPATTERNREGEX, IDC_FILEPATTERNTEXT, IDC_FILEPATTERNTEXT);
			SetDlgItemText(hwndDlg, IDC_SIZEEDIT, _T("2000"));
			SendDlgItemMessage(hwndDlg, IDC_SIZECOMBO, CB_INSERTSTRING, (WPARAM)-1, (LPARAM)_T("less than"));
			SendDlgItemMessage(hwndDlg, IDC_SIZECOMBO, CB_INSERTSTRING, (WPARAM)-1, (LPARAM)_T("equal to"));
			SendDlgItemMessage(hwndDlg, IDC_SIZECOMBO, CB_INSERTSTRING, (WPARAM)-1, (LPARAM)_T("greater than"));
			SendDlgItemMessage(hwndDlg, IDC_SIZECOMBO, CB_SETCURSEL, 0, 0);
			SendDlgItemMessage(hwndDlg, IDC_INCLUDESUBFOLDERS, BM_SETCHECK, BST_CHECKED, 0);
			SendDlgItemMessage(hwndDlg, IDC_CREATEBACKUP, BM_SETCHECK, BST_CHECKED, 0);
			EnableWindow(GetDlgItem(*this, IDC_ADDTOBOOKMARKS), FALSE);

			SetFocus(GetDlgItem(hwndDlg, IDC_SEARCHTEXT));

			m_resizer.Init(hwndDlg);
			m_resizer.AddControl(hwndDlg, IDC_GROUPSEARCHIN, RESIZER_TOPLEFTRIGHT);
			m_resizer.AddControl(hwndDlg, IDC_SEARCHPATH, RESIZER_TOPLEFTRIGHT);
			m_resizer.AddControl(hwndDlg, IDC_SEARCHPATHBROWSE, RESIZER_TOPRIGHT);
			m_resizer.AddControl(hwndDlg, IDC_GROUPSEARCHFOR, RESIZER_TOPLEFTRIGHT);
			m_resizer.AddControl(hwndDlg, IDC_REGEXRADIO, RESIZER_TOPLEFT);
			m_resizer.AddControl(hwndDlg, IDC_TEXTRADIO, RESIZER_TOPLEFT);
			m_resizer.AddControl(hwndDlg, IDC_SEARCHFORLABEL, RESIZER_TOPLEFT);
			m_resizer.AddControl(hwndDlg, IDC_SEARCHTEXT, RESIZER_TOPLEFTRIGHT);
			m_resizer.AddControl(hwndDlg, IDC_REPLACEWITHLABEL, RESIZER_TOPLEFT);
			m_resizer.AddControl(hwndDlg, IDC_REPLACETEXT, RESIZER_TOPLEFTRIGHT);
			m_resizer.AddControl(hwndDlg, IDC_CASE_SENSITIVE, RESIZER_TOPLEFT);
			m_resizer.AddControl(hwndDlg, IDC_REGEXOKLABEL, RESIZER_TOPRIGHT);
			m_resizer.AddControl(hwndDlg, IDC_CREATEBACKUP, RESIZER_TOPLEFT);
			m_resizer.AddControl(hwndDlg, IDC_TESTREGEX, RESIZER_TOPLEFT);
			m_resizer.AddControl(hwndDlg, IDC_ADDTOBOOKMARKS, RESIZER_TOPRIGHT);
			m_resizer.AddControl(hwndDlg, IDC_BOOKMARKS, RESIZER_TOPRIGHT);
			m_resizer.AddControl(hwndDlg, IDC_GROUPLIMITSEARCH, RESIZER_TOPLEFTRIGHT);
			m_resizer.AddControl(hwndDlg, IDC_ALLSIZERADIO, RESIZER_TOPLEFT);
			m_resizer.AddControl(hwndDlg, IDC_SIZERADIO, RESIZER_TOPLEFT);
			m_resizer.AddControl(hwndDlg, IDC_SIZECOMBO, RESIZER_TOPLEFT);
			m_resizer.AddControl(hwndDlg, IDC_SIZEEDIT, RESIZER_TOPLEFT);
			m_resizer.AddControl(hwndDlg, IDC_KBTEXT, RESIZER_TOPLEFT);
			m_resizer.AddControl(hwndDlg, IDC_INCLUDESYSTEM, RESIZER_TOPLEFT);
			m_resizer.AddControl(hwndDlg, IDC_INCLUDEHIDDEN, RESIZER_TOPLEFT);
			m_resizer.AddControl(hwndDlg, IDC_INCLUDESUBFOLDERS, RESIZER_TOPLEFT);
			m_resizer.AddControl(hwndDlg, IDC_PATTERNLABEL, RESIZER_TOPLEFT);
			m_resizer.AddControl(hwndDlg, IDC_PATTERN, RESIZER_TOPLEFTRIGHT);
			m_resizer.AddControl(hwndDlg, IDC_FILEPATTERNREGEX, RESIZER_TOPLEFT);
			m_resizer.AddControl(hwndDlg, IDC_FILEPATTERNTEXT, RESIZER_TOPLEFT);
			m_resizer.AddControl(hwndDlg, IDOK, RESIZER_TOPRIGHT);
			m_resizer.AddControl(hwndDlg, IDC_GROUPSEARCHRESULTS, RESIZER_TOPLEFTBOTTOMRIGHT);
			m_resizer.AddControl(hwndDlg, IDC_RESULTLIST, RESIZER_TOPLEFTBOTTOMRIGHT);
			m_resizer.AddControl(hwndDlg, IDC_SEARCHINFOLABEL, RESIZER_BOTTOMLEFTRIGHT);
		}
		return FALSE;
	case WM_COMMAND:
		return DoCommand(LOWORD(wParam), HIWORD(wParam));
	case WM_CONTEXTMENU:
		{
			if (HWND(wParam) == GetDlgItem(*this, IDC_RESULTLIST))
			{
				ShowContextMenu(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			}
		}
		break;
	case WM_NOTIFY:
		{
			if (wParam == IDC_RESULTLIST)
			{
				DoListNotify((LPNMITEMACTIVATE)lParam);
			}
		}
		break;
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
	case WM_SETCURSOR:
		{
			if (m_dwThreadRunning)
			{
				SetCursor(LoadCursor(NULL, IDC_WAIT));
				return TRUE;
			}
			return FALSE;
		}
		break;
	case SEARCH_START:
		{
			m_totalitems = 0;
			m_searchedItems = 0;
			UpdateInfoLabel();
		}
		break;
	case SEARCH_FOUND:
		if ((wParam != 0)||(m_searchString.empty()))
			AddFoundEntry((CSearchInfo*)lParam);
		UpdateInfoLabel();
		break;
	case SEARCH_PROGRESS:
		{
			if (wParam)
				m_searchedItems++;
			m_totalitems++;
			UpdateInfoLabel();
		}
		break;
	case SEARCH_END:
		{
			HWND hListControl = GetDlgItem(*this, IDC_RESULTLIST);
			ListView_SetColumnWidth(hListControl, 0, LVSCW_AUTOSIZE_USEHEADER);
			ListView_SetColumnWidth(hListControl, 1, LVSCW_AUTOSIZE_USEHEADER);
			ListView_SetColumnWidth(hListControl, 2, LVSCW_AUTOSIZE_USEHEADER);
			ListView_SetColumnWidth(hListControl, 3, LVSCW_AUTOSIZE_USEHEADER);
			UpdateInfoLabel();

			TCHAR buf[4] = {0};
			GetDlgItemText(*this, IDC_REPLACETEXT, buf, 4);
			bool bValid = (_tcslen(buf)>0);
			if (bValid)
			{
				::SetDlgItemText(*this, IDOK, _T("&Replace"));
			}
			else
			{
				::SetDlgItemText(*this, IDOK, _T("&Search"));
			}
		}
		break;
	case WM_HELP:
		{
			CInfoDlg::ShowDialog(IDR_INFODLG, hResource);
		}
		break;
	case WM_SYSCOMMAND:
		{
			if ((wParam & 0xFFF0) == ID_ABOUTBOX)
			{
				CAboutDlg dlgAbout(*this);
				dlgAbout.DoModal(hResource, IDD_ABOUT, *this);
			}
		}
		break;
	case WM_COPYDATA:
		{
			if (lParam)
			{
				PCOPYDATASTRUCT pCopyData = (PCOPYDATASTRUCT)lParam;
				wstring newpath = wstring((LPCTSTR)pCopyData->lpData, pCopyData->cbData/sizeof(wchar_t));
				if (!newpath.empty())
				{
					m_searchpath += _T("|");
					m_searchpath += newpath;
					SetDlgItemText(hwndDlg, IDC_SEARCHPATH, m_searchpath.c_str());
				}
			}
			return TRUE;
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
			if (m_dwThreadRunning)
			{
				InterlockedExchange(&m_Cancelled, TRUE);
			}
			else
			{
				// get all the information we need from the dialog
				TCHAR buf[MAX_PATH*4] = {0};
				GetDlgItemText(*this, IDC_SEARCHPATH, buf, MAX_PATH*4);
				m_searchpath = buf;
				GetDlgItemText(*this, IDC_SEARCHTEXT, buf, MAX_PATH*4);
				m_searchString = buf;
				GetDlgItemText(*this, IDC_REPLACETEXT, buf, MAX_PATH*4);
				m_replaceString = buf;
				GetDlgItemText(*this, IDC_PATTERN, buf, MAX_PATH*4);
				m_patternregex = buf;
				// split the pattern string into single patterns and
				// add them to an array
				TCHAR * pBuf = buf;
				size_t pos = 0;
				m_patterns.clear();
				do 
				{
					pos = _tcscspn(pBuf, _T(",; "));
					wstring s = wstring(pBuf, pos);
					if (!s.empty())
					{
						std::transform(s.begin(), s.end(), s.begin(), (int(*)(int)) std::tolower);
						m_patterns.push_back(s);
					}
					pBuf += pos;
					pBuf++;
				} while(*pBuf && (*(pBuf-1)));

				if (m_searchpath.empty())
					break;

				m_bUseRegex = (IsDlgButtonChecked(*this, IDC_REGEXRADIO) == BST_CHECKED);
				if (m_bUseRegex)
				{
					// check if the regex is valid before doing the search
					bool bValid = true;
					try
					{
						wregex expression = wregex(m_searchString);
					}
					catch (const exception&)
					{
						bValid = false;
					}
					if ((!bValid)&&(!m_searchString.empty()))
						break;
				}
				m_bUseRegexForPaths = (IsDlgButtonChecked(*this, IDC_FILEPATTERNREGEX) == BST_CHECKED);
				if (m_bUseRegexForPaths)
				{
					// check if the regex is valid before doing the search
					bool bValid = true;
					try
					{
						wregex expression = wregex(m_patternregex);
					}
					catch (const exception&)
					{
						bValid = false;
					}
					if (!bValid)
						break;
				}

				m_bAllSize = (IsDlgButtonChecked(*this, IDC_ALLSIZERADIO) == BST_CHECKED);
				m_lSize = 0;
				m_sizeCmp = 0;
				if (!m_bAllSize)
				{
					GetDlgItemText(*this, IDC_SIZEEDIT, buf, MAX_PATH*4);
					m_lSize = _tstol(buf);
					m_lSize *= 1024;
					m_sizeCmp = SendDlgItemMessage(*this, IDC_SIZECOMBO, CB_GETCURSEL, 0, 0);
				}
				m_bIncludeSystem = (IsDlgButtonChecked(*this, IDC_INCLUDESYSTEM) == BST_CHECKED);
				m_bIncludeHidden = (IsDlgButtonChecked(*this, IDC_INCLUDEHIDDEN) == BST_CHECKED);
				m_bIncludeSubfolders = (IsDlgButtonChecked(*this, IDC_INCLUDESUBFOLDERS) == BST_CHECKED);
				m_bCreateBackup = (IsDlgButtonChecked(*this, IDC_CREATEBACKUP) == BST_CHECKED);
				m_bCaseSensitive = (IsDlgButtonChecked(*this, IDC_CASE_SENSITIVE) == BST_CHECKED);

				m_searchedItems = 0;
				m_totalitems = 0;

				InitResultList();

				m_AutoCompleteFilePatterns.AddEntry(m_patternregex.c_str());
				m_AutoCompleteSearchPatterns.AddEntry(m_searchString.c_str());
				m_AutoCompleteReplacePatterns.AddEntry(m_replaceString.c_str());


				InterlockedExchange(&m_dwThreadRunning, TRUE);
				InterlockedExchange(&m_Cancelled, FALSE);
				SetDlgItemText(*this, IDOK, _T("&Cancel"));
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
		}
		break;
	case IDCANCEL:
		if (m_dwThreadRunning)
			InterlockedExchange(&m_Cancelled, TRUE);
		else
		{
			m_AutoCompleteFilePatterns.Save();
			m_AutoCompleteSearchPatterns.Save();
			m_AutoCompleteReplacePatterns.Save();
			EndDialog(*this, id);
		}
		break;
	case IDC_TESTREGEX:
		{
			// get all the information we need from the dialog
			TCHAR buf[MAX_PATH*4] = {0};
			GetDlgItemText(*this, IDC_SEARCHTEXT, buf, MAX_PATH*4);
			m_searchString = buf;
			GetDlgItemText(*this, IDC_REPLACETEXT, buf, MAX_PATH*4);
			m_replaceString = buf;

			CRegexTestDlg dlg(*this);
			dlg.SetStrings(m_searchString, m_replaceString);
			if (dlg.DoModal(hResource, IDD_REGEXTEST, *this) == IDOK)
			{
				m_searchString = dlg.GetSearchString();
				m_replaceString = dlg.GetReplaceString();
				SetDlgItemText(*this, IDC_SEARCHTEXT, m_searchString.c_str());
				SetDlgItemText(*this, IDC_REPLACETEXT, m_replaceString.c_str());
			}
		}
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
				TCHAR buf[MAX_PATH*4] = {0};
				GetDlgItemText(*this, IDC_SEARCHTEXT, buf, MAX_PATH*4);
				int len = _tcslen(buf);
				if (IsDlgButtonChecked(*this, IDC_REGEXRADIO) == BST_CHECKED)
				{
					// check if the regex is valid
					bool bValid = true;
					if (len)
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
					if (len)
					{
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
				else
				{
					SetDlgItemText(*this, IDC_REGEXOKLABEL, _T(""));
				}
				EnableWindow(GetDlgItem(*this, IDC_ADDTOBOOKMARKS), len > 0);				
			}
		}
		break;
	case IDC_REPLACETEXT:
		{
			if (msg == EN_CHANGE)
			{
				TCHAR buf[MAX_PATH*4] = {0};
				GetDlgItemText(*this, IDC_REPLACETEXT, buf, MAX_PATH*4);
				bool bValid = (_tcslen(buf)>0);
				if ((bValid)&&(!m_dwThreadRunning))
				{
					::SetDlgItemText(*this, IDOK, _T("&Replace"));
				}
				else
				{
					::SetDlgItemText(*this, IDOK, _T("&Search"));
				}
				::EnableWindow(GetDlgItem(*this, IDC_CREATEBACKUP), bValid);
			}
		}
		break;
	case IDC_SIZEEDIT:
		{
			if (msg == EN_CHANGE)
			{
				TCHAR buf[10] = {0};
				GetDlgItemText(*this, IDC_SIZEEDIT, buf, 10);
				if (_tcslen(buf))
				{
					if (IsDlgButtonChecked(*this, IDC_ALLSIZERADIO) == BST_CHECKED)
					{
						CheckRadioButton(*this, IDC_ALLSIZERADIO, IDC_SIZERADIO, IDC_SIZERADIO);
					}
				}
				else
				{
					if (IsDlgButtonChecked(*this, IDC_SIZERADIO) == BST_CHECKED)
					{
						CheckRadioButton(*this, IDC_ALLSIZERADIO, IDC_SIZERADIO, IDC_ALLSIZERADIO);
					}
				}
			}
		}
		break;
	case IDC_REGEXRADIO:
	case IDC_TEXTRADIO:
		{
			bool bText = (IsDlgButtonChecked(*this, IDC_TEXTRADIO) == BST_CHECKED);
			::EnableWindow(GetDlgItem(*this, IDC_REPLACETEXT), !bText);
			::EnableWindow(GetDlgItem(*this, IDC_CREATEBACKUP), !bText);
			if ((!bText)&&(!m_dwThreadRunning)&&(GetWindowTextLength(GetDlgItem(*this, IDC_REPLACETEXT))>0))
			{
				::SetDlgItemText(*this, IDOK, _T("&Replace"));
			}
			else
			{
				::SetDlgItemText(*this, IDOK, _T("&Search"));
			}
		}
		break;
	case IDC_ADDTOBOOKMARKS:
		{
			TCHAR buf[MAX_PATH*4] = {0};
			GetDlgItemText(*this, IDC_SEARCHTEXT, buf, MAX_PATH*4);
			m_searchString = buf;
			GetDlgItemText(*this, IDC_REPLACETEXT, buf, MAX_PATH*4);
			m_replaceString = buf;
			bool bUseRegex = (IsDlgButtonChecked(*this, IDC_REGEXRADIO) == BST_CHECKED);

			CNameDlg nameDlg(*this);
			if (nameDlg.DoModal(hResource, IDD_NAME, *this) == IDOK)
			{
				// add the bookmark
				CBookmarks bks;
				bks.Load();
				bks.AddBookmark(nameDlg.GetName(), m_searchString, m_replaceString, bUseRegex);
				bks.Save();
			}
		}
		break;
	case IDC_BOOKMARKS:
		{
			CBookmarksDlg dlg(*this);
			if (dlg.DoModal(hResource, IDD_BOOKMARKS, *this, IDC_GREPWIN) == IDOK)
			{
				m_searchString = dlg.GetSelectedSearchString();
				m_replaceString = dlg.GetSelectedReplaceString();
				m_bUseRegex = dlg.GetSelectedUseRegex();
				SetDlgItemText(*this, IDC_SEARCHTEXT, m_searchString.c_str());
				SetDlgItemText(*this, IDC_REPLACETEXT, m_replaceString.c_str());
				CheckRadioButton(*this, IDC_REGEXRADIO, IDC_TEXTRADIO, m_bUseRegex ? IDC_REGEXRADIO : IDC_TEXTRADIO);
			}
		}
		break;
	}
	return 1;
}

void CSearchDlg::UpdateInfoLabel()
{
	TCHAR buf[1024] = {0};
	_stprintf_s(buf, 1024, _T("Searched %ld files, skipped %ld files. Found %ld files which match the search parameters."),
		m_searchedItems, m_totalitems-m_searchedItems, m_items.size());
	SetDlgItemText(*this, IDC_SEARCHINFOLABEL, buf);
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
	lvc.pszText = _T("Encoding");
	ListView_InsertColumn(hListControl, 4, &lvc);

	ListView_SetColumnWidth(hListControl, 0, 300);
	ListView_SetColumnWidth(hListControl, 1, 50);
	ListView_SetColumnWidth(hListControl, 2, LVSCW_AUTOSIZE_USEHEADER);
	ListView_SetColumnWidth(hListControl, 3, LVSCW_AUTOSIZE_USEHEADER);
	ListView_SetColumnWidth(hListControl, 4, LVSCW_AUTOSIZE_USEHEADER);

	m_items.clear();

	return true;
}

bool CSearchDlg::AddFoundEntry(CSearchInfo * pInfo, bool bOnlyListControl)
{
	HWND hListControl = GetDlgItem(*this, IDC_RESULTLIST);
	LVITEM lv = {0};
	lv.mask = LVIF_TEXT | LVIF_IMAGE;
	TCHAR * pBuf = new TCHAR[pInfo->filepath.size()+1];
	wstring name = pInfo->filepath.substr(pInfo->filepath.find_last_of('\\')+1);
	_tcscpy_s(pBuf, pInfo->filepath.size()+1, name.c_str());
	lv.pszText = pBuf;
	lv.iImage = CSysImageList::GetInstance().GetFileIconIndex(pInfo->filepath);
	lv.iItem = ListView_GetItemCount(hListControl);
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
		_tcscpy_s(sb, MAX_PATH*4, pInfo->filepath.substr(0, pInfo->filepath.size()-name.size()-1).c_str());
		ListView_SetItem(hListControl, &lv);
		lv.iSubItem = 4;
		switch (pInfo->encoding)
		{
		case CTextFile::ANSI:
			_tcscpy_s(sb, MAX_PATH*4, _T("ANSI"));
			break;
		case CTextFile::UNICODE_LE:
			_tcscpy_s(sb, MAX_PATH*4, _T("UNICODE"));
			break;
		case CTextFile::UTF8:
			_tcscpy_s(sb, MAX_PATH*4, _T("UTF8"));
			break;
		case CTextFile::BINARY:
			_tcscpy_s(sb, MAX_PATH*4, _T("BINARY"));
			break;
		default:
			_tcscpy_s(sb, MAX_PATH*4, _T(""));
			break;
		}
		ListView_SetItem(hListControl, &lv);
	}
	if ((ret != -1)&&(!bOnlyListControl))
		m_items.push_back(*pInfo);

	return (ret != -1);
}

void CSearchDlg::ShowContextMenu(int x, int y)
{
	HWND hListControl = GetDlgItem(*this, IDC_RESULTLIST);
	int nCount = ListView_GetItemCount(hListControl);
	if (nCount == 0)
		return;
	CShellContextMenu shellMenu;
	int iItem = -1;
	vector<wstring> paths;
	while ((iItem = ListView_GetNextItem(hListControl, iItem, LVNI_SELECTED)) != (-1))
		paths.push_back(m_items[iItem].filepath);

	if (paths.size() == 0)
		return;

	shellMenu.SetObjects(paths);

	POINT pt = {x,y};
	if ((x==-1)&&(y==-1))
	{
		RECT rc;
		ListView_GetItemRect(hListControl, ListView_GetSelectionMark(hListControl), &rc, LVIR_LABEL);
		pt.x = (rc.right-rc.left)/2;
		pt.y = (rc.bottom-rc.top)/2;
		ClientToScreen(hListControl, &pt);
	}
	shellMenu.ShowContextMenu(hListControl, pt);
}

bool CSearchDlg::PreTranslateMessage(MSG* pMsg)
{
	if ((pMsg->message == WM_KEYDOWN)&&(pMsg->wParam == VK_RETURN))
	{
		HWND hListControl = GetDlgItem(*this, IDC_RESULTLIST);
		if (GetFocus() == hListControl)
		{
			int selItem = ListView_GetSelectionMark(hListControl);
			if ((selItem >= 0)&&(selItem < (int)m_items.size()))
			{
				SHELLEXECUTEINFO shExInfo = {0};
				shExInfo.cbSize = sizeof(SHELLEXECUTEINFO);
				shExInfo.fMask = SEE_MASK_UNICODE;
				shExInfo.hwnd = *this;
				shExInfo.lpFile = m_items[selItem].filepath.c_str();
				shExInfo.nShow = SW_SHOW;
				ShellExecuteEx(&shExInfo);
				return true;
			}
		}
	}
	return false;
}

void CSearchDlg::DoListNotify(LPNMITEMACTIVATE lpNMItemActivate)
{
	if (lpNMItemActivate->hdr.code == NM_DBLCLK)
	{
		if (lpNMItemActivate->iItem >= 0)
		{
			SHELLEXECUTEINFO shExInfo = {0};
			shExInfo.cbSize = sizeof(SHELLEXECUTEINFO);
			shExInfo.fMask = SEE_MASK_UNICODE;
			shExInfo.hwnd = *this;
			shExInfo.lpFile = m_items[lpNMItemActivate->iItem].filepath.c_str();
			shExInfo.nShow = SW_SHOW;
			ShellExecuteEx(&shExInfo);
		}
	}
	if (lpNMItemActivate->hdr.code == LVN_COLUMNCLICK)
	{
		m_bAscending = !m_bAscending;
		switch (lpNMItemActivate->iSubItem)
		{
		case 0:
			if (m_bAscending)
				sort(m_items.begin(), m_items.end(), NameCompareAsc);
			else
				sort(m_items.begin(), m_items.end(), NameCompareDesc);
			break;
		case 1:
			if (m_bAscending)
				sort(m_items.begin(), m_items.end(), SizeCompareAsc);
			else
				sort(m_items.begin(), m_items.end(), SizeCompareDesc);
			break;
		case 2:
			if (m_bAscending)
				sort(m_items.begin(), m_items.end(), MatchesCompareAsc);
			else
				sort(m_items.begin(), m_items.end(), MatchesCompareDesc);
			break;
		case 3:
			if (m_bAscending)
				sort(m_items.begin(), m_items.end(), PathCompareAsc);
			else
				sort(m_items.begin(), m_items.end(), PathCompareDesc);
			break;
		}

		HWND hListControl = GetDlgItem(*this, IDC_RESULTLIST);
		ListView_DeleteAllItems(hListControl);
		for (vector<CSearchInfo>::iterator it = m_items.begin(); it != m_items.end(); ++it)
		{
			AddFoundEntry(&(*it), true);
		}

		ListView_SetColumnWidth(hListControl, 0, LVSCW_AUTOSIZE_USEHEADER);
		ListView_SetColumnWidth(hListControl, 1, LVSCW_AUTOSIZE_USEHEADER);
		ListView_SetColumnWidth(hListControl, 2, LVSCW_AUTOSIZE_USEHEADER);
		ListView_SetColumnWidth(hListControl, 3, LVSCW_AUTOSIZE_USEHEADER);

		HDITEM hd = {0};
		hd.mask = HDI_FORMAT;
		HWND hHeader = ListView_GetHeader(hListControl);
		int iCount = Header_GetItemCount(hHeader);
		for (int i=0; i<iCount; ++i)
		{
			Header_GetItem(hHeader, i, &hd);
			hd.fmt &= ~(HDF_SORTDOWN | HDF_SORTUP);
			Header_SetItem(hHeader, i, &hd);
		}
		Header_GetItem(hHeader, lpNMItemActivate->iSubItem, &hd);
		hd.fmt |= (m_bAscending ? HDF_SORTUP : HDF_SORTDOWN);
		Header_SetItem(hHeader, lpNMItemActivate->iSubItem, &hd);

	}
}

bool CSearchDlg::NameCompareAsc(const CSearchInfo Entry1, const CSearchInfo Entry2)
{
	wstring name1 = Entry1.filepath.substr(Entry1.filepath.find_last_of('\\')+1);
	wstring name2 = Entry2.filepath.substr(Entry2.filepath.find_last_of('\\')+1);
	return name1.compare(name2) < 0;
}

bool CSearchDlg::SizeCompareAsc(const CSearchInfo Entry1, const CSearchInfo Entry2)
{
	return Entry1.filesize < Entry2.filesize;
}

bool CSearchDlg::MatchesCompareAsc(const CSearchInfo Entry1, const CSearchInfo Entry2)
{
	return Entry1.matchends.size() < Entry2.matchends.size();
}

bool CSearchDlg::PathCompareAsc(const CSearchInfo Entry1, const CSearchInfo Entry2)
{
	return Entry1.filepath.compare(Entry2.filepath) < 0;
}

bool CSearchDlg::NameCompareDesc(const CSearchInfo Entry1, const CSearchInfo Entry2)
{
	wstring name1 = Entry1.filepath.substr(Entry1.filepath.find_last_of('\\')+1);
	wstring name2 = Entry2.filepath.substr(Entry2.filepath.find_last_of('\\')+1);
	return name1.compare(name2) > 0;
}

bool CSearchDlg::SizeCompareDesc(const CSearchInfo Entry1, const CSearchInfo Entry2)
{
	return Entry1.filesize > Entry2.filesize;
}

bool CSearchDlg::MatchesCompareDesc(const CSearchInfo Entry1, const CSearchInfo Entry2)
{
	return Entry1.matchends.size() > Entry2.matchends.size();
}

bool CSearchDlg::PathCompareDesc(const CSearchInfo Entry1, const CSearchInfo Entry2)
{
	return Entry1.filepath.compare(Entry2.filepath) > 0;
}

DWORD CSearchDlg::SearchThread()
{
	TCHAR pathbuf[MAX_PATH*4] = {0};

	// split the path string into single paths and
	// add them to an array
	const TCHAR * pBuf = m_searchpath.c_str();
	size_t pos = 0;
	vector<wstring> pathvector;
	do 
	{
		pos = _tcscspn(pBuf, _T("|"));
		wstring s = wstring(pBuf, pos);
		if (!s.empty())
		{
			pathvector.push_back(s);
		}
		pBuf += pos;
		pBuf++;
	} while(*pBuf && (*(pBuf-1)));

	for (vector<wstring>::const_iterator it = pathvector.begin(); it != pathvector.end(); ++it)
	{
		wstring searchpath = *it;
		if (!searchpath.empty())
		{
			bool bIsDirectory = false;
			CDirFileEnum fileEnumerator(searchpath.c_str());
			bool bRecurse = m_bIncludeSubfolders;

			SendMessage(*this, SEARCH_START, 0, 0);
			while ((fileEnumerator.NextFile(pathbuf, bRecurse, &bIsDirectory))&&(!m_Cancelled))
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
					bRecurse = ((m_bIncludeSubfolders)&&(bSearch));
					bool bPattern = false;
                    // find start of pathname
                    const TCHAR * pName = _tcsrchr(pathbuf, '\\');
                    if (pName == NULL)
                        pName = pathbuf;
                    else
                        pName++;    // skip the last '\\' char
					if (m_bUseRegexForPaths)
					{
						try
						{
							wregex expression = wregex(m_patternregex, regex::normal|regbase::icase);
							wcmatch whatc;
							if (regex_match(pName, whatc, expression))
							{
								bPattern = true;
							}
						}
						catch (const exception&)
						{

						}
					}
					else
					{
						if (m_patterns.size())
						{
							wstring fname = pName;
							std::transform(fname.begin(), fname.end(), fname.begin(), (int(*)(int)) std::tolower);

							for (vector<wstring>::const_iterator it = m_patterns.begin(); it != m_patterns.end(); ++it)
								bPattern = bPattern || wcswildcmp(it->c_str(), fname.c_str());
						}
						else
							bPattern = true;
					}

					int nFound = -1;
					if (bSearch && bPattern)
					{
						CSearchInfo sinfo(pathbuf);
						sinfo.filesize = pFindData->nFileSizeLow;
						if (m_searchString.empty())
							SendMessage(*this, SEARCH_FOUND, 0, (LPARAM)&sinfo);
						else
						{
							nFound = SearchFile(sinfo, m_bUseRegex, m_bCaseSensitive, m_searchString);
							if (nFound >= 0)
								SendMessage(*this, SEARCH_FOUND, nFound, (LPARAM)&sinfo);
						}
					}
					SendMessage(*this, SEARCH_PROGRESS, bSearch && bPattern && (nFound >= 0), 0);
				}
				else
				{
					const WIN32_FIND_DATA * pFindData = fileEnumerator.GetFileInfo();
					bool bSearch = ((m_bIncludeHidden)||((pFindData->dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) == 0));
					bSearch = bSearch && ((m_bIncludeSystem)||((pFindData->dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) == 0));
					bRecurse = ((bIsDirectory)&&(m_bIncludeSubfolders)&&(bSearch));
				}
			}
		}
	}
	SendMessage(*this, SEARCH_END, 0, 0);
	InterlockedExchange(&m_dwThreadRunning, FALSE);

	// refresh cursor
	POINT pt;
	GetCursorPos(&pt);
	SetCursorPos(pt.x, pt.y);

	return 0L;
}

int CSearchDlg::SearchFile(CSearchInfo& sinfo, bool bUseRegex, bool bCaseSensitive, const wstring& searchString)
{
	int nFound = 0;
	// we keep it simple:
	// files bigger than 10MB are considered binary. Binary files are searched
	// as if they're ANSI text files.
	wstring localSearchString = searchString;
	if (!bUseRegex)
		localSearchString = _T("\\Q") + searchString + _T("\\E");
	if (sinfo.filesize < 10*1024*1024)
	{
		CTextFile textfile;

		CTextFile::UnicodeType type = CTextFile::AUTOTYPE;
		if (textfile.Load(sinfo.filepath.c_str(), type))
		{
			sinfo.encoding = type;
			wstring::const_iterator start, end;
			start = textfile.GetFileString().begin();
			end = textfile.GetFileString().end();
			match_results<wstring::const_iterator> what;
			try
			{
				int ft = regex::normal;
				if (!bCaseSensitive)
					ft |= regbase::icase;
				wregex expression = wregex(localSearchString, ft);
				match_results<wstring::const_iterator> whatc;
				if (m_replaceString.empty())
				{
					match_flag_type flags = match_default | match_not_dot_newline;
					while (regex_search(start, end, whatc, expression, flags))   
					{
						if (whatc[0].matched)
						{
							nFound++;
							sinfo.matchstarts.push_back(whatc[0].first-textfile.GetFileString().begin());
							sinfo.matchends.push_back(whatc[0].second-textfile.GetFileString().begin());
						}
						// update search position:
						start = whatc[0].second;      
						// update flags:
						flags |= match_prev_avail;
						flags |= match_not_bob;
					}
				}
				else
				{
					match_flag_type flags = match_default | format_all | match_not_dot_newline;
					wstring replaced = regex_replace(textfile.GetFileString(), expression, m_replaceString, flags);
					if (replaced.compare(textfile.GetFileString()))
					{
						nFound++;
						sinfo.matchstarts.push_back(0);
						sinfo.matchends.push_back(0);
						textfile.SetFileContent(replaced);
						if (m_bCreateBackup)
						{
							wstring backupfile = sinfo.filepath + _T(".bak");
							CopyFile(sinfo.filepath.c_str(), backupfile.c_str(), FALSE);
						}
						textfile.Save(sinfo.filepath.c_str());
					}
				}
			}
			catch (const exception&)
			{
				return -1;
			}
		}
		else
			return -1;
	}
	else
	{
		// assume binary file

		sinfo.encoding = CTextFile::BINARY;
		string filepath = CUnicodeUtils::StdGetANSI(sinfo.filepath);
		string searchfor = CUnicodeUtils::StdGetUTF8(searchString);

		if (!bUseRegex)
		{
			searchfor = "\\Q";
			searchfor += CUnicodeUtils::StdGetUTF8(searchString);
			searchfor += "\\E";
		}
		spirit::file_iterator<> start(filepath.c_str());
		spirit::file_iterator<> fbeg = start;
		spirit::file_iterator<> end = start.make_end();

		match_results<string::const_iterator> what;
		match_flag_type flags = match_default | match_not_dot_newline;
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
			return -1;
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
