/*
 * Copyright (c) 2026 OpenHarmony Certification Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#include "spi_if.h"
#include "hdf_device_desc.h"
#include "hdf_manager.h"
#include "hdf_log.h"
#include "driver/spi_master.h"
#include <string.h>
#include <stdlib.h>

typedef struct {
    spi_device_handle_t spiHandle;
    uint32_t busNum;
    bool opened;
} EspSpiDevice;

DevHandle SpiOpen(const struct SpiDevInfo *info)
{
    if (info == NULL) {
        return NULL;
    }

    EspSpiDevice *dev = (EspSpiDevice *)calloc(1, sizeof(EspSpiDevice));
    if (dev == NULL) {
        return NULL;
    }

    spi_bus_config_t buscfg = {
        .miso_io_num = (info->busNum == 2) ? 19 : 25,
        .mosi_io_num = (info->busNum == 2) ? 23 : 26,
        .sclk_io_num = (info->busNum == 2) ? 18 : 27,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096
    };

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = (info->speedHz > 0) ? (int)info->speedHz : 1000000,
        .mode = 0,
        .spics_io_num = (int)info->csPin,
        .queue_size = 3
    };

    spi_host_device_t host = (info->busNum == 2) ? SPI2_HOST : SPI3_HOST;
    spi_bus_initialize(host, &buscfg, SPI_DMA_CH_AUTO);
    esp_err_t err = spi_bus_add_device(host, &devcfg, &dev->spiHandle);

    if (err == ESP_OK) {
        dev->busNum = info->busNum;
        dev->opened = true;
        HDF_LOGI("SPI%lu device opened (CS:%lu, Speed:%luhz)",
                 (unsigned long)info->busNum, (unsigned long)info->csPin, (unsigned long)devcfg.clock_speed_hz);
        return (DevHandle)dev;
    }

    free(dev);
    return NULL;
}

void SpiClose(DevHandle handle)
{
    if (handle == NULL) return;
    EspSpiDevice *dev = (EspSpiDevice *)handle;
    if (dev->opened && dev->spiHandle) {
        spi_bus_remove_device(dev->spiHandle);
        HDF_LOGI("SPI%lu device closed", (unsigned long)dev->busNum);
    }
    free(dev);
}

int32_t SpiTransfer(DevHandle handle, struct SpiMsg *msgs, uint32_t count)
{
    if (handle == NULL || msgs == NULL || count == 0) {
        return HDF_ERR_INVALID_PARAM;
    }

    EspSpiDevice *dev = (EspSpiDevice *)handle;
    for (uint32_t i = 0; i < count; i++) {
        struct SpiMsg *m = &msgs[i];
        if (m->len == 0) continue;

        spi_transaction_t t;
        memset(&t, 0, sizeof(t));
        t.length = m->len * 8;
        t.tx_buffer = m->wbuf;
        t.rx_buffer = m->rbuf;

        esp_err_t ret = spi_device_polling_transmit(dev->spiHandle, &t);
        if (ret != ESP_OK) {
            return HDF_FAILURE;
        }
    }

    return HDF_SUCCESS;
}

static int32_t HdfSpiDriverInit(struct HdfDeviceObject *deviceObject)
{
    (void)deviceObject;
    HDF_LOGI("HDF SPI platform driver initialized");
    return HDF_SUCCESS;
}

static struct HdfDriverEntry s_spiDriverEntry = {
    .driverName = "HDF_PLATFORM_SPI",
    .Init = HdfSpiDriverInit,
    .Bind = NULL,
    .Release = NULL,
};

static void __attribute__((constructor)) RegisterSpiDriver(void)
{
    HdfRegisterDriver(&s_spiDriverEntry);
}
