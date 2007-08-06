#include "stdafx.h"
#include "registry.h"

#ifdef _MFC_VER
//MFC is available - also use the MFC-based classes	

CRegDWORD::CRegDWORD(void)
{
	m_value = 0;
	m_defaultvalue = 0;
	m_key = "";
	m_base = HKEY_CURRENT_USER;
	m_read = FALSE;
	m_force = FALSE;
	LastError = ERROR_SUCCESS;
}

/**
 * Constructor.
 * @param key the path to the key, including the key. example: "Software\\Company\\SubKey\\MyValue"
 * @param def the default value used when the key does not exist or a read error occured
 * @param force set to TRUE if no cache should be used, i.e. always read and write directly from/to registry
 * @param base a predefined base key like HKEY_LOCAL_MACHINE. see the SDK documentation for more information.
 */
CRegDWORD::CRegDWORD(const CString& key, DWORD def, BOOL force, HKEY base)
{
	m_value = 0;
	m_defaultvalue = def;
	m_force = force;
	m_base = base;
	m_read = FALSE;
	m_key = key;
	m_key.TrimLeft(_T("\\"));
	int backslashpos = m_key.ReverseFind('\\');
	m_path = m_key.Left(backslashpos);
	m_path.TrimRight(_T("\\"));
	m_key = m_key.Mid(backslashpos);
	m_key.Trim(_T("\\"));
	read();
	LastError = ERROR_SUCCESS;
}

CRegDWORD::~CRegDWORD(void)
{
	if (m_hKey)
		RegCloseKey(m_hKey);
}

DWORD	CRegDWORD::read()
{
	if ((LastError = RegOpenKeyEx(m_base, m_path, 0, KEY_EXECUTE, &m_hKey))==ERROR_SUCCESS)
	{
		int size = sizeof(m_value);
		DWORD type;
		if ((LastError = RegQueryValueEx(m_hKey, m_key, NULL, &type, (BYTE*) &m_value,(LPDWORD) &size))==ERROR_SUCCESS)
		{
			ASSERT(type==REG_DWORD);
			m_read = TRUE;
			LastError = RegCloseKey(m_hKey);
			m_hKey = NULL;
			return m_value;
		}
		else
		{
			LastError = RegCloseKey(m_hKey);
			m_hKey = NULL;
			m_value = m_defaultvalue;
			m_read = TRUE;
			return m_defaultvalue;
		}
	}
	m_value = m_defaultvalue;
	return m_defaultvalue;
}

void CRegDWORD::write()
{
	DWORD disp;
	if ((LastError = RegCreateKeyEx(m_base, m_path, 0, _T(""), REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &m_hKey, &disp))!=ERROR_SUCCESS)
	{
		return;
	}
	if ((LastError = RegSetValueEx(m_hKey, m_key, 0, REG_DWORD,(const BYTE*) &m_value, sizeof(m_value)))==ERROR_SUCCESS)
	{
		m_read = TRUE;
		m_hKey = NULL;
		return;
	}
	LastError = RegCloseKey(m_hKey);
	m_hKey = NULL;
}


CRegDWORD::operator DWORD()
{
	if ((m_read)&&(!m_force))
	{
		LastError = 0;
		return m_value;
	}
	else
	{
		return read();
	}
}

CRegDWORD& CRegDWORD::operator =(DWORD d)
{
	if ((d==m_value)&&(!m_force))
	{
		//no write to the registry required, its the same value
		LastError = 0;
		return *this;
	}
	m_value = d;
	write();
	return *this;
}

//////////////////////////////////////////////////////////////////////////////////////////////

CRegString::CRegString(void)
{
	m_value = _T("");
	m_defaultvalue = _T("");
	m_key = _T("");
	m_base = HKEY_CURRENT_USER;
	m_read = FALSE;
	m_force = FALSE;
	LastError = ERROR_SUCCESS;
}

