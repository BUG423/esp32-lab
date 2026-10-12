/*
 * Copyright (c) 2026 OpenHarmony Certification Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#include "los_task.h"
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

typedef struct {
    TaskHandle_t   xTask;
    TSK_ENTRY_FUNC pfnEntry;
    UINT32         uwArg;
    UINT16         usPrio;
    CHAR           name[16];
    BOOL           inUse;
} LosTaskEntry;

static LosTaskEntry s_taskTable[LOSCFG_BASE_CORE_TSK_LIMIT];
static SemaphoreHandle_t s_taskTableMutex = NULL;
static BOOL s_taskInitialized = FALSE;

static void InitTaskTableIfNeeded(void)
{
    if (!s_taskInitialized) {
        s_taskTableMutex = xSemaphoreCreateMutex();
        memset(s_taskTable, 0, sizeof(s_taskTable));
        s_taskInitialized = TRUE;
    }
}

static UBaseType_t LosPrioToFreeRTOS(UINT16 losPrio)
{
    if (losPrio > LOS_TASK_PRIORITY_LOWEST) {
        losPrio = LOS_TASK_PRIORITY_LOWEST;
    }
    UBaseType_t maxPrio = configMAX_PRIORITIES - 1;
    return maxPrio - (losPrio * maxPrio / LOS_TASK_PRIORITY_LOWEST);
}

static UINT16 FreeRTOSToLosPrio(UBaseType_t frPrio)
{
    UBaseType_t maxPrio = configMAX_PRIORITIES - 1;
    if (frPrio > maxPrio) {
        frPrio = maxPrio;
    }
    return (UINT16)((maxPrio - frPrio) * LOS_TASK_PRIORITY_LOWEST / maxPrio);
}

static void LosTaskWrapper(void *pvParameters)
{
    uint32_t taskId = (uint32_t)(uintptr_t)pvParameters;
    TSK_ENTRY_FUNC entry = NULL;
    UINT32 arg = 0;

    if (s_taskTableMutex) {
        xSemaphoreTake(s_taskTableMutex, portMAX_DELAY);
        if (taskId < LOSCFG_BASE_CORE_TSK_LIMIT && s_taskTable[taskId].inUse) {
            entry = s_taskTable[taskId].pfnEntry;
            arg = s_taskTable[taskId].uwArg;
        }
        xSemaphoreGive(s_taskTableMutex);
    }

    if (entry != NULL) {
        entry((VOID *)(uintptr_t)arg);
    }

    /* Auto cleanup if task exits */
    LOS_TaskDelete(taskId);
    vTaskDelete(NULL);
}

UINT32 LOS_TaskCreate(UINT32 *puwTaskID, TSK_INIT_PARAM_S *pstInitParam)
{
    if (puwTaskID == NULL || pstInitParam == NULL) {
        return LOS_ERRNO_TSK_PTR_NULL;
    }
    if (pstInitParam->pfnTaskEntry == NULL) {
        return LOS_ERRNO_TSK_ENTRY_NULL;
    }
    if (pstInitParam->usTaskPrio > LOS_TASK_PRIORITY_LOWEST) {
        return LOS_ERRNO_TSK_PRIOR_ERROR;
    }

    InitTaskTableIfNeeded();

    xSemaphoreTake(s_taskTableMutex, portMAX_DELAY);
    uint32_t allocatedId = 0xFFFFFFFF;
    for (uint32_t i = 1; i < LOSCFG_BASE_CORE_TSK_LIMIT; i++) {
        if (!s_taskTable[i].inUse) {
            allocatedId = i;
            s_taskTable[i].inUse = TRUE;
            break;
        }
    }
    xSemaphoreGive(s_taskTableMutex);

    if (allocatedId == 0xFFFFFFFF) {
        return LOS_ERRNO_TSK_NOT_CREATED;
    }

    uint32_t stackSize = pstInitParam->uwStackSize;
    if (stackSize < LOSCFG_BASE_CORE_TSK_MIN_STACK_SIZE) {
        stackSize = LOSCFG_BASE_CORE_TSK_MIN_STACK_SIZE;
    }

    const char *name = pstInitParam->pcName ? pstInitParam->pcName : "los_task";
    strncpy(s_taskTable[allocatedId].name, name, sizeof(s_taskTable[allocatedId].name) - 1);
    s_taskTable[allocatedId].pfnEntry = pstInitParam->pfnTaskEntry;
    s_taskTable[allocatedId].uwArg = pstInitParam->uwArg;
    s_taskTable[allocatedId].usPrio = pstInitParam->usTaskPrio;

    UBaseType_t frPrio = LosPrioToFreeRTOS(pstInitParam->usTaskPrio);
    BaseType_t ret = xTaskCreate(
        LosTaskWrapper,
        name,
        stackSize,
        (void *)(uintptr_t)allocatedId,
        frPrio,
        &s_taskTable[allocatedId].xTask
    );

    if (ret != pdPASS) {
        xSemaphoreTake(s_taskTableMutex, portMAX_DELAY);
        s_taskTable[allocatedId].inUse = FALSE;
        xSemaphoreGive(s_taskTableMutex);
        return LOS_ERRNO_TSK_NOT_CREATED;
    }

    *puwTaskID = allocatedId;
    return LOS_OK;
}

