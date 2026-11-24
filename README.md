# comm-log

面向嵌入式发布场景的轻量级日志组件。提供 C 接口和静态库，支持异步写入、单目录按天文件、容量优先清理和按天保留。业务代码使用 `LOG_INFO` / `LOG_ERROR` 等宏即可，建议在程序启动早期完成初始化。

## 构建

```bash
make
```

产物是当前目录下的 `libcomm_log.a`（已加入 `.gitignore`）。交叉编译时替换编译器和归档工具：

```bash
make CXX=aarch64-linux-gnu-g++ AR=aarch64-linux-gnu-ar
```

链接业务程序时加上 `-Iinclude -L. -lcomm_log -lpthread`。

## 功能

- 分级日志：TRACE / DEBUG / INFO / WARN / ERROR / FATAL
- 异步写入：阻塞队列，队列满时等待，不丢日志
- 单目录按天文件：`YYYY-MM-DD.log`，容量限制启用时超过约十分之一总容量切换为 `.001.log` 等分片
- 空间管理：先按保留天数清理，再按总容量删除最老文件
- FATAL：同步落盘后 `abort`
- 对外接口：`include/comm_log_api.h`，宏在 `include/comm_log.h`

控制台输出在调用线程同步打印。文件输出进入队列，由后台线程写入。队列写空时立即刷新 stdio 缓冲区；持续有日志时按 `flush_interval_ms` 攒一批再刷新。`comm_log_flush()` 等待此前已接受的文件日志处理完成，并执行 `fflush` 和 `fsync`；普通日志仍是异步写入，未 flush 的记录可能在进程崩溃时丢失。清理在初始化、跨天和定期触发时执行，过期天数与旧分片均参与容量清理；当前活动文件不会被删除，单条超大日志不拆分，因此活动文件可能暂时超过总容量。配置 `cleanup_interval_ms` 控制定期清理间隔，0 表示关闭。

## 示例

初始化：

```c
#include <string.h>

CommLogConfig cfg;
memset(&cfg, 0, sizeof(cfg));
cfg.log_dir = "/home/log";
cfg.max_total_bytes = 100ULL * 1024ULL * 1024ULL;
cfg.keep_days = 30;
cfg.queue_capacity = 4096;
cfg.flush_interval_ms = 1000;
cfg.cleanup_interval_ms = 60000;
cfg.level = LOG_LEVEL_INFO;
cfg.console_level = LOG_LEVEL_INFO;
cfg.file_level = LOG_LEVEL_INFO;
comm_log_init(&cfg);
```

业务调用：

```c
LOG_INFO("session start");
LOG_WARN("buffer low");
LOG_ERROR("connect failed");
```

`LOG_FATAL` 用于不可恢复错误：写下最后一条日志并终止进程。普通错误用 `LOG_ERROR` / `LOG_WARN`。

```c
LOG_FATAL("fatal error: %d", err);
```

## 结构

- API：`comm_log_init` / `comm_log_write` / `comm_log_fatal`
- 门面 `LoggerFacade`：初始化、后台线程、清理
- `AsyncDispatcher`：阻塞队列
- `FileSink`：按日期写文件
- `RetentionPolicy` / `TextFormatter`：清理和格式化
