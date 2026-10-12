/*
 * Copyright (c) 2026 OpenHarmony Certification Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#ifndef OH_LOS_SEM_H
#define OH_LOS_SEM_H

#include "los_typedef.h"
#include "los_errno.h"

#ifdef __cplusplus
extern "C" {
#endif

UINT32 LOS_SemCreate(UINT16 usCount, UINT32 *puwSemHandle);
UINT32 LOS_BinarySemCreate(UINT16 usCount, UINT32 *puwSemHandle);
UINT32 LOS_SemDelete(UINT32 uwSemHandle);
UINT32 LOS_SemPend(UINT32 uwSemHandle, UINT32 uwTimeout);
UINT32 LOS_SemPost(UINT32 uwSemHandle);

#ifdef __cplusplus
}
#endif

#endif /* OH_LOS_SEM_H */
