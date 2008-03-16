// grepWin - regex search and replace for Windows

// Copyright (C) 2007-2008 - Stefan Kueng

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
#include "StdAfx.h"
#include "DirFileEnum.h"


CSimpleFileFind::CSimpleFileFind(LPCTSTR szPath, LPCTSTR pPattern) :
m_dError(ERROR_SUCCESS),
m_bFirst(TRUE)
{
	_tcscpy_s(m_szPathPrefix, MAX_PATH, szPath);
	// Add a trailing \ to m_sPathPrefix if it is missing.
	// Do not add one to "C:" since "C:" and "C:\" are different.
	{
		int len = (int)_tcslen(m_szPathPrefix);
		if (len != 0) 
		{
			TCHAR ch = szPath[len-1];
			if (ch != '\\' && (ch != ':' || len != 2)) 
		 {
			 _tcscat_s(m_szPathPrefix, MAX_PATH, _T("\\"));
			}
		}
	}

	TCHAR patternpath[MAX_PATH];
	_tcscpy_s(patternpath, MAX_PATH, m_szPathPrefix);
	_tcscat_s(patternpath, MAX_PATH, pPattern);
	m_hFindFile = ::FindFirstFile(patternpath, &m_FindFileData); 
	if (m_hFindFile == INVALID_HANDLE_VALUE) 
	{
		m_dError = ::GetLastError();
	}
}

CSimpleFileFind::~CSimpleFileFind()
{
	if (m_hFindFile != INVALID_HANDLE_VALUE) 
	{
		::FindClose(m_hFindFile);
	}
}

BOOL CSimpleFileFind::FindNextFile()
{
	if (m_dError) 
		return FALSE;

	if (m_bFirst) 
	{
		m_bFirst = FALSE;
		return TRUE;
	}

	if (!::FindNextFile(m_hFindFile, &m_FindFileData)) 
	{
		m_dError = ::GetLastError();
		return FALSE;
	}

	return TRUE;
}

BOOL CSimpleFileFind::FindNextFileNoDots()
{
	BOOL result;
	do 
	{
		result = FindNextFile();
	} while (result && IsDots());

	return result;
}

BOOL CSimpleFileFind::FindNextFileNoDirectories()
{
	BOOL result;
	do 
	{
		result = FindNextFile();
	} while (result && IsDirectory());

	return result;
}


/*
* Implementation notes:
*
* This is a depth-first traversal.  Directories are visited before
* their contents.
*
* We keep a stack of directories.  The deepest directory is at the top
* of the stack, the originally-requested directory is at the bottom.
* If we come across a directory, we first return it to the user, then
* recurse into it.  The finder at the bottom of the stack always points
* to the file or directory last returned to the user (except immediately
* after creation, when the finder points to the first valid thing we need
* to return, but we haven't actually returned anything yet - hence the
* m_bIsNew variable).
*
* Errors reading a directory are assumed to be end-of-directory, and
* are otherwise ignored.
*
* The "." and ".." psedo-directories are ignored for obvious reasons.
*/


CDirFileEnum::CDirStackEntry::CDirStackEntry(CDirStackEntry * seNext,
											 LPCTSTR szDirName)
											 : CSimpleFileFind(szDirName),
											 m_seNext(seNext)
{
}

CDirFileEnum::CDirStackEntry::~CDirStackEntry()
{
}

inline void CDirFileEnum::PopStack()
{
	CDirStackEntry * seToDelete = m_seStack;
	m_seStack = seToDelete->m_seNext;
	delete seToDelete;
}

inline void CDirFileEnum::PushStack(LPCTSTR szDirName)
{
	m_seStack = new CDirStackEntry(m_seStack,szDirName);
}

CDirFileEnum::CDirFileEnum(LPCTSTR szDirName) :
m_seStack(NULL),
m_bIsNew(TRUE)
{
	PushStack(szDirName);
}

CDirFileEnum::~CDirFileEnum()
{
	while (m_seStack != NULL) 
	{
		PopStack();
	}
}

BOOL CDirFileEnum::NextFile(LPTSTR szResult, bool bRecurse, bool* pbIsDirectory)
{
	if (m_bIsNew) 
	{
		// Special-case first time - haven't found anything yet,
		// so don't do recurse-into-directory check.
		m_bIsNew = FALSE;
	} 
	else if (bRecurse && m_seStack && m_seStack->IsDirectory()) 
	{
		TCHAR path[MAX_PATH];
		m_seStack->GetFilePath(path);
		PushStack(path);
	}

	while (m_seStack && !m_seStack->FindNextFileNoDots()) 
	{
		// No more files in this directory, try parent.
		PopStack();
	}

	if (m_seStack) 
	{
		m_seStack->GetFilePath(szResult);
		if(pbIsDirectory != NULL)
		{
			*pbIsDirectory = m_seStack->IsDirectory();
		}
		
		return TRUE;
	} 
	else 
	{
		return FALSE;
	}
}
