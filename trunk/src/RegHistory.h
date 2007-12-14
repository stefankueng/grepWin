#pragma once
#include <string>
#include <vector>


/**
 * Maintains a list of X string items in the registry and provides methods
 * to add new items. The list can be used as a 'recently used' or 'recent items' list.
 */
class CRegHistory
{
public:
	CRegHistory();
	virtual ~CRegHistory();

	/// Loads the history
	/// \param lpszSection the section in the registry, e.g., "Software\\CompanyName\\History"
	/// \param lpszKeyPrefix the name of the registry values, e.g., "historyItem"
	/// \return the number of history items loaded
	int Load(LPCTSTR lpszSection, LPCTSTR lpszKeyPrefix);
	/// Saves the history.
	bool Save() const;
	/// Adds a new string to the history list.
	bool AddEntry(LPCTSTR szText);
	/// Removes the entry at index \c pos.
	void RemoveEntry(int pos);
	/// Sets the maximum number of items in the history. Default is 25.
	void SetMaxHistoryItems(int nMax) {m_nMaxHistoryItems = nMax;}
	/// Returns the number of items in the history.
	size_t GetCount() const {return m_arEntries.size(); }
	/// Returns the entry at index \c pos
	LPCTSTR GetEntry(int pos) {return m_arEntries[pos].c_str();}

protected:
	std::wstring m_sSection;
	std::wstring m_sKeyPrefix;
	std::vector<std::wstring> m_arEntries;
	int m_nMaxHistoryItems;
};
