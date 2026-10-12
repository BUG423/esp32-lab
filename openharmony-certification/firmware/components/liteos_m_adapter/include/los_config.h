/*
 * Copyright (c) 2026 OpenHarmony Certification Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#ifndef OH_LOS_CONFIG_H
#define OH_LOS_CONFIG_H

#include "los_typedef.h"

#ifdef __cplusplus
extern "C" {
#endif

/* LiteOS-M Priorities: 0 (Highest) to 31 (Lowest) */
#define LOS_TASK_PRIORITY_HIGHEST     0
#define LOS_TASK_PRIORITY_LOWEST      31
#define LOS_TASK_DEFAULT_PRIORITY     20

/* FreeRTOS uses inverted priority: 0 lowest, configMAX_PRIORITIES-1 highest */
/* Mapping function converts between LiteOS-M (0 high) and FreeRTOS (0 low) */

#define LOSCFG_BASE_CORE_TSK_LIMIT              64
#define LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE 4096
#define LOSCFG_BASE_CORE_TSK_MIN_STACK_SIZE     1024

#define LOSCFG_BASE_IPC_SEM_LIMIT               64
#define LOSCFG_BASE_IPC_MUX_LIMIT               64
#define LOSCFG_BASE_IPC_QUEUE_LIMIT             32
#define LOSCFG_BASE_CORE_SWTMR_LIMIT            32

#ifdef __cplusplus
}
#endif

#endif /* OH_LOS_CONFIG_H */
