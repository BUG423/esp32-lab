/*
 * Copyright (c) 2026 OpenHarmony Certification Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#ifndef OH_LOS_TASK_H
#define OH_LOS_TASK_H

#include "los_typedef.h"
#include "los_config.h"
#include "los_errno.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef VOID *(*TSK_ENTRY_FUNC)(VOID *arg);

typedef struct tagTskInitParam {
    TSK_ENTRY_FUNC pfnTaskEntry;
    UINT16         usTaskPrio;
    UINT32         uwStackSize;
    CHAR           *pcName;
    UINT32         uwArg;
} TSK_INIT_PARAM_S;

UINT32 LOS_TaskCreate(UINT32 *puwTaskID, TSK_INIT_PARAM_S *pstInitParam);
UINT32 LOS_TaskDelete(UINT32 uwTaskID);
UINT32 LOS_TaskDelay(UINT32 uwTick);
UINT32 LOS_TaskSuspend(UINT32 uwTaskID);
UINT32 LOS_TaskResume(UINT32 uwTaskID);
UINT32 LOS_TaskPriSet(UINT32 uwTaskID, UINT16 usTaskPrio);
UINT16 LOS_TaskPriGet(UINT32 uwTaskID);
UINT32 LOS_CurTaskIDGet(VOID);
UINT32 LOS_MS2Tick(UINT32 millisec);
UINT32 LOS_Tick2MS(UINT32 ticks);

#ifdef __cplusplus
}
#endif

#endif /* OH_LOS_TASK_H */
