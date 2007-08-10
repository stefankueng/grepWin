#include "StdAfx.h"
#include "Bookmarks.h"
#include <shlobj.h>

CBookmarks::CBookmarks(void)
{
}

CBookmarks::~CBookmarks(void)
{
}

void CBookmarks::Load()
{
	TCHAR path[MAX_PATH] = {0};
	SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, path);
	m_iniPath = path;
	m_iniPath += _T("\\grepWin");
	CreateDirectory(m_iniPath.c_str(), NULL);
	m_iniPath += _T("\\bookmarks");
	LoadFile(m_iniPath.c_str());
}

void CBookmarks::Save()
{
	TCHAR path[MAX_PATH] = {0};
	SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, path);
	m_iniPath = path;
	m_iniPath += _T("\\grepWin");
	CreateDirectory(m_iniPath.c_str(), NULL);
	m_iniPath += _T("\\bookmarks");
	FILE * pFile = NULL;
	_tfopen_s(&pFile, m_iniPath.c_str(), _T("wb"));
	SaveFile(pFile);
	fclose(pFile);
}

void CBookmarks::AddBookmark(const wstring& name, const wstring& search, const wstring& replace)
{
	SetValue(name.c_str(), _T("searchString"), search.c_str());
	SetValue(name.c_str(), _T("replaceString"), replace.c_str());
}

void CBookmarks::RemoveBookmark(const wstring& name)
{
	Delete(name.c_str(), _T("searchString"), true);
	Delete(name.c_str(), _T("replaceString"), true);
}

