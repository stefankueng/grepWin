// grepWin - regex search and replace for Windows

// Copyright (C) 2011-2012, 2014-2015, 2021, 2023-2024 - Stefan Kueng

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
#include <algorithm>
#include <map>
#include "StringUtils.h"
#pragma warning(push)
#pragma warning(disable : 4996) // warning STL4010: Various members of std::allocator are deprecated in C++17
#include <boost/regex.hpp>
#pragma warning(pop)

template<typename CharT>
class NumberReplacer
{
public:
    NumberReplacer()
        : leadZero(false)
        , padding(0)
        , start(1)
        , increment(1)
    {
    }

    bool                     leadZero;
    int                      padding;
    int                      start;
    int                      increment;
    std::basic_string<CharT> expression;
};

std::wstring ExpandString(const std::wstring &replaceString);


// Iter is the same as the BidirectionalIterator in which `regex_replace` it is used
template<typename CharT, typename Iter = std::basic_string<CharT>::const_iterator>
class RegexReplaceFormatter
{
public:
    RegexReplaceFormatter(const std::basic_string<CharT>& str)
        : m_sReplace(str)
    {
        m_incVec.clear();
        // parse for ${count0L}, ${count0L(n)}, ${count0L(n,m)}, where
        // ${count}
        // is replaced later with numbers starting from 1, incremented by 1
        // ${count(n)}
        // is replaced with numbers starting from n, incremented by 1
        // ${count(n,m)}
        // is replaced with numbers starting from n, incremented by m
        // 0 and L are optional and specify the size of the right-aligned
        // number string. If 0 is specified, zeros are used for padding, otherwise spaces.
        // expr: "\\$\\{count(?<leadzero>0)?(?<length>\\d+)?(\\((?<startval>[-0-9]+)\\)||\\((?<startval>[-0-9]+),(?<increment>[-0-9]+)\\))?\\}"
        CharT expr[] = {
            '\\', '$', '\\', '{',
                'c', 'o', 'u', 'n', 't',
                '(', '?', '<', 'l', 'e', 'a', 'd', 'z', 'e', 'r', 'o', '>', '0', ')', '?',
                '(', '?', '<', 'l', 'e', 'n', 'g', 't', 'h', '>', '\\', 'd', '+', ')', '?',
                '(',
                    '\\', '(', '(', '?', '<', 's', 't', 'a', 'r', 't', 'v', 'a', 'l', '>', '[', '-', '0', '-', '9', ']', '+', ')', '\\', ')', '|', '|',
                    '\\', '(', '(', '?', '<', 's', 't', 'a', 'r', 't', 'v', 'a', 'l', '>', '[', '-', '0', '-', '9', ']', '+', ')', ',',
                               '(', '?', '<', 'i', 'n', 'c', 'r', 'e', 'm', 'e', 'n', 't', '>', '[', '-', '0', '-', '9', ']', '+', ')',
                    '\\', ')',
                ')', '?',
            '\\', '}', 0
        };
        boost::basic_regex<CharT>                                      regEx = boost::basic_regex<CharT>(expr, boost::regex::normal);
        boost::match_results<std::basic_string<CharT>::const_iterator> whatC;
        typename std::basic_string<CharT>::const_iterator              start = m_sReplace.begin();
        typename std::basic_string<CharT>::const_iterator              end   = m_sReplace.end();
        boost::match_flag_type                                         flags = boost::match_default | boost::format_all;
        while (boost::regex_search(start, end, whatC, regEx, flags))
        {
            if (whatC[0].matched)
            {
                NumberReplacer<CharT> nr;
                CharT leadzero[]      = {'l', 'e', 'a', 'd', 'z', 'e', 'r', 'o', 0};
                CharT zero[]          = {'0', 0};
                nr.leadZero    = (static_cast<std::basic_string<CharT>>(whatC[leadzero]) == zero);
                CharT length[] = {'l', 'e', 'n', 'g', 't', 'h', 0};
                nr.padding     = t_ttoi(static_cast<std::basic_string<CharT>>(whatC[length]).c_str());
                CharT startval[] = {'s', 't', 'a', 'r', 't', 'v', 'a', 'l', 0};
                std::basic_string<CharT> s = static_cast<std::basic_string<CharT>>(whatC[startval]);
                if (!s.empty())
                    nr.start = t_ttoi(s.c_str());
                CharT increment[] = {'i', 'n', 'c', 'r', 'e', 'm', 'e', 'n', 't', 0};
                s = static_cast<std::basic_string<CharT>>(whatC[increment]);
                if (!s.empty())
                    nr.increment = t_ttoi(s.c_str());
                if (nr.increment == 0)
                    nr.increment = 1;
                nr.expression = static_cast<std::basic_string<CharT>>(whatC[0]);
                m_incVec.push_back(nr);
            }
            // update search position:
            if (start == whatC[0].second)
            {
                if (start == end)
                    break;
                ++start;
            }
            else
                start = whatC[0].second;
            // update flags:
            flags |= boost::match_prev_avail;
            flags |= boost::match_not_bob;
        }
    }

