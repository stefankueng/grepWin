#pragma once
#include <string>
#include <vector>

using namespace std;

/**
 * handles text files.
 */
class CTextFile
{
public:
	CTextFile(void);
	~CTextFile(void);

	enum UnicodeType
	{
		AUTOTYPE,
		BINARY,
		ANSI,
		UNICODE_LE,
		UTF8,
	};

	/**
	 * Loads a file from the specified \c path.
	 */
	bool			Load(LPCTSTR path);

	/**
	 * Saves the file contents to disk at \c path.
	 */
	bool			Save(LPCTSTR path);

	/**
	 * modifies the contents of a file.
	 * \param pBuf pointer to a buffer holding the new contents of the file.
	 *             note: the buffer must be created with 'new BYTE[len]'
	 * \param newLen length of the new file content in bytes
	 * \note the old buffer is automatically freed.
	 */
	bool			ContentsModified(LPVOID pBuf, DWORD newLen);

	/**
	 * Returns the line number from a given character position inside the file.
	 */
	long			LineFromPosition(long pos) const;

	/**
	 * Returns the file content as a text string.
	 * \note the text string can not be modified and is to be treated read-only.
	 */
	const wstring&	GetFileString() const {return textcontent;}

	/**
	 * Returns a pointer to the file contents. Call GetFileLength() to get
	 * the size in number of bytes of this buffer.
	 */
	LPVOID			GetFileContent() {return pFileBuf;}

	/**
	 * Returns the size of the file in bytes
	 */
	long			GetFileLength() const {return filelen;}

	/**
	 * Returns the encoding of the file
	 */
	UnicodeType		GetEncoding() const {return encoding;}

	/**
	 * Returns the filename
	 */
	const wstring&	GetFileName() const {return filename;}

	/**
	 * Returns the filename without the extension (if any)
	 */
	wstring			GetFileNameWithoutExtension();
	
	/**
	 * Returns the filename extension (if any)
	 */
	wstring			GetFileNameExtension();
protected:
	/**
	 * Tries to find out the encoding of the file (utf8, utf16, ansi)
	 */
	UnicodeType		CheckUnicodeType(LPVOID pBuffer, int cb);
	/**
	 * Fills an array with line information to make it faster later
	 * to get the line from a char position.
	 */
	bool			CalculateLines();
private:
	LPVOID			pFileBuf;
	DWORD			filelen;
	wstring			textcontent;
	vector<size_t>	linepositions;
	UnicodeType		encoding;
	wstring			filename;
};
