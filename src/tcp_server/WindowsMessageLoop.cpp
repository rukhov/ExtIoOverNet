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

#include "WindowsMessageLoop.h"

#include <windows.h>

namespace
{
    class MainWidget : public IMessageLoop
    {
        std::thread _thread;
        boost::synchronized_value<std::queue<IMessageLoop::task_t>> _taskQueue;
        UINT _newTaskMsg = ::RegisterWindowMessage("ExtIOoverNet-IMessageLoop-new-task");
        std::barrier<> _msgLoopSyncPoint;

    public:

        MainWidget()
            : _msgLoopSyncPoint(2)
        {
            _thread = std::thread([this]() { Run(); });
            _msgLoopSyncPoint.arrive_and_wait();
        }

        ~MainWidget()
        {
            ::PostThreadMessage(::GetThreadId(_thread.native_handle()), WM_QUIT, 0, 0);
            _thread.join();
        }

        // IMainWidget
    private:

        void post(task_t&& t) override
        {
            _taskQueue->push(std::move(t));
            ::PostThreadMessage(::GetThreadId(_thread.native_handle()), _newTaskMsg, 0, 0);
        }

        void send(task_t&& t) override
        {
            auto p = std::make_shared<std::promise<void>>();
            auto f = p->get_future();
            post([t = std::move(t), p = std::move(p)]() mutable {
                t();
                p->set_value();
                });
            f.wait();
        }

    private:

        int Run()
        {
            BOOL bRet = FALSE;
            MSG msg;

            _msgLoopSyncPoint.arrive_and_wait();

            // Start the message loop. 
            while ((bRet = ::GetMessage(&msg, NULL, 0, 0)) != 0)
            {
                if (bRet == -1)
                {
                    // handle the error and possibly exit
                    break;
                }
                else if (msg.message == _newTaskMsg)
                {
                    auto q = _taskQueue.synchronize();
                    while (!q->empty())
                    {
                        auto t = std::move(q->front());
                        q->pop();
                        t();
                    }
                }
                else
                {
                    ::TranslateMessage(&msg);
                    ::DispatchMessage(&msg);
                }
            }

            return bRet;
        }
    };
}

std::unique_ptr<IMessageLoop> MakeMessageLoop()
{
    return std::make_unique<MainWidget>();
}
