#include "comm_log.h"
#include "internal/async_dispatcher.h"
#include "internal/file_catalog.h"
#include "internal/retention_policy.h"

#include <pthread.h>
#include <stdio.h>
#include <string>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

namespace comm_log
{
void SetFatalAbortHook(void (*fn)());
}

static int g_failed = 0;
static int g_abort_hits = 0;

#define CHECK(cond)                                                          \
    do                                                                       \
    {                                                                        \
        if (!(cond))                                                         \
        {                                                                    \
            fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); \
            g_failed++;                                                      \
        }                                                                    \
    } while (0)

static void TestAbort()
{
    g_abort_hits++;
}

static std::string MakeDir(const char* name)
{
    char path[256];
    snprintf(path, sizeof(path), "/tmp/comm-log-fix-%d-%s", (int)getpid(), name);
    mkdir(path, 0755);
    return path;
}

static void WriteFile(const std::string& path, size_t bytes)
{
    FILE* fp = fopen(path.c_str(), "wb");
    CHECK(fp != NULL);
    if (!fp)
    {
        return;
    }
    std::string data(bytes, 'x');
    fwrite(data.data(), 1, data.size(), fp);
    fclose(fp);
}

static bool Exists(const std::string& path)
{
    return access(path.c_str(), F_OK) == 0;
}

static std::string TodayKey()
{
    time_t now = time(NULL);
    struct tm tmval;
    localtime_r(&now, &tmval);
    char buf[16];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
             tmval.tm_year + 1900, tmval.tm_mon + 1, tmval.tm_mday);
    return buf;
}

static void FillBase(CommLogConfig* cfg, const std::string& dir)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->log_dir = dir.c_str();
    cfg->max_total_bytes = 1024 * 1024;
    cfg->keep_days = 30;
    cfg->queue_capacity = 16;
    cfg->flush_interval_ms = 1000;
    cfg->level = LOG_LEVEL_INFO;
    cfg->console_level = LOG_LEVEL_INFO;
    cfg->file_level = LOG_LEVEL_INFO;
}

static void TestParseDate()
{
    std::string key;
    CHECK(!comm_log::FileCatalog::ParseDateKey("2026-13-40.log", key));
    CHECK(!comm_log::FileCatalog::ParseDateKey("2023-02-29.log", key));
    CHECK(comm_log::FileCatalog::ParseDateKey("2024-02-29.log", key));
    CHECK(key == "2024-02-29");
}

static void TestKeepDays()
{
    std::string dir = MakeDir("days");
    WriteFile(dir + "/2026-01-01.log", 8);
    WriteFile(dir + "/2026-01-02.log", 8);
    WriteFile(dir + "/2026-01-31.log", 8);

    comm_log::RetentionPolicy policy;
    policy.Configure(0, 30);
    policy.SetCurrentDateKey("2026-01-31");
    CHECK(policy.Cleanup(dir));
    CHECK(!Exists(dir + "/2026-01-01.log"));
    CHECK(Exists(dir + "/2026-01-02.log"));
    CHECK(Exists(dir + "/2026-01-31.log"));
}

static void TestSkipUndeletable()
{
    std::string dir = MakeDir("skip");
    std::string blocked = dir + "/2020-01-01.log";
    CHECK(mkdir(blocked.c_str(), 0755) == 0);
    WriteFile(blocked + "/keep", 4);
    WriteFile(dir + "/2020-01-02.log", 8);

    comm_log::RetentionPolicy policy;
    policy.Configure(0, 1);
    policy.SetCurrentDateKey("2026-01-31");
    CHECK(policy.Cleanup(dir));
    CHECK(Exists(blocked));
    CHECK(!Exists(dir + "/2020-01-02.log"));
}

