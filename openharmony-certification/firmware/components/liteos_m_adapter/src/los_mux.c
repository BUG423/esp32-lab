/*
 * Copyright (c) 2026 OpenHarmony Certification Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#include "los_mux.h"
#include "los_config.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

typedef struct {
    SemaphoreHandle_t xMutex;
    BOOL inUse;
} LosMuxEntry;

static LosMuxEntry s_muxTable[LOSCFG_BASE_IPC_MUX_LIMIT];
static SemaphoreHandle_t s_muxTableLock = NULL;
static BOOL s_muxInitialized = FALSE;

static void InitMuxTableIfNeeded(void)
{
    if (!s_muxInitialized) {
        s_muxTableLock = xSemaphoreCreateMutex();
        memset(s_muxTable, 0, sizeof(s_muxTable));
        s_muxInitialized = TRUE;
    }
}

UINT32 LOS_MuxCreate(UINT32 *puwMuxHandle)
{
    if (puwMuxHandle == NULL) {
        return LOS_ERRNO_MUX_PTR_NULL;
    }

    InitMuxTableIfNeeded();

    xSemaphoreTake(s_muxTableLock, portMAX_DELAY);
    uint32_t allocatedId = 0xFFFFFFFF;
    for (uint32_t i = 1; i < LOSCFG_BASE_IPC_MUX_LIMIT; i++) {
        if (!s_muxTable[i].inUse) {
            allocatedId = i;
            s_muxTable[i].inUse = TRUE;
            break;
        }
    }
    xSemaphoreGive(s_muxTableLock);

    if (allocatedId == 0xFFFFFFFF) {
        return LOS_ERRNO_MUX_INVALID;
    }

    s_muxTable[allocatedId].xMutex = xSemaphoreCreateRecursiveMutex();
    if (s_muxTable[allocatedId].xMutex == NULL) {
        xSemaphoreTake(s_muxTableLock, portMAX_DELAY);
        s_muxTable[allocatedId].inUse = FALSE;
        xSemaphoreGive(s_muxTableLock);
        return LOS_ERRNO_MUX_INVALID;
    }

    *puwMuxHandle = allocatedId;
    return LOS_OK;
}

UINT32 LOS_MuxDelete(UINT32 uwMuxHandle)
{
    InitMuxTableIfNeeded();
    if (uwMuxHandle == 0 || uwMuxHandle >= LOSCFG_BASE_IPC_MUX_LIMIT) {
        return LOS_ERRNO_MUX_INVALID;
    }

    xSemaphoreTake(s_muxTableLock, portMAX_DELAY);
    if (!s_muxTable[uwMuxHandle].inUse) {
        xSemaphoreGive(s_muxTableLock);
        return LOS_ERRNO_MUX_INVALID;
    }

    SemaphoreHandle_t m = s_muxTable[uwMuxHandle].xMutex;
    s_muxTable[uwMuxHandle].xMutex = NULL;
    s_muxTable[uwMuxHandle].inUse = FALSE;
    xSemaphoreGive(s_muxTableLock);

    if (m != NULL) {
        vSemaphoreDelete(m);
    }
    return LOS_OK;
}

UINT32 LOS_MuxPend(UINT32 uwMuxHandle, UINT32 uwTimeout)
{
    InitMuxTableIfNeeded();
    if (uwMuxHandle == 0 || uwMuxHandle >= LOSCFG_BASE_IPC_MUX_LIMIT || !s_muxTable[uwMuxHandle].inUse) {
        return LOS_ERRNO_MUX_INVALID;
    }

    TickType_t ticks = (uwTimeout == LOS_WAIT_FOREVER) ? portMAX_DELAY : (TickType_t)uwTimeout;
    if (xSemaphoreTakeRecursive(s_muxTable[uwMuxHandle].xMutex, ticks) == pdTRUE) {
        return LOS_OK;
    }
    return LOS_ERRNO_MUX_TIMEOUT;
}

UINT32 LOS_MuxPost(UINT32 uwMuxHandle)
{
    InitMuxTableIfNeeded();
    if (uwMuxHandle == 0 || uwMuxHandle >= LOSCFG_BASE_IPC_MUX_LIMIT || !s_muxTable[uwMuxHandle].inUse) {
        return LOS_ERRNO_MUX_INVALID;
    }

    if (xSemaphoreGiveRecursive(s_muxTable[uwMuxHandle].xMutex) == pdTRUE) {
        return LOS_OK;
    }
    return LOS_ERRNO_MUX_INVALID;
}
