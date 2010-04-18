// Library: SimpleIni
// File:    SimpleIni.h
// Author:  Brodie Thiesfield <code@jellycan.com>
// Source:  http://code.jellycan.com/simpleini/
// Version: 2.8
//
// INTRODUCTION
// ============
//  This component allows an INI-style configuration file to be used on both
//  Windows and Linux/Unix. It is fast, simple and source code using this
//  component will compile unchanged on either OS.
//
// FEATURES
// ========
//  * MIT Licence allows free use in all software (including GPL and commercial)
//  * multi-platform (Windows 95/98/ME/NT/2K/XP/2003, Windows CE, Linux, Unix)
//  * loading and saving of INI-style configuration files
//  * configuration files can have any newline format on all platforms
//  * liberal acceptance of file format
//      - key/values with no section
//      - removal of whitespace around sections, keys and values
//  * support for multi-line values (values with embedded newline characters)
//  * optional support for multiple keys with the same name
//  * optional case-insensitive sections and keys (for ASCII characters only)
//  * supports both char or wchar_t programming interfaces
//  * supports both MBCS (system locale) and UTF-8 file encodings
//  * system locale does not need to be UTF-8 on Linux/Unix to load UTF-8 file
//  * support for non-ASCII characters in section, keys, values and comments
//  * support for non-standard character types or file encodings
//    via user-written converter classes
//  * support for adding/modifying values programmatically
//  * compiles cleanly at warning level 4 (Windows/VC.NET 2003), warning level
//    3 (Windows/VC6) and -Wall (Linux/gcc)
//
// USAGE SUMMARY
// =============
//  1. Define the appropriate symbol for the converter you wish to use and
//     include the SimpleIni.h header file. If no specific converter is defined
//     then the default converter is used. The default conversion mode uses
//     SI_CONVERT_WIN32 on Windows and SI_CONVERT_GENERIC on all other
//     platforms. If you are using ICU then SI_CONVERT_ICU is supported on all
//     platforms.
//
//  2. Declare an instance the appropriate class. Note that the following
//     definitions are just shortcuts for commonly used types. Other types
//     (PRUnichar, unsigned short, unsigned char) are also possible.
//
//      Interface   Case-sensitive  Load UTF-8  Load MBCS   Typedef
//      ---------   --------------  ----------  ---------   ---------------
//    SI_CONVERT_GENERIC
//      char        No              Yes         Yes *1      CSimpleIniA
//      char        Yes             Yes         Yes         CSimpleIniCaseA
//      wchar_t     No              Yes         Yes         CSimpleIniW
//      wchar_t     Yes             Yes         Yes         CSimpleIniCaseW
//    SI_CONVERT_WIN32
//      char        No              No *2       Yes         CSimpleIniA
//      char        Yes             Yes         Yes         CSimpleIniCaseA
//      wchar_t     No              Yes         Yes         CSimpleIniW
//      wchar_t     Yes             Yes         Yes         CSimpleIniCaseW
//    SI_CONVERT_ICU
//      char        No              Yes         Yes         CSimpleIniA
//      char        Yes             Yes         Yes         CSimpleIniCaseA
//      UChar       No              Yes         Yes         CSimpleIniW
//      UChar       Yes             Yes         Yes         CSimpleIniCaseW
//
//      *1  On Windows you are better to use CSimpleIniA with SI_CONVERT_WIN32.
//      *2  Only affects Windows. On Windows this uses MBCS functions and
//          so may fold case incorrectly leading to uncertain results.
//
//  2. Call LoadFile() to load and parse the INI configuration file
//
//  3. Access and modify the data of the file using the following functions
//
//      GetAllSections  Return all section names
//      GetAllKeys      Return all key names for a section
//      GetSection      Return all key names and values in a section
//      GetSectionSize  Return the number of keys in a section
//      GetValue        Return a value for a section & key
//      GetAllValues    Return all values for a section & key
//      SetValue        Add or update a value for a section & key
//      Delete          Remove a section, or a key from a section
//
//  4. Call Save(), SaveFile() or SaveString() to save the INI configuration
//     data (as necessary)
//
// MULTI-LINE VALUES
// =================
// Values that span multiple lines are created using the following format. 
//      
//      key = <<<ENDTAG
//      .... multiline value ....
//      ENDTAG
//
// Note the following:
//  * The text used for ENDTAG can be anything and is used to find
//    where the multi-line text ends.
//  * The newline after ENDTAG in the start tag, and the newline
//    before ENDTAG in the end tag is not included in the data value.
//  * The ending tag must be on it's own line with no whitespace before
//    or after it.
//  * The multi-line value is not modified at load or save. This means
//    that the newline format (PC, Unix, Mac) is whatever the original
//    file uses.
//
// NOTES
// =====
//  * To compile using Microsoft Visual Studio 6 or Embedded Visual C++ 4,
//    you need to modify this header file and remove all instances of the
//    "typename" keyword where error C2899 occurs.
//  * To load UTF-8 data on Windows 95, you need to use Microsoft Layer for
//    Unicode, or SI_CONVERT_GENERIC, or SI_CONVERT_ICU.
//  * When using SI_CONVERT_GENERIC, ConvertUTF.c must be compiled and linked.
//  * When using SI_CONVERT_ICU, ICU header files must be on the include
//    path and icuuc.lib must be linked in.
//  * To load a UTF-8 file on Windows AND expose it with SI_CHAR == char,
//    you should use SI_CONVERT_GENERIC.
//  * The collation (sorting) order used for sections and keys returned from
//    iterators is NOT DEFINED. If collation order of the text is important
//    then it should be done yourself by either supplying a replacement
//    SI_STRLESS class, or by sorting the strings external to this library.
//  * Usage of the <mbstring.h> header on Windows can be disabled by defining
//    SI_NO_MBCS. This is defined automatically on Windows CE platforms.
//
// MIT LICENCE
// ===========
// The licence text below is the boilerplate "MIT Licence" used from:
// http://www.opensource.org/licenses/mit-license.php
//
// Copyright (c) 2006, Brodie Thiesfield
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is furnished
// to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef INCLUDED_SimpleIni_h
#define INCLUDED_SimpleIni_h

// Disable these warnings in MSVC:
//  4127 "conditional expression is constant" as the conversion classes trigger
//  it with the statement if (sizeof(SI_CHAR) == sizeof(char)). This test will
//  be optimized away in a release build.
//  4702 "unreachable code" as the MS STL header causes it in release mode.
//  Again, the code causing the warning will be cleaned up by the compiler.
#ifdef _MSC_VER
# pragma warning (disable: 4127 4702)
#endif

#include <string>
#include <map>
#include <list>
#include <assert.h>

enum SI_Error {
    SI_OK       =  0,   //!< No error
    SI_UPDATED  =  1,   //!< An existing value was updated
    SI_INSERTED =  2,   //!< A new value was inserted

    // note: test for any error with (retval < 0)
    SI_FAIL     = -1,   //!< Generic failure
    SI_NOMEM    = -2,   //!< Out of memory error
    SI_FILE     = -3,   //!< File error (see errno for detail error)
};

#define SI_BOM_UTF8     "\xEF\xBB\xBF"

#ifdef _WIN32
# define SI_NEWLINE_A   "\r\n"
# define SI_NEWLINE_W   L"\r\n"
#else // !_WIN32
# define SI_NEWLINE_A   "\n"
# define SI_NEWLINE_W   L"\n"
#endif // _WIN32

#if defined(_WIN32) || defined(SI_CONVERT_ICU)
# define SI_HAS_WIDE_LOADFILE
#endif

#if defined(SI_CONVERT_ICU)
# include <unicode/ustring.h>
#endif

// ---------------------------------------------------------------------------
//                              MAIN TEMPLATE CLASS
// ---------------------------------------------------------------------------

/**
 * Simple INI file reader. This can be instantiated with the choice of unicode
 * or native characterset, and case sensitive or insensitive comparisons of
 * section and key names. The supported combinations are pre-defined with the
 * following typedefs:
 *
 *  Interface   Case-sensitive      Typedef
 *  ---------   --------------      ---------------
 *  char        No                  CSimpleIniA
 *  char        Yes                 CSimpleIniCaseA
 *  wchar_t     No                  CSimpleIniW
 *  wchar_t     Yes                 CSimpleIniCaseW
 *
 * Note that using other types for the SI_CHAR is supported. For instance,
 * unsigned char, unsigned short, etc. Note that where the alternative type
 * is a different size to char/wchar_t you may need to supply new helper
 * classes for SI_STRLESS and SI_CONVERTER.
 */
template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
class CSimpleIniTempl
{
public:
    /** map keys to values */
    typedef std::multimap<const SI_CHAR *,const SI_CHAR *,SI_STRLESS> TKeyVal;

    /** map sections to key/value map */
    typedef std::map<const SI_CHAR *,TKeyVal,SI_STRLESS> TSection;

    /** set of dependent string pointers. Note that these pointers are
     * dependent on memory owned by CSimpleIni. */
    typedef std::list<const SI_CHAR *> TNamesDepend;

    /** interface definition for the OutputWriter object to pass to Save()
     * in order to output the INI file data. */
    class OutputWriter {
    public:
        OutputWriter() { }
        virtual ~OutputWriter() { }
        virtual void Write(const char * a_pBuf) = 0;
    };

    /** OutputWriter class to write the INI data to a file */
    class FileWriter : public OutputWriter {
        FILE * m_file;
    public:
        FileWriter(FILE * a_file) : m_file(a_file) { }
        void Write(const char * a_pBuf) {
            fputs(a_pBuf, m_file);
        }
    };

