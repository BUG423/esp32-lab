#include "ds3231.h"

#include <stdint.h>
#include <string.h>

#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "ds3231";

#if CONFIG_ICECLOCK_DS3231_ENABLE

#define DS3231_ADDRESS       0x68
#define DS3231_REG_TIME      0x00
#define DS3231_REG_STATUS    0x0f
#define DS3231_REG_TEMP_MSB  0x11
#define DS3231_STATUS_OSF    0x80
#define I2C_TIMEOUT_MS       100

static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_device;
static bool s_available;

static uint8_t bcd_to_binary(uint8_t value)
{
    return (value >> 4) * 10 + (value & 0x0f);
}

static uint8_t binary_to_bcd(int value)
{
    return (uint8_t)(((value / 10) << 4) | (value % 10));
}

static esp_err_t read_registers(uint8_t first_register, uint8_t *data,
                                size_t length)
{
    return i2c_master_transmit_receive(s_device, &first_register, 1,
                                       data, length, I2C_TIMEOUT_MS);
}

static esp_err_t write_registers(uint8_t first_register, const uint8_t *data,
                                 size_t length)
{
    uint8_t buffer[8];
    ESP_RETURN_ON_FALSE(length <= sizeof(buffer) - 1, ESP_ERR_INVALID_SIZE,
                        TAG, "write too large");
    buffer[0] = first_register;
    memcpy(&buffer[1], data, length);
    return i2c_master_transmit(s_device, buffer, length + 1, I2C_TIMEOUT_MS);
}

esp_err_t ds3231_init(void)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = -1,
        .sda_io_num = CONFIG_ICECLOCK_I2C_SDA_GPIO,
        .scl_io_num = CONFIG_ICECLOCK_I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &s_bus), TAG,
                        "create I2C bus");

    i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = DS3231_ADDRESS,
        .scl_speed_hz = 100000,
    };
    esp_err_t err = i2c_master_bus_add_device(s_bus, &device_config, &s_device);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "add device failed: %s", esp_err_to_name(err));
        return err;
    }

    uint8_t status;
    err = read_registers(DS3231_REG_STATUS, &status, 1);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "RTC not found on GPIO %d/%d: %s",
                 CONFIG_ICECLOCK_I2C_SDA_GPIO, CONFIG_ICECLOCK_I2C_SCL_GPIO,
                 esp_err_to_name(err));
        return err;
    }

    s_available = true;
    ESP_LOGI(TAG, "RTC detected%s", (status & DS3231_STATUS_OSF) ?
             " (time lost; waiting for NTP)" : "");
    return ESP_OK;
}

bool ds3231_is_available(void)
{
    return s_available;
}

esp_err_t ds3231_read_time(struct tm *time_info)
{
    ESP_RETURN_ON_FALSE(s_available && time_info, ESP_ERR_INVALID_STATE, TAG,
                        "RTC unavailable");

    uint8_t status;
    ESP_RETURN_ON_ERROR(read_registers(DS3231_REG_STATUS, &status, 1), TAG,
                        "read status");
    ESP_RETURN_ON_FALSE((status & DS3231_STATUS_OSF) == 0,
                        ESP_ERR_INVALID_STATE, TAG, "RTC time is invalid");

    uint8_t data[7];
    ESP_RETURN_ON_ERROR(read_registers(DS3231_REG_TIME, data, sizeof(data)),
                        TAG, "read time");

    memset(time_info, 0, sizeof(*time_info));
    time_info->tm_sec = bcd_to_binary(data[0] & 0x7f);
    time_info->tm_min = bcd_to_binary(data[1] & 0x7f);
    if (data[2] & 0x40) {
        int hour = bcd_to_binary(data[2] & 0x1f);
        bool afternoon = (data[2] & 0x20) != 0;
        time_info->tm_hour = (hour % 12) + (afternoon ? 12 : 0);
    } else {
        time_info->tm_hour = bcd_to_binary(data[2] & 0x3f);
    }
    time_info->tm_wday = (bcd_to_binary(data[3] & 0x07) + 6) % 7;
    time_info->tm_mday = bcd_to_binary(data[4] & 0x3f);
    time_info->tm_mon = bcd_to_binary(data[5] & 0x1f) - 1;
    time_info->tm_year = bcd_to_binary(data[6]) + 100;
    time_info->tm_isdst = -1;

    ESP_RETURN_ON_FALSE(time_info->tm_sec <= 59 && time_info->tm_min <= 59 &&
                        time_info->tm_hour <= 23 && time_info->tm_mday >= 1 &&
                        time_info->tm_mday <= 31 && time_info->tm_mon >= 0 &&
                        time_info->tm_mon <= 11,
                        ESP_ERR_INVALID_RESPONSE, TAG, "invalid RTC fields");
    return ESP_OK;
}

esp_err_t ds3231_write_time(const struct tm *time_info)
{
    ESP_RETURN_ON_FALSE(s_available && time_info, ESP_ERR_INVALID_STATE, TAG,
                        "RTC unavailable");
    ESP_RETURN_ON_FALSE(time_info->tm_year >= 100 && time_info->tm_year <= 199,
                        ESP_ERR_INVALID_ARG, TAG, "year outside 2000..2099");

    uint8_t data[7] = {
        binary_to_bcd(time_info->tm_sec),
        binary_to_bcd(time_info->tm_min),
        binary_to_bcd(time_info->tm_hour),
        binary_to_bcd(time_info->tm_wday + 1),
        binary_to_bcd(time_info->tm_mday),
        binary_to_bcd(time_info->tm_mon + 1),
        binary_to_bcd(time_info->tm_year - 100),
    };
    ESP_RETURN_ON_ERROR(write_registers(DS3231_REG_TIME, data, sizeof(data)),
                        TAG, "write time");

    uint8_t status;
    ESP_RETURN_ON_ERROR(read_registers(DS3231_REG_STATUS, &status, 1), TAG,
                        "read status");
    status &= ~DS3231_STATUS_OSF;
    ESP_RETURN_ON_ERROR(write_registers(DS3231_REG_STATUS, &status, 1), TAG,
                        "clear oscillator-stop flag");
    return ESP_OK;
}

esp_err_t ds3231_read_temperature(float *temperature_c)
{
    ESP_RETURN_ON_FALSE(s_available && temperature_c, ESP_ERR_INVALID_STATE,
                        TAG, "RTC unavailable");
    uint8_t data[2];
    ESP_RETURN_ON_ERROR(read_registers(DS3231_REG_TEMP_MSB, data, sizeof(data)),
                        TAG, "read temperature");
    int16_t quarters = (int16_t)((int8_t)data[0]) * 4 + (data[1] >> 6);
    *temperature_c = quarters * 0.25f;
    return ESP_OK;
}

#else

esp_err_t ds3231_init(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

bool ds3231_is_available(void)
{
    return false;
}

esp_err_t ds3231_read_time(struct tm *time_info)
{
    (void)time_info;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t ds3231_write_time(const struct tm *time_info)
{
    (void)time_info;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t ds3231_read_temperature(float *temperature_c)
{
    (void)temperature_c;
    return ESP_ERR_NOT_SUPPORTED;
}

#endif
