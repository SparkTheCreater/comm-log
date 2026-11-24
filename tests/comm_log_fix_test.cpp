#include "comm_log.h"
#include "internal/async_dispatcher.h"
#include "internal/file_catalog.h"
#include "internal/file_sink.h"
#include "internal/retention_policy.h"
#include "internal/time_util.h"

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
    static unsigned int next_dir = 0;
    snprintf(path, sizeof(path), "/tmp/comm-log-fix-%d-%u-%s", (int)getpid(), next_dir++, name);
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

static void TestTodayParts()
{
    std::string dir = MakeDir("today-parts");
    WriteFile(dir + "/2020-01-02.log", 10);
    WriteFile(dir + "/2020-01-02.001.log", 20);
    comm_log::RetentionPolicy policy;
    policy.Configure(25, 0);
    policy.SetCurrentDateKey("2020-01-02");
    CHECK(policy.Cleanup(dir, dir + "/2020-01-02.001.log"));
    CHECK(!Exists(dir + "/2020-01-02.log"));
    CHECK(Exists(dir + "/2020-01-02.001.log"));
}

static void TestRotateAndRestart()
{
    std::string dir = MakeDir("rotate-restart");
    CommLogConfig cfg;
    FillBase(&cfg, dir);
    cfg.console_level = LOG_LEVEL_NONE;
    cfg.max_total_bytes = 200;
    CHECK(comm_log_init(&cfg) == 0);
    comm_log_write(LOG_LEVEL_INFO, "test", "test", 1, "%080d", 1);
    comm_log_flush();
    comm_log_fini();
    std::string base = dir + "/" + TodayKey() + ".log";
    CHECK(Exists(base));
    CHECK(comm_log_init(&cfg) == 0);
    comm_log_write(LOG_LEVEL_INFO, "test", "test", 2, "%080d", 2);
    comm_log_flush();
    comm_log_fini();
    CHECK(Exists(base));
    CHECK(Exists(dir + "/" + TodayKey() + ".001.log"));
    CHECK(comm_log_init(&cfg) == 0);
    comm_log_write(LOG_LEVEL_INFO, "test", "test", 3, "%080d", 3);
    comm_log_write(LOG_LEVEL_INFO, "test", "test", 4, "%080d", 4);
    comm_log_flush();
    comm_log_fini();
    CHECK(!Exists(dir + "/" + TodayKey() + ".001.log"));
    comm_log::FileCatalog catalog;
    CHECK(catalog.Scan(dir));
    CHECK(!catalog.Files().empty());
    CHECK(catalog.Files().back().date_key == TodayKey());
    std::string latest = catalog.Files().back().path;
    FILE* fp = fopen(latest.c_str(), "rb");
    CHECK(fp != NULL);
    if (fp == NULL)
    {
        return;
    }
    char buffer[4096];
    size_t size = fread(buffer, 1, sizeof(buffer) - 1, fp);
    fclose(fp);
    buffer[size] = 0;
    CHECK(strstr(buffer, "0004") != NULL);
}

static void TestPeriodicCleanup()
{
    std::string dir = MakeDir("periodic-cleanup");
    WriteFile(dir + "/unrelated.txt", 1);
    CommLogConfig cfg;
    FillBase(&cfg, dir);
    cfg.console_level = LOG_LEVEL_NONE;
    cfg.keep_days = 3;
    cfg.cleanup_interval_ms = 50;
    CHECK(comm_log_init(&cfg) == 0);
    WriteFile(dir + "/2020-01-02.log", 1);
    uint64 deadline = comm_log::TimeUtil::NowMs() + 2000;
    while (Exists(dir + "/2020-01-02.log") && comm_log::TimeUtil::NowMs() < deadline)
    {
        usleep(10 * 1000);
    }
    CHECK(!Exists(dir + "/2020-01-02.log"));
    CHECK(Exists(dir + "/unrelated.txt"));
    comm_log_fini();
}

static void TestIoErrors()
{
    comm_log::FileSink sink;
    if (access("/dev/full", W_OK) == 0)
    {
        CHECK(sink.OpenPath("/dev/full"));
        CHECK(sink.WriteLine("trigger-no-space"));
        CHECK(!sink.FlushSync());
        CHECK(errno == ENOSPC);
        sink.Close();
    }

    std::string dir = MakeDir("io-error");
    CommLogConfig cfg;
    FillBase(&cfg, dir);
    CHECK(comm_log_init(&cfg) == 0);
    std::string blocked = dir + "/blocked";
    CHECK(mkdir(blocked.c_str(), 0755) == 0);
    LOG_INFO("%s", "cannot-open-file");
    comm_log_flush();
    CHECK(comm_log_get_last_io_error() != 0);
    comm_log_fini();
}