/**
 * Constructor.
 * @param key the path to the key, including the key. example: "Software\\Company\\SubKey\\MyValue"
 * @param def the default value used when the key does not exist or a read error occured
 * @param force set to TRUE if no cache should be used, i.e. always read and write directly from/to registry
 * @param base a predefined base key like HKEY_LOCAL_MACHINE. see the SDK documentation for more information.
 */
CRegString::CRegString(const CString& key, const CString& def, BOOL force, HKEY base)
{
	m_value = "";
	m_defaultvalue = def;
	m_force = force;
	m_base = base;
	m_read = FALSE;
	m_key = key;
	m_key.TrimLeft(_T("\\"));
	int backslashpos = m_key.ReverseFind('\\');
	m_path = m_key.Left(backslashpos);
	m_path.TrimRight(_T("\\"));
	m_key = m_key.Mid(backslashpos);
	m_key.Trim(_T("\\"));
	read();
	LastError = ERROR_SUCCESS;
}

CRegString::~CRegString(void)
{
	if (m_hKey)
		RegCloseKey(m_hKey);
}

CString	CRegString::read()
{
	if ((LastError = RegOpenKeyEx(m_base, m_path, 0, KEY_EXECUTE, &m_hKey))==ERROR_SUCCESS)
	{
		int size = 0;
		DWORD type;
		LastError = RegQueryValueEx(m_hKey, m_key, NULL, &type, NULL, (LPDWORD) &size);
		TCHAR* pStr = new TCHAR[size];
		if ((LastError = RegQueryValueEx(m_hKey, m_key, NULL, &type, (BYTE*) pStr,(LPDWORD) &size))==ERROR_SUCCESS)
		{
			m_value = CString(pStr);
			delete [] pStr;
			ASSERT(type==REG_SZ || type==REG_EXPAND_SZ);
			m_read = TRUE;
			LastError = RegCloseKey(m_hKey);
			m_hKey = NULL;
			return m_value;
		}
		else
		{
			delete [] pStr;
			RegCloseKey(m_hKey);
			m_hKey = NULL;
			m_value = m_defaultvalue;
			m_read = TRUE;
			return m_defaultvalue;
		}
	}
	m_value = m_defaultvalue;
	return m_defaultvalue;
}

void CRegString::write()
{
	DWORD disp;
	if ((LastError = RegCreateKeyEx(m_base, m_path, 0, _T(""), REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &m_hKey, &disp))!=ERROR_SUCCESS)
	{
		return;
	}
#ifdef _UNICODE
	if ((LastError = RegSetValueEx(m_hKey, m_key, 0, REG_SZ, (BYTE *)(LPCTSTR)m_value, (m_value.GetLength()+1)*2))==ERROR_SUCCESS)
#else
	if ((LastError = RegSetValueEx(m_hKey, m_key, 0, REG_SZ, (BYTE *)(LPCTSTR)m_value, m_value.GetLength()+1))==ERROR_SUCCESS)
#endif
	{
		m_read = TRUE;
	}
	RegCloseKey(m_hKey);
	m_hKey = NULL;
}

CRegString::operator CString()
{
	if ((m_read)&&(!m_force))
	{
		LastError = 0;
		return m_value;
	}
	else
	{
		return read();
	}
}

CRegString& CRegString::operator =(const CString& s)
{
	if ((s==m_value)&&(!m_force))
	{
		//no write to the registry required, its the same value
		LastError = 0;
		return *this;
	}
	m_value = s;
	write();
	return *this;
}

//////////////////////////////////////////////////////////////////////////////////////////////

CRegRect::CRegRect(void)
{
	m_value = CRect(0,0,0,0);
	m_defaultvalue = CRect(0,0,0,0);
	m_key = _T("");
	m_base = HKEY_CURRENT_USER;
	m_read = FALSE;
	m_force = FALSE;
	LastError = ERROR_SUCCESS;
}

/**
 * Constructor.
 * @param key the path to the key, including the key. example: "Software\\Company\\SubKey\\MyValue"
 * @param def the default value used when the key does not exist or a read error occured
 * @param force set to TRUE if no cache should be used, i.e. always read and write directly from/to registry
 * @param base a predefined base key like HKEY_LOCAL_MACHINE. see the SDK documentation for more information.
 */
