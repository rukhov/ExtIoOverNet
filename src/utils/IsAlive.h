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

#include <memory>
#include <type_traits>

class AliveInstance
{
    using flag_ptr = std::shared_ptr<bool>;
    flag_ptr _flag;
public:

    friend class AliveFlag;

    AliveInstance(const AliveInstance&) = delete;
    AliveInstance(AliveInstance&&) = delete;
    AliveInstance& operator = (const AliveInstance&) = delete;
    AliveInstance& operator = (AliveInstance&&) = delete;

    AliveInstance() 
        : _flag(std::make_shared<bool>(true))
    {
    }

    ~AliveInstance()
    {
        *_flag = false;
    }

    template<typename FuncT>
    class FunctionWrapper final
    {
        using fnT = std::decay_t<FuncT>;
        flag_ptr _flag;
        fnT _func;

        bool check() const { return *_flag; }

    public:

        FunctionWrapper(flag_ptr const& flag, FuncT fn)
            : _func(std::forward<FuncT>(fn))
            , _flag(flag) {}

        template<typename ...Args>
        auto operator () (Args... args)
        {
            if (!check()) return std::invoke_result_t<fnT, Args...>();
            return _func(std::forward<Args>(args)...);
        }

        template<typename ...Args>
        auto operator () (Args... args) const
        {
            if (!check()) return std::invoke_result_t<fnT, Args...>();
            return _func(std::forward<Args>(args)...);
        }
    };

    template<typename FnT>
    auto Wrap(FnT fn)
    {
        return FunctionWrapper<FnT>(_flag, std::forward<FnT>(fn));
    }
};

class AliveFlag
{
    AliveInstance::flag_ptr _flag;
public:

    AliveFlag(AliveInstance& inst)
        : _flag(inst._flag)
    {
    }

    bool IsAlive() const
    {
        return *_flag;
    }
};

