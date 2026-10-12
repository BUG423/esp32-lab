/*
 * Copyright (c) 2026 OpenHarmony Certification Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#ifndef OH_I2C_IF_H
#define OH_I2C_IF_H

#include "hdf_base.h"

#ifdef __cplusplus
extern "C" {
#endif

#define I2C_FLAG_READ  0x0001
#define I2C_FLAG_WRITE 0x0000

struct I2cMsg {
    uint16_t addr;
    uint16_t flags;
    uint16_t len;
    uint8_t  *buf;
};

DevHandle I2cOpen(int16_t number);
void I2cClose(DevHandle handle);
int32_t I2cTransfer(DevHandle handle, struct I2cMsg *msgs, int16_t count);

#ifdef __cplusplus
}
#endif

#endif /* OH_I2C_IF_H */
