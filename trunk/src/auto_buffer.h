// TortoiseSVN - a Windows shell extension for easy version control

// Copyright (C) 2009 - TortoiseSVN

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

/**
 * A simplified analog to std::auto_ptr<> that encapsulates
 * an array allocated dynamically via new[].
 *
 * Use this where you could not use a std::auto_ptr<> (works
 * for single elements only) nor a std::vector<> (no guarantees
 * w.r.t. to internal organization, i.e. no access to mem buffer).
 */

template<class T>
class auto_buffer
{
private:

    T* buffer;

    /// no copy nor assignment

    auto_buffer(const auto_buffer&);
    auto_buffer& operator=(const auto_buffer&);

public:

    explicit auto_buffer (size_t size = 0) throw()
        : buffer (size == 0 ? NULL : new T[size]())
    {
    }

    ~auto_buffer()
    {
        delete[] buffer;
    }

    operator T*() const throw()
    {
        return buffer;
    }

    operator void*() const throw()
    {
        return buffer;
    }

    operator bool() const throw()
    {
        return buffer != NULL;
    }

    T* operator->() const throw()
    {
        return buffer;
    }

    T *get() const throw()
    {
        return buffer;
    }

    T* release() throw()
    {
        T* temp = buffer;
        buffer = NULL;
        return temp;
    }

    void reset (size_t newSize = 0)
    {
        delete[] buffer;
        buffer = (newSize == 0 ? NULL : new T[newSize]);
    }
};
