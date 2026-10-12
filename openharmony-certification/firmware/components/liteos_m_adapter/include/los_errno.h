/*
 * Copyright (c) 2026 OpenHarmony Certification Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#ifndef OH_LOS_ERRNO_H
#define OH_LOS_ERRNO_H

#include "los_typedef.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LOS_ERRNO_OS_NORMAL(mid, errNo)     (0x02000000 | ((mid) << 16) | (errNo))
#define LOS_ERRNO_OS_ERROR(mid, errNo)      (0x03000000 | ((mid) << 16) | (errNo))
#define LOS_ERRNO_OS_FATAL(mid, errNo)      (0x04000000 | ((mid) << 16) | (errNo))

#define LOS_MOD_TSK                         0x01
#define LOS_MOD_MEM                         0x02
#define LOS_MOD_SEM                         0x03
#define LOS_MOD_MUX                         0x04
#define LOS_MOD_QUEUE                       0x05
#define LOS_MOD_SWTMR                       0x06

#define LOS_ERRNO_TSK_PTR_NULL              LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x00)
#define LOS_ERRNO_TSK_NAME_EMPTY            LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x01)
#define LOS_ERRNO_TSK_PRIOR_ERROR           LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x02)
#define LOS_ERRNO_TSK_ENTRY_NULL            LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x03)
#define LOS_ERRNO_TSK_STKSZ_TOO_SMALL       LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x04)
#define LOS_ERRNO_TSK_NOT_CREATED           LOS_ERRNO_OS_ERROR(LOS_MOD_TSK, 0x05)

#define LOS_ERRNO_MUX_PTR_NULL              LOS_ERRNO_OS_ERROR(LOS_MOD_MUX, 0x00)
#define LOS_ERRNO_MUX_INVALID               LOS_ERRNO_OS_ERROR(LOS_MOD_MUX, 0x01)
#define LOS_ERRNO_MUX_TIMEOUT               LOS_ERRNO_OS_ERROR(LOS_MOD_MUX, 0x02)

#define LOS_ERRNO_SEM_PTR_NULL              LOS_ERRNO_OS_ERROR(LOS_MOD_SEM, 0x00)
#define LOS_ERRNO_SEM_INVALID               LOS_ERRNO_OS_ERROR(LOS_MOD_SEM, 0x01)
#define LOS_ERRNO_SEM_TIMEOUT               LOS_ERRNO_OS_ERROR(LOS_MOD_SEM, 0x02)

#define LOS_ERRNO_QUEUE_PTR_NULL            LOS_ERRNO_OS_ERROR(LOS_MOD_QUEUE, 0x00)
#define LOS_ERRNO_QUEUE_INVALID             LOS_ERRNO_OS_ERROR(LOS_MOD_QUEUE, 0x01)
#define LOS_ERRNO_QUEUE_TIMEOUT             LOS_ERRNO_OS_ERROR(LOS_MOD_QUEUE, 0x02)

#define LOS_ERRNO_SWTMR_PTR_NULL            LOS_ERRNO_OS_ERROR(LOS_MOD_SWTMR, 0x00)
#define LOS_ERRNO_SWTMR_INVALID             LOS_ERRNO_OS_ERROR(LOS_MOD_SWTMR, 0x01)

#ifdef __cplusplus
}
#endif

#endif /* OH_LOS_ERRNO_H */
