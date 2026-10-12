/*
 * Copyright (c) 2026 OpenHarmony Certification Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#include "i2c_if.h"
#include "hdf_device_desc.h"
#include "hdf_manager.h"
#include "hdf_log.h"
#include "driver/i2c.h"
#include <string.h>

#define MAX_I2C_PORTS 2

typedef struct {
    int16_t port;
    bool opened;
} EspI2cDevice;

static EspI2cDevice s_i2cDevices[MAX_I2C_PORTS] = {
    { .port = 0, .opened = false },
    { .port = 1, .opened = false }
};

DevHandle I2cOpen(int16_t number)
{
    if (number < 0 || number >= MAX_I2C_PORTS) {
        return NULL;
    }

    EspI2cDevice *dev = &s_i2cDevices[number];
    if (dev->opened) {
        return (DevHandle)dev;
    }

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = (number == 0) ? 21 : 18,
        .scl_io_num = (number == 0) ? 22 : 19,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };

    esp_err_t err = i2c_param_config((i2c_port_t)number, &conf);
    if (err == ESP_OK) {
        err = i2c_driver_install((i2c_port_t)number, conf.mode, 0, 0, 0);
    }

    if (err == ESP_OK) {
        dev->opened = true;
        HDF_LOGI("I2C%d opened (SDA:%d, SCL:%d)", number, conf.sda_io_num, conf.scl_io_num);
        return (DevHandle)dev;
    }

    return NULL;
}

void I2cClose(DevHandle handle)
{
    if (handle == NULL) return;
    EspI2cDevice *dev = (EspI2cDevice *)handle;
    if (dev->opened) {
        i2c_driver_delete((i2c_port_t)dev->port);
        dev->opened = false;
        HDF_LOGI("I2C%d closed", dev->port);
    }
}

int32_t I2cTransfer(DevHandle handle, struct I2cMsg *msgs, int16_t count)
{
    if (handle == NULL || msgs == NULL || count <= 0) {
        return HDF_ERR_INVALID_PARAM;
    }
    EspI2cDevice *dev = (EspI2cDevice *)handle;

    for (int16_t i = 0; i < count; i++) {
        struct I2cMsg *m = &msgs[i];
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);

        if (m->flags & I2C_FLAG_READ) {
            i2c_master_write_byte(cmd, (m->addr << 1) | I2C_MASTER_READ, true);
            if (m->len > 1) {
                i2c_master_read(cmd, m->buf, m->len - 1, I2C_MASTER_ACK);
            }
            if (m->len > 0) {
                i2c_master_read_byte(cmd, m->buf + m->len - 1, I2C_MASTER_NACK);
            }
        } else {
            i2c_master_write_byte(cmd, (m->addr << 1) | I2C_MASTER_WRITE, true);
            if (m->len > 0) {
                i2c_master_write(cmd, m->buf, m->len, true);
            }
        }

        i2c_master_stop(cmd);
        esp_err_t ret = i2c_master_cmd_begin((i2c_port_t)dev->port, cmd, pdMS_TO_TICKS(100));
        i2c_cmd_link_delete(cmd);

        if (ret != ESP_OK) {
            return HDF_FAILURE;
        }
    }

    return HDF_SUCCESS;
}

static int32_t HdfI2cDriverInit(struct HdfDeviceObject *deviceObject)
{
    (void)deviceObject;
    HDF_LOGI("HDF I2C platform driver initialized");
    return HDF_SUCCESS;
}

static struct HdfDriverEntry s_i2cDriverEntry = {
    .driverName = "HDF_PLATFORM_I2C",
    .Init = HdfI2cDriverInit,
    .Bind = NULL,
    .Release = NULL,
};

static void __attribute__((constructor)) RegisterI2cDriver(void)
{
    HdfRegisterDriver(&s_i2cDriverEntry);
}
