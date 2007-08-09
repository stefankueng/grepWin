#include "stdafx.h"
#include "SysImageList.h"


// Singleton constructor and destructor (private)

CSysImageList * CSysImageList::instance = 0;

CSysImageList::CSysImageList()
{
	m_hSystemImageList = NULL;
	SHFILEINFO ssfi;
	TCHAR windir[MAX_PATH];
	GetWindowsDirectory(windir, MAX_PATH);	// MAX_PATH ok.
	m_hSystemImageList =
		(HIMAGELIST)SHGetFileInfo(
			windir,
			0,
			&ssfi, sizeof ssfi,
			SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
}

CSysImageList::~CSysImageList()
{
}


// Singleton specific operations

CSysImageList& CSysImageList::GetInstance()
{
	if (instance == 0)
		instance = new CSysImageList;
	return *instance;
}

void CSysImageList::Cleanup()
{
	delete instance;
	instance = 0;
}


// Operations

int CSysImageList::GetDirIconIndex() const
{
	SHFILEINFO sfi;
	ZeroMemory(&sfi, sizeof sfi);

	SHGetFileInfo(
		_T("Doesn't matter"),
		FILE_ATTRIBUTE_DIRECTORY,
		&sfi, sizeof sfi,
		SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);

	return sfi.iIcon;
}

int CSysImageList::GetDirOpenIconIndex() const
{
	SHFILEINFO sfi;
	ZeroMemory(&sfi, sizeof sfi);

	SHGetFileInfo(
		_T("Doesn't matter"),
		FILE_ATTRIBUTE_DIRECTORY,
		&sfi, sizeof sfi,
		SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES | SHGFI_OPENICON);

	return sfi.iIcon;
}

int CSysImageList::GetDefaultIconIndex() const
{
	SHFILEINFO sfi;
	ZeroMemory(&sfi, sizeof sfi);

	SHGetFileInfo(
		_T(""),
		FILE_ATTRIBUTE_NORMAL,
		&sfi, sizeof sfi,
		SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);

	return sfi.iIcon;
}

int CSysImageList::GetFileIconIndex(const std::wstring& file) const
{
	SHFILEINFO sfi;
	ZeroMemory(&sfi, sizeof sfi);

	SHGetFileInfo(
		file.c_str(),
		FILE_ATTRIBUTE_NORMAL,
		&sfi, sizeof sfi,
		SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);

	return sfi.iIcon;
}

CSysImageList::operator HIMAGELIST()
{
	return m_hSystemImageList;
}