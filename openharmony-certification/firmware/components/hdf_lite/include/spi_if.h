/*
 * Copyright (c) 2026 OpenHarmony Certification Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#ifndef OH_SPI_IF_H
#define OH_SPI_IF_H

#include "hdf_base.h"

#ifdef __cplusplus
extern "C" {
#endif

struct SpiMsg {
    const uint8_t *wbuf;
    uint8_t       *rbuf;
    uint32_t      len;
};

struct SpiDevInfo {
    uint32_t busNum;
    uint32_t csPin;
    uint32_t speedHz;
};

DevHandle SpiOpen(const struct SpiDevInfo *info);
void SpiClose(DevHandle handle);
int32_t SpiTransfer(DevHandle handle, struct SpiMsg *msgs, uint32_t count);

#ifdef __cplusplus
}
#endif

#endif /* OH_SPI_IF_H */