    /** OutputWriter class to write the INI data to a string */
    class StringWriter : public OutputWriter {
        std::string & m_string;
    public:
        StringWriter(std::string & a_string) : m_string(a_string) { }
        void Write(const char * a_pBuf) {
            m_string.append(a_pBuf);
        }
    };

    /** Characterset conversion utility class to convert strings to the
     * same format as is used for the storage. */
    class Converter : private SI_CONVERTER {
    public:
        Converter(bool a_bStoreIsUtf8) : SI_CONVERTER(a_bStoreIsUtf8) {
            m_scratch.resize(1024);
        }
        Converter(const Converter & rhs) { operator=(rhs); }
        Converter & operator=(const Converter & rhs) {
            m_scratch = rhs.m_scratch;
            return *this;
        }
        bool ConvertToStore(const SI_CHAR * a_pszString) {
            size_t uLen = SizeToStore(a_pszString);
            if (uLen == (size_t)(-1)) {
                return false;
            }
            while (uLen > m_scratch.size()) {
                m_scratch.resize(m_scratch.size() * 2);
            }
            return SI_CONVERTER::ConvertToStore(
                a_pszString,
                const_cast<char*>(m_scratch.data()),
                m_scratch.size());
        }
        const char * Data() { return m_scratch.data(); }
    private:
        std::string m_scratch;
    };

public:
    /**
     * Default constructor.
     *
     * @param a_bIsUtf8     See the method SetUnicode() for details.
     * @param a_bMultiKey   See the method SetMultiKey() for details.
     * @param a_bMultiLine  See the method SetMultiLine() for details.
     */
    CSimpleIniTempl(bool a_bIsUtf8 = false, bool a_bMultiKey = false, bool a_bMultiLine = false);

    /**
     * Destructor
     */
    ~CSimpleIniTempl();

    /**
     * Deallocate all memory stored by this object
     */
    void Reset();

    /**
     * Set the storage format of the INI data. This affects both the loading
     * and saving of the INI data using all of the Load/Save API functions.
     * This value cannot be changed after any INI data has been loaded.
     *
     * If the file is not set to Unicode (UTF-8), then the data encoding is
     * assumed to be the OS native encoding. This encoding is the system
     * locale on Linux/Unix and the legacy MBCS encoding on Windows NT/2K/XP.
     * If the storage format is set to Unicode then the file will be loaded
     * as UTF-8 encoded data regardless of the native file encoding. If
     * SI_CHAR == char then all of the char* parameters take and return UTF-8
     * encoded data regardless of the system locale.
     */
    void SetUnicode(bool a_bIsUtf8 = true) {
        if (!m_pData) m_bStoreIsUtf8 = a_bIsUtf8;
    }

    /**
     * Get the storage format of the INI data.
     */
    bool IsUnicode() const { return m_bStoreIsUtf8; }

    /**
     * Should multiple identical keys be permitted in the file. If set to false
     * then the last value encountered will be used as the value of the key.
     * If set to true, then all values will be available to be queried. For
     * example, with the following input:
     *
     * [section]
     *   test=value1
     *   test=value2
     *
     * Then with SetMultiKey(true), both of the values "value1" and "value2"
     * will be returned for the key test. If SetMultiKey(false) is used, then
     * the value for "test" will only be "value2". This value may be changed
     * at any time.
     */
    void SetMultiKey(bool a_bAllowMultiKey = true) {
        m_bAllowMultiKey = a_bAllowMultiKey;
    }

    /**
     * Get the storage format of the INI data.
     */
    bool IsMultiKey() const { return m_bAllowMultiKey; }

    /**
     * Should data values be permitted to span multiple lines in the file. If 
     * set to false then the multi-line construct <<<TAG as a value will be
     * returned as is instead of loading the data. This value may be changed 
     * at any time.
     */
    void SetMultiLine(bool a_bAllowMultiLine = true) {
        m_bAllowMultiLine = a_bAllowMultiLine;
    }

    /**
     * Query the status of multi-line data.
     */
    bool IsMultiLine() const { return m_bAllowMultiLine; }

    /**
     * Load an INI file from disk into memory
     *
     * @param a_pszFile     Path of the file to be loaded. This will be passed
     *                      to fopen() and so must be a valid path for the
     *                      current platform.
     *
     * @return SI_Error     See error definitions
     */
    SI_Error LoadFile(const char * a_pszFile);

#ifdef SI_HAS_WIDE_LOADFILE
    /**
     * Load an INI file from disk into memory
     *
     * @param a_pwszFile    Path of the file to be loaded in UTF-16. This will
     *                      be passed to _wfopen() on Windows. There is no
     *                      wchar_t fopen function on Linux/Unix so this
     *                      function is not supported there.
     *
     * @return SI_Error     See error definitions
     */
    SI_Error LoadFile(const wchar_t * a_pwszFile);
#endif // _WIN32

    /**
     * Load INI file data direct from memory
     *
     * @param a_pData       Data to be loaded
     * @param a_uDataLen    Length of the data in bytes
     *
     * @return SI_Error     See error definitions
     */
    SI_Error LoadFile(
        const char *    a_pData,
        size_t          a_uDataLen);

    /**
     * Save the INI data. The data will be written to the output device
     * in a format appropriate to the current data, selected by:
     *
     *      SI_CHAR     FORMAT
     *      -------     ------
     *      char        same format as when loaded (MBCS or UTF-8)
     *      wchar_t     UTF-8
     *      other       UTF-8
     *
     * Note that comments, etc from the original data are not saved. Only the
     * valid data contents stored in the file are written out. Any data
     * prepended or appended to the output device should use the same format
     * (MBCS or UTF-8) as this data, use the GetConverter() method to convert
     * text to the correct format.
     *
     * To add a BOM to UTF-8 data, write it out manually at the very beginning
     * like is done in SaveFile when a_bUseBOM is true.
     */
    SI_Error Save(OutputWriter & a_oOutput) const;

    /**
     * Save the INI data to a file. See Save() for details. Do not set
     * a_bUseBOM to true if any information has been written to the file
     * prior to calling this method.
     *
     * @param a_pFile       Handle to a file. File should be opened for
     *                      binary output.
     * @param a_bUseBOM     Prepend the UTF-8 BOM if the output stream is
     *                      in UTF-8 format.
     */
    SI_Error SaveFile(FILE * a_pFile, bool a_bUseBOM = false) const {
        FileWriter writer(a_pFile);
        if (m_bStoreIsUtf8 && a_bUseBOM) {
            writer.Write(SI_BOM_UTF8);
        }
        return Save(writer);
    }

    /**
     * Save the INI data to a string. See Save() for details.
     *
     * @param a_sBuffer     String to have the INI data appended to.
     */
    SI_Error SaveString(std::string & a_sBuffer) const {
        StringWriter writer(a_sBuffer);
        return Save(writer);
    }

    /**
     * Retrieve the value for a specific key. If multiple keys are enabled
     * (see SetMultiKey) then only the first value associated with that key
     * will be returned, see GetAllValues for getting all values with multikey.
     *
     * NOTE! The returned value is a pointer to string data stored in memory
     * owned by CSimpleIni. Ensure that the CSimpleIni object is not destroyed
     * or Reset while you are using this pointer!
     *
     * @param a_pSection        Section to search
     * @param a_pKey            Key to search for
     * @param a_pDefault        Value to return if the key is not found
     * @param a_pHasMultiple    Optionally receive notification of if there are
     *                          multiple entries for this key.
     *
     * @return a_pDefault       Key was not found in the section
     * @return other            Value of the key
     */
    const SI_CHAR * GetValue(
        const SI_CHAR * a_pSection,
        const SI_CHAR * a_pKey,
        const SI_CHAR * a_pDefault = 0,
        bool *          a_pHasMultiple = 0) const;

    /**
     * Retrieve all values for a specific key. This method can be used when
     * multiple keys are both enabled and disabled.
     *
     * NOTE! The returned values are pointers to string data stored in memory
     * owned by CSimpleIni. Ensure that the CSimpleIni object is not destroyed
     * or Reset while you are using this pointer!
     *
     * @param a_pSection        Section to search
     * @param a_pKey            Key to search for
     * @param a_values          List to return if the key is not found
     * @param a_pHasMultiple    Optionally receive notification of if there are
     *                          multiple entries for this key.
     *
     * @return a_pDefault       Key was not found in the section
     * @return other            Value of the key
     */
    bool GetAllValues(
        const SI_CHAR * a_pSection,
        const SI_CHAR * a_pKey,
        TNamesDepend &  a_values) const;

    /**
     * Add or update a section or value. This will always insert
     * when multiple keys are enabled.
     *
     * @param a_pSection    Section to add or update
     * @param a_pKey        Key to add or update. Set to NULL to
     *                      create an empty section.
     * @param a_pValue      Value to set. Set to NULL to create an
     *                      empty section.
     *
     * @return SI_Error     See error definitions
     * @return SI_UPDATED   Value was updated
     * @return SI_INSERTED  Value was inserted
     */
    SI_Error SetValue(
        const SI_CHAR * a_pSection,
        const SI_CHAR * a_pKey,
        const SI_CHAR * a_pValue)
    {
        return AddEntry(a_pSection, a_pKey, a_pValue, true);
    }

    /**
     * Delete an entire section, or a key from a section. Note that the
     * data returned by GetSection is invalid and must not be used after
     * anything has been deleted from that section using this method.
     * Note when multiple keys is enabled, this will delete all keys with
     * that name; there is no way to selectively delete individual key/values
     * in this situation.
     *
     * @param a_pSection        Section to delete key from, or if
     *                          a_pKey is NULL, the section to remove.
     * @param a_pKey            Key to remove from the section. Set to
     *                          NULL to remove the entire section.
     * @param a_bRemoveEmpty    If the section is empty after this key has
     *                          been deleted, should the empty section be
     *                          removed?
     *
     * @return true             Key or section was deleted.
     * @return false            Key or section was not found.
     */
    bool Delete(
        const SI_CHAR * a_pSection,
        const SI_CHAR * a_pKey,
        bool            a_bRemoveEmpty = false);

