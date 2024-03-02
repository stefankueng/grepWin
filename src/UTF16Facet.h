#pragma once
#include <locale>


// custom utf16 facet because codecvt_utf16 is deprecated and will be removed in C++26
// this is basically a simplified version of codecvt_utf16 with the template parateters fixed
class UTF16Facet : public std::codecvt<wchar_t, char, mbstate_t>
{
    // facet for converting between wchar_t and UTF-16 multibyte sequences
    enum
    {
        Bytes_Per_Word = 2
    };

public:
    explicit UTF16Facet(size_t refs = 0)
        : std::codecvt<wchar_t, char, mbstate_t>(refs)
    {
    }

    ~UTF16Facet() noexcept override {}

protected:
    result do_in(mbstate_t& /*state*/, const char* first1, const char* last1, const char*& mid1,
                 wchar_t* first2, wchar_t* last2, wchar_t*& mid2) const override
    {
        // convert bytes [_First1, _Last1) to [_First2, _Last)
        mid1 = first1;
        mid2 = first2;

        while (Bytes_Per_Word <= last1 - mid1 && mid2 != last2)
        {
            // convert a multibyte sequence
            const auto     ptr = reinterpret_cast<const unsigned char*>(mid1);
            unsigned long  ch;
            unsigned short ch0 = static_cast<unsigned short>(ptr[1] << 8 | ptr[0]);

            if (ch0 < 0xd800 || 0xdc00 <= ch0)
            {
                // one word, consume bytes
                mid1 += Bytes_Per_Word;
                ch = ch0;
            }
            else if (last1 - mid1 < 2 * Bytes_Per_Word)
            {
                break;
            }
            else
            {
                // get second word
                unsigned short ch1 = static_cast<unsigned short>(ptr[3] << 8 | ptr[2]);

                if (ch1 < 0xdc00 || 0xe000 <= ch1)
                {
                    return __super::error;
                }

                mid1 += 2 * Bytes_Per_Word;
                ch = static_cast<unsigned long>(ch0 - 0xd800 + 0x0040) << 10 | (ch1 - 0xdc00);
            }

            if (0x10ffff < ch)
            {
                return __super::error; // code too large
            }
            *mid2++ = static_cast<wchar_t>(ch);
        }

        return first1 == mid1 ? __super::partial : __super::ok;
    }

    result do_out(mbstate_t& /*state*/, const wchar_t* first1, const wchar_t* last1, const wchar_t*& mid1,
                  char* first2, char* last2, char*& mid2) const override
    {
        mid1 = first1;
        mid2 = first2;

        while (mid1 != last1 && Bytes_Per_Word <= last2 - mid2)
        {
            // convert and put a wide char
            bool          extra = false;
            unsigned long ch    = static_cast<unsigned long>(*mid1++);

            if (0x10ffff < ch)
            {
                return __super::error; // value too large
            }
            else if (ch <= 0xffff)
            {
                // one word, can't be code for first of two
                if (0xd800 <= ch && ch < 0xdc00)
                {
                    return __super::error;
                }
            }
            else if (last2 - mid2 < 2 * Bytes_Per_Word)
            {
                // not enough room for two-word output, back up
                --mid1;
                return __super::partial;
            }
            else
            {
                extra = true;
            }

            if (!extra)
            {
                // put a single word LS byte first
                *mid2++ = static_cast<char>(ch);
                *mid2++ = static_cast<char>(ch >> 8);
            }
            else
            {
                // put a pair of words LS byte first
                unsigned short ch0 =
                    static_cast<unsigned short>(0xd800 | static_cast<unsigned short>(ch >> 10) - 0x0040);
                *mid2++ = static_cast<char>(ch0);
                *mid2++ = static_cast<char>(ch0 >> 8);

                ch0     = static_cast<unsigned short>(0xdc00 | (static_cast<unsigned short>(ch) & 0x03ff));
                *mid2++ = static_cast<char>(ch0);
                *mid2++ = static_cast<char>(ch0 >> 8);
            }
        }

        return first1 == mid1 ? __super::partial : __super::ok;
    }
};
