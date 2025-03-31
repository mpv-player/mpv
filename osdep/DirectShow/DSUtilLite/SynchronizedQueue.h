/*
 *      Copyright (C) 2010-2021 Hendrik Leppkes
 *      http://www.1f0.de
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#pragma once

#include <deque>

template <class T> class CSynchronizedQueue : public CCritSec
{
  public:
    CSynchronizedQueue(){};

    void Push(T item)
    {
        CAutoLock lock(this);

        m_queue.push_back(item);
    }

    T Pop(void)
    {
        CAutoLock lock(this);

        if (m_queue.empty())
            return nullptr;

        T item = m_queue.front();
        m_queue.pop_front();

        return item;
    }

    bool Empty()
    {
        CAutoLock lock(this);

        return m_queue.empty();
    }

    size_t Size()
    {
        CAutoLock lock(this);

        return m_queue.size();
    }

  private:
    std::deque<T> m_queue;
};
