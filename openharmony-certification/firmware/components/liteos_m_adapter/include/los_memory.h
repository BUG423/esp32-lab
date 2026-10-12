/*
 * Copyright (c) 2026 OpenHarmony Certification Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#ifndef OH_LOS_MEMORY_H
#define OH_LOS_MEMORY_H

#include "los_typedef.h"

#ifdef __cplusplus
extern "C" {
#endif

VOID *LOS_MemAlloc(VOID *pPool, UINT32 size);
UINT32 LOS_MemFree(VOID *pPool, VOID *pMem);
UINT32 LOS_MemPoolInit(VOID *pPool, UINT32 size);
VOID *LOS_MemAllocAlign(VOID *pPool, UINT32 size, UINT32 boundary);

#ifdef __cplusplus
}
#endif

#endif /* OH_LOS_MEMORY_H */
