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

#include "boost_defines.h"
#include "Connection.h"
#include "Messages.h"
#include "Protocol.pb.h"

namespace Protocol
{
    constexpr uint32_t c_protocolVersion = 1;

    using AsyncCb_T = std::move_only_function<void(const boost::system::error_code&)>;
    using OnMsgCb_T = std::move_only_function<void(const boost::system::error_code&, const ExtIO_TCP_Proto::Message&, int64_t did)>;

    class IParser
    {
    public:
        
        static std::string const& GetMessageName(const ExtIO_TCP_Proto::Message& msg);

        virtual ~IParser() = default;

        virtual void Cancel() = 0;
        virtual void AsyncReceiveRequest(OnMsgCb_T&&) = 0;
        virtual void AsyncReceiveResponce(int64_t did, OnMsgCb_T&&) = 0;
        virtual void AsyncDisconnect(AsyncCb_T&& cb) = 0;
        virtual int64_t AsyncSendRequest(const ExtIO_TCP_Proto::Message& msg, OnMsgCb_T&& handler) = 0;
        virtual void AsyncSendResponce(const ExtIO_TCP_Proto::Message& msg, int64_t did, AsyncCb_T&& handler) = 0;
        virtual void AsyncSendMessage(std::unique_ptr<ExtIO_TCP_Proto::Message>&& msg, AsyncCb_T&& handler) = 0;
    };


    std::unique_ptr<IParser> MakeParser(IConnection&);

    template<typename T>
    std::tuple<std::promise<T>, std::future<T>> MakePFPair()
    {
        auto p = std::promise<T>();
        auto f = p.get_future();
        return std::make_tuple<>(std::move(p), std::move(f));
    }
}
