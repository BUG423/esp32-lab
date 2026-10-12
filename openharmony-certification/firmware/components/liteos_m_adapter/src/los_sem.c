/*
 * Copyright (c) 2026 OpenHarmony Certification Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#include "los_sem.h"
#include "los_config.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

typedef struct {
    SemaphoreHandle_t xSem;
    BOOL inUse;
} LosSemEntry;

static LosSemEntry s_semTable[LOSCFG_BASE_IPC_SEM_LIMIT];
static SemaphoreHandle_t s_semTableLock = NULL;
static BOOL s_semInitialized = FALSE;

static void InitSemTableIfNeeded(void)
{
    if (!s_semInitialized) {
        s_semTableLock = xSemaphoreCreateMutex();
        memset(s_semTable, 0, sizeof(s_semTable));
        s_semInitialized = TRUE;
    }
}

UINT32 LOS_SemCreate(UINT16 usCount, UINT32 *puwSemHandle)
{
    if (puwSemHandle == NULL) {
        return LOS_ERRNO_SEM_PTR_NULL;
    }

    InitSemTableIfNeeded();

    xSemaphoreTake(s_semTableLock, portMAX_DELAY);
    uint32_t allocatedId = 0xFFFFFFFF;
    for (uint32_t i = 1; i < LOSCFG_BASE_IPC_SEM_LIMIT; i++) {
        if (!s_semTable[i].inUse) {
            allocatedId = i;
            s_semTable[i].inUse = TRUE;
            break;
        }
    }
    xSemaphoreGive(s_semTableLock);

    if (allocatedId == 0xFFFFFFFF) {
        return LOS_ERRNO_SEM_INVALID;
    }

    s_semTable[allocatedId].xSem = xSemaphoreCreateCounting(65535, usCount);
    if (s_semTable[allocatedId].xSem == NULL) {
        xSemaphoreTake(s_semTableLock, portMAX_DELAY);
        s_semTable[allocatedId].inUse = FALSE;
        xSemaphoreGive(s_semTableLock);
        return LOS_ERRNO_SEM_INVALID;
    }

    *puwSemHandle = allocatedId;
    return LOS_OK;
}

UINT32 LOS_BinarySemCreate(UINT16 usCount, UINT32 *puwSemHandle)
{
    if (puwSemHandle == NULL) {
        return LOS_ERRNO_SEM_PTR_NULL;
    }

    InitSemTableIfNeeded();

    xSemaphoreTake(s_semTableLock, portMAX_DELAY);
    uint32_t allocatedId = 0xFFFFFFFF;
    for (uint32_t i = 1; i < LOSCFG_BASE_IPC_SEM_LIMIT; i++) {
        if (!s_semTable[i].inUse) {
            allocatedId = i;
            s_semTable[i].inUse = TRUE;
            break;
        }
    }
    xSemaphoreGive(s_semTableLock);

    if (allocatedId == 0xFFFFFFFF) {
        return LOS_ERRNO_SEM_INVALID;
    }

    s_semTable[allocatedId].xSem = xSemaphoreCreateBinary();
    if (s_semTable[allocatedId].xSem == NULL) {
        xSemaphoreTake(s_semTableLock, portMAX_DELAY);
        s_semTable[allocatedId].inUse = FALSE;
        xSemaphoreGive(s_semTableLock);
        return LOS_ERRNO_SEM_INVALID;
    }

    if (usCount > 0) {
        xSemaphoreGive(s_semTable[allocatedId].xSem);
    }

    *puwSemHandle = allocatedId;
    return LOS_OK;
}

UINT32 LOS_SemDelete(UINT32 uwSemHandle)
{
    InitSemTableIfNeeded();
    if (uwSemHandle == 0 || uwSemHandle >= LOSCFG_BASE_IPC_SEM_LIMIT) {
        return LOS_ERRNO_SEM_INVALID;
    }

    xSemaphoreTake(s_semTableLock, portMAX_DELAY);
    if (!s_semTable[uwSemHandle].inUse) {
        xSemaphoreGive(s_semTableLock);
        return LOS_ERRNO_SEM_INVALID;
    }

    SemaphoreHandle_t s = s_semTable[uwSemHandle].xSem;
    s_semTable[uwSemHandle].xSem = NULL;
    s_semTable[uwSemHandle].inUse = FALSE;
    xSemaphoreGive(s_semTableLock);

    if (s != NULL) {
        vSemaphoreDelete(s);
    }
    return LOS_OK;
}

UINT32 LOS_SemPend(UINT32 uwSemHandle, UINT32 uwTimeout)
{
    InitSemTableIfNeeded();
    if (uwSemHandle == 0 || uwSemHandle >= LOSCFG_BASE_IPC_SEM_LIMIT || !s_semTable[uwSemHandle].inUse) {
        return LOS_ERRNO_SEM_INVALID;
    }

    TickType_t ticks = (uwTimeout == LOS_WAIT_FOREVER) ? portMAX_DELAY : (TickType_t)uwTimeout;
    if (xSemaphoreTake(s_semTable[uwSemHandle].xSem, ticks) == pdTRUE) {
        return LOS_OK;
    }
    return LOS_ERRNO_SEM_TIMEOUT;
}

UINT32 LOS_SemPost(UINT32 uwSemHandle)
{
    InitSemTableIfNeeded();
    if (uwSemHandle == 0 || uwSemHandle >= LOSCFG_BASE_IPC_SEM_LIMIT || !s_semTable[uwSemHandle].inUse) {
        return LOS_ERRNO_SEM_INVALID;
    }

    if (xSemaphoreGive(s_semTable[uwSemHandle].xSem) == pdTRUE) {
        return LOS_OK;
    }
    return LOS_ERRNO_SEM_INVALID;
}
