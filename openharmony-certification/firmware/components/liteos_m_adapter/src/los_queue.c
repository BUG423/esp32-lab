/*
 * Copyright (c) 2026 OpenHarmony Certification Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#include "los_queue.h"
#include "los_config.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

typedef struct {
    QueueHandle_t xQueue;
    UINT16 usMaxMsgSize;
    UINT16 usLen;
    BOOL inUse;
} LosQueueEntry;

static LosQueueEntry s_queueTable[LOSCFG_BASE_IPC_QUEUE_LIMIT];
static SemaphoreHandle_t s_queueTableLock = NULL;
static BOOL s_queueInitialized = FALSE;

static void InitQueueTableIfNeeded(void)
{
    if (!s_queueInitialized) {
        s_queueTableLock = xSemaphoreCreateMutex();
        memset(s_queueTable, 0, sizeof(s_queueTable));
        s_queueInitialized = TRUE;
    }
}

UINT32 LOS_QueueCreate(CHAR *pcQueueName, UINT16 usLen, UINT32 *puwQueueID, UINT32 uwFlags, UINT16 usMaxMsgSize)
{
    (void)pcQueueName;
    (void)uwFlags;

    if (puwQueueID == NULL) {
        return LOS_ERRNO_QUEUE_PTR_NULL;
    }
    if (usLen == 0 || usMaxMsgSize == 0) {
        return LOS_ERRNO_QUEUE_INVALID;
    }

    InitQueueTableIfNeeded();

    xSemaphoreTake(s_queueTableLock, portMAX_DELAY);
    uint32_t allocatedId = 0xFFFFFFFF;
    for (uint32_t i = 1; i < LOSCFG_BASE_IPC_QUEUE_LIMIT; i++) {
        if (!s_queueTable[i].inUse) {
            allocatedId = i;
            s_queueTable[i].inUse = TRUE;
            break;
        }
    }
    xSemaphoreGive(s_queueTableLock);

    if (allocatedId == 0xFFFFFFFF) {
        return LOS_ERRNO_QUEUE_INVALID;
    }

    s_queueTable[allocatedId].xQueue = xQueueCreate(usLen, usMaxMsgSize);
    if (s_queueTable[allocatedId].xQueue == NULL) {
        xSemaphoreTake(s_queueTableLock, portMAX_DELAY);
        s_queueTable[allocatedId].inUse = FALSE;
        xSemaphoreGive(s_queueTableLock);
        return LOS_ERRNO_QUEUE_INVALID;
    }

    s_queueTable[allocatedId].usLen = usLen;
    s_queueTable[allocatedId].usMaxMsgSize = usMaxMsgSize;

    *puwQueueID = allocatedId;
    return LOS_OK;
}

UINT32 LOS_QueueDelete(UINT32 uwQueueID)
{
    InitQueueTableIfNeeded();
    if (uwQueueID == 0 || uwQueueID >= LOSCFG_BASE_IPC_QUEUE_LIMIT) {
        return LOS_ERRNO_QUEUE_INVALID;
    }

    xSemaphoreTake(s_queueTableLock, portMAX_DELAY);
    if (!s_queueTable[uwQueueID].inUse) {
        xSemaphoreGive(s_queueTableLock);
        return LOS_ERRNO_QUEUE_INVALID;
    }

    QueueHandle_t q = s_queueTable[uwQueueID].xQueue;
    s_queueTable[uwQueueID].xQueue = NULL;
    s_queueTable[uwQueueID].inUse = FALSE;
    xSemaphoreGive(s_queueTableLock);

    if (q != NULL) {
        vQueueDelete(q);
    }
    return LOS_OK;
}

UINT32 LOS_QueueRead(UINT32 uwQueueID, VOID *pBufferAddr, UINT32 uwBufferSize, UINT32 uwTimeout)
{
    InitQueueTableIfNeeded();
    if (uwQueueID == 0 || uwQueueID >= LOSCFG_BASE_IPC_QUEUE_LIMIT || !s_queueTable[uwQueueID].inUse) {
        return LOS_ERRNO_QUEUE_INVALID;
    }
    if (pBufferAddr == NULL || uwBufferSize < s_queueTable[uwQueueID].usMaxMsgSize) {
        return LOS_ERRNO_QUEUE_INVALID;
    }

    TickType_t ticks = (uwTimeout == LOS_WAIT_FOREVER) ? portMAX_DELAY : (TickType_t)uwTimeout;
    if (xQueueReceive(s_queueTable[uwQueueID].xQueue, pBufferAddr, ticks) == pdTRUE) {
        return LOS_OK;
    }
    return LOS_ERRNO_QUEUE_TIMEOUT;
}

UINT32 LOS_QueueWrite(UINT32 uwQueueID, VOID *pBufferAddr, UINT32 uwBufferSize, UINT32 uwTimeout)
{
    InitQueueTableIfNeeded();
    if (uwQueueID == 0 || uwQueueID >= LOSCFG_BASE_IPC_QUEUE_LIMIT || !s_queueTable[uwQueueID].inUse) {
        return LOS_ERRNO_QUEUE_INVALID;
    }
    if (pBufferAddr == NULL) {
        return LOS_ERRNO_QUEUE_INVALID;
    }

    TickType_t ticks = (uwTimeout == LOS_WAIT_FOREVER) ? portMAX_DELAY : (TickType_t)uwTimeout;
    if (xQueueSend(s_queueTable[uwQueueID].xQueue, pBufferAddr, ticks) == pdTRUE) {
        return LOS_OK;
    }
    return LOS_ERRNO_QUEUE_TIMEOUT;
}

UINT32 LOS_QueueInfoGet(UINT32 uwQueueID, QUEUE_INFO_S *pstQueueInfo)
{
    InitQueueTableIfNeeded();
    if (uwQueueID == 0 || uwQueueID >= LOSCFG_BASE_IPC_QUEUE_LIMIT || !s_queueTable[uwQueueID].inUse) {
        return LOS_ERRNO_QUEUE_INVALID;
    }
    if (pstQueueInfo == NULL) {
        return LOS_ERRNO_QUEUE_PTR_NULL;
    }

    QueueHandle_t q = s_queueTable[uwQueueID].xQueue;
    pstQueueInfo->uwQueueID = uwQueueID;
    pstQueueInfo->usQueueLen = s_queueTable[uwQueueID].usLen;
    pstQueueInfo->usQueueSize = s_queueTable[uwQueueID].usMaxMsgSize;
    pstQueueInfo->usReadableCnt = (UINT16)uxQueueMessagesWaiting(q);
    pstQueueInfo->usWritableCnt = (UINT16)uxQueueSpacesAvailable(q);
    pstQueueInfo->usQueueHead = 0;
    pstQueueInfo->usQueueTail = 0;
    return LOS_OK;
}
