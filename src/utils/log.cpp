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

//#define BOOST_LOG_DYN_LINK

#include "Log.h"

#include <boost/log/common.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/expressions.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/syslog_backend.hpp>
#include <boost/log/trivial.hpp>

#include <filesystem>

#include "GlobalDefs.h"

namespace logging = boost::log;
namespace expr = boost::log::expressions;
namespace keywords = boost::log::keywords;
namespace attrs = boost::log::attributes;
namespace sinks = boost::log::sinks;

BOOST_LOG_ATTRIBUTE_KEYWORD(named_scope, "Scope", attrs::named_scope::value_type)
BOOST_LOG_ATTRIBUTE_KEYWORD(ThreadId, "ThreadId", attrs::current_thread_id::value_type)

namespace
{
	constexpr char c_dateTimeFormat[] = "%y.%m.%d-%H:%M:%S.%f";

	struct Log : public Logging::ILog
	{
		boost::shared_ptr<boost::log::v2s_mt_nt6::sinks::synchronous_sink<boost::log::v2s_mt_nt6::sinks::text_ostream_backend>> conLog;
		boost::shared_ptr<boost::log::v2s_mt_nt6::sinks::synchronous_sink<boost::log::v2s_mt_nt6::sinks::text_file_backend>> fileLog;

		void StopLogging() override
		{
			boost::log::core::get()->remove_sink(fileLog);
			fileLog.reset();
		}

		void SetSeverityLevel(boost::log::trivial::severity_level level) override
		{
			logging::core::get()->set_filter
			(
				logging::trivial::severity >= level
				/*[](const boost::log::v2s_mt_nt6::attribute_value_set s) -> bool {
				return true;
				}*/
			);
		}
	};
}

namespace Logging
{

	std::unique_ptr<ILog> MakeLog(bool console, const std::string& fileNamePattern)
	{
		auto retVal = std::make_unique<Log>();

		auto tmpDir = std::filesystem::temp_directory_path().string() + c_appName + "/";

		logging::add_common_attributes();
		
		logging::core::get()->add_global_attribute(
			"Scope", attrs::named_scope()
		);

		logging::core::get()->add_global_attribute(
			"ThreadId", attrs::current_thread_id()
		);

		if (console)
		{
			retVal->conLog = logging::add_console_log(
				std::clog,
				keywords::format =
				(
					expr::stream
					<< expr::format_date_time< boost::posix_time::ptime >("TimeStamp", c_dateTimeFormat)
					<< "[" << ThreadId << "]"
					//<< "[" << LineID << "]"
					<< "[" << expr::attr< boost::log::trivial::severity_level >("Severity") << "]"
					<< "[" << named_scope << "]"
					<< ":" << expr::smessage
					)
			);

			//con->locked_backend()->auto_flush(true);
		}

		if (fileNamePattern.size())
		{

#define SIMPLE 0
#if (SIMPLE == 0)

			retVal->fileLog = logging::add_file_log(
				keywords::rotation_size = 256 * 1024,
				keywords::time_based_rotation = sinks::file::rotation_at_time_point(0, 0, 0),
				keywords::file_name = tmpDir + fileNamePattern + ".log",
				keywords::target_file_name = tmpDir + fileNamePattern + "_%Y%m%d_%H%M%S_%2N.log",
				keywords::format =
				(
					expr::stream
					<< expr::format_date_time< boost::posix_time::ptime >("TimeStamp", c_dateTimeFormat)
					<< "[" << ThreadId << "]"
					<< "[" << expr::attr< boost::log::trivial::severity_level >("Severity") << "]"
					<< "[" << named_scope << "]"
					<< ":" << expr::smessage
					)
			);

			retVal->fileLog->locked_backend()->auto_flush(true);
#else

			//Create a text file sink
			boost::shared_ptr< sinks::synchronous_sink< sinks::text_file_backend > > file_sink(
				new sinks::synchronous_sink< sinks::text_file_backend >(
					keywords::file_name = fileNamePattern + ".log",  // file name pattern
					keywords::target_file_name = fileNamePattern + "_%Y%m%d_%H%M%S_%2N.log",
					keywords::rotation_size = 16384/*,
					keywords::format =
					(
						expr::stream
						<< expr::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d %H:%M:%S")
						//<< "[" << ThreadId << "] "
						<< "[" << named_scope << "] "
						<< ": " << expr::smessage
						)*/
				));
			// Set up where the rotated files will be stored
			file_sink->locked_backend()->set_file_collector(sinks::file::make_collector(
				keywords::target = "logs",
				keywords::max_size = 64 * 1024 * 1024,
				keywords::min_free_space = 100 * 1024 * 1024,
				keywords::max_files = 512
			));

			// Upon restart, scan the target directory for files matching the file_name pattern
			file_sink->locked_backend()->scan_for_files();


			file_sink->set_formatter
			(
				expr::format("%1% Application 1 Log - ID: %2% | %3% : \"%4%\"")
				% expr::attr< boost::posix_time::ptime >("TimeStamp")
				% expr::attr< unsigned int >("RecordID")
				% expr::smessage
			);

			logging::core::get()->add_sink(file_sink);

			// Add logging attributes
			logging::core::get()->add_global_attribute("RecordID", attrs::counter< unsigned int >());
			logging::core::get()->add_global_attribute("TimeStamp", attrs::utc_clock());
			logging::core::get()->add_global_attribute(
				"ProcessID", attrs::current_process_id());
#endif
		}

		return retVal;
	}
}