/*
 * Copyright (c) 2026 OpenHarmony Certification Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#ifndef OH_GPIO_IF_H
#define OH_GPIO_IF_H

#include "hdf_base.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GPIO_DIR_IN      0
#define GPIO_DIR_OUT     1

#define GPIO_VAL_LOW     0
#define GPIO_VAL_HIGH    1

#define GPIO_INT_NONE    0
#define GPIO_INT_RISING  1
#define GPIO_INT_FALLING 2
#define GPIO_INT_BOTH    3

typedef void (*GpioIrqFunc)(uint16_t gpio, void *data);

int32_t GpioSetDir(uint16_t gpio, uint16_t dir);
int32_t GpioGetDir(uint16_t gpio, uint16_t *dir);
int32_t GpioWrite(uint16_t gpio, uint16_t val);
int32_t GpioRead(uint16_t gpio, uint16_t *val);
int32_t GpioSetIrq(uint16_t gpio, uint16_t mode, GpioIrqFunc func, void *arg);
int32_t GpioUnsetIrq(uint16_t gpio);
int32_t GpioEnableIrq(uint16_t gpio);
int32_t GpioDisableIrq(uint16_t gpio);

#ifdef __cplusplus
}
#endif

#endif /* OH_GPIO_IF_H */
