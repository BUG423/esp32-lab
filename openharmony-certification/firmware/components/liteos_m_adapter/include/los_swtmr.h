/*
 * Copyright (c) 2026 OpenHarmony Certification Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#ifndef OH_LOS_SWTMR_H
#define OH_LOS_SWTMR_H

#include "los_typedef.h"
#include "los_errno.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef VOID (*SWTMR_PROC_FUNC)(UINT32 uwArg);

#define LOS_SWTMR_MODE_ONCE            0
#define LOS_SWTMR_MODE_PERIOD          1
#define LOS_SWTMR_MODE_NO_SELFDELETE   2

UINT32 LOS_SwtmrCreate(UINT32 uwInterval, UINT8 ucMode, SWTMR_PROC_FUNC pfnHandler, UINT16 *pusSwtmrId, UINT32 uwArg);
UINT32 LOS_SwtmrStart(UINT16 usSwtmrId);
UINT32 LOS_SwtmrStop(UINT16 usSwtmrId);
UINT32 LOS_SwtmrDelete(UINT16 usSwtmrId);

#ifdef __cplusplus
}
#endif

#endif /* OH_LOS_SWTMR_H */
