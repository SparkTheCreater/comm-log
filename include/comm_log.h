/**
* \file    comm_log.h
* \brief   通用日志组件 (轻量级增强版)
* \author Spark
*/
#ifndef __COMM_LOG_H__
#define __COMM_LOG_H__

#include "comm_log_api.h"
#include <string.h>

// 1. 辅助：获取文件名
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : \
                    (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__))

// 2. 宏定义接口 (API)
#define LOG_SetLevel(level) comm_log_set_level(level)
#define LOG_SetConsoleLevel(level) comm_log_set_console_level(level)
#define LOG_SetFileLevel(level) comm_log_set_file_level(level)

#define LOG_TRACE(fmt, ...) comm_log_write(LOG_LEVEL_TRACE, __FILENAME__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) comm_log_write(LOG_LEVEL_DEBUG, __FILENAME__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  comm_log_write(LOG_LEVEL_INFO,  __FILENAME__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  comm_log_write(LOG_LEVEL_WARN,  __FILENAME__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) comm_log_write(LOG_LEVEL_ERROR, __FILENAME__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...) comm_log_fatal(__FILENAME__, __func__, __LINE__, fmt, ##__VA_ARGS__)

#endif