#pragma once
#include <string>
#include <vector>
#include <ShlDisp.h>
#include <ShlGuid.h>

#include "RegHistory.h"

using namespace std;

/**
* Helper class for the CAutoComplete class: implements the string enumerator.
*/
class CAutoCompleteEnum : public IEnumString
{
public:
	CAutoCompleteEnum(const vector<wstring*>& vec);
	CAutoCompleteEnum(const vector<wstring>& vec);
	//IUnknown members
	STDMETHOD(QueryInterface)(REFIID, void**);
	STDMETHOD_(ULONG, AddRef)(void);
	STDMETHOD_(ULONG, Release)(void);

	//IEnumString members
	STDMETHOD(Next)(ULONG, LPOLESTR*, ULONG*);
	STDMETHOD(Skip)(ULONG);
	STDMETHOD(Reset)(void);
	STDMETHOD(Clone)(IEnumString**);

private:
	vector<wstring>				m_vecStrings;
	ULONG						m_cRefCount;
	size_t						m_iCur;
};



class CAutoComplete : public CRegHistory
{
public:
	CAutoComplete(void);
	~CAutoComplete(void);

	bool		Init(HWND hEdit);
	bool		Enable(bool bEnable);
private:
	CAutoCompleteEnum *			m_pcacs;
	IAutoComplete2 *			m_pac;

};
