// grepWin - regex search and replace for Windows

// Copyright (C) 2011-2012 - Stefan Kueng

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
#include <map>
#include <boost/regex.hpp>

class NumberReplacer
{
public:
    NumberReplacer()
        : leadzero(false)
        , padding(0)
        , start(1)
        , increment(1)
    {

    }

    bool            leadzero;
    int             padding;
    int             start;
    int             increment;
    std::wstring    expression;
};

extern std::vector<NumberReplacer> g_incVec;

class RegexReplaceFormatter
{
public:
    RegexReplaceFormatter(const std::wstring sReplace)
        : m_sReplace(sReplace)
    {
        g_incVec.clear();
        // parse for ${count0L}, ${count0L(n)}, ${count0L(n,m)}, where
        // ${count}
        // is replaced later with numbers starting from 1, incremented by 1
        // ${count(n)}
        // is replaced with numbers starting from n, incremented by 1
        // ${count(n,m)}
        // is replaced with numbers starting from n, incremented by m
        // 0 and L are optional and specify the size of the right-aligned
        // number string. If 0 is specified, zeros are used for padding, otherwise spaces.
        //boost::wregex expression = boost::wregex(L"(?<!\\\\)\\$\\{count(?<leadzero>0)?(?<length>\\d+)?(\\((?<startval>[-0-9]+)\\)||\\((?<startval>[-0-9]+),(?<increment>[-0-9]+)\\))?\\}", boost::regex::normal);
        boost::wregex expression = boost::wregex(L"\\$\\{count(?<leadzero>0)?(?<length>\\d+)?(\\((?<startval>[-0-9]+)\\)||\\((?<startval>[-0-9]+),(?<increment>[-0-9]+)\\))?\\}", boost::regex::normal);
        boost::match_results<std::wstring::const_iterator> whatc;
        std::wstring::const_iterator start, end;
        start = m_sReplace.begin();
        end = m_sReplace.end();
        boost::match_flag_type flags = boost::match_default | boost::format_all;
        while (boost::regex_search(start, end, whatc, expression, flags))
        {
            if (whatc[0].matched)
            {
                NumberReplacer nr;
                nr.leadzero = (((std::wstring)whatc[L"leadzero"]) == L"0");
                nr.padding = _wtoi(((std::wstring)whatc[L"length"]).c_str());
                std::wstring s = (std::wstring)whatc[L"startval"];
                if (s.size())
                    nr.start = _wtoi(s.c_str());
                s = (std::wstring)whatc[L"increment"];
                if (s.size())
                    nr.increment = _wtoi(s.c_str());
                if (nr.increment == 0)
                    nr.increment = 1;
                nr.expression = (std::wstring)whatc[0];
                g_incVec.push_back(nr);
            }
            // update search position:
            if (start == whatc[0].second)
            {
                if (start == end)
                    break;
                start++;
            }
            else
                start = whatc[0].second;
            // update flags:
            flags |= boost::match_prev_avail;
            flags |= boost::match_not_bob;
        }
    }

    void SetReplacePair(const std::wstring& s1, const std::wstring& s2)
    {
        m_replaceMap[s1] = s2;
    }

    std::wstring operator()(boost::match_results<std::wstring::const_iterator> what) const
    {
        std::wstring sReplace = what.format(m_sReplace);
        if (m_replaceMap.size())
        {
            for (auto it = m_replaceMap.cbegin(); it != m_replaceMap.cend(); ++it)
            {
                auto it_begin = std::search(sReplace.begin(), sReplace.end(), it->first.begin(), it->first.end());
                while (it_begin != sReplace.end())
                {
                    if ((it_begin == sReplace.begin())||((*(it_begin-1)) != '\\'))
                    {
                        auto it_end= it_begin + it->first.size();
                        sReplace.replace(it_begin, it_end, it->second);
                    }
                    else if ((*(it_begin-1)) == '\\')
                    {
                        sReplace.erase(it_begin-1);
                    };
                    it_begin = std::search(sReplace.begin(), sReplace.end(), it->first.begin(), it->first.end());
                }
            }
        }
        if (g_incVec.size())
        {
            for (auto it = g_incVec.begin(); it != g_incVec.end(); ++it)
            {
                auto it_begin = std::search(sReplace.begin(), sReplace.end(), it->expression.begin(), it->expression.end());
                if (it_begin != sReplace.end())
                {
                    if ((it_begin == sReplace.begin())||((*(it_begin-1)) != '\\'))
                    {
                        auto it_end= it_begin + it->expression.size();
                        wchar_t format[10] = {0};
                        if (it->padding)
                        {
                            if (it->leadzero)
                                swprintf_s(format, _countof(format), L"%%0%dd", it->padding);
                            else
                                swprintf_s(format, _countof(format), L"%%%dd", it->padding);
                        }
                        else
                            wcscpy_s(format, L"%d");
                        wchar_t buf[50] = {0};
                        swprintf_s(buf, _countof(buf), format, it->start);
                        sReplace.replace(it_begin, it_end, buf);
                        it->start += it->increment;
                    }
                    else if ((*(it_begin-1)) == '\\')
                    {
                        sReplace.erase(it_begin-1);
                    };
                }
            }
        }

        //sReplace = boost::regex_replace(what[0].str(), sReplace, boost::match_default);

        return sReplace;
    }

private:
    std::wstring            m_sReplace;
    std::map<std::wstring, std::wstring> m_replaceMap;
};
