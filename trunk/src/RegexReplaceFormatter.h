// grepWin - regex search and replace for Windows

// Copyright (C) 2011 - Stefan Kueng

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
#pragma once
#include <string>
#include <stdio.h>
#include <wchar.h>
#include <algorithm>
#include <cctype>
#include <boost/regex.hpp>

class RegexReplaceFormatter
{
public:
    RegexReplaceFormatter(const std::wstring sReplace)
        : m_sReplace(sReplace)
    {

    }

    void SetReplacePair(const std::wstring& s1, const std::wstring& s2)
    {
        m_replaceMap[s1] = s2;
    }

    std::wstring operator()(boost::match_results<std::wstring::const_iterator> what) const
    {
        std::wstring sReplace = m_sReplace;
        if (m_replaceMap.size())
        {
            for (auto it = m_replaceMap.cbegin(); it != m_replaceMap.cend(); ++it)
            {
                auto it_begin = std::search(sReplace.begin(), sReplace.end(), it->first.begin(), it->first.end());
                if (it_begin != sReplace.end())
                {
                    if ((it_begin != sReplace.begin())&&((*(it_begin-1)) != '\\'))
                    {
                        auto it_end= it_begin + it->first.size();
                        sReplace.replace(it_begin, it_end, it->second);
                    }
                    else if ((*(it_begin-1)) == '\\')
                    {
                        sReplace.erase(it_begin-1);
                    };
                }
            }
        }

        return sReplace;
    }

private:
    std::wstring            m_sReplace;
    std::map<std::wstring, std::wstring> m_replaceMap;
};
