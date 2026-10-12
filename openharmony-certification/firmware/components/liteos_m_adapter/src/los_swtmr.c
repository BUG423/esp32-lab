/*
 * Copyright (c) 2026 OpenHarmony Certification Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#include "los_swtmr.h"
#include "los_config.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"

typedef struct {
    TimerHandle_t xTimer;
    SWTMR_PROC_FUNC pfnHandler;
    UINT32 uwArg;
    UINT8 ucMode;
    BOOL inUse;
} LosSwtmrEntry;

static LosSwtmrEntry s_swtmrTable[LOSCFG_BASE_CORE_SWTMR_LIMIT];
static SemaphoreHandle_t s_swtmrTableLock = NULL;
static BOOL s_swtmrInitialized = FALSE;

static void InitSwtmrTableIfNeeded(void)
{
    if (!s_swtmrInitialized) {
        s_swtmrTableLock = xSemaphoreCreateMutex();
        memset(s_swtmrTable, 0, sizeof(s_swtmrTable));
        s_swtmrInitialized = TRUE;
    }
}

static void FreeRTOSTimerCallback(TimerHandle_t xTimer)
{
    uint32_t timerId = (uint32_t)(uintptr_t)pvTimerGetTimerID(xTimer);
    SWTMR_PROC_FUNC handler = NULL;
    UINT32 arg = 0;

    if (s_swtmrTableLock) {
        xSemaphoreTake(s_swtmrTableLock, portMAX_DELAY);
        if (timerId < LOSCFG_BASE_CORE_SWTMR_LIMIT && s_swtmrTable[timerId].inUse) {
            handler = s_swtmrTable[timerId].pfnHandler;
            arg = s_swtmrTable[timerId].uwArg;
        }
        xSemaphoreGive(s_swtmrTableLock);
    }

    if (handler != NULL) {
        handler(arg);
    }
}

UINT32 LOS_SwtmrCreate(UINT32 uwInterval, UINT8 ucMode, SWTMR_PROC_FUNC pfnHandler, UINT16 *pusSwtmrId, UINT32 uwArg)
{
    if (pusSwtmrId == NULL) {
        return LOS_ERRNO_SWTMR_PTR_NULL;
    }
    if (pfnHandler == NULL || uwInterval == 0) {
        return LOS_ERRNO_SWTMR_INVALID;
    }

    InitSwtmrTableIfNeeded();

    xSemaphoreTake(s_swtmrTableLock, portMAX_DELAY);
    uint32_t allocatedId = 0xFFFFFFFF;
    for (uint32_t i = 1; i < LOSCFG_BASE_CORE_SWTMR_LIMIT; i++) {
        if (!s_swtmrTable[i].inUse) {
            allocatedId = i;
            s_swtmrTable[i].inUse = TRUE;
            break;
        }
    }
    xSemaphoreGive(s_swtmrTableLock);

    if (allocatedId == 0xFFFFFFFF) {
        return LOS_ERRNO_SWTMR_INVALID;
    }

    UBaseType_t autoReload = (ucMode == LOS_SWTMR_MODE_PERIOD) ? pdTRUE : pdFALSE;
    TickType_t periodTicks = (TickType_t)uwInterval;
    if (periodTicks == 0) periodTicks = 1;

    TimerHandle_t t = xTimerCreate(
        "los_tmr",
        periodTicks,
        autoReload,
        (void *)(uintptr_t)allocatedId,
        FreeRTOSTimerCallback
    );

    if (t == NULL) {
        xSemaphoreTake(s_swtmrTableLock, portMAX_DELAY);
        s_swtmrTable[allocatedId].inUse = FALSE;
        xSemaphoreGive(s_swtmrTableLock);
        return LOS_ERRNO_SWTMR_INVALID;
    }

    s_swtmrTable[allocatedId].xTimer = t;
    s_swtmrTable[allocatedId].pfnHandler = pfnHandler;
    s_swtmrTable[allocatedId].uwArg = uwArg;
    s_swtmrTable[allocatedId].ucMode = ucMode;

    *pusSwtmrId = (UINT16)allocatedId;
    return LOS_OK;
}

UINT32 LOS_SwtmrStart(UINT16 usSwtmrId)
{
    InitSwtmrTableIfNeeded();
    if (usSwtmrId == 0 || usSwtmrId >= LOSCFG_BASE_CORE_SWTMR_LIMIT || !s_swtmrTable[usSwtmrId].inUse) {
        return LOS_ERRNO_SWTMR_INVALID;
    }

    if (xTimerStart(s_swtmrTable[usSwtmrId].xTimer, portMAX_DELAY) == pdPASS) {
        return LOS_OK;
    }
    return LOS_ERRNO_SWTMR_INVALID;
}

UINT32 LOS_SwtmrStop(UINT16 usSwtmrId)
{
    InitSwtmrTableIfNeeded();
    if (usSwtmrId == 0 || usSwtmrId >= LOSCFG_BASE_CORE_SWTMR_LIMIT || !s_swtmrTable[usSwtmrId].inUse) {
        return LOS_ERRNO_SWTMR_INVALID;
    }

    if (xTimerStop(s_swtmrTable[usSwtmrId].xTimer, portMAX_DELAY) == pdPASS) {
        return LOS_OK;
    }
    return LOS_ERRNO_SWTMR_INVALID;
}

UINT32 LOS_SwtmrDelete(UINT16 usSwtmrId)
{
    InitSwtmrTableIfNeeded();
    if (usSwtmrId == 0 || usSwtmrId >= LOSCFG_BASE_CORE_SWTMR_LIMIT) {
        return LOS_ERRNO_SWTMR_INVALID;
    }

    xSemaphoreTake(s_swtmrTableLock, portMAX_DELAY);
    if (!s_swtmrTable[usSwtmrId].inUse) {
        xSemaphoreGive(s_swtmrTableLock);
        return LOS_ERRNO_SWTMR_INVALID;
    }

    TimerHandle_t t = s_swtmrTable[usSwtmrId].xTimer;
    s_swtmrTable[usSwtmrId].xTimer = NULL;
    s_swtmrTable[usSwtmrId].inUse = FALSE;
    xSemaphoreGive(s_swtmrTableLock);

    if (t != NULL) {
        xTimerDelete(t, portMAX_DELAY);
    }
    return LOS_OK;
}