UINT32 LOS_TaskDelete(UINT32 uwTaskID)
{
    InitTaskTableIfNeeded();
    if (uwTaskID >= LOSCFG_BASE_CORE_TSK_LIMIT) {
        return LOS_ERRNO_TSK_NOT_CREATED;
    }

    xSemaphoreTake(s_taskTableMutex, portMAX_DELAY);
    if (!s_taskTable[uwTaskID].inUse) {
        xSemaphoreGive(s_taskTableMutex);
        return LOS_ERRNO_TSK_NOT_CREATED;
    }

    TaskHandle_t handle = s_taskTable[uwTaskID].xTask;
    s_taskTable[uwTaskID].inUse = FALSE;
    s_taskTable[uwTaskID].xTask = NULL;
    xSemaphoreGive(s_taskTableMutex);

    if (handle != NULL && handle != xTaskGetCurrentTaskHandle()) {
        vTaskDelete(handle);
    }
    return LOS_OK;
}

UINT32 LOS_TaskDelay(UINT32 uwTick)
{
    vTaskDelay((TickType_t)uwTick);
    return LOS_OK;
}

UINT32 LOS_TaskSuspend(UINT32 uwTaskID)
{
    InitTaskTableIfNeeded();
    if (uwTaskID >= LOSCFG_BASE_CORE_TSK_LIMIT || !s_taskTable[uwTaskID].inUse) {
        return LOS_ERRNO_TSK_NOT_CREATED;
    }
    vTaskSuspend(s_taskTable[uwTaskID].xTask);
    return LOS_OK;
}

UINT32 LOS_TaskResume(UINT32 uwTaskID)
{
    InitTaskTableIfNeeded();
    if (uwTaskID >= LOSCFG_BASE_CORE_TSK_LIMIT || !s_taskTable[uwTaskID].inUse) {
        return LOS_ERRNO_TSK_NOT_CREATED;
    }
    vTaskResume(s_taskTable[uwTaskID].xTask);
    return LOS_OK;
}

UINT32 LOS_TaskPriSet(UINT32 uwTaskID, UINT16 usTaskPrio)
{
    InitTaskTableIfNeeded();
    if (uwTaskID >= LOSCFG_BASE_CORE_TSK_LIMIT || !s_taskTable[uwTaskID].inUse) {
        return LOS_ERRNO_TSK_NOT_CREATED;
    }
    if (usTaskPrio > LOS_TASK_PRIORITY_LOWEST) {
        return LOS_ERRNO_TSK_PRIOR_ERROR;
    }
    s_taskTable[uwTaskID].usPrio = usTaskPrio;
    vTaskPrioritySet(s_taskTable[uwTaskID].xTask, LosPrioToFreeRTOS(usTaskPrio));
    return LOS_OK;
}

UINT16 LOS_TaskPriGet(UINT32 uwTaskID)
{
    InitTaskTableIfNeeded();
    if (uwTaskID >= LOSCFG_BASE_CORE_TSK_LIMIT || !s_taskTable[uwTaskID].inUse) {
        return (UINT16)LOS_TASK_PRIORITY_LOWEST;
    }
    return FreeRTOSToLosPrio(uxTaskPriorityGet(s_taskTable[uwTaskID].xTask));
}

UINT32 LOS_CurTaskIDGet(VOID)
{
    InitTaskTableIfNeeded();
    TaskHandle_t cur = xTaskGetCurrentTaskHandle();
    for (uint32_t i = 1; i < LOSCFG_BASE_CORE_TSK_LIMIT; i++) {
        if (s_taskTable[i].inUse && s_taskTable[i].xTask == cur) {
            return i;
        }
    }
    return 0; /* Fallback / system task */
}

UINT32 LOS_MS2Tick(UINT32 millisec)
{
    return (UINT32)pdMS_TO_TICKS(millisec);
}

UINT32 LOS_Tick2MS(UINT32 ticks)
{
    return (UINT32)(ticks * portTICK_PERIOD_MS);
}