static void TestFatalDrainsPriorLogs()
{
    std::string dir = MakeDir("fatal-drain");
    CommLogConfig cfg;
    FillBase(&cfg, dir);
    cfg.console_level = LOG_LEVEL_NONE;
    cfg.flush_interval_ms = 60000;
    CHECK(comm_log_init(&cfg) == 0);
    for (int i = 0; i < 100; ++i)
    {
        LOG_INFO("fatal-before-%d", i);
    }
    g_abort_hits = 0;
    comm_log::SetFatalAbortHook(TestAbort);
    comm_log_fatal("test", "fn", 1, "%s", "fatal-last");
    comm_log::SetFatalAbortHook(NULL);
    CHECK(g_abort_hits == 1);
    comm_log_fini();

    FILE* fp = fopen((dir + "/" + TodayKey() + ".log").c_str(), "r");
    CHECK(fp != NULL);
    if (fp)
    {
        std::string data;
        char buffer[4096];
        size_t size = 0;
        while ((size = fread(buffer, 1, sizeof(buffer), fp)) > 0)
        {
            data.append(buffer, size);
        }
        size_t previous = data.find("fatal-before-99");
        size_t fatal = data.find("fatal-last");
        CHECK(previous != std::string::npos);
        CHECK(fatal != std::string::npos);
        CHECK(previous < fatal);
        fclose(fp);
    }
}

static void TestFlushBarrier()
{
    std::string dir = MakeDir("flush-barrier");
    CommLogConfig cfg;
    FillBase(&cfg, dir);
    cfg.console_level = LOG_LEVEL_NONE;
    cfg.flush_interval_ms = 60000;
    CHECK(comm_log_init(&cfg) == 0);
    for (int i = 0; i < 100; ++i)
    {
        LOG_INFO("barrier-before-%d", i);
    }
    comm_log_flush();

    FILE* fp = fopen((dir + "/" + TodayKey() + ".log").c_str(), "r");
    CHECK(fp != NULL);
    if (fp)
    {
        std::string data;
        char buffer[4096];
        size_t size = 0;
        while ((size = fread(buffer, 1, sizeof(buffer), fp)) > 0)
        {
            data.append(buffer, size);
        }
        CHECK(data.find("barrier-before-99") != std::string::npos);
        fclose(fp);
    }
    comm_log_fini();
}

static void TestLongMessages()
{
    std::string dir = MakeDir("long-message");
    CommLogConfig cfg;
    FillBase(&cfg, dir);
    cfg.console_level = LOG_LEVEL_NONE;
    CHECK(comm_log_init(&cfg) == 0);
    std::string message(8192, 'x');
    message += "-中文结尾";
    LOG_INFO("%s", message.c_str());

    std::string fatal_message(4096, 'f');
    g_abort_hits = 0;
    comm_log::SetFatalAbortHook(TestAbort);
    comm_log_fatal("test", "fn", 1, "%s", fatal_message.c_str());
    comm_log::SetFatalAbortHook(NULL);
    CHECK(g_abort_hits == 1);
    comm_log_fini();

    FILE* fp = fopen((dir + "/" + TodayKey() + ".log").c_str(), "r");
    CHECK(fp != NULL);
    if (fp)
    {
        std::string data;
        char buffer[4096];
        size_t size = 0;
        while ((size = fread(buffer, 1, sizeof(buffer), fp)) > 0)
        {
            data.append(buffer, size);
        }
        CHECK(data.find(message) != std::string::npos);
        CHECK(data.find(fatal_message) != std::string::npos);
        fclose(fp);
    }
}

static void TestNullFormat()
{
    std::string dir = MakeDir("null-format");
    CommLogConfig cfg;
    FillBase(&cfg, dir);
    CHECK(comm_log_init(&cfg) == 0);
    comm_log_write(LOG_LEVEL_INFO, "test", "fn", 1, NULL);
    LOG_INFO("%s", "valid-message");
    comm_log_fini();

    FILE* fp = fopen((dir + "/" + TodayKey() + ".log").c_str(), "r");
    CHECK(fp != NULL);
    if (fp)
    {
        char data[1024] = {0};
        size_t size = fread(data, 1, sizeof(data) - 1, fp);
        data[size] = '\0';
        CHECK(strstr(data, "valid-message") != NULL);
        fclose(fp);
    }
}

