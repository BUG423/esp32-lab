/*
 * Copyright (c) 2026 OpenHarmony Certification Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#ifndef OH_LOS_QUEUE_H
#define OH_LOS_QUEUE_H

#include "los_typedef.h"
#include "los_errno.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tagQueueInfo {
    UINT32 uwQueueID;
    UINT16 usQueueLen;
    UINT16 usQueueSize;
    UINT16 usQueueHead;
    UINT16 usQueueTail;
    UINT16 usWritableCnt;
    UINT16 usReadableCnt;
} QUEUE_INFO_S;

UINT32 LOS_QueueCreate(CHAR *pcQueueName, UINT16 usLen, UINT32 *puwQueueID, UINT32 uwFlags, UINT16 usMaxMsgSize);
UINT32 LOS_QueueDelete(UINT32 uwQueueID);
UINT32 LOS_QueueRead(UINT32 uwQueueID, VOID *pBufferAddr, UINT32 uwBufferSize, UINT32 uwTimeout);
UINT32 LOS_QueueWrite(UINT32 uwQueueID, VOID *pBufferAddr, UINT32 uwBufferSize, UINT32 uwTimeout);
UINT32 LOS_QueueInfoGet(UINT32 uwQueueID, QUEUE_INFO_S *pstQueueInfo);

#ifdef __cplusplus
}
#endif

#endif /* OH_LOS_QUEUE_H */
