#ifndef COMM_LOG_API_H
#define COMM_LOG_API_H

#include "comm_types.h"
#include <stdarg.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
函数名称 : LogLevel
函数功能 : 日志等级定义
输入参数说明 : 无
函数返回值的说明 : 无
*/
typedef enum LogLevel
{
    LOG_LEVEL_TRACE = 0,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL,
    LOG_LEVEL_NONE
}
LogLevel;

/**
函数名称 : CommLogConfig
函数功能 : 日志初始化配置
输入参数说明 : log_dir 日志目录; max_total_bytes 总容量; keep_days 保留天数;
               queue_capacity 队列容量; flush_interval_ms 持续有日志时的落盘间隔，队列写空时立即落盘;
               console_level 控制台等级; file_level 文件等级; level 全局等级
函数返回值的说明 : 无
*/
typedef struct CommLogConfig
{
    const char* log_dir;
    uint64 max_total_bytes;
    uint32 keep_days;
    uint32 queue_capacity;
    uint32 flush_interval_ms;
    LogLevel console_level;
    LogLevel file_level;
    LogLevel level;
}
CommLogConfig;

/**
函数名称 : comm_log_init
函数功能 : 初始化日志组件
输入参数说明 : config 配置
函数返回值的说明 : 成功返回0，失败返回-1
*/
int32 comm_log_init(const CommLogConfig* config);
/**
函数名称 : comm_log_fini
函数功能 : 反初始化日志组件
输入参数说明 : 无
函数返回值的说明 : 无
*/
void comm_log_fini(void);

/**
函数名称 : comm_log_set_level
函数功能 : 设置全局日志等级，同时同步控制台与文件等级
输入参数说明 : level 等级
函数返回值的说明 : 无
*/
void comm_log_set_level(LogLevel level);
/**
函数名称 : comm_log_get_level
函数功能 : 获取全局日志等级
输入参数说明 : 无
函数返回值的说明 : 返回当前等级
*/
LogLevel comm_log_get_level(void);

/**
函数名称 : comm_log_set_console_level
函数功能 : 设置控制台输出等级
输入参数说明 : level 等级
函数返回值的说明 : 无
*/
void comm_log_set_console_level(LogLevel level);
/**
函数名称 : comm_log_get_console_level
函数功能 : 获取控制台输出等级
输入参数说明 : 无
函数返回值的说明 : 返回当前等级
*/
LogLevel comm_log_get_console_level(void);
/**
函数名称 : comm_log_set_file_level
函数功能 : 设置文件输出等级
输入参数说明 : level 等级
函数返回值的说明 : 无
*/
void comm_log_set_file_level(LogLevel level);
/**
函数名称 : comm_log_get_file_level
函数功能 : 获取文件输出等级
输入参数说明 : 无
函数返回值的说明 : 返回当前等级
*/
LogLevel comm_log_get_file_level(void);

/**
函数名称 : comm_log_flush
函数功能 : 等待此前已接受的文件日志写完，并同步到持久化存储
输入参数说明 : 无
函数返回值的说明 : 无
*/
void comm_log_flush(void);

/**
函数功能 : 获取最近一次文件 I/O 错误码，0 表示未记录错误
函数返回值的说明 : errno 错误码
*/
int32 comm_log_get_last_io_error(void);

/**
函数名称 : comm_log_write
函数功能 : 写日志（格式化输出）
输入参数说明 : level 等级; file 文件; func 函数; line 行号; format 格式字符串
函数返回值的说明 : 无
*/
void comm_log_write(LogLevel level,
                    const char* file,
                    const char* func,
                    int line,
                    const char* format,
                    ...);

/**
函数名称 : comm_log_write_v
函数功能 : 写日志（va_list 版本）
输入参数说明 : level 等级; file 文件; func 函数; line 行号; format 格式字符串; args 参数
函数返回值的说明 : 无
*/
void comm_log_write_v(LogLevel level,
                      const char* file,
                      const char* func,
                      int line,
                      const char* format,
                      va_list args);

/**
函数名称 : comm_log_fatal
函数功能 : 致命错误日志，强制落盘并终止进程
输入参数说明 : file 文件; func 函数; line 行号; format 格式字符串
函数返回值的说明 : 无
*/
void comm_log_fatal(const char* file,
                    const char* func,
                    int line,
                    const char* format,
                    ...);

/**
函数名称 : comm_log_fatal_v
函数功能 : 致命错误日志（va_list 版本），强制落盘并终止进程
输入参数说明 : file 文件; func 函数; line 行号; format 格式字符串; args 参数
函数返回值的说明 : 无
*/
void comm_log_fatal_v(const char* file,
                      const char* func,
                      int line,
                      const char* format,
                      va_list args);

#ifdef __cplusplus
}
#endif

#endif
