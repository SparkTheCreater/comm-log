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
- 单目录按天文件：`YYYY-MM-DD.log`
- 空间管理：先按总容量删除最老文件，再按保留天数清理
- FATAL：同步落盘后 `abort`
- 对外接口：`include/comm_log_api.h`，宏在 `include/comm_log.h`

控制台输出在调用线程同步打印。文件输出进入队列，由后台线程写入。队列写空时立即落盘；持续有日志时按 `flush_interval_ms` 攒一批再落盘。清理在初始化、跨天和定期触发时执行。

## 示例

初始化：

```c
CommLogConfig cfg;
cfg.log_dir = "/home/log";
cfg.max_total_bytes = 100ULL * 1024ULL * 1024ULL;
cfg.keep_days = 30;
cfg.queue_capacity = 4096;
cfg.flush_interval_ms = 1000;
cfg.level = LOG_LEVEL_INFO;
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
