#pragma once
#include <atomic>
#include <vector>

template <typename CharT = char>
class TextOffset
{
private:
    long                lenBOM;
    std::vector<size_t> linePositions;

public:
    TextOffset()
        : lenBOM(0)
    {
    }

    // UTF8
    const char* SkipBOM(const char* start, const char* end)
    {
        char BOM[] = "\xEF\xBB\xBF";
        if (end - start > 2 && memcmp(start, BOM, 3) == 0)
        {
            lenBOM = 3;
            return start + 3;
        }
        return start;
    }

    // UTF-16LE, UTF-16BE
    const wchar_t* SkipBOM(const wchar_t* start, const wchar_t* end)
    {
        if (end - start > 1 && (*start == 0xFEFF || *start == 0xFFFE))
        {
            lenBOM = 1;
            return start + 1;
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
            if (*it == '\r')
            {
                // cr lineending
                bGot = true;
                if (it + 1 < end)
                {
                    if (it[1] == '\n')
                    {
                        // crlf lineending
                        ++it;
                        ++pos;
                    }
                }
            }
            else if (*it == '\n')
            {
                // lf lineending
                bGot = true;
            }
            if (bGot)
                linePositions.push_back(pos);
            ++pos;
        }
        if (!bGot)
            linePositions.push_back(pos);
        return true;
    }

    long LineFromPosition(long pos) const
    {
        auto lb     = std::lower_bound(linePositions.begin(), linePositions.end(), static_cast<size_t>(pos));
        auto lbLine = lb - linePositions.begin();
        return static_cast<long>(lbLine + 1);
    }

    long ColumnFromPosition(long pos, long line) const
    {
        if (line < 0)
            line = LineFromPosition(pos);
        long lastLineEnd = -1;
        if (line > 1)
            lastLineEnd = static_cast<long>(linePositions[line - 2]);
        else
            lastLineEnd += lenBOM;
        return pos - lastLineEnd;
    }
};