static void TestGlobalLevelFilter()
{
    std::string dir = MakeDir("global-level");
    CommLogConfig cfg;
    FillBase(&cfg, dir);
    cfg.level = LOG_LEVEL_ERROR;
    cfg.console_level = LOG_LEVEL_TRACE;
    cfg.file_level = LOG_LEVEL_TRACE;
    cfg.console_level = LOG_LEVEL_NONE;
    CHECK(comm_log_init(&cfg) == 0);

    LOG_INFO("%s", "filtered-info");
    LOG_ERROR("%s", "visible-error");
    comm_log_fini();

    FILE* fp = fopen((dir + "/" + TodayKey() + ".log").c_str(), "r");
    CHECK(fp != NULL);
    if (fp)
    {
        char data[1024] = {0};
        size_t size = fread(data, 1, sizeof(data) - 1, fp);
        data[size] = '\0';
        CHECK(strstr(data, "filtered-info") == NULL);
        CHECK(strstr(data, "visible-error") != NULL);
        fclose(fp);
    }
}

static void TestRetryFileOpen()
{
    std::string dir = MakeDir("open-retry");
    comm_log::FileSink sink;
    CHECK(!sink.Open(dir, "missing/child"));
    CHECK(!sink.IsOpen());
    CHECK(sink.CurrentPath().empty());
    CHECK(sink.Open(dir, TodayKey()));
    CHECK(sink.WriteLine("retry-succeeds"));
    sink.Close();
}

static void TestPreserveDirectoryPermissions()
{
    std::string dir = MakeDir("permissions");
    CHECK(chmod(dir.c_str(), 0750) == 0);

    CommLogConfig cfg;
    FillBase(&cfg, dir);
    CHECK(comm_log_init(&cfg) == 0);
    comm_log_fini();

    struct stat st;
    CHECK(stat(dir.c_str(), &st) == 0);
    CHECK((st.st_mode & 0777) == 0750);
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

    LOG_INFO("%s", "still-first-dir");
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
    bool pushed;
    uint64 sequence;
};

static void* PushWaiter(void* arg)
{
    PushArg* push = (PushArg*)arg;
    push->pushed = push->dispatcher->Push(push->record, push->sequence);
    return NULL;
}

static void* ProduceUntilStopped(void* arg)
{
    unsigned int* count = (unsigned int*)arg;
    for (unsigned int i = 0; i < 100000; ++i)
    {
        LOG_INFO("shutdown-message-%u", i);
        ++*count;
    }
    return NULL;
}

static void TestConcurrentShutdown()
{
    for (int round = 0; round < 20; ++round)
    {
        std::string dir = MakeDir("concurrent-shutdown");
        CommLogConfig cfg;
        FillBase(&cfg, dir);
        cfg.queue_capacity = 2;
        cfg.console_level = LOG_LEVEL_NONE;
        CHECK(comm_log_init(&cfg) == 0);
        unsigned int counts[4] = {0};
        pthread_t writers[4];
        for (int i = 0; i < 4; ++i)
        {
            CHECK(pthread_create(&writers[i], NULL, ProduceUntilStopped, &counts[i]) == 0);
        }
        comm_log_fini();
        for (int i = 0; i < 4; ++i)
        {
            pthread_join(writers[i], NULL);
        }
        CHECK(comm_log_init(&cfg) == 0);
        comm_log_fini();
    }
}

static void TestStopRejectsWaitingRecord()
{
    comm_log::AsyncDispatcher dispatcher;
    CHECK(dispatcher.Start(1));

    comm_log::LogRecord first;
    first.message = "first";
    uint64 sequence = 0;
    dispatcher.Push(first, sequence);

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
    CHECK(!arg.pushed);

    comm_log::LogRecord out;
    CHECK(dispatcher.Pop(out));
    CHECK(out.message == "first");
    CHECK(!dispatcher.Pop(out));
}

int main()
{
    TestParseDate();
    TestKeepDays();
    TestSkipUndeletable();
    TestFatalBeforeInit();
    TestTodayOversize();
    TestPreserveDirectoryPermissions();
    TestGlobalLevelFilter();
    TestNullFormat();
    TestLongMessages();
    TestFlushBarrier();
    TestFatalDrainsPriorLogs();
    TestIoErrors();
    TestTodayParts();
    TestRotateAndRestart();
    TestPeriodicCleanup();
    TestRetryFileOpen();
    TestInitPaths();
    TestConcurrentShutdown();
    TestStopRejectsWaitingRecord();
    if (g_failed)
    {
        fprintf(stderr, "comm_log_fix_test failed=%d\n", g_failed);
        return 1;
    }
    printf("comm_log_fix_test passed\n");
    return 0;
}