CRegRect::CRegRect(const CString& key, CRect def, BOOL force, HKEY base)
{
	m_value = CRect(0,0,0,0);
	m_defaultvalue = def;
	m_force = force;
	m_base = base;
	m_read = FALSE;
	m_key = key;
	m_key.TrimLeft(_T("\\"));
	int backslashpos = m_key.ReverseFind('\\');
	m_path = m_key.Left(backslashpos);
	m_path.TrimRight(_T("\\"));
	m_key = m_key.Mid(backslashpos);
	m_key.Trim(_T("\\"));
	read();
	LastError = ERROR_SUCCESS;
}

CRegRect::~CRegRect(void)
{
	if (m_hKey)
		RegCloseKey(m_hKey);
}

CRect	CRegRect::read()
{
	if ((LastError = RegOpenKeyEx(m_base, m_path, 0, KEY_EXECUTE, &m_hKey))==ERROR_SUCCESS)
	{
		int size = 0;
		DWORD type;
		RegQueryValueEx(m_hKey, m_key, NULL, &type, NULL, (LPDWORD) &size);
		LPRECT pRect = (LPRECT)new char[size];
		if ((LastError = RegQueryValueEx(m_hKey, m_key, NULL, &type, (BYTE*) pRect,(LPDWORD) &size))==ERROR_SUCCESS)
		{
			m_value = CRect(pRect);
			delete [] pRect;
			ASSERT(type==REG_BINARY);
			m_read = TRUE;
			LastError = RegCloseKey(m_hKey);
			m_hKey = NULL;
			return m_value;
		}
		else
		{
			delete [] pRect;
			RegCloseKey(m_hKey);
			m_hKey = NULL;
			m_value = m_defaultvalue;
			m_read = TRUE;
			return m_defaultvalue;
		}
	}
	m_value = m_defaultvalue;
	return m_defaultvalue;
}

void CRegRect::write()
{
	DWORD disp;
	if ((LastError = RegCreateKeyEx(m_base, m_path, 0, _T(""), REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &m_hKey, &disp))!=ERROR_SUCCESS)
	{
		return;
	}
	
	if ((LastError = RegSetValueEx(m_hKey, m_key, 0, REG_BINARY, (BYTE *)(LPRECT)m_value, sizeof(m_value)))==ERROR_SUCCESS)
	{
		m_read = TRUE;
	}
	RegCloseKey(m_hKey);
	m_hKey = NULL;
}

CRegRect::operator CRect()
{
	if ((m_read)&&(!m_force))
	{
		LastError = 0;
		return m_value;
	}
	else
	{
		return read();
	}
}

CRegRect& CRegRect::operator =(CRect s)
{
	if ((s==m_value)&&(!m_force))
	{
		//no write to the registry required, its the same value
		LastError = 0;
		return *this;
	}
	m_value = s;
	write();
	return *this;
}

//////////////////////////////////////////////////////////////////////////////////////////////

CRegPoint::CRegPoint(void)
{
	m_value = CPoint(0,0);
	m_defaultvalue = CPoint(0,0);
	m_key = "";
	m_base = HKEY_CURRENT_USER;
	m_read = FALSE;
	m_force = FALSE;
	LastError = ERROR_SUCCESS;
}

/**
 * Constructor.
 * @param key the path to the key, including the key. example: "Software\\Company\\SubKey\\MyValue"
 * @param def the default value used when the key does not exist or a read error occured
 * @param force set to TRUE if no cache should be used, i.e. always read and write directly from/to registry
 * @param base a predefined base key like HKEY_LOCAL_MACHINE. see the SDK documentation for more information.
 */
