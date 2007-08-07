#pragma once
#include <string>
#include <vector>

using namespace std;

class CSearchInfo
{
public:
	CSearchInfo(void);
	CSearchInfo(const wstring& path) : filepath(path) {};
	~CSearchInfo(void);

	wstring				filepath;
	DWORD				filesize;
	vector<wstring>		matches;	
};
