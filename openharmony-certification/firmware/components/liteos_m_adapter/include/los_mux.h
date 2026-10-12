/*
 * Copyright (c) 2026 OpenHarmony Certification Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#ifndef OH_LOS_MUX_H
#define OH_LOS_MUX_H

#include "los_typedef.h"
#include "los_errno.h"

#ifdef __cplusplus
extern "C" {
#endif

UINT32 LOS_MuxCreate(UINT32 *puwMuxHandle);
UINT32 LOS_MuxDelete(UINT32 uwMuxHandle);
UINT32 LOS_MuxPend(UINT32 uwMuxHandle, UINT32 uwTimeout);
UINT32 LOS_MuxPost(UINT32 uwMuxHandle);

#ifdef __cplusplus
}
#endif

#endif /* OH_LOS_MUX_H */