CRegPoint::CRegPoint(const CString& key, CPoint def, BOOL force, HKEY base)
{
	m_value = CPoint(0,0);
	m_defaultvalue = def;
	m_force = force;
	m_base = base;
	m_read = FALSE;
	m_key = key;
	m_key.TrimLeft(_T("\\"));
	int backslashpos = m_key.ReverseFind('\\');
	m_path = m_key.Left(backslashpos);
	m_path.TrimRight(_T("\\"));
	m_key = m_key.Mid(backslashpos);
	m_key.Trim(_T("\\"));
	read();
	LastError = ERROR_SUCCESS;
}

CRegPoint::~CRegPoint(void)
{
	if (m_hKey)
		RegCloseKey(m_hKey);
}

CPoint	CRegPoint::read()
{
	if ((LastError = RegOpenKeyEx(m_base, m_path, 0, KEY_EXECUTE, &m_hKey))==ERROR_SUCCESS)
	{
		int size = 0;
		DWORD type;
		RegQueryValueEx(m_hKey, m_key, NULL, &type, NULL, (LPDWORD) &size);
		POINT* pPoint = (POINT *)new char[size];
		if ((LastError = RegQueryValueEx(m_hKey, m_key, NULL, &type, (BYTE*) pPoint,(LPDWORD) &size))==ERROR_SUCCESS)
		{
			m_value = CPoint(*pPoint);
			delete [] pPoint;
			ASSERT(type==REG_BINARY);
			m_read = TRUE;
			LastError = RegCloseKey(m_hKey);
			m_hKey = NULL;
			return m_value;
		}
		else
		{
			delete [] pPoint;
			RegCloseKey(m_hKey);
			m_hKey = NULL;
			m_value = m_defaultvalue;
			m_read = TRUE;
			return m_defaultvalue;
		}
	}
	m_value = m_defaultvalue;
	return m_defaultvalue;
}

void CRegPoint::write()
{
	DWORD disp;
	if ((LastError = RegCreateKeyEx(m_base, m_path, 0, _T(""), REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &m_hKey, &disp))!=ERROR_SUCCESS)
	{
		return;
	}
	
	if ((LastError = RegSetValueEx(m_hKey, m_key, 0, REG_BINARY, (BYTE *)(POINT *)&m_value, sizeof(m_value)))==ERROR_SUCCESS)
	{
		m_read = TRUE;
	}
	RegCloseKey(m_hKey);
	m_hKey = NULL;
}

CRegPoint::operator CPoint()
{
	if ((m_read)&&(!m_force))
	{
		LastError = 0;
		return m_value;
	}
	else
	{
		return read();
	}
}

CRegPoint& CRegPoint::operator =(CPoint s)
{
	if ((s==m_value)&&(!m_force))
	{
		//no write to the registry required, its the same value
		LastError = 0;
		return *this;
	}
	m_value = s;
	write();
	return *this;
}

/////////////////////////////////////////////////////////////////////

CRegistryKey::CRegistryKey(const CString& key, HKEY base)
{
	m_base = base;
	m_hKey = NULL;
	m_path = key;
	m_path.TrimLeft(_T("\\"));
}

CRegistryKey::~CRegistryKey()
{
	if (m_hKey)
		RegCloseKey(m_hKey);
}

DWORD CRegistryKey::createKey()
{
	DWORD disp;
	DWORD rc = RegCreateKeyEx(m_base, m_path, 0, _T(""), REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &m_hKey, &disp);
	if (rc != ERROR_SUCCESS)
	{
		return rc;
	}
	return RegCloseKey(m_hKey);
}

DWORD CRegistryKey::removeKey()
{
	RegOpenKeyEx(m_base, m_path, 0, KEY_WRITE, &m_hKey);
	return SHDeleteKey(m_base, (LPCTSTR)m_path);
}

bool CRegistryKey::getValues(CStringList& values)
{
	values.RemoveAll();

	if (RegOpenKeyEx(m_base, m_path, 0, KEY_EXECUTE, &m_hKey)==ERROR_SUCCESS)
	{
		for (int i = 0, rc = ERROR_SUCCESS; rc == ERROR_SUCCESS; i++)
		{ 
			TCHAR value[255];
			DWORD size = sizeof value / sizeof TCHAR;
			rc = RegEnumValue(m_hKey, i, value, &size, NULL, NULL, NULL, NULL);
			if (rc == ERROR_SUCCESS) 
			{
				values.AddTail(value);
			}
		}
	}

	return values.GetCount() > 0;
}