    void SetReplacePair(const std::basic_string<CharT>& s1, const std::basic_string<CharT>& s2)
    {
        m_replaceMap[s1] = s2;
    }

    std::basic_string<CharT> operator()(boost::match_results<Iter> what)
    {
        std::basic_string<CharT> sReplace = what.format(m_sReplace);
        if (!m_replaceMap.empty())
        {
            for (auto it = m_replaceMap.cbegin(); it != m_replaceMap.cend(); ++it)
            {
                auto itBegin = std::search(sReplace.begin(), sReplace.end(), it->first.begin(), it->first.end());
                while (itBegin != sReplace.end())
                {
                    if ((itBegin == sReplace.begin()) || ((*(itBegin - 1)) != '\\'))
                    {
                        auto itEnd = itBegin + it->first.size();
                        sReplace.replace(itBegin, itEnd, it->second);
                    }
                    else if ((*(itBegin - 1)) == '\\')
                    {
                        sReplace.erase(itBegin - 1);
                    };
                    itBegin = std::search(sReplace.begin(), sReplace.end(), it->first.begin(), it->first.end());
                }
            }
        }
        if (!m_incVec.empty())
        {
            for (auto it = m_incVec.begin(); it != m_incVec.end(); ++it)
            {
                auto itBegin = std::search(sReplace.begin(), sReplace.end(), it->expression.begin(), it->expression.end());
                if (itBegin != sReplace.end())
                {
                    if ((itBegin == sReplace.begin()) || ((*(itBegin - 1)) != '\\'))
                    {
                        auto    itEnd      = itBegin + it->expression.size();
                        CharT   format[20] = {0};
                        if (it->padding)
                        {
                            CharT *fmt;
                            CharT fmt1[] = {'%', '%', '0', '%', 'd', 'd', 0};
                            CharT fmt2[] = {'%', '%', '%', 'd', 'd', 0};
                            if (it->leadZero)
                                fmt = fmt1;
                            else
                                fmt = fmt2;
                            t_stprintf_s(format, _countof(format), fmt, it->padding);
                        }
                        else
                        {
                            CharT fmt[] = {'%', 'd', 0};
                            t_tcscpy_s(format, fmt);
                        }
                        if (it->padding < 50)
                        {
                            // for small strings, reserve space on the stack
                            CharT buf[128] = {0};
                            t_stprintf_s(buf, _countof(buf), format, it->start);
                            sReplace.replace(itBegin, itEnd, buf);
                        }
                        else
                        {
                            auto s = CStringUtils::Format(format, it->start);
                            sReplace.replace(itBegin, itEnd, s);
                        }
                        it->start += it->increment;
                    }
                    else if ((*(itBegin - 1)) == '\\')
                    {
                        sReplace.erase(itBegin - 1);
                    };
                }
            }
        }

        return sReplace;
    }

private:
    int t_ttoi(const wchar_t *str)
    {
        return _wtoi(str);
    }

    int t_ttoi(const char *str)
    {
        return atoi(str);
    }

    int t_stprintf_s(wchar_t *buffer, size_t sizeOfBuffer, const wchar_t *format, ...)
    {
        int _Result;
        va_list _ArgList;
        __crt_va_start(_ArgList, format);
        _Result = _vswprintf_s_l(buffer, sizeOfBuffer, format, NULL, _ArgList);
        __crt_va_end(_ArgList);
        return _Result;
    }

    int t_stprintf_s(char *buffer, size_t sizeOfBuffer, const char *format, ...)
    {
        int _Result;
        va_list _ArgList;
        __crt_va_start(_ArgList, format);
        _Result = _vsprintf_s_l(buffer, sizeOfBuffer, format, NULL, _ArgList);
        __crt_va_end(_ArgList);
        return _Result;
    }

    template <size_t size>
    errno_t t_tcscpy_s(wchar_t (&dest)[size], const wchar_t *src)
    {
        return wcscpy_s(dest, size, src);
    }

    template <size_t size>
    errno_t t_tcscpy_s(char (&dest)[size], const char *src)
    {
        return strcpy_s(dest, size, src);
    }

private:
    std::vector<NumberReplacer<CharT>>                           m_incVec;
    std::basic_string<CharT>                                     m_sReplace;
    std::map<std::basic_string<CharT>, std::basic_string<CharT>> m_replaceMap;
};
