#pragma once
#include <string>
#include <vector>
#include "TextFile.h"

using namespace std;


class CSearchInfo
{
public:
	CSearchInfo(void);
	CSearchInfo(const wstring& path) : filepath(path) {};
	~CSearchInfo(void);

	wstring				filepath;
	DWORD				filesize;
	vector<DWORD>		matchstarts;	
	vector<DWORD>		matchends;
	CTextFile::UnicodeType	encoding;
};
