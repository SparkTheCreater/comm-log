#ifndef COMM_THREAD_H
#define COMM_THREAD_H

#include "comm_types.h"

#include <pthread.h>
#include <sched.h>

typedef pthread_t COMM_THREAD_HANDLE;
#define COMM_INVALID_THREAD ((COMM_THREAD_HANDLE)(-1))

typedef void* (CALLBACK *COMM_THREAD_START_ROUTINE)(void*);

class Thread
{
public:
    static COMM_THREAD_HANDLE Create(COMM_THREAD_START_ROUTINE StartAddress,
                                    void* Params,
                                    uint32 StackSize = 0,
                                    bool IsSuspend = false,
                                    int32 Priority = 0,
                                    int32 SchedPolicy = 0)
    {
        (void)IsSuspend;

        pthread_attr_t attr;
        pthread_attr_init(&attr);

        if (StackSize > 0)
        {
            pthread_attr_setstacksize(&attr, StackSize);
        }

        if (SchedPolicy != 0)
        {
            pthread_attr_setschedpolicy(&attr, SchedPolicy);
        }

        if (Priority != 0)
        {
            struct sched_param param;
            param.sched_priority = Priority;
            pthread_attr_setschedparam(&attr, &param);
        }

        COMM_THREAD_HANDLE handle;
        int ret = pthread_create(&handle, &attr, StartAddress, Params);
        pthread_attr_destroy(&attr);

        if (ret != 0)
        {
            return COMM_INVALID_THREAD;
        }
        return handle;
    }

    static int32 Wait(COMM_THREAD_HANDLE ThreadHandle)
    {
        if (pthread_join(ThreadHandle, NULL) == 0)
        {
            return RET_OK;
        }
        return RET_ERROR;
    }
};

#endif
