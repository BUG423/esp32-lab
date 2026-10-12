/*
 * Copyright (c) 2026 OpenHarmony Certification Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#ifndef OH_LOS_TYPEDEF_H
#define OH_LOS_TYPEDEF_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t   UINT8;
typedef uint16_t  UINT16;
typedef uint32_t  UINT32;
typedef int8_t    INT8;
typedef int16_t   INT16;
typedef int32_t   INT32;
typedef float     FLOAT;
typedef double    DOUBLE;
typedef char      CHAR;
typedef bool      BOOL;
typedef void      VOID;
typedef uintptr_t UINTPTR;

#ifndef TRUE
#define TRUE  1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

#define LOS_OK            0
#define LOS_NOK           1
#define LOS_WAIT_FOREVER  0xFFFFFFFF
#define LOS_NO_WAIT       0

#ifdef __cplusplus
}
#endif

#endif /* OH_LOS_TYPEDEF_H */