bool CRegistryKey::getSubKeys(CStringList& subkeys)
{
	subkeys.RemoveAll();

	if (RegOpenKeyEx(m_base, m_path, 0, KEY_EXECUTE, &m_hKey)==ERROR_SUCCESS)
	{
		for (int i = 0, rc = ERROR_SUCCESS; rc == ERROR_SUCCESS; i++)
		{ 
			TCHAR value[1024];
			DWORD size = sizeof value / sizeof TCHAR;
			FILETIME last_write_time;
			rc = RegEnumKeyEx(m_hKey, i, value, &size, NULL, NULL, NULL, &last_write_time);
			if (rc == ERROR_SUCCESS) 
			{
				subkeys.AddTail(value);
			}
		}
	}

	return subkeys.GetCount() > 0;
}
#endif

/////////////////////////////////////////////////////////////////////

CRegStdString::CRegStdString(void)
{
	m_value = _T("");
	m_defaultvalue = _T("");
	m_key = _T("");
	m_base = HKEY_CURRENT_USER;
	m_read = FALSE;
	m_force = FALSE;
	LastError = ERROR_SUCCESS;
}

/**
 * Constructor.
 * @param key the path to the key, including the key. example: "Software\\Company\\SubKey\\MyValue"
 * @param def the default value used when the key does not exist or a read error occured
 * @param force set to TRUE if no cache should be used, i.e. always read and write directly from/to registry
 * @param base a predefined base key like HKEY_LOCAL_MACHINE. see the SDK documentation for more information.
 */
CRegStdString::CRegStdString(const stdstring& key, const stdstring& def, BOOL force, HKEY base)
{
	m_value = _T("");
	m_defaultvalue = def;
	m_force = force;
	m_base = base;
	m_read = FALSE;

	stdstring::size_type pos = key.find_last_of(_T('\\'));
    m_path = key.substr(0, pos);
	m_key = key.substr(pos + 1);
	read();
	LastError = ERROR_SUCCESS;
}

CRegStdString::~CRegStdString(void)
{
	if (m_hKey)
		RegCloseKey(m_hKey);
}

stdstring	CRegStdString::read()
{
	if ((LastError = RegOpenKeyEx(m_base, m_path.c_str(), 0, KEY_EXECUTE, &m_hKey))==ERROR_SUCCESS)
	{
		int size = 0;
		DWORD type;
		RegQueryValueEx(m_hKey, m_key.c_str(), NULL, &type, NULL, (LPDWORD) &size);
		TCHAR* pStr = new TCHAR[size];
		if ((LastError = RegQueryValueEx(m_hKey, m_key.c_str(), NULL, &type, (BYTE*) pStr,(LPDWORD) &size))==ERROR_SUCCESS)
		{
			m_value.assign(pStr);
			delete [] pStr;
			m_read = TRUE;
			LastError = RegCloseKey(m_hKey);
			m_hKey = NULL;
			return m_value;
		}
		else
		{
			delete [] pStr;
			RegCloseKey(m_hKey);
			m_hKey = NULL;
			m_value = m_defaultvalue;
			m_read = TRUE;
			return m_defaultvalue;
		}
	}
	m_value = m_defaultvalue;
	return m_defaultvalue;
}

void CRegStdString::write()
{
	DWORD disp;
	if ((LastError = RegCreateKeyEx(m_base, m_path.c_str(), 0, _T(""), REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &m_hKey, &disp))!=ERROR_SUCCESS)
	{
		return;
	}
	if ((LastError = RegSetValueEx(m_hKey, m_key.c_str(), 0, REG_SZ, (BYTE *)m_value.c_str(), ((DWORD)m_value.size()+1)*sizeof(TCHAR)))==ERROR_SUCCESS)
	{
		m_read = TRUE;
	}
	RegCloseKey(m_hKey);
	m_hKey = NULL;
}

