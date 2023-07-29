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

#include "stdafx.h"

#include "majordomo.h"
#include "session.h"
#include "options.h"
#include "../utils/log.h"

using namespace boost;

namespace
{
    class Majordomo : public IMajordomo
    {
        using strand_type = asio::strand<asio::io_context::executor_type>;
        using work_guard_type = asio::executor_work_guard<strand_type>;

        strand_type& _strand;
        std::unique_ptr<work_guard_type> _work_guard;
        asio::ip::tcp::acceptor _acceptor;
        bool _isStopping = false;

        std::vector<std::weak_ptr<ISession>> _sessions;

    public:
        Majordomo(strand_type& strand)
            : _strand(strand)
            , _work_guard(std::make_unique<work_guard_type>(
                asio::make_work_guard(_strand)))
            , _acceptor(strand, 
                asio::ip::tcp::endpoint(asio::ip::tcp::v4(), Options::get().listeningPort))
        {
            AsyncAccept();
        }

    private:

        ~Majordomo()
        {

        }

        void AsyncStop() override
        {
            _isStopping = true;

            asio::post(_strand, [this]() {
                for (auto& s : _sessions)
                {
                    auto sp = s.lock(); if (sp) sp->AyncStop();
                }
                
                });

            asio::post( _strand, [this]() {
                _work_guard.reset();
                delete this;
                } );
        }

        void AsyncAccept()
        {
            if (_isStopping)
                return;

            _acceptor.async_accept(
                [this](system::error_code ec, asio::ip::tcp::socket socket)
                {
                    OnAccept(ec, std::move(socket));
                });
        }

        void OnAccept(const system::error_code& ec, asio::ip::tcp::socket&& socket)
        {
            if (_isStopping)
                return;

            if (ec.failed()) 
            {
                AsyncAccept();
            }

            LOG(info) << "New client accepted.";

            boost::asio::socket_base::keep_alive option(true);
            socket.set_option(option);

            auto ftr = MakeSession(
                boost::asio::make_strand(_strand.get_inner_executor()),
                std::move(socket));

            auto session = ftr.get();

            _sessions.push_back(session);

            AsyncAccept();
        }
    };
}

std::future<IMajordomo&> MakeMajordomo(
    boost::asio::strand<boost::asio::io_context::executor_type>& strand)
{
    std::promise<IMajordomo&> prom;

    auto retVal = prom.get_future();

    boost::asio::post(
        strand,
        [prom = std::move(prom), &strand]() mutable
        {
            prom.set_value(*(new Majordomo(strand)));
        });

    return retVal;
}
