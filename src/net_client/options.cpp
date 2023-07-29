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

namespace
{
    void LoadOptionsFile(const std::filesystem::path& optionsFileName, Options& opt)
    {
        if (optionsFileName.empty())
            return;

        std::ifstream file(optionsFileName);

        po::options_description desc("Allowed options");
        desc.add_options()("server_addr", po::value<std::string>()->default_value("localhost"), 
            "ExtIoOverNet server address, default is localhost");
        desc.add_options()("server_port", po::value<uint16_t>()->default_value(2056),
            "ExtIoOverNet server port, default is 2056");
        desc.add_options()("log_level", po::value<int16_t>()->default_value(4), 
            "Integer value of logging level: trace=0; debug=1; info=2; warning=3; error=4; fatal=5, default is 4.");

        po::variables_map vm;

        try
        {
            const char* argv[] = {""};
            // to apply default values
            po::store(po::parse_command_line(1, argv, desc), vm);

            file.exceptions(file.failbit);
            std::stringstream ss;
            ss << file.rdbuf();
            po::store(po::parse_config_file(ss, desc), vm);
            LOG(info) << "Config file <" << optionsFileName << "> was read succesfully.";
        }
        catch (const std::ios_base::failure& e)
        {
            LOG(error) << 
                std::format("Failed to read config file <{}> because of <{}>.", optionsFileName.string(), e.what());
        }
        
        po::notify(vm);

        if (vm.count("server_addr"))
            opt.serverAddress = vm["server_addr"].as<std::string>();
        if (vm.count("server_port"))
            opt.serverPort = vm["server_port"].as<uint16_t>();
        if (vm.count("log_level"))
            opt.logLevel = vm["log_level"].as<int16_t>();
        
    }}

Options::Options(const std::filesystem::path& optionsFileName/* = {}*/)
{
    LoadOptionsFile(optionsFileName, *this);
}
