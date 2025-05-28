#ifndef LOGMACROS_H
#define LOGMACROS_H

#include "AsyncLog.h"

// 简化日志宏定义
#define LOG_DEBUG(format, ...) \
    AsyncLogger::instance().log(AsyncLogger::Level::Debug, __FUNCTION__, __FILE__, __LINE__, format, ##__VA_ARGS__)

#define LOG_INFO(format, ...) \
    AsyncLogger::instance().log(AsyncLogger::Level::Info, __FUNCTION__, __FILE__, __LINE__, format, ##__VA_ARGS__)

#define LOG_WARNING(format, ...) \
    AsyncLogger::instance().log(AsyncLogger::Level::Warning, __FUNCTION__, __FILE__, __LINE__, format, ##__VA_ARGS__)

#define LOG_ERROR(format, ...) \
    AsyncLogger::instance().log(AsyncLogger::Level::Error, __FUNCTION__, __FILE__, __LINE__, format, ##__VA_ARGS__)

#define LOG_FATAL(format, ...) \
    AsyncLogger::instance().log(AsyncLogger::Level::Fatal, __FUNCTION__, __FILE__, __LINE__, format, ##__VA_ARGS__)


#endif
