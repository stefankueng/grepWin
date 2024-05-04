// grepWin - regex search and replace for Windows

// Copyright (C) 2024 - Stefan Kueng

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
#include <atomic>
#include <vector>

template <typename CharT = char>
class TextOffset
{
private:
    long                lenBOM; // by the encoding
    bool                bBigEndian;
    std::vector<size_t> linePositions;

public:
    TextOffset()
        : lenBOM(0)
        , bBigEndian(false)
    {
    }

    // forcibly treated as char
    const char* SkipBOM(const char* start, const char* end)
    {
        char BOM[] = "\xEF\xBB\xBF";
        if (end - start > 2 && memcmp(start, BOM, 3) == 0)
        {
            lenBOM = 3;
            return start + 3;
        }
        else if (end - start > 1)
        {
            const wchar_t* startW = reinterpret_cast<const wchar_t*>(start);
            if (*startW == 0xFEFF || (bBigEndian = *startW == 0xFFFE) == true)
            {
                lenBOM = 2;
                return start + 2;
            }
        }
        return start;
    }

    bool CalculateLines(const CharT* start, const CharT* end, const std::atomic_bool& bCancelled)
    {
        if (start >= end)
            return false;

        linePositions.clear();
        linePositions.reserve((end - start) / 10);

        size_t pos  = 0;
        bool   bGot = false;
        for (auto it = start; it < end && !bCancelled; ++it)
        {
            bGot = false;
            if (*it == '\r' || (bBigEndian && *it == 0x0d00))
            {
                // cr lineending
                bGot = true;
                if (it + 1 < end)
                {
                    if (it[1] == '\n' || (bBigEndian && *it == 0x0a00))
                    {
                        // crlf lineending
                        ++it;
                        ++pos;
                    }
                }
            }
            else if (*it == '\n' || (bBigEndian && *it == 0x0a00))
            {
                // lf lineending
                bGot = true;
            }
            ++pos;
            if (bGot)
                linePositions.push_back(pos);
        }
        if (!bGot)
            linePositions.push_back(pos);
        return true;
    }

    long LineFromPosition(long pos) const
    {
        auto lb     = std::ranges::lower_bound(linePositions, static_cast<size_t>(pos));
        auto lbLine = lb - linePositions.begin();
        return static_cast<long>(lbLine + 1);
    }

    std::tuple<size_t, size_t> PositionsFromLine(long line) const
    {
        if (line > 0 && static_cast<size_t>(line) <= linePositions.size())
            return std::make_tuple(line > 1 ? linePositions[line - 2] - lenBOM : 0, linePositions[line - 1] - lenBOM);
        return std::make_tuple(-1, -1);
    }

    long ColumnFromPosition(long pos, long line) const
    {
        if (line < 0)
            line = LineFromPosition(pos);
        long lastLineEnd = -1;
        if (line > 1)
            lastLineEnd = static_cast<long>(linePositions[line - 2]) - lenBOM;
        else
            lastLineEnd = lenBOM;
        return pos - lastLineEnd;
    }
};
