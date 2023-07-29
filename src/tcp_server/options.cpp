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

#include "options.h"
#include "../utils/log.h"

namespace po = boost::program_options;
namespace as = boost::asio;

namespace
{
    Options& getGlobal()
    {
        static Options opt;
        return opt;
    }
}

void Options::parse_options(int argc, const char* argv[])
{
	BOOST_LOG_NAMED_SCOPE("parse_options");

	boost::log::sources::logger lg;

	try
	{
		po::options_description desc("Allowed options");

		desc.add_options()
			("extio_path", po::value<std::string>(), "ExtIo shared library path.");
		desc.add_options()
			("listening_port", po::value<uint16_t>()->default_value(2056), "The port number to be listened for client connections, default is 2056.");
		desc.add_options()
			("log_level", po::value<int16_t>()->default_value(4), "Integer value of logging level: trace=0; debug=1; info=2; warning=3; error=4; fatal=5, default is 4.");

		po::variables_map vm;
		po::store(po::parse_command_line(argc, argv, desc), vm);
		po::notify(vm);

		if (vm.count("extio_path"))
			extIoSharedLibName = vm["extio_path"].as<std::string>();
		if (vm.count("listening_port"))
			listeningPort = vm["listening_port"].as<uint16_t>();
		if (vm.count("log_level"))
			logLevel = vm["log_level"].as<int16_t>();
	}
	catch (const std::exception& e)
	{
		LOG(error) << e.what();
		//BOOST_ASSERT();
		throw;
	}
}

void Options::init(Options&& src)
{
    getGlobal() = src;
}

const Options& Options::get()
{
    return getGlobal();
}
