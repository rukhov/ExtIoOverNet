/*****************************************************************************
 * This file is a part of ExtIoOverNet software.
 * 
 * Copyright (C) 2023 Roman Ukhov. All rights reserved.
 * 
 * Licensed under the GNU General Public License Version 3 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at 
 * https://www.gnu.org/licenses/gpl-3.0.txt
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-FileCopyrightText: 2023 Roman Ukhov <ukhov.roman@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *****************************************************************************/

#pragma once

template<typename T>
class Mutexed final
{
    T data;
    std::recursive_mutex mx;
public:

    class Ptr final
    {
        Mutexed* m;
    public:

        Ptr() : m(nullptr) {}

        Ptr(Mutexed& _m) :m(&_m) 
        {
            if(m) m->mx.lock();
        }

        ~Ptr()
        {
            if (m) m->mx.unlock();
        }

        Ptr(const Mutexed&) = delete;
        Ptr& operator = (const Mutexed&) = delete;

        const T* operator -> () const {
            return &m->data;
        }

        T* operator -> () {
            return &m->data;
        }

        const T& operator * () const {
            return m->data;
        }

        T& operator * () {
            return m->data;
        }
    };

    Mutexed() = default;
    Mutexed(const T& d) : data(d) { }
    Mutexed(T&& d) : data(std::move(d)) { }

    Mutexed& operator = (const T& d) = delete;
    Mutexed& operator = (T&& d) = delete;

    Ptr lock() {
        return Ptr(*this);
    }
};
