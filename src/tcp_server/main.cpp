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
#include "options.h"
#include "majordomo.h"

#include <iostream>

#include <boost/program_options.hpp>

Options opt;

int do_run()
{
	boost::asio::io_context ctx;

	std::vector<std::thread> pool;

	for (int i = 0; i < 2; ++i)
		pool.emplace_back(std::thread([&]() { ctx.run(); }));

	boost::asio::signal_set signals(ctx, SIGINT, SIGTERM);

	auto mainStrand = boost::asio::make_strand(ctx);

	auto& majordomo = MakeMajordomo(mainStrand).get();

	static auto ctrl_c_handler = [&ctx, &majordomo](const boost::system::error_code& error, int signal_number)
	{
		BOOST_LOG_NAMED_SCOPE("Ctrl+C handler");
		LOG(info) << "Ctrl+C detected. Stopping.";
		majordomo.AsyncStop();
	};

	signals.async_wait(ctrl_c_handler);

	ctx.run();

	for (auto& t : pool)
		t.join();

	LOG(info) << "Exiting.";
	
	return 0;
}

int main(int argc, const char* argv[])
{
	std::cout
		<< "ExtIO software radio API propagation over network.\n"
		<< "Copyright(C) 2023 Roman Ukhov <ukhov.roman@gmail.com>. All rights reserved.\n"
		<< "This software is licensed under the GNU General Public License Version 3.\n\n"
		<< "Command line options:\n"
		<< "--extio_path=<Path to the ExtIO_XXX.dll>  - ExtIO API dynamic linking library to be propagated over the network. This is mandatory parameter.\n"
		<< "--listening_port=12345  - The port number to be listened for client connections, default is 2056.\n"
		<< "--log_level=0  - Integer value of logging level: trace=0; debug=1; info=2; warning=3; error=4; fatal=5, default is 4.\n"
		<< "\n\n";

	auto log = Logging::MakeLog(true, "ExtIoOverNet_server");
	BOOST_LOG_NAMED_SCOPE("main");
	LOG(info) << "Ext2Tcp server starting.";

	for (int i = 0; i < argc; ++i)
		LOG(info) << "argv[" << i << "]=" << argv[i];

	{
		Options opt;
		opt.parse_options(argc, argv);
		Options::init(std::move(opt));

		LOG(trace) << "extio_path=" << Options::get().extIoSharedLibName;
		LOG(trace) << "listening_port=" << Options::get().listeningPort;
		LOG(trace) << "log_level=" << Options::get().logLevel;
	}

	LOG(trace) << "Setting log level to " << Options::get().logLevel;
	log->SetSeverityLevel(boost::log::trivial::severity_level(Options::get().logLevel));

	do_run();
	log->StopLogging();
	return 0;
}
