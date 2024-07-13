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
#include <boost/regex.hpp>

template <typename CharT>
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

std::wstring ExpandString(const std::wstring& replaceString);

// Iter is the same as the BidirectionalIterator in which `regex_replace` it is used
template <typename CharT, typename Iter = typename std::basic_string<CharT>::const_iterator>
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
        constexpr CharT expr[] = {
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
            '\\', '}', 0};
        boost::basic_regex<CharT>                                               regEx = boost::basic_regex<CharT>(expr, boost::regex::normal);
        boost::match_results<typename std::basic_string<CharT>::const_iterator> whatC;
        typename std::basic_string<CharT>::const_iterator                       start = m_sReplace.begin();
        typename std::basic_string<CharT>::const_iterator                       end   = m_sReplace.end();
        boost::match_flag_type                                                  flags = boost::match_default | boost::format_all;
        while (boost::regex_search(start, end, whatC, regEx, flags))
        {
            if (whatC[0].matched)
            {
                NumberReplacer<CharT> nr;
                constexpr CharT       leadzero[]    = {'l', 'e', 'a', 'd', 'z', 'e', 'r', 'o', 0};
                constexpr CharT       zero[]        = {'0', 0};
                nr.leadZero                         = (static_cast<std::basic_string<CharT>>(whatC[leadzero]) == zero);
                constexpr CharT length[]            = {'l', 'e', 'n', 'g', 't', 'h', 0};
                nr.padding                          = t_ttoi(static_cast<std::basic_string<CharT>>(whatC[length]).c_str());
                constexpr CharT          startval[] = {'s', 't', 'a', 'r', 't', 'v', 'a', 'l', 0};
                std::basic_string<CharT> s          = static_cast<std::basic_string<CharT>>(whatC[startval]);
                if (!s.empty())
                    nr.start = t_ttoi(s.c_str());
                constexpr CharT increment[] = {'i', 'n', 'c', 'r', 'e', 'm', 'e', 'n', 't', 0};
                s                           = static_cast<std::basic_string<CharT>>(whatC[increment]);
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
            for (const auto& [key, value] : m_replaceMap)
            {
                auto itBegin = std::search(sReplace.begin(), sReplace.end(), key.begin(), key.end());
                while (itBegin != sReplace.end())
                {
                    if ((itBegin == sReplace.begin()) || ((*(itBegin - 1)) != '\\'))
                    {
                        auto itEnd = itBegin + key.size();
                        sReplace.replace(itBegin, itEnd, value);
                    }
                    else if ((*(itBegin - 1)) == '\\')
                    {
                        sReplace.erase(itBegin - 1);
                    };
                    itBegin = std::search(sReplace.begin(), sReplace.end(), key.begin(), key.end());
                }
            }
        }
        if (!m_incVec.empty())
        {
            for (auto& numberReplacer : m_incVec)
            {
                auto itBegin = std::search(sReplace.begin(), sReplace.end(), numberReplacer.expression.begin(), numberReplacer.expression.end());
                if (itBegin != sReplace.end())
                {
                    if ((itBegin == sReplace.begin()) || ((*(itBegin - 1)) != '\\'))
                    {
                        auto  itEnd      = itBegin + numberReplacer.expression.size();
                        CharT format[30] = {0};
                        if (numberReplacer.padding)
                        {
                            const CharT*    fmt;
                            constexpr CharT fmt1[] = {'%', '%', '0', '%', 'd', 'd', 0};
                            constexpr CharT fmt2[] = {'%', '%', '%', 'd', 'd', 0};
                            if (numberReplacer.leadZero)
                                fmt = fmt1;
                            else
                                fmt = fmt2;
                            t_stprintf_s(format, _countof(format), fmt, numberReplacer.padding);
                        }
                        else
                        {
                            constexpr CharT fmt[] = {'%', 'd', 0};
                            t_tcscpy_s(format, fmt);
                        }
                        if (numberReplacer.padding < 50)
                        {
                            // for small strings, reserve space on the stack
                            CharT buf[128] = {0};
                            t_stprintf_s(buf, _countof(buf), format, numberReplacer.start);
                            sReplace.replace(itBegin, itEnd, buf);
                        }
                        else
                        {
                            auto s = CStringUtils::Format(format, numberReplacer.start);
                            sReplace.replace(itBegin, itEnd, s);
                        }
                        numberReplacer.start += numberReplacer.increment;
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
    static int t_ttoi(const wchar_t* str)
    {
        return _wtoi(str);
    }

    static int t_ttoi(const char* str)
    {
        return atoi(str);
    }

    static int t_stprintf_s(wchar_t* buffer, size_t sizeOfBuffer, const wchar_t* format, ...)
    {
        va_list argList;
        __crt_va_start(argList, format);
        int result = _vswprintf_s_l(buffer, sizeOfBuffer, format, nullptr, argList);
        __crt_va_end(argList);
        return result;
    }

    static int t_stprintf_s(char* buffer, size_t sizeOfBuffer, const char* format, ...)
    {
        va_list argList;
        __crt_va_start(argList, format);
        int result = _vsprintf_s_l(buffer, sizeOfBuffer, format, nullptr, argList);
        __crt_va_end(argList);
        return result;
    }

    template <size_t Size>
    static errno_t t_tcscpy_s(wchar_t (&dest)[Size], const wchar_t* src)
    {
        return wcscpy_s(dest, Size, src);
    }

    template <size_t Size>
    static errno_t t_tcscpy_s(char (&dest)[Size], const char* src)
    {
        return strcpy_s(dest, Size, src);
    }

    std::vector<NumberReplacer<CharT>>                           m_incVec;
    std::basic_string<CharT>                                     m_sReplace;
    std::map<std::basic_string<CharT>, std::basic_string<CharT>> m_replaceMap;
};