    /**
     * Query the number of keys in a specific section. Note that if multiple
     * keys are enabled, then this value may be different to the number of
     * keys returned by GetAllKeys.
     *
     * @param a_pSection        Section to request data for
     *
     * @return -1               Section does not exist in the file
     * @return >=0              Number of keys in the section
     */
    int GetSectionSize(
        const SI_CHAR * a_pSection ) const;

    /**
     * Retrieve all key and value pairs for a section. The data is returned
     * as a pointer to an STL map and can be iterated or searched as
     * desired. Note that multiple entries for the same key may exist when
     * multiple keys have been enabled.
     *
     * NOTE! This structure contains only pointers to strings. The actual
     * string data is stored in memory owned by CSimpleIni. Ensure that the
     * CSimpleIni object is not destroyed or Reset() while these strings
     * are in use!
     *
     * @param a_pSection        Name of the section to return
     * @param a_pData           Pointer to the section data.
     * @return boolean          Was a section matching the supplied
     *                          name found.
     */
    const TKeyVal * GetSection(
        const SI_CHAR * a_pSection) const;

    /**
     * Retrieve all section names. The list is returned as an STL vector of
     * names and can be iterated or searched as necessary. Note that the
     * collation order of the returned strings is NOT DEFINED.
     *
     * NOTE! This structure contains only pointers to strings. The actual
     * string data is stored in memory owned by CSimpleIni. Ensure that the
     * CSimpleIni object is not destroyed or Reset() while these strings
     * are in use!
     *
     * @param a_names           Vector that will receive all of the section
     *                          names. See note above!
     */
    void GetAllSections(
        TNamesDepend & a_names ) const;

    /**
     * Retrieve all unique key names in a section. The collation order of the
     * returned strings is NOT DEFINED. Only unique key names are returned.
     *
     * NOTE! This structure contains only pointers to strings. The actual
     * string data is stored in memory owned by CSimpleIni. Ensure that the
     * CSimpleIni object is not destroyed or Reset() while these strings
     * are in use!
     *
     * @param a_pSection        Section to request data for
     * @param a_names           List that will receive all of the key
     *                          names. See note above!
     */
    void GetAllKeys(
        const SI_CHAR * a_pSection,
        TNamesDepend &  a_names ) const;

    /**
     * Return a conversion object to convert text to the same encoding
     * as is used by the Save(), SaveFile() and SaveString() functions.
     * Use this to prepare the strings that you wish to append or prepend
     * to the output INI data.
     */
    Converter GetConverter() const {
        return Converter(m_bStoreIsUtf8);
    }

private:
    /** Load the file from a file pointer. */
    SI_Error LoadFile(FILE * a_fpFile);

    /** Parse the data looking for the next valid entry. The memory pointed to
     * by a_pData is modified by inserting NULL characters. The pointer is
     * updated to the current location in the block of text. */
    bool FindEntry(
        SI_CHAR *&  a_pData,
        const SI_CHAR *&  a_pSection,
        const SI_CHAR *&  a_pKey,
        const SI_CHAR *&  a_pVal ) const;

    /** Add the section/key/value to our data. Strings will be copied only
     * if a_bCopyStrings is set to true, otherwise the pointers will be
     * used as is. */
    SI_Error AddEntry(
        const SI_CHAR * a_pSection,
        const SI_CHAR * a_pKey,
        const SI_CHAR * a_pValue,
        bool            a_bCopyStrings);

