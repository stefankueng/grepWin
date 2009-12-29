// grepWin - regex search and replace for Windows

// Copyright (C) 2007-2009 - Stefan Kueng

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//
#include "stdafx.h"
#include "registry.h"

DWORD CRegStdBase::removeKey() 
{ 
	RegOpenKeyEx(m_base, m_path.c_str(), 0, KEY_WRITE, &m_hKey); 
	LSTATUS ret = SHDeleteKey(m_base, m_path.c_str()); 
	RegCloseKey(m_hKey); 
	m_hKey = NULL;
	return ret;
}

LONG CRegStdBase::removeValue() 
{ 
	RegOpenKeyEx(m_base, m_path.c_str(), 0, KEY_WRITE, &m_hKey); 
	LONG ret = RegDeleteValue(m_hKey, m_key.c_str()); 
	RegCloseKey(m_hKey); 
	m_hKey = NULL;
	return ret;
}


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
