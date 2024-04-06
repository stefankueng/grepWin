// grepWin - regex search and replace for Windows

// Copyright (C) 2011, 2015, 2024 - Stefan Kueng

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
#include "RegexReplaceFormatter.h"

std::wstring ExpandString(const std::wstring& replaceString)
{
    // ${now,formatString}
    wchar_t buf[4096] = {};
    GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, nullptr, nullptr, buf, _countof(buf));
    std::wstring dateStr = buf;
    GetTimeFormat(LOCALE_USER_DEFAULT, 0, nullptr, nullptr, buf, _countof(buf));
    dateStr += L" - ";
    dateStr += buf;
    std::time_t                                        now         = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    UINT                                               syntaxFlags = boost::regex::normal;
    boost::wregex                                      expression  = boost::wregex(L"\\$\\{now\\s*,?([^}]*)\\}", syntaxFlags);
    boost::match_results<std::wstring::const_iterator> whatC;
    boost::match_flag_type                             matchFlags   = boost::match_default | boost::format_all;
    auto                                               resultString = replaceString;
    SearchReplace(resultString, L"${now}", dateStr);

    try
    {
        auto start = resultString.cbegin();
        auto end   = resultString.cend();
        while (regex_search(start, end, whatC, expression, matchFlags))
        {
            if (whatC[0].matched)
            {
                auto         fullMatch = whatC.str();

                std::wstring formatStr;
                if (whatC.size() > 1)
                {
                    formatStr = whatC[1].str();
                    if (!formatStr.empty())
                    {
                        std::wstring formattedDateStr(4096, '\0');
                        struct tm    locTime;
                        _localtime64_s(&locTime, &now);
                        try
                        {
                            std::wcsftime(&formattedDateStr[0], formattedDateStr.size(), formatStr.c_str(), &locTime);
                            SearchReplace(resultString, fullMatch, formattedDateStr);
                        }
                        catch (const std::exception&)
                        {
                        }
                    }
                }
                else
                {
                    SearchReplace(resultString, fullMatch, dateStr);
                }
            }
            if (start == whatC[0].second)
            {
                if (start == end)
                    break;
                ++start;
            }
            else
                start = whatC[0].second;
        }
    }
    catch (const std::exception&)
    {
    }
    return resultString;
}