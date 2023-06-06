#include "logger.h"
#include <memory>
#include <iostream>

namespace LogCore {

	const std::string LOGTAG = "RTMPCLI";

	void Logger::startup()
	{
		int ret;

		ret = zlog_init("zlog.conf");
		if (ret)
		{
			std::cout << "zlog init error" << std::endl;
			return;
		}
		logHandler_m = zlog_get_category(LOGTAG.c_str());
		if (!logHandler_m)
		{
			std::cout << "zlog get category error" << std::endl;
			return;
		}
	}

	void Logger::shutdown()
	{
		zlog_fini();
	}

	zlog_category_t *Logger::logger()
	{
		return logHandler_m;
		//return nullptr;
	}

}