static void TestTodayOversize()
{
    std::string dir = MakeDir("today");
    WriteFile(dir + "/" + TodayKey() + ".log", 100);

    comm_log::RetentionPolicy policy;
    policy.Configure(10, 0);
    policy.SetCurrentDateKey(TodayKey());
    CHECK(!policy.Cleanup(dir));
    CHECK(Exists(dir + "/" + TodayKey() + ".log"));

    CommLogConfig cfg;
    FillBase(&cfg, dir);
    cfg.max_total_bytes = 10;
    cfg.keep_days = 0;
    CHECK(comm_log_init(&cfg) != 0);
    CHECK(Exists(dir + "/" + TodayKey() + ".log"));
}

static void TestInitPaths()
{
    std::string bad_parent = MakeDir("notdir");
    std::string file_path = bad_parent + "/file";
    WriteFile(file_path, 4);
    std::string nested = file_path + "/logs";

    CommLogConfig bad;
    FillBase(&bad, nested);
    CHECK(comm_log_init(&bad) != 0);

    bad.level = (LogLevel)99;
    bad.log_dir = MakeDir("unused-level").c_str();
    std::string unused = MakeDir("unused-level");
    bad.log_dir = unused.c_str();
    CHECK(comm_log_init(&bad) != 0);

    std::string cap_dir = MakeDir("cap");
    FillBase(&bad, cap_dir);
    bad.queue_capacity = 0;
    CHECK(comm_log_init(&bad) != 0);

    std::string dir = MakeDir("ok");
    CommLogConfig cfg;
    FillBase(&cfg, dir);
    CHECK(comm_log_init(&cfg) == 0);

    std::string other = MakeDir("other");
    CommLogConfig again;
    FillBase(&again, other);
    CHECK(comm_log_init(&again) != 0);

    LOG_INFO("still-first-dir");
    comm_log_flush();
    usleep(50 * 1000);
    CHECK(Exists(dir + "/" + TodayKey() + ".log"));
    CHECK(!Exists(other + "/" + TodayKey() + ".log"));
    comm_log_fini();
}

static void TestFatalBeforeInit()
{
    g_abort_hits = 0;
    comm_log::SetFatalAbortHook(TestAbort);
    comm_log_fatal("test", "fn", 1, NULL);
    CHECK(g_abort_hits == 1);
    comm_log_fatal("test", "fn", 2, "boom %d", 1);
    CHECK(g_abort_hits == 2);
    comm_log::SetFatalAbortHook(NULL);
}

struct PushArg
{
    comm_log::AsyncDispatcher* dispatcher;
    comm_log::LogRecord record;
};

static void* PushWaiter(void* arg)
{
    PushArg* push = (PushArg*)arg;
    push->dispatcher->Push(push->record);
    return NULL;
}

static void TestStopKeepsWaitingRecord()
{
    comm_log::AsyncDispatcher dispatcher;
    CHECK(dispatcher.Start(1));

    comm_log::LogRecord first;
    first.message = "first";
    dispatcher.Push(first);

    comm_log::LogRecord second;
    second.message = "second";
    PushArg arg;
    arg.dispatcher = &dispatcher;
    arg.record = second;
    pthread_t thread;
    CHECK(pthread_create(&thread, NULL, PushWaiter, &arg) == 0);
    usleep(100 * 1000);
    dispatcher.Stop();
    pthread_join(thread, NULL);

    comm_log::LogRecord out;
    CHECK(dispatcher.Pop(out));
    CHECK(out.message == "first");
    CHECK(dispatcher.Pop(out));
    CHECK(out.message == "second");
    CHECK(!dispatcher.Pop(out));
}

int main()
{
    TestParseDate();
    TestKeepDays();
    TestSkipUndeletable();
    TestFatalBeforeInit();
    TestTodayOversize();
    TestInitPaths();
    TestStopKeepsWaitingRecord();
    if (g_failed)
    {
        fprintf(stderr, "comm_log_fix_test failed=%d\n", g_failed);
        return 1;
    }
    printf("comm_log_fix_test passed\n");
    return 0;
}
