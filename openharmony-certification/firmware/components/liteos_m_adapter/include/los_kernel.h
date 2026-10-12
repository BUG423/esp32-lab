/*
 * Copyright (c) 2026 OpenHarmony Certification Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#ifndef OH_LOS_KERNEL_H
#define OH_LOS_KERNEL_H

#include "los_typedef.h"
#include "los_task.h"
#include "los_mux.h"
#include "los_sem.h"
#include "los_queue.h"
#include "los_swtmr.h"
#include "los_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

UINT32 LOS_KernelInit(VOID);
UINT32 LOS_KernelStart(VOID);

#ifdef __cplusplus
}
#endif

#endif /* OH_LOS_KERNEL_H */
