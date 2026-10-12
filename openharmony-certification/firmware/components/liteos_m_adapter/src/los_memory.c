/*
 * Copyright (c) 2026 OpenHarmony Certification Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#include "los_memory.h"
#include <stdlib.h>
#include "esp_heap_caps.h"

VOID *LOS_MemAlloc(VOID *pPool, UINT32 size)
{
    (void)pPool;
    if (size == 0) {
        return NULL;
    }
    return heap_caps_malloc(size, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
}

UINT32 LOS_MemFree(VOID *pPool, VOID *pMem)
{
    (void)pPool;
    if (pMem == NULL) {
        return LOS_NOK;
    }
    free(pMem);
    return LOS_OK;
}

UINT32 LOS_MemPoolInit(VOID *pPool, UINT32 size)
{
    (void)pPool;
    (void)size;
    /* ESP-IDF heap allocator is globally available */
    return LOS_OK;
}

VOID *LOS_MemAllocAlign(VOID *pPool, UINT32 size, UINT32 boundary)
{
    (void)pPool;
    if (size == 0 || boundary == 0) {
        return NULL;
    }
    return heap_caps_aligned_alloc(boundary, size, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
}
