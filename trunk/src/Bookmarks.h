#pragma once
#include "SimpleIni.h"
#include <string>
#include <vector>

using namespace std;

class CBookmarks : public CSimpleIni
{
public:
	CBookmarks(void);
	~CBookmarks(void);

	void				Load();
	void				Save();
	void				AddBookmark(const wstring& name, const wstring& search, const wstring& replace, bool bRegex);
	void				RemoveBookmark(const wstring& name);

protected:
	wstring				m_iniPath;
	TNamesDepend		m_sections;
};
