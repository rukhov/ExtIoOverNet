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

#include "../utils/log.h"
#include "service.h"
#include "../utils/GlobalDefs.h"

#include <boost/dll/smart_library.hpp>
#include <boost/dll/runtime_symbol_info.hpp>

namespace
{
	namespace as = boost::asio;
	
	class Dll
	{
		std::vector<std::thread> _pool;
		as::io_service _ctx;
		std::unique_ptr<Logging::ILog> _logKeeper;
		std::unique_ptr<Options> _options;

		void InitGlobals()
		{
			static std::once_flag flag;
			std::call_once(flag, [this]() {
				_logKeeper = Logging::MakeLog(false, "ExtIoOverNet_client");

				auto optionsPath = boost::dll::this_line_location().remove_filename().
					append(c_clientOptionsFileName);
				_options = std::make_unique<Options>(optionsPath.string());

				LOG(info) << "server_addr=" << _options->serverAddress;
				LOG(info) << "server_port=" << _options->serverPort;
				LOG(info) << "log_level=" << _options->logLevel;
				LOG(trace) << "Setting log level to " << _options->logLevel;
				_logKeeper->SetSeverityLevel((boost::log::trivial::severity_level)_options->logLevel);
				}
			);
		}

		void InitMainGlobals()
		{
			static std::once_flag flag;
			std::call_once(flag, [this]() {

				}
			);
		}

		void threadProc(const std::string threadName)
		{
			InitMainGlobals();

			BOOST_LOG_NAMED_SCOPE(">");

			LOG(trace) << "thread <" << threadName << "> is started.";

			try {
				_ctx.run();
			}
			catch (const std::exception& e)
			{
				LOG(trace) << "Unhandled exception was thrown: " << e.what();
			}
			catch (...)
			{
				LOG(trace) << "Unhandled exception was thrown.";
			}

			LOG(trace) << "thread <" << threadName << "> is finished.";
		}

		void atExit()
		{
			static std::once_flag flag;
			std::call_once(flag, [this]() {
				if (auto srv = GetService().lock())
				{
					auto& ref = *srv;
					srv.reset(); // prevent destruction in this thread
					ref.Stop();
				}

				for (auto& t : _pool)
				{
					if (t.joinable())
						t.join();
				}
				LOG(trace) << "Service: all threads finished.";
				_pool.clear();
				_ctx.stop();
				_logKeeper->StopLogging();
				}
			);
		}

	public:

		Dll()
			: _ctx(1)
		{
			InitGlobals();

			for (size_t i = 0; i < 1; ++i)
			{
				std::string threadName = std::format("eio2tcp#{}", i);
				_pool.emplace_back([this, threadName]() {threadProc(threadName); });
			}
		}

		~Dll()
		{
			atExit();
		}

		std::weak_ptr<IService> GetService()
		{
			static std::weak_ptr<IService> g_service =
				MakeService(boost::asio::make_strand(_ctx), *_options).get();

			return g_service;
		}

	};

	Dll* pDll = nullptr;
}

void DllInit()
{
	pDll = new Dll();
}

void DllDeinit()
{
	delete pDll;
	pDll = nullptr;
}

std::shared_ptr<IService> GetLockedService()
{
	return pDll->GetService().lock();
}
