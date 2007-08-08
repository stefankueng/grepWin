#pragma once

/**
 * Enumerates over a directory tree, non-recursively.
 * Advantages over CFileFind:
 * 1) Return values are not broken.  An error return from
 *    CFileFind::FindNext() indicates that the *next*
 *    call to CFileFind::FindNext() will fail.
 *    A failure from CSimpleFileFind means *that* call
 *    failed, which is what I'd expect.
 * 2) Error handling.  If you use CFileFind, you are
 *    required to call ::GetLastError() yourself, there
 *    is no abstraction.
 * 3) Support for ignoring the "." and ".." directories
 *    automatically.
 * 4) No dynamic memory allocation.
 */
class CSimpleFileFind {
private:
   /**
    * Windows FindFirstFile() handle.
    */
   HANDLE m_hFindFile;

   /**
    * Windows error code - if all is well, ERROR_SUCCESS.
    * At end of directory, ERROR_NO_MORE_FILES.
    */
   DWORD m_dError;

   /**
    * Flag indicating that FindNextFile() has not yet been
    * called.
    */
   BOOL m_bFirst;

protected:
   /**
    * The prefix for files in this directory.
    * Ends with a "\", unless it's a drive letter only
    * ("C:" is different from "C:\", and "C:filename" is
    * legal anyway.)
    */
   TCHAR m_szPathPrefix[MAX_PATH];

   /**
    * The file data returned by FindFirstFile()/FindNextFile().
    */
   WIN32_FIND_DATA m_FindFileData;

public:

   /**
    * Constructor.
    *
    * \param sPath    The path to search in.
    * \param sPattern The filename pattern - default all files.
    */
   CSimpleFileFind(LPCTSTR szPath, LPCTSTR pPattern = _T("*.*"));
   ~CSimpleFileFind();

   /**
    * Advance to the next file.
    * Note that the state of this object is undefined until
    * this method is called the first time.
    *
    * \return TRUE if a file was found, FALSE on error or
    * end-of-directory (use IsError() and IsPastEnd() to
    * disambiguate).
    */
   BOOL FindNextFile();

   /**
    * Advance to the next file, ignoring the "." and ".."
    * pseudo-directories (if seen).
    *
    * Behaves like FindNextFile(), apart from ignoring "."
    * and "..".
    *
    * \return TRUE if a file was found, FALSE on error or
    * end-of-directory.
    */
   BOOL FindNextFileNoDots();

   /**
    * Advance to the next file, ignoring all directories.
    *
    * Behaves like FindNextFile(), apart from ignoring
    * directories.
    *
    * \return TRUE if a file was found, FALSE on error or
    * end-of-directory.
    */
   BOOL FindNextFileNoDirectories();

   /**
    * Get the Windows error code.
    * Only useful when IsError() returns true.
    *
    * \return Windows error code.
    */
   inline DWORD GetError() const
   {
      return m_dError;
   }

   /**
    * Check if the current file data is valid.
    * (I.e. there has not been an error and we are not past
    * the end of the directory).
    *
    * \return TRUE iff the current file data is valid.
    */
   inline BOOL IsValid() const
   {
      return (m_dError == ERROR_SUCCESS);
   }

   /**
    * Check if we have passed the end of the directory.
    *
    * \return TRUE iff we have passed the end of the directory.
    */
   inline BOOL IsPastEnd() const
   {
      return (m_dError == ERROR_NO_MORE_FILES);
   }

   /**
    * Check if there has been an unexpected error - i.e.
    * any error other than passing the end of the directory.
    *
    * \return TRUE iff there has been an unexpected error.
    */
   inline BOOL IsError() const
   {
      return (m_dError != ERROR_SUCCESS)
          && (m_dError != ERROR_NO_MORE_FILES);
   }

   /**
    * Check if the current file is a directory.
    *
    * \return TRUE iff the current file is a directory.
    */
   inline bool IsDirectory() const
   {
      return !!(m_FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
   }

   /**
    * Get the current file name (excluding the path).
    *
    * \return the current file name.
    */
   inline void GetFileName(LPTSTR szFileName) const
   {
	   _tcscpy_s(szFileName, MAX_PATH, m_FindFileData.cFileName);
   }

   const WIN32_FIND_DATA * GetFileFindData() const {return &m_FindFileData;}
   /*
    * Get the current file name, including the path.
    *
    * \return the current file path.
    */
   inline void GetFilePath(LPTSTR szPath) const
   {
	   _tcscpy_s(szPath, MAX_PATH, m_szPathPrefix);
	   _tcscat_s(szPath, MAX_PATH, m_FindFileData.cFileName);
   }

   /**
    * Check if the current file is the "." or ".."
    * pseudo-directory.
    *
    * \return TRUE iff the current file is the "." or ".."
    * pseudo-directory.
    */
   inline BOOL IsDots() const 
   {
      return IsDirectory()
          && m_FindFileData.cFileName[0] == _T('.')
          && ( (m_FindFileData.cFileName[1] == 0)
            || (m_FindFileData.cFileName[1] == _T('.')
             && m_FindFileData.cFileName[2] == 0) );
   }
};

/**
 * Enumerates over a directory tree, recursively.
 */
class CDirFileEnum
{
private:

   class CDirStackEntry : public CSimpleFileFind {
   public:
      CDirStackEntry(CDirStackEntry * seNext, LPCTSTR szDirName);
      ~CDirStackEntry();

      CDirStackEntry * m_seNext;
   };

   CDirStackEntry * m_seStack;
   BOOL m_bIsNew;

   inline void PopStack();
   inline void PushStack(LPCTSTR szDirName);

public:
   /**
    * Iterate through the specified directory and all subdirectories.
    * It does not matter whether or not the specified directory ends
    * with a slash.  Both relative and absolute paths are allowed,
    * the results of this iterator will be consistent with the style
    * passed to this constructor.
    *
    * @param dirName The directory to search in.
    */
	CDirFileEnum(LPCTSTR dirName);

   /**
    * Destructor.  Frees all resources.
    */
   ~CDirFileEnum();

   /**
    * Get the next file from this iterator.
    *
    * \param  result On successful return, holds the full path to the found
    *                file. (If this function returns FALSE, the value of
    *                result is unspecified).
	* \param  bNoRecurse if true, then the last returned directory is not
	*                recursed into. Only valid for directories.
	* \param  pbIsDirectory Pointer to a bool variable which will hold
	*                TRUE if the \c result path is a directory, FALSE
	*				 if it's a file. Pass NULL if you don't need that information.
    * \return TRUE iff a file was found, false at end of the iteration.
    */
   BOOL NextFile(LPTSTR result, bool bNoRecurse, bool* pbIsDirectory);

   const WIN32_FIND_DATA * GetFileInfo() {return m_seStack->GetFileFindData();}
};

