#include "stdafx.h"
#include "CmdLineParser.h"
#include <locale>
#include <algorithm>

const TCHAR CCmdLineParser::m_sDelims[] = _T("-/");
const TCHAR CCmdLineParser::m_sQuotes[] = _T("\"");
const TCHAR CCmdLineParser::m_sValueSep[] = _T(" :"); // don't forget space!!


CCmdLineParser::CCmdLineParser(LPCTSTR sCmdLine)
{
	if(sCmdLine) 
	{
		Parse(sCmdLine);
	}
}

CCmdLineParser::~CCmdLineParser()
{
	m_valueMap.clear();
}

BOOL CCmdLineParser::Parse(LPCTSTR sCmdLine) 
{
	const stdstring sEmpty = _T("");			//use this as a value if no actual value is given in commandline
	int nArgs = 0;

	if(!sCmdLine) 
		return false;
	
	m_valueMap.clear();
	m_sCmdLine = sCmdLine;

	LPCTSTR sCurrent = sCmdLine;

	for(;;)
	{
		//format is  -Key:"arg"
		
		if (_tcslen(sCurrent) == 0)
			break;		// no more data, leave loop

		LPCTSTR sArg = _tcspbrk(sCurrent, m_sDelims);
		if(!sArg) 
			break; // no (more) delimiters found
		sArg =  _tcsinc(sArg);

		if(_tcslen(sArg) == 0) 
			break; // ends with delim
			
		LPCTSTR sVal = _tcspbrk(sArg, m_sValueSep);
		if(sVal == NULL) 
		{ 
			stdstring Key(sArg);
			std::transform(Key.begin(), Key.end(), Key.begin(), ::tolower);
			m_valueMap.insert(CValsMap::value_type(Key, sEmpty));
			break;
		} 
		else if (sVal[0] == _T(' ') || _tcslen(sVal) == 1 ) 
		{ 
			// cmdline ends with /Key: or a key with no value 
			stdstring Key(sArg, (int)(sVal - sArg));
			if(!Key.empty()) 
			{ 
				std::transform(Key.begin(), Key.end(), Key.begin(), ::tolower);
				m_valueMap.insert(CValsMap::value_type(Key, sEmpty));
			}
			sCurrent = _tcsinc(sVal);
			continue;
		}
		else 
		{ 
			// key has value
			stdstring Key(sArg, (int)(sVal - sArg));
			std::transform(Key.begin(), Key.end(), Key.begin(), ::tolower);

			sVal = _tcsinc(sVal);

			LPCTSTR sQuote = _tcspbrk(sVal, m_sQuotes), sEndQuote(NULL);
			if(sQuote == sVal) 
			{ 
				// string with quotes (defined in m_sQuotes, e.g. '")
				sQuote = _tcsinc(sVal);
				sEndQuote = _tcspbrk(sQuote, m_sQuotes);
			} 
			else 
			{
				sQuote = sVal;
				sEndQuote = _tcschr(sQuote, _T(' '));
			}

			if(sEndQuote == NULL) 
			{ 
				// no end quotes or terminating space, take the rest of the string to its end
				stdstring csVal(sQuote);
				if(!Key.empty()) 
				{ 
					m_valueMap.insert(CValsMap::value_type(Key, csVal));
				}
				break;
			} 
			else 
			{ 
				// end quote
				if(!Key.empty()) 
				{	
					stdstring csVal(sQuote, (int)(sEndQuote - sQuote));
					m_valueMap.insert(CValsMap::value_type(Key, csVal));
				}
				sCurrent = _tcsinc(sEndQuote);
				continue;
			}
		}
	}
	
	return (nArgs > 0);		//TRUE if arguments were found
}

CCmdLineParser::CValsMap::const_iterator CCmdLineParser::findKey(LPCTSTR sKey) const 
{
	stdstring s(sKey);
	std::transform(s.begin(), s.end(), s.begin(), ::tolower);
	return m_valueMap.find(s);
}

BOOL CCmdLineParser::HasKey(LPCTSTR sKey) const 
{
	CValsMap::const_iterator it = findKey(sKey);
	if(it == m_valueMap.end()) 
		return false;
	return true;
}


BOOL CCmdLineParser::HasVal(LPCTSTR sKey) const 
{
	CValsMap::const_iterator it = findKey(sKey);
	if(it == m_valueMap.end()) 
		return false;
	if(it->second.empty()) 
		return false;
	return true;
}

LPCTSTR CCmdLineParser::GetVal(LPCTSTR sKey) const 
{
	CValsMap::const_iterator it = findKey(sKey);
	if (it == m_valueMap.end()) 
		return 0;
	return it->second.c_str();
}

LONG CCmdLineParser::GetLongVal(LPCTSTR sKey) const
{
	CValsMap::const_iterator it = findKey(sKey);
	if (it == m_valueMap.end())
		return 0;
	return _tstol(it->second.c_str());
}


CCmdLineParser::ITERPOS CCmdLineParser::begin() const 
{
	return m_valueMap.begin();
}

CCmdLineParser::ITERPOS CCmdLineParser::getNext(ITERPOS& pos, stdstring& sKey, stdstring& sValue) const 
{
	if (m_valueMap.end() == pos)
	{
		sKey.clear();
		return pos;
	} 
	else 
	{
		sKey = pos->first;
		sValue = pos->second;
		pos ++;
		return pos;
	}
}

BOOL CCmdLineParser::isLast(const ITERPOS& pos) const 
{
	return (pos == m_valueMap.end());
}