CRegStdString::operator LPCTSTR()
{
	if ((m_read)&&(!m_force))
	{
		LastError = 0;
		return m_value.c_str();
	}
	else
		return read().c_str();
}

CRegStdString::operator stdstring()
{
	if ((m_read)&&(!m_force))
	{
		LastError = 0;
		return m_value;
	}
	else
	{
		return read();
	}
}

CRegStdString& CRegStdString::operator =(stdstring s)
{
	if ((s.compare(m_value)==0)&&(!m_force))
	{
		//no write to the registry required, its the same value
		LastError = 0;
		return *this;
	}
	m_value = s;
	write();
	return *this;
}

/////////////////////////////////////////////////////////////////////

CRegStdWORD::CRegStdWORD(void)
{
	m_value = 0;
	m_defaultvalue = 0;
	m_key = _T("");
	m_base = HKEY_CURRENT_USER;
	m_read = FALSE;
	m_force = FALSE;
	LastError = ERROR_SUCCESS;
}

/**
 * Constructor.
 * @param key the path to the key, including the key. example: "Software\\Company\\SubKey\\MyValue"
 * @param def the default value used when the key does not exist or a read error occured
 * @param force set to TRUE if no cache should be used, i.e. always read and write directly from/to registry
 * @param base a predefined base key like HKEY_LOCAL_MACHINE. see the SDK documentation for more information.
 */
CRegStdWORD::CRegStdWORD(const stdstring& key, DWORD def, BOOL force, HKEY base)
{
	m_value = 0;
	m_defaultvalue = def;
	m_force = force;
	m_base = base;
	m_read = FALSE;

	stdstring::size_type pos = key.find_last_of(_T('\\'));
    m_path = key.substr(0, pos);
	m_key = key.substr(pos + 1);
	read();
	LastError = ERROR_SUCCESS;
}

CRegStdWORD::~CRegStdWORD(void)
{
	if (m_hKey)
		RegCloseKey(m_hKey);
}

DWORD	CRegStdWORD::read()
{
	if ((LastError = RegOpenKeyEx(m_base, m_path.c_str(), 0, KEY_EXECUTE, &m_hKey))==ERROR_SUCCESS)
	{
		int size = sizeof(m_value);
		DWORD type;
		if ((LastError = RegQueryValueEx(m_hKey, m_key.c_str(), NULL, &type, (BYTE*) &m_value,(LPDWORD) &size))==ERROR_SUCCESS)
		{
			m_read = TRUE;
			LastError = RegCloseKey(m_hKey); 
			m_hKey = NULL;
			return m_value;
		}
		else
		{
			RegCloseKey(m_hKey);
			m_hKey = NULL;
			m_value = m_defaultvalue;
			m_read = TRUE;
			return m_defaultvalue;
		}
	}
	m_value = m_defaultvalue;
	return m_defaultvalue;
}

void CRegStdWORD::write()
{
	DWORD disp;
	if ((LastError = RegCreateKeyEx(m_base, m_path.c_str(), 0, _T(""), REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &m_hKey, &disp))!=ERROR_SUCCESS)
	{
		return;
	}
	if ((LastError = RegSetValueEx(m_hKey, m_key.c_str(), 0, REG_DWORD,(const BYTE*) &m_value, sizeof(m_value)))==ERROR_SUCCESS)
	{
		m_read = TRUE;
	}
	RegCloseKey(m_hKey);
	m_hKey = NULL;
}

CRegStdWORD::operator DWORD()
{
	if ((m_read)&&(!m_force))
	{
		LastError = 0;
		return m_value;
	}
	else
	{
		return read();
	}
}

CRegStdWORD& CRegStdWORD::operator =(DWORD d)
{
	if ((d==m_value)&&(!m_force))
	{
		//no write to the registry required, its the same value
		LastError = 0;
		return *this;
	}
	m_value = d;
	write();
	return *this;
}
