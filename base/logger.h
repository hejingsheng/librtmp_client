#ifndef _LOGGER_H_
#define _LOGGER_H_

#include <memory>
#include <string>
#include "singleton.h"
#include "zlog.h"

namespace LogCore {

class Logger : public core::Singleton<Logger>
{
public:
	void startup();

	void shutdown();

	zlog_category_t *logger();

private:
	zlog_category_t *logHandler_m;
};

}

#define DLOG(...) zlog_debug(LogCore::Logger::instance()->logger(), __VA_ARGS__)
#define ILOG(...) zlog_info(LogCore::Logger::instance()->logger(), __VA_ARGS__)
#define NLOG(...) zlog_notice(LogCore::Logger::instance()->logger(), __VA_ARGS__)
#define WLOG(...) zlog_warn(LogCore::Logger::instance()->logger(), __VA_ARGS__)
#define ELOG(...) zlog_error(LogCore::Logger::instance()->logger(), __VA_ARGS__)
#define FLOG(...) zlog_fatal(LogCore::Logger::instance()->logger(), __VA_ARGS__)

//#define DLOG(...)
//#define ILOG(...)
//#define NLOG(...)
//#define WLOG(...)
//#define ELOG(...)
//#define FLOG(...)

#endif