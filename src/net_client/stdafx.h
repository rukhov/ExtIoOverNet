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

// std

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <list>
#include <filesystem>
#include <format>
#include <thread>
#include <chrono>
#include <iostream>
#include <fstream>

// boost

#include <boost/config.hpp>
#include <boost/program_options.hpp>
#include <boost/log/common.hpp>
#include <boost/log/sources/logger.hpp>

#include <boost/asio.hpp>

#include <boost/thread/synchronized_value.hpp>