    /** Is the supplied character a whitespace character? */
    inline bool IsSpace(SI_CHAR ch) const {
        return (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n');
    }

    /** Does the supplied character start a comment line? */
    inline bool IsComment(SI_CHAR ch) const {
        return (ch == ';' || ch == '#');
    }

    /** Make a copy of the supplied string, replacing the original pointer. */
    SI_Error CopyString(const SI_CHAR *& a_pString);

    /** Delete a string from the copied strings buffer if necessary. */
    void DeleteString(const SI_CHAR * a_pString);

    /** Internal use of our string comparison function */
    bool IsEqual(const SI_CHAR * a_pLeft, const SI_CHAR * a_pRight) const {
        static const SI_STRLESS strless = SI_STRLESS();
        return !strless(a_pLeft, a_pRight) && !strless(a_pRight, a_pLeft);
    }

    bool IsMultiLineTag(const SI_CHAR * a_pData) const;
    bool IsMultiLineData(const SI_CHAR * a_pData) const;
    bool FindMultiLine(SI_CHAR *& a_pData, const SI_CHAR *& a_pVal) const;
    bool IsNewLineChar(SI_CHAR a_c) const;

private:
    /** Copy of the INI file data in our character format. This will be
     * modified when parsed to have NULL characters added after all
     * interesting string entries. All of the string pointers to sections,
     * keys and values point into this block of memory.
     */
    SI_CHAR * m_pData;

    /** Length of the data that we have stored. Used when deleting strings
     * to determine if the string is stored here or in the allocated string
     * buffer.
     */
    size_t m_uDataLen;

    /** Parsed INI data. Section -> (Key -> Value). */
    TSection m_data;

    /** This vector stores allocated memory for copies of strings that have
     * been supplied after the file load. It will be empty unless SetValue()
     * has been called.
     */
    TNamesDepend m_strings;

    /** Is the format of our datafile UTF-8 or MBCS? */
    bool m_bStoreIsUtf8;

    /** Are multiple values permitted for the same key? */
    bool m_bAllowMultiKey;

    /** Are data values permitted to span multiple lines? */
    bool m_bAllowMultiLine;
};

// ---------------------------------------------------------------------------
//                                  IMPLEMENTATION
// ---------------------------------------------------------------------------

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
CSimpleIniTempl<SI_CHAR,SI_STRLESS,SI_CONVERTER>::CSimpleIniTempl(
    bool a_bIsUtf8, bool a_bAllowMultiKey, bool a_bAllowMultiLine)
  : m_pData(0),
    m_uDataLen(0),
    m_bStoreIsUtf8(a_bIsUtf8),
    m_bAllowMultiKey(a_bAllowMultiKey),
    m_bAllowMultiLine(a_bAllowMultiLine)
{ }

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
CSimpleIniTempl<SI_CHAR,SI_STRLESS,SI_CONVERTER>::~CSimpleIniTempl()
{
    Reset();
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
void
CSimpleIniTempl<SI_CHAR,SI_STRLESS,SI_CONVERTER>::Reset()
{
    // remove all data
    delete[] m_pData;
    m_pData = 0;
    m_uDataLen = 0;
    if (!m_data.empty()) {
        m_data.erase(m_data.begin(), m_data.end());
    }

    // remove all strings
    if (!m_strings.empty()) {
        typename TNamesDepend::iterator i = m_strings.begin();
        for (; i != m_strings.end(); ++i) {
            delete[] const_cast<SI_CHAR*>(*i);
        }
        m_strings.erase(m_strings.begin(), m_strings.end());
    }
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
SI_Error
CSimpleIniTempl<SI_CHAR,SI_STRLESS,SI_CONVERTER>::LoadFile(
    const char *    a_pszFile)
{
    FILE * fp = fopen(a_pszFile, "rb");
    if (!fp) {
        return SI_FILE;
    }
    SI_Error rc = LoadFile(fp);
    fclose(fp);
    return rc;
}

#ifdef SI_HAS_WIDE_LOADFILE
template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
SI_Error
CSimpleIniTempl<SI_CHAR,SI_STRLESS,SI_CONVERTER>::LoadFile(
    const wchar_t * a_pwszFile)
{
#ifdef _WIN32
    FILE * fp;
	errno_t err = _wfopen_s(&fp, a_pwszFile, L"rb");
    if ((err!=0)||(!fp)) {
        return SI_FILE;
    }
    SI_Error rc = LoadFile(fp);
    fclose(fp);
    return rc;
#else // SI_CONVERT_ICU
    char szFile[256];
    u_austrncpy(szFile, a_pwszFile, sizeof(szFile));
    return LoadFile(szFile);
#endif
}
#endif // _WIN32

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
SI_Error
CSimpleIniTempl<SI_CHAR,SI_STRLESS,SI_CONVERTER>::LoadFile(
    FILE *  a_fpFile)
{
    // load the raw file data
    int retval = fseek(a_fpFile, 0, SEEK_END);
    if (retval != 0) {
        return SI_FILE;
    }
    long lSize = ftell(a_fpFile);
    if (lSize < 0) {
        return SI_FILE;
    }
    char * pData = new char[lSize];
    if (!pData) {
        return SI_NOMEM;
    }
    fseek(a_fpFile, 0, SEEK_SET);
    size_t uRead = fread(pData, sizeof(char), lSize, a_fpFile);
    if (uRead != (size_t) lSize) {
        delete[] pData;
        return SI_FILE;
    }

    // convert the raw data to unicode
    SI_Error rc = LoadFile(pData, uRead);
    delete[] pData;
    return rc;
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
SI_Error
CSimpleIniTempl<SI_CHAR,SI_STRLESS,SI_CONVERTER>::LoadFile(
    const char *    a_pData,
    size_t          a_uDataLen)
{
    SI_CONVERTER converter(m_bStoreIsUtf8);

    // consume the UTF-8 BOM if it exists
    if (m_bStoreIsUtf8 && a_uDataLen >= 3) {
        if (memcmp(a_pData, SI_BOM_UTF8, 3) == 0) {
            a_pData    += 3;
            a_uDataLen -= 3;
        }
    }

    // determine the length of the converted data
    size_t uLen = converter.SizeFromStore(a_pData, a_uDataLen);
    if (uLen == (size_t)(-1)) {
        return SI_FAIL;
    }

    // allocate memory for the data, ensure that there is a NULL
    // terminator wherever the converted data ends
    SI_CHAR * pData = new SI_CHAR[uLen+1];
    if (!pData) {
        return SI_NOMEM;
    }
    memset(pData, 0, sizeof(SI_CHAR)*(uLen+1));

    // convert the data
    if (!converter.ConvertFromStore(a_pData, a_uDataLen, pData, uLen)) {
        delete[] pData;
        return SI_FAIL;
    }

    // parse it
    const static SI_CHAR empty = 0;
    SI_CHAR * pWork = pData;
    const SI_CHAR * pSection = &empty;
    const SI_CHAR * pKey = 0;
    const SI_CHAR * pVal = 0;

    // add every entry in the file to the data table. We copy the strings if
    // we are loading data into this class when we already have stored some
    // because we only store a single block.
    SI_Error rc;
    bool bCopyStrings = (m_pData != 0);
    while (FindEntry(pWork, pSection, pKey, pVal)) {
        rc = AddEntry(pSection, pKey, pVal, bCopyStrings);
        if (rc < 0) return rc;
    }

    // store these strings if we didn't copy them
    if (bCopyStrings) {
        delete[] pData;
    }
    else {
        m_pData = pData;
        m_uDataLen = uLen+1;
    }

    return SI_OK;
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
bool
CSimpleIniTempl<SI_CHAR,SI_STRLESS,SI_CONVERTER>::FindEntry(
    SI_CHAR *&        a_pData,
    const SI_CHAR *&  a_pSection,
    const SI_CHAR *&  a_pKey,
    const SI_CHAR *&  a_pVal ) const
{
    SI_CHAR * pTrail;
    while (*a_pData) {
        // skip spaces and empty lines
        while (*a_pData && IsSpace(*a_pData)) {
            ++a_pData;
        }
        if (!*a_pData) {
            break;
        }

        // skip comment lines
        if (IsComment(*a_pData)) {
            while (*a_pData && !IsNewLineChar(*a_pData)) {
                ++a_pData;
            }
            continue;
        }

        // process section names
        if (*a_pData == '[') {
            // skip leading spaces
            ++a_pData;
            while (*a_pData && IsSpace(*a_pData)) {
                ++a_pData;
            }

            // find the end of the section name (it may contain spaces)
            // and convert it to lowercase as necessary
            a_pSection = a_pData;
            while (*a_pData && *a_pData != ']' && !IsNewLineChar(*a_pData)) {
                ++a_pData;
            }

            // if it's an invalid line, just skip it
            if (*a_pData != ']') {
                continue;
            }

            // remove trailing spaces from the section
            pTrail = a_pData - 1;
            while (pTrail >= a_pSection && IsSpace(*pTrail)) {
                --pTrail;
            }
            ++pTrail;
            *pTrail = 0;

            // skip to the end of the line
            ++a_pData;  // safe as checked that it == ']' above
            while (*a_pData && !IsNewLineChar(*a_pData)) {
                ++a_pData;
            }

            a_pKey = 0;
            a_pVal = 0;
            return true;
        }

        // find the end of the key name (it may contain spaces)
        // and convert it to lowercase as necessary
        a_pKey = a_pData;
        while (*a_pData && *a_pData != '=' && !IsNewLineChar(*a_pData)) {
            ++a_pData;
        }

        // if it's an invalid line, just skip it
        if (*a_pData != '=') {
            continue;
        }

        // empty keys are invalid
        if (a_pKey == a_pData) {
            while (*a_pData && !IsNewLineChar(*a_pData)) {
                ++a_pData;
            }
            continue;
        }

        // remove trailing spaces from the key
        pTrail = a_pData - 1;
        while (pTrail >= a_pKey && IsSpace(*pTrail)) {
            --pTrail;
        }
        ++pTrail;
        *pTrail = 0;

        // skip leading whitespace on the value
        ++a_pData;  // safe as checked that it == '=' above
        while (*a_pData && !IsNewLineChar(*a_pData) && IsSpace(*a_pData)) {
            ++a_pData;
        }

        // find the end of the value which is the end of this line
        a_pVal = a_pData;
        while (*a_pData && !IsNewLineChar(*a_pData)) {
            ++a_pData;
        }

        // remove trailing spaces from the value
        pTrail = a_pData - 1;
        if (*a_pData) { // prepare for the next round
            ++a_pData;
        }
        while (pTrail >= a_pVal && IsSpace(*pTrail)) {
            --pTrail;
        }
        ++pTrail;
        *pTrail = 0;

        // check for multi-line entries
        if (m_bAllowMultiLine && IsMultiLineTag(a_pVal)) {
            return FindMultiLine(a_pData, a_pVal);
        }

        // return the standard entry
        return true;
    }

    return false;
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
bool
CSimpleIniTempl<SI_CHAR,SI_STRLESS,SI_CONVERTER>::IsMultiLineTag(
    const SI_CHAR * a_pVal
    ) const
{
    // check for the "<<<" prefix for a multi-line entry
    if (*a_pVal++ != '<') return false;
    if (*a_pVal++ != '<') return false;
    if (*a_pVal++ != '<') return false;
    return true;
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
bool
CSimpleIniTempl<SI_CHAR,SI_STRLESS,SI_CONVERTER>::IsMultiLineData(
    const SI_CHAR * a_pData
    ) const
{
    // data is multi-line if it has any of the following features:
    //  * whitespace prefix 
    //  * embedded newlines
    //  * whitespace suffix 

    // empty string
    if (!*a_pData) {
        return false;
    }

    // check for prefix
    if (IsSpace(*a_pData)) {
        return true;
    }

    // embedded newlines
    while (*a_pData) {
        if (IsNewLineChar(*a_pData)) {
            return true;
        }
    }

    // check for suffix
    if (IsSpace(*--a_pData)) {
        return true;
    }

    return false;
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
bool
CSimpleIniTempl<SI_CHAR,SI_STRLESS,SI_CONVERTER>::IsNewLineChar(
    SI_CHAR a_c ) const
{
    return a_c == '\n' || a_c == '\r';
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
bool
CSimpleIniTempl<SI_CHAR,SI_STRLESS,SI_CONVERTER>::FindMultiLine(
    SI_CHAR *&        a_pData,
    const SI_CHAR *& a_pVal 
    ) const
{
    // skip the "<<<" to get the tag that will end the multiline
    const SI_CHAR * pTagName = a_pVal + 3;
    a_pVal = a_pData; // real value starts on next line

    // find the end tag. This tag must start in column 1 and be
    // followed by a newline. No whitespace removal is done while 
    // searching for this tag.
    SI_CHAR *pLine;
    SI_CHAR cRememberThis;
    for(;;) {
        // find the beginning and end of this line
        while (IsNewLineChar(*a_pData)) ++a_pData;
        pLine = a_pData;
        while (*a_pData && !IsNewLineChar(*a_pData)) ++a_pData;
        
        // end the line with a NULL
        cRememberThis = *a_pData;
        *a_pData = 0;

        // see if we have found the tag
        if (IsEqual(pLine, pTagName)) {
            // null terminate the data before the newline of the previous line. 
            // If you want a new line at the end of the line then add an empty 
            // line before the tag.
            --pLine;
            if (*(pLine-1) == '\r') {
                // handle Windows style newlines. This handles Unix newline files
                // on Windows and Windows style newlines on Unix. \n\r
                --pLine; 
            }
            *pLine = 0;

            if (cRememberThis) {
                ++a_pData;
            }
            return true;
        }

        // otherwise put the char back and continue checking
        if (!cRememberThis) {
            return false;
        }
        *a_pData++ = cRememberThis;
    }
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
SI_Error
CSimpleIniTempl<SI_CHAR,SI_STRLESS,SI_CONVERTER>::CopyString(
    const SI_CHAR *& a_pString)
{
    size_t uLen = 0;
    if (sizeof(SI_CHAR) == sizeof(char)) {
        uLen = strlen((const char *)a_pString);
    }
    else if (sizeof(SI_CHAR) == sizeof(wchar_t)) {
        uLen = wcslen((const wchar_t *)a_pString);
    }
    else {
        for ( ; a_pString[uLen]; ++uLen) /*loop*/ ;
    }
    ++uLen; // NULL character
    SI_CHAR * pCopy = new SI_CHAR[uLen];
    if (!pCopy) {
        return SI_NOMEM;
    }
    memcpy(pCopy, a_pString, sizeof(SI_CHAR)*uLen);
    m_strings.push_back(pCopy);
    a_pString = pCopy;
    return SI_OK;
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
SI_Error
CSimpleIniTempl<SI_CHAR,SI_STRLESS,SI_CONVERTER>::AddEntry(
    const SI_CHAR * a_pSection,
    const SI_CHAR * a_pKey,
    const SI_CHAR * a_pValue,
    bool            a_bCopyStrings)
{
    SI_Error rc;
    bool bInserted = false;

    // check for existence of the section first if we need string copies
    typename TSection::iterator iSection = m_data.end();
    if (a_bCopyStrings) {
        iSection = m_data.find(a_pSection);
        if (iSection == m_data.end()) {
            // if the section doesn't exist then we need a copy as the
            // string needs to last beyond the end of this function
            // because we will be inserting the section next
            rc = CopyString(a_pSection);
            if (rc < 0) return rc;
        }
    }

    // create the section entry
    if (iSection == m_data.end()) {
        std::pair<typename TSection::iterator,bool> i =
            m_data.insert(
                typename TSection::value_type( a_pSection, TKeyVal() ) );
        iSection = i.first;
        bInserted = true;
    }
    if (!a_pKey || !a_pValue) {
        // section only entries are specified with pKey and pVal as NULL
        return bInserted ? SI_INSERTED : SI_UPDATED;
    }

    // check for existence of the key
    TKeyVal & keyval = iSection->second;
    typename TKeyVal::iterator iKey = keyval.find(a_pKey);

    // make string copies if necessary
    if (a_bCopyStrings) {
        if (m_bAllowMultiKey || iKey == keyval.end()) {
            // if the key doesn't exist then we need a copy as the
            // string needs to last beyond the end of this function
            // because we will be inserting the key next
            rc = CopyString(a_pKey);
            if (rc < 0) return rc;
        }

        // we always need a copy of the value
        rc = CopyString(a_pValue);
        if (rc < 0) return rc;
    }

    // create the key entry
    if (iKey == keyval.end() || m_bAllowMultiKey) {
        iKey = keyval.insert(typename TKeyVal::value_type(a_pKey, (const SI_CHAR *)0));
        bInserted = true;
    }
    iKey->second = a_pValue;
    return bInserted ? SI_INSERTED : SI_UPDATED;
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
const SI_CHAR *
CSimpleIniTempl<SI_CHAR,SI_STRLESS,SI_CONVERTER>::GetValue(
    const SI_CHAR * a_pSection,
    const SI_CHAR * a_pKey,
    const SI_CHAR * a_pDefault,
    bool *          a_pHasMultiple) const
{
    if (a_pHasMultiple) {
        *a_pHasMultiple = false;
    }
    if (!a_pSection || !a_pKey) {
        return a_pDefault;
    }
    typename TSection::const_iterator iSection = m_data.find(a_pSection);
    if (iSection == m_data.end()) {
        return a_pDefault;
    }
    typename TKeyVal::const_iterator iKeyVal = iSection->second.find(a_pKey);
    if (iKeyVal == iSection->second.end()) {
        return a_pDefault;
    }

    // check for multiple entries with the same key
    if (m_bAllowMultiKey && a_pHasMultiple) {
        typename TKeyVal::const_iterator iTemp = iKeyVal;
        if (++iTemp != iSection->second.end()) {
            if (IsEqual(a_pKey, iTemp->first)) {
                *a_pHasMultiple = true;
            }
        }
    }

    return iKeyVal->second;
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
bool
CSimpleIniTempl<SI_CHAR,SI_STRLESS,SI_CONVERTER>::GetAllValues(
    const SI_CHAR * a_pSection,
    const SI_CHAR * a_pKey,
    TNamesDepend &  a_values) const
{
    if (!a_pSection || !a_pKey) {
        return false;
    }
    typename TSection::const_iterator iSection = m_data.find(a_pSection);
    if (iSection == m_data.end()) {
        return false;
    }
    typename TKeyVal::const_iterator iKeyVal = iSection->second.find(a_pKey);
    if (iKeyVal == iSection->second.end()) {
        return false;
    }

    // insert all values for this key
    a_values.push_back(iKeyVal->second);
    if (m_bAllowMultiKey) {
        ++iKeyVal;
        while (iKeyVal != iSection->second.end() && IsEqual(a_pKey, iKeyVal->first)) {
            a_values.push_back(iKeyVal->second);
            ++iKeyVal;
        }
    }
    return true;
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
int
CSimpleIniTempl<SI_CHAR,SI_STRLESS,SI_CONVERTER>::GetSectionSize(
    const SI_CHAR * a_pSection ) const
{
    if (!a_pSection) {
        return -1;
    }

    typename TSection::const_iterator iSection = m_data.find(a_pSection);
    if (iSection == m_data.end()) {
        return -1;
    }
    const TKeyVal & section = iSection->second;

    // if multi-key isn't permitted then the section size is
    // the number of keys that we have.
    if (!m_bAllowMultiKey || section.empty()) {
        return (int) section.size();
    }

    // otherwise we need to count them
    int nCount = 0;
    const SI_CHAR * pLastKey = 0;
    typename TKeyVal::const_iterator iKeyVal = section.begin();
    for (int n = 0; iKeyVal != section.end(); ++iKeyVal, ++n) {
        if (!pLastKey || !IsEqual(pLastKey, iKeyVal->first)) {
            ++nCount;
            pLastKey = iKeyVal->first;
        }
    }
    return nCount;
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
const typename CSimpleIniTempl<SI_CHAR,SI_STRLESS,SI_CONVERTER>::TKeyVal *
CSimpleIniTempl<SI_CHAR,SI_STRLESS,SI_CONVERTER>::GetSection(
    const SI_CHAR * a_pSection) const
{
    if (a_pSection) {
        typename TSection::const_iterator i = m_data.find(a_pSection);
        if (i != m_data.end()) {
            return &(i->second);
        }
    }
    return 0;
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
void
CSimpleIniTempl<SI_CHAR,SI_STRLESS,SI_CONVERTER>::GetAllSections(
    TNamesDepend & a_names ) const
{
    typename TSection::const_iterator i = m_data.begin();
    for (int n = 0; i != m_data.end(); ++i, ++n ) {
        a_names.push_back(i->first);
    }
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
void
CSimpleIniTempl<SI_CHAR,SI_STRLESS,SI_CONVERTER>::GetAllKeys(
    const SI_CHAR * a_pSection,
    TNamesDepend &  a_names ) const
{
    if (!a_pSection) {
        return;
    }

    typename TSection::const_iterator iSection = m_data.find(a_pSection);
    if (iSection == m_data.end()) {
        return;
    }

    const TKeyVal & section = iSection->second;
    const SI_CHAR * pLastKey = 0;
    typename TKeyVal::const_iterator iKeyVal = section.begin();
    for (int n = 0; iKeyVal != section.end(); ++iKeyVal, ++n ) {
        if (!pLastKey || !IsEqual(pLastKey, iKeyVal->first)) {
            a_names.push_back(iKeyVal->first);
            pLastKey = iKeyVal->first;
        }
    }
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
SI_Error
CSimpleIniTempl<SI_CHAR,SI_STRLESS,SI_CONVERTER>::Save(
    OutputWriter & a_oOutput) const
{
    Converter convert(m_bStoreIsUtf8);

    // iterate through our sections and output the data
    bool bFirstLine = true;
    typename TSection::const_iterator iSection = m_data.begin();
    for ( ; iSection != m_data.end(); ++iSection ) {
        // write the section (unless there is no section name)
        if (iSection->first[0]) {
            if (!convert.ConvertToStore(iSection->first)) {
                return SI_FAIL;
            }
            if (!bFirstLine) {
                a_oOutput.Write(SI_NEWLINE_A);
            }
            a_oOutput.Write("[");
            a_oOutput.Write(convert.Data());
            a_oOutput.Write("]");
            a_oOutput.Write(SI_NEWLINE_A);
        }

        // write all keys and values
        typename TKeyVal::const_iterator iKeyVal = iSection->second.begin();
        for ( ; iKeyVal != iSection->second.end(); ++iKeyVal) {
            // write the key
            if (!convert.ConvertToStore(iKeyVal->first)) {
                return SI_FAIL;
            }
            a_oOutput.Write(convert.Data());

            // write the value
            if (!convert.ConvertToStore(iKeyVal->second)) {
                return SI_FAIL;
            }
            a_oOutput.Write("=");
            if (m_bAllowMultiLine && IsMultiLineData(iKeyVal->second)) {
                a_oOutput.Write("<<<SI-END-OF-MULTILINE-TEXT" SI_NEWLINE_A);
                a_oOutput.Write(convert.Data());
                a_oOutput.Write(SI_NEWLINE_A "SI-END-OF-MULTILINE-TEXT");
            }
            else {
                a_oOutput.Write(convert.Data());
            }
            a_oOutput.Write(SI_NEWLINE_A);
        }

        bFirstLine = false;
    }

    return SI_OK;
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
bool
CSimpleIniTempl<SI_CHAR,SI_STRLESS,SI_CONVERTER>::Delete(
    const SI_CHAR * a_pSection,
    const SI_CHAR * a_pKey,
    bool            a_bRemoveEmpty)
{
    if (!a_pSection) {
        return false;
    }

    typename TSection::iterator iSection = m_data.find(a_pSection);
    if (iSection == m_data.end()) {
        return false;
    }

    // remove a single key if we have a keyname
    if (a_pKey) {
        typename TKeyVal::iterator iKeyVal = iSection->second.find(a_pKey);
        if (iKeyVal == iSection->second.end()) {
            return false;
        }

        // remove any copied strings and then the key
        typename TKeyVal::iterator iDelete;
        do {
            iDelete = iKeyVal++;

            DeleteString(iDelete->first);
            DeleteString(iDelete->second);
            iSection->second.erase(iDelete);
        }
        while (iKeyVal != iSection->second.end()
            && IsEqual(a_pKey, iKeyVal->first));

        // done now if the section is not empty or we are not pruning away
        // the empty sections. Otherwise let it fall through into the section
        // deletion code
        if (!a_bRemoveEmpty || !iSection->second.empty()) {
            return true;
        }
    }
    else {
        // delete all copied strings from this section. The actual
        // entries will be removed when the section is removed.
        typename TKeyVal::iterator iKeyVal = iSection->second.begin();
        for ( ; iKeyVal != iSection->second.end(); ++iKeyVal) {
            DeleteString(iKeyVal->first);
            DeleteString(iKeyVal->second);
        }
    }

    // delete the section itself
    DeleteString(iSection->first);
    m_data.erase(iSection);

    return true;
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
void
CSimpleIniTempl<SI_CHAR,SI_STRLESS,SI_CONVERTER>::DeleteString(
    const SI_CHAR * a_pString )
{
    // strings may exist either inside the data block, or they will be
    // individually allocated and stored in m_strings. We only physically
    // delete those stored in m_strings.
    if (a_pString < m_pData || a_pString >= m_pData + m_uDataLen) {
        typename TNamesDepend::iterator i = m_strings.begin();
        for (;i != m_strings.end(); ++i) {
            if (a_pString == *i) {
                delete[] const_cast<SI_CHAR*>(*i);
                m_strings.erase(i);
                break;
            }
        }
    }
}

// ---------------------------------------------------------------------------
//                              CONVERSION FUNCTIONS
// ---------------------------------------------------------------------------

// Defines the conversion classes for different libraries. Before including
// SimpleIni.h, set the converter that you wish you use by defining one of the
// following symbols.
//
//  SI_CONVERT_GENERIC      Use the Unicode reference conversion library in
//                          the accompanying files ConvertUTF.h/c
//  SI_CONVERT_ICU          Use the IBM ICU conversion library. Requires
//                          ICU headers on include path and icuuc.lib
//  SI_CONVERT_WIN32        Use the Win32 API functions for conversion.

#if !defined(SI_CONVERT_GENERIC) && !defined(SI_CONVERT_WIN32) && !defined(SI_CONVERT_ICU)
# ifdef _WIN32
#  define SI_CONVERT_WIN32
# else
#  define SI_CONVERT_GENERIC
# endif
#endif

/**
 * Generic case-sensitive <less> comparison. This class returns numerically
 * ordered ASCII case-sensitive text for all possible sizes and types of
 * SI_CHAR.
 */
template<class SI_CHAR>
struct SI_GenericCase {
	bool operator()(const SI_CHAR * pLeft, const SI_CHAR * pRight) const {
        long cmp;
        for ( ;*pLeft && *pRight; ++pLeft, ++pRight) {
            cmp = (long) *pLeft - (long) *pRight;
            if (cmp != 0) {
                return cmp < 0;
            }
        }
        return *pRight != 0;
    }
};

/**
 * Generic ASCII case-insensitive <less> comparison. This class returns
 * numerically ordered ASCII case-insensitive text for all possible sizes
 * and types of SI_CHAR. It is not safe for MBCS text comparison where
 * ASCII A-Z characters are used in the encoding of multi-byte characters.
 */
template<class SI_CHAR>
struct SI_GenericNoCase {
    inline SI_CHAR locase(SI_CHAR ch) const {
        return (ch < 'A' || ch > 'Z') ? ch : (ch - 'A' + 'a');
    }
	bool operator()(const SI_CHAR * pLeft, const SI_CHAR * pRight) const {
        long cmp;
        for ( ;*pLeft && *pRight; ++pLeft, ++pRight) {
            cmp = (long) locase(*pLeft) - (long) locase(*pRight);
            if (cmp != 0) {
                return cmp < 0;
            }
        }
        return *pRight != 0;
    }
};

/**
 * Null conversion class for MBCS/UTF-8 to char (or equivalent).
 */
template<class SI_CHAR>
class SI_ConvertA {
    bool m_bStoreIsUtf8;
protected:
    SI_ConvertA() { }
public:
    SI_ConvertA(bool a_bStoreIsUtf8) : m_bStoreIsUtf8(a_bStoreIsUtf8) { }

    /* copy and assignment */
    SI_ConvertA(const SI_ConvertA & rhs) { operator=(rhs); }
    SI_ConvertA & operator=(const SI_ConvertA & rhs) {
        m_bStoreIsUtf8 = rhs.m_bStoreIsUtf8;
        return *this;
    }

    /** Calculate the number of SI_CHAR required for converting the input
     * from the storage format. The storage format is always UTF-8 or MBCS.
     *
     * @param a_pInputData  Data in storage format to be converted to SI_CHAR.
     * @param a_uInputDataLen Length of storage format data in bytes. This
     *                      must be the actual length of the data, including
     *                      NULL byte if NULL terminated string is required.
     * @return              Number of SI_CHAR required by the string when
     *                      converted. If there are embedded NULL bytes in the
     *                      input data, only the string up and not including
     *                      the NULL byte will be converted.
     * @return              -1 cast to size_t on a conversion error.
     */
    size_t SizeFromStore(
        const char *    /*a_pInputData*/,
        size_t          a_uInputDataLen)
    {
        assert(a_uInputDataLen != (size_t) -1);

        // ASCII/MBCS/UTF-8 needs no conversion
        return a_uInputDataLen;
    }

    /** Convert the input string from the storage format to SI_CHAR.
     * The storage format is always UTF-8 or MBCS.
     *
     * @param a_pInputData  Data in storage format to be converted to SI_CHAR.
     * @param a_uInputDataLen Length of storage format data in bytes. This
     *                      must be the actual length of the data, including
     *                      NULL byte if NULL terminated string is required.
     * @param a_pOutputData Pointer to the output buffer to received the
     *                      converted data.
     * @param a_uOutputDataSize Size of the output buffer in SI_CHAR.
     * @return              true if all of the input data was successfully
     *                      converted.
     */
    bool ConvertFromStore(
        const char *    a_pInputData,
        size_t          a_uInputDataLen,
        SI_CHAR *       a_pOutputData,
        size_t          a_uOutputDataSize)
    {
        // ASCII/MBCS/UTF-8 needs no conversion
        if (a_uInputDataLen > a_uOutputDataSize) {
            return false;
        }
        memcpy(a_pOutputData, a_pInputData, a_uInputDataLen);
        return true;
    }

    /** Calculate the number of char required by the storage format of this
     * data. The storage format is always UTF-8 or MBCS.
     *
     * @param a_pInputData  NULL terminated string to calculate the number of
     *                      bytes required to be converted to storage format.
     * @return              Number of bytes required by the string when
     *                      converted to storage format. This size always
     *                      includes space for the terminating NULL character.
     * @return              -1 cast to size_t on a conversion error.
     */
    size_t SizeToStore(
        const SI_CHAR * a_pInputData)
    {
        // ASCII/MBCS/UTF-8 needs no conversion
        return strlen((const char *)a_pInputData) + 1;
    }

    /** Convert the input string to the storage format of this data.
     * The storage format is always UTF-8 or MBCS.
     *
     * @param a_pInputData  NULL terminated source string to convert. All of
     *                      the data will be converted including the
     *                      terminating NULL character.
     * @param a_pOutputData Pointer to the buffer to receive the converted
     *                      string.
     * @param a_pOutputDataSize Size of the output buffer in char.
     * @return              true if all of the input data, including the
     *                      terminating NULL character was successfully
     *                      converted.
     */
    bool ConvertToStore(
        const SI_CHAR * a_pInputData,
        char *          a_pOutputData,
        size_t          a_uOutputDataSize)
    {
        // calc input string length (SI_CHAR type and size independent)
        size_t uInputLen = strlen((const char *)a_pInputData) + 1;
        if (uInputLen > a_uOutputDataSize) {
            return false;
        }

        // ascii/UTF-8 needs no conversion
        memcpy(a_pOutputData, a_pInputData, uInputLen);
        return true;
    }
};


// ---------------------------------------------------------------------------
//                              SI_CONVERT_GENERIC
// ---------------------------------------------------------------------------
#ifdef SI_CONVERT_GENERIC

#define SI_Case     SI_GenericCase
#define SI_NoCase   SI_GenericNoCase

#include <wchar.h>
#include "ConvertUTF.h"

/**
 * Converts UTF-8 to a wchar_t (or equivalent) using the Unicode reference
 * library functions. This can be used on all platforms.
 */
template<class SI_CHAR>
class SI_ConvertW {
    bool m_bStoreIsUtf8;
protected:
    SI_ConvertW() { }
public:
    SI_ConvertW(bool a_bStoreIsUtf8) : m_bStoreIsUtf8(a_bStoreIsUtf8) { }

    /* copy and assignment */
    SI_ConvertW(const SI_ConvertW & rhs) { operator=(rhs); }
    SI_ConvertW & operator=(const SI_ConvertW & rhs) {
        m_bStoreIsUtf8 = rhs.m_bStoreIsUtf8;
        return *this;
    }

    /** Calculate the number of SI_CHAR required for converting the input
     * from the storage format. The storage format is always UTF-8 or MBCS.
     *
     * @param a_pInputData  Data in storage format to be converted to SI_CHAR.
     * @param a_uInputDataLen Length of storage format data in bytes. This
     *                      must be the actual length of the data, including
     *                      NULL byte if NULL terminated string is required.
     * @return              Number of SI_CHAR required by the string when
     *                      converted. If there are embedded NULL bytes in the
     *                      input data, only the string up and not including
     *                      the NULL byte will be converted.
     * @return              -1 cast to size_t on a conversion error.
     */
    size_t SizeFromStore(
        const char *    a_pInputData,
        size_t          a_uInputDataLen)
    {
        assert(a_uInputDataLen != (size_t) -1);

        if (m_bStoreIsUtf8) {
            // worst case scenario for UTF-8 to wchar_t is 1 char -> 1 wchar_t
            // so we just return the same number of characters required as for
            // the source text.
            return a_uInputDataLen;
        }
        else {
            return mbstowcs(NULL, a_pInputData, a_uInputDataLen);
        }
    }

    /** Convert the input string from the storage format to SI_CHAR.
     * The storage format is always UTF-8 or MBCS.
     *
     * @param a_pInputData  Data in storage format to be converted to SI_CHAR.
     * @param a_uInputDataLen Length of storage format data in bytes. This
     *                      must be the actual length of the data, including
     *                      NULL byte if NULL terminated string is required.
     * @param a_pOutputData Pointer to the output buffer to received the
     *                      converted data.
     * @param a_uOutputDataSize Size of the output buffer in SI_CHAR.
     * @return              true if all of the input data was successfully
     *                      converted.
     */
    bool ConvertFromStore(
        const char *    a_pInputData,
        size_t          a_uInputDataLen,
        SI_CHAR *       a_pOutputData,
        size_t          a_uOutputDataSize)
    {
        if (m_bStoreIsUtf8) {
            // This uses the Unicode reference implementation to do the
            // conversion from UTF-8 to wchar_t. The required files are
            // ConvertUTF.h and ConvertUTF.c which should be included in
            // the distribution but are publically available from unicode.org
            // at http://www.unicode.org/Public/PROGRAMS/CVTUTF/
            ConversionResult retval;
            const UTF8 * pUtf8 = (const UTF8 *) a_pInputData;
            if (sizeof(wchar_t) == sizeof(UTF32)) {
                UTF32 * pUtf32 = (UTF32 *) a_pOutputData;
                retval = ConvertUTF8toUTF32(
                    &pUtf8, pUtf8 + a_uInputDataLen,
                    &pUtf32, pUtf32 + a_uOutputDataSize,
                    lenientConversion);
            }
            else if (sizeof(wchar_t) == sizeof(UTF16)) {
                UTF16 * pUtf16 = (UTF16 *) a_pOutputData;
                retval = ConvertUTF8toUTF16(
                    &pUtf8, pUtf8 + a_uInputDataLen,
                    &pUtf16, pUtf16 + a_uOutputDataSize,
                    lenientConversion);
            }
            return retval == conversionOK;
        }
        else {
            size_t retval = mbstowcs(a_pOutputData,
                a_pInputData, a_uOutputDataSize);
            return retval != (size_t)(-1);
        }
    }

    /** Calculate the number of char required by the storage format of this
     * data. The storage format is always UTF-8 or MBCS.
     *
     * @param a_pInputData  NULL terminated string to calculate the number of
     *                      bytes required to be converted to storage format.
     * @return              Number of bytes required by the string when
     *                      converted to storage format. This size always
     *                      includes space for the terminating NULL character.
     * @return              -1 cast to size_t on a conversion error.
     */
    size_t SizeToStore(
        const SI_CHAR * a_pInputData)
    {
        if (m_bStoreIsUtf8) {
            // worst case scenario for wchar_t to UTF-8 is 1 wchar_t -> 6 char
            size_t uLen = 0;
            while (a_pInputData[uLen]) {
                ++uLen;
            }
            return (6 * uLen) + 1;
        }
        else {
            size_t uLen = wcstombs(NULL, a_pInputData, 0);
            if (uLen == (size_t)(-1)) {
                return uLen;
            }
            return uLen + 1; // include NULL terminator
        }
    }

    /** Convert the input string to the storage format of this data.
     * The storage format is always UTF-8 or MBCS.
     *
     * @param a_pInputData  NULL terminated source string to convert. All of
     *                      the data will be converted including the
     *                      terminating NULL character.
     * @param a_pOutputData Pointer to the buffer to receive the converted
     *                      string.
     * @param a_pOutputDataSize Size of the output buffer in char.
     * @return              true if all of the input data, including the
     *                      terminating NULL character was successfully
     *                      converted.
     */
    bool ConvertToStore(
        const SI_CHAR * a_pInputData,
        char *          a_pOutputData,
        size_t          a_uOutputDataSize)
    {
        if (m_bStoreIsUtf8) {
            // calc input string length (SI_CHAR type and size independent)
            size_t uInputLen = 0;
            while (a_pInputData[uInputLen]) {
                ++uInputLen;
            }
            ++uInputLen; // include the NULL char

            // This uses the Unicode reference implementation to do the
            // conversion from wchar_t to UTF-8. The required files are
            // ConvertUTF.h and ConvertUTF.c which should be included in
            // the distribution but are publically available from unicode.org
            // at http://www.unicode.org/Public/PROGRAMS/CVTUTF/
            ConversionResult retval;
            UTF8 * pUtf8 = (UTF8 *) a_pOutputData;
            if (sizeof(wchar_t) == sizeof(UTF32)) {
                const UTF32 * pUtf32 = (const UTF32 *) a_pInputData;
                retval = ConvertUTF32toUTF8(
                    &pUtf32, pUtf32 + uInputLen + 1,
                    &pUtf8, pUtf8 + a_uOutputDataSize,
                    lenientConversion);
            }
            else if (sizeof(wchar_t) == sizeof(UTF16)) {
                const UTF16 * pUtf16 = (const UTF16 *) a_pInputData;
                retval = ConvertUTF16toUTF8(
                    &pUtf16, pUtf16 + uInputLen + 1,
                    &pUtf8, pUtf8 + a_uOutputDataSize,
                    lenientConversion);
            }
            return retval == conversionOK;
        }
        else {
            size_t retval = wcstombs(a_pOutputData,
                a_pInputData, a_uOutputDataSize);
            return retval != (size_t) -1;
        }
    }
};

#endif // SI_CONVERT_GENERIC


// ---------------------------------------------------------------------------
//                              SI_CONVERT_ICU
// ---------------------------------------------------------------------------
#ifdef SI_CONVERT_ICU

#define SI_Case     SI_GenericCase
#define SI_NoCase   SI_GenericNoCase

#include <unicode/ucnv.h>

/**
 * Converts MBCS/UTF-8 to UChar using ICU. This can be used on all platforms.
 */
template<class SI_CHAR>
class SI_ConvertW {
    const char * m_pEncoding;
    UConverter * m_pConverter;
protected:
    SI_ConvertW() : m_pEncoding(NULL), m_pConverter(NULL) { }
public:
    SI_ConvertW(bool a_bStoreIsUtf8) : m_pConverter(NULL) {
        m_pEncoding = a_bStoreIsUtf8 ? "UTF-8" : NULL;
    }

    /* copy and assignment */
    SI_ConvertW(const SI_ConvertW & rhs) { operator=(rhs); }
    SI_ConvertW & operator=(const SI_ConvertW & rhs) {
        m_pEncoding = rhs.m_pEncoding;
        m_pConverter = NULL;
        return *this;
    }
    ~SI_ConvertW() { if (m_pConverter) ucnv_close(m_pConverter); }

    /** Calculate the number of UChar required for converting the input
     * from the storage format. The storage format is always UTF-8 or MBCS.
     *
     * @param a_pInputData  Data in storage format to be converted to UChar.
     * @param a_uInputDataLen Length of storage format data in bytes. This
     *                      must be the actual length of the data, including
     *                      NULL byte if NULL terminated string is required.
     * @return              Number of UChar required by the string when
     *                      converted. If there are embedded NULL bytes in the
     *                      input data, only the string up and not including
     *                      the NULL byte will be converted.
     * @return              -1 cast to size_t on a conversion error.
     */
    size_t SizeFromStore(
        const char *    a_pInputData,
        size_t          a_uInputDataLen)
    {
        assert(a_uInputDataLen != (size_t) -1);

        UErrorCode nError;

        if (!m_pConverter) {
            nError = U_ZERO_ERROR;
            m_pConverter = ucnv_open(m_pEncoding, &nError);
            if (U_FAILURE(nError)) {
                return (size_t) -1;
            }
        }

        nError = U_ZERO_ERROR;
        ucnv_resetToUnicode(m_pConverter);
        int32_t nLen = ucnv_toUChars(m_pConverter, NULL, 0,
            a_pInputData, (int32_t) a_uInputDataLen, &nError);
        if (nError != U_BUFFER_OVERFLOW_ERROR) {
            return (size_t) -1;
        }

        return (size_t) nLen;
    }

    /** Convert the input string from the storage format to UChar.
     * The storage format is always UTF-8 or MBCS.
     *
     * @param a_pInputData  Data in storage format to be converted to UChar.
     * @param a_uInputDataLen Length of storage format data in bytes. This
     *                      must be the actual length of the data, including
     *                      NULL byte if NULL terminated string is required.
     * @param a_pOutputData Pointer to the output buffer to received the
     *                      converted data.
     * @param a_uOutputDataSize Size of the output buffer in UChar.
     * @return              true if all of the input data was successfully
     *                      converted.
     */
    bool ConvertFromStore(
        const char *    a_pInputData,
        size_t          a_uInputDataLen,
        UChar *         a_pOutputData,
        size_t          a_uOutputDataSize)
    {
        UErrorCode nError;

        if (!m_pConverter) {
            nError = U_ZERO_ERROR;
            m_pConverter = ucnv_open(m_pEncoding, &nError);
            if (U_FAILURE(nError)) {
                return false;
            }
        }

        nError = U_ZERO_ERROR;
        ucnv_resetToUnicode(m_pConverter);
        ucnv_toUChars(m_pConverter,
            a_pOutputData, (int32_t) a_uOutputDataSize,
            a_pInputData, (int32_t) a_uInputDataLen, &nError);
        if (U_FAILURE(nError)) {
            return false;
        }

        return true;
    }

    /** Calculate the number of char required by the storage format of this
     * data. The storage format is always UTF-8 or MBCS.
     *
     * @param a_pInputData  NULL terminated string to calculate the number of
     *                      bytes required to be converted to storage format.
     * @return              Number of bytes required by the string when
     *                      converted to storage format. This size always
     *                      includes space for the terminating NULL character.
     * @return              -1 cast to size_t on a conversion error.
     */
    size_t SizeToStore(
        const UChar * a_pInputData)
    {
        UErrorCode nError;

        if (!m_pConverter) {
            nError = U_ZERO_ERROR;
            m_pConverter = ucnv_open(m_pEncoding, &nError);
            if (U_FAILURE(nError)) {
                return (size_t) -1;
            }
        }

        nError = U_ZERO_ERROR;
        ucnv_resetFromUnicode(m_pConverter);
        int32_t nLen = ucnv_fromUChars(m_pConverter, NULL, 0,
            a_pInputData, -1, &nError);
        if (nError != U_BUFFER_OVERFLOW_ERROR) {
            return (size_t) -1;
        }

        return (size_t) nLen + 1;
    }

    /** Convert the input string to the storage format of this data.
     * The storage format is always UTF-8 or MBCS.
     *
     * @param a_pInputData  NULL terminated source string to convert. All of
     *                      the data will be converted including the
     *                      terminating NULL character.
     * @param a_pOutputData Pointer to the buffer to receive the converted
     *                      string.
     * @param a_pOutputDataSize Size of the output buffer in char.
     * @return              true if all of the input data, including the
     *                      terminating NULL character was successfully
     *                      converted.
     */
    bool ConvertToStore(
        const UChar *   a_pInputData,
        char *          a_pOutputData,
        size_t          a_uOutputDataSize)
    {
        UErrorCode nError;

        if (!m_pConverter) {
            nError = U_ZERO_ERROR;
            m_pConverter = ucnv_open(m_pEncoding, &nError);
            if (U_FAILURE(nError)) {
                return false;
            }
        }

        nError = U_ZERO_ERROR;
        ucnv_resetFromUnicode(m_pConverter);
        ucnv_fromUChars(m_pConverter,
            a_pOutputData, (int32_t) a_uOutputDataSize,
            a_pInputData, -1, &nError);
        if (U_FAILURE(nError)) {
            return false;
        }

        return true;
    }
};

#endif // SI_CONVERT_ICU


// ---------------------------------------------------------------------------
//                              SI_CONVERT_WIN32
// ---------------------------------------------------------------------------
#ifdef SI_CONVERT_WIN32

#define SI_Case     SI_GenericCase

// Windows CE doesn't have errno or MBCS libraries
#ifdef _WIN32_WCE
# ifndef SI_NO_MBCS
#  define SI_NO_MBCS
# endif
#endif

#include <windows.h>
#ifdef SI_NO_MBCS
# define SI_NoCase   SI_GenericNoCase
#else // !SI_NO_MBCS
/**
 * Case-insensitive comparison class using Win32 MBCS functions. This class
 * returns a case-insensitive semi-collation order for MBCS text. It may not
 * be safe for UTF-8 text returned in char format as we don't know what
 * characters will be folded by the function! Therefore, if you are using
 * SI_CHAR == char and SetUnicode(true), then you need to use the generic
 * SI_NoCase class instead.
 */
#include <mbstring.h>
template<class SI_CHAR>
struct SI_NoCase {
	bool operator()(const SI_CHAR * pLeft, const SI_CHAR * pRight) const {
        if (sizeof(SI_CHAR) == sizeof(char)) {
            return _mbsicmp((const unsigned char *)pLeft,
                (const unsigned char *)pRight) < 0;
        }
        if (sizeof(SI_CHAR) == sizeof(wchar_t)) {
            return _wcsicmp((const wchar_t *)pLeft,
                (const wchar_t *)pRight) < 0;
        }
        return SI_GenericNoCase<SI_CHAR>()(pLeft, pRight);
    }
};
#endif // SI_NO_MBCS

/**
 * Converts MBCS and UTF-8 to a wchar_t (or equivalent) on Windows. This uses
 * only the Win32 functions and doesn't require the external Unicode UTF-8
 * conversion library. It will not work on Windows 95 without using Microsoft
 * Layer for Unicode in your application.
 */
template<class SI_CHAR>
class SI_ConvertW {
    UINT m_uCodePage;
protected:
    SI_ConvertW() { }
public:
    SI_ConvertW(bool a_bStoreIsUtf8) {
        m_uCodePage = a_bStoreIsUtf8 ? CP_UTF8 : CP_ACP;
    }

    /* copy and assignment */
    SI_ConvertW(const SI_ConvertW & rhs) { operator=(rhs); }
    SI_ConvertW & operator=(const SI_ConvertW & rhs) {
        m_uCodePage = rhs.m_uCodePage;
        return *this;
    }

    /** Calculate the number of SI_CHAR required for converting the input
     * from the storage format. The storage format is always UTF-8 or MBCS.
     *
     * @param a_pInputData  Data in storage format to be converted to SI_CHAR.
     * @param a_uInputDataLen Length of storage format data in bytes. This
     *                      must be the actual length of the data, including
     *                      NULL byte if NULL terminated string is required.
     * @return              Number of SI_CHAR required by the string when
     *                      converted. If there are embedded NULL bytes in the
     *                      input data, only the string up and not including
     *                      the NULL byte will be converted.
     * @return              -1 cast to size_t on a conversion error.
     */
    size_t SizeFromStore(
        const char *    a_pInputData,
        size_t          a_uInputDataLen)
    {
        assert(a_uInputDataLen != (size_t) -1);

        int retval = MultiByteToWideChar(
            m_uCodePage, 0,
            a_pInputData, (int) a_uInputDataLen,
            0, 0);
        return (size_t)(retval > 0 ? retval : -1);
    }

    /** Convert the input string from the storage format to SI_CHAR.
     * The storage format is always UTF-8 or MBCS.
     *
     * @param a_pInputData  Data in storage format to be converted to SI_CHAR.
     * @param a_uInputDataLen Length of storage format data in bytes. This
     *                      must be the actual length of the data, including
     *                      NULL byte if NULL terminated string is required.
     * @param a_pOutputData Pointer to the output buffer to received the
     *                      converted data.
     * @param a_uOutputDataSize Size of the output buffer in SI_CHAR.
     * @return              true if all of the input data was successfully
     *                      converted.
     */
    bool ConvertFromStore(
        const char *    a_pInputData,
        size_t          a_uInputDataLen,
        SI_CHAR *       a_pOutputData,
        size_t          a_uOutputDataSize)
    {
        int nSize = MultiByteToWideChar(
            m_uCodePage, 0,
            a_pInputData, (int) a_uInputDataLen,
            (wchar_t *) a_pOutputData, (int) a_uOutputDataSize);
        return (nSize > 0);
    }

    /** Calculate the number of char required by the storage format of this
     * data. The storage format is always UTF-8.
     *
     * @param a_pInputData  NULL terminated string to calculate the number of
     *                      bytes required to be converted to storage format.
     * @return              Number of bytes required by the string when
     *                      converted to storage format. This size always
     *                      includes space for the terminating NULL character.
     * @return              -1 cast to size_t on a conversion error.
     */
    size_t SizeToStore(
        const SI_CHAR * a_pInputData)
    {
        int retval = WideCharToMultiByte(
            m_uCodePage, 0,
            (const wchar_t *) a_pInputData, -1,
            0, 0, 0, 0);
        return (size_t) (retval > 0 ? retval : -1);
    }

    /** Convert the input string to the storage format of this data.
     * The storage format is always UTF-8 or MBCS.
     *
     * @param a_pInputData  NULL terminated source string to convert. All of
     *                      the data will be converted including the
     *                      terminating NULL character.
     * @param a_pOutputData Pointer to the buffer to receive the converted
     *                      string.
     * @param a_pOutputDataSize Size of the output buffer in char.
     * @return              true if all of the input data, including the
     *                      terminating NULL character was successfully
     *                      converted.
     */
    bool ConvertToStore(
        const SI_CHAR * a_pInputData,
        char *          a_pOutputData,
        size_t          a_uOutputDataSize)
    {
        int retval = WideCharToMultiByte(
            m_uCodePage, 0,
            (const wchar_t *) a_pInputData, -1,
            a_pOutputData, (int) a_uOutputDataSize, 0, 0);
        return retval > 0;
    }
};

#endif // SI_CONVERT_WIN32


// ---------------------------------------------------------------------------
//                                  TYPE DEFINITIONS
// ---------------------------------------------------------------------------

typedef CSimpleIniTempl<char,
    SI_NoCase<char>,SI_ConvertA<char> >                 CSimpleIniA;
typedef CSimpleIniTempl<char,
    SI_Case<char>,SI_ConvertA<char> >                   CSimpleIniCaseA;

#if defined(SI_CONVERT_ICU)
typedef CSimpleIniTempl<UChar,
    SI_NoCase<UChar>,SI_ConvertW<UChar> >               CSimpleIniW;
typedef CSimpleIniTempl<UChar,
    SI_Case<UChar>,SI_ConvertW<UChar> >                 CSimpleIniCaseW;
#else
typedef CSimpleIniTempl<wchar_t,
    SI_NoCase<wchar_t>,SI_ConvertW<wchar_t> >           CSimpleIniW;
typedef CSimpleIniTempl<wchar_t,
    SI_Case<wchar_t>,SI_ConvertW<wchar_t> >             CSimpleIniCaseW;
#endif

#ifdef _UNICODE
# define CSimpleIni      CSimpleIniW
# define CSimpleIniCase  CSimpleIniCaseW
# define SI_NEWLINE      SI_NEWLINE_W
#else // !_UNICODE
# define CSimpleIni      CSimpleIniA
# define CSimpleIniCase  CSimpleIniCaseA
# define SI_NEWLINE      SI_NEWLINE_A
#endif // _UNICODE

#endif // INCLUDED_SimpleIni_h

