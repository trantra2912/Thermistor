#include "../components/DS_1307.h"

#include "driver/i2c.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>
#define SECONDS_MASK 0x7f
#define HOUR12_MASK  0x1f
#define HOUR24_MASK  0x3f
#define HOUR12_BIT  (1 << 6)
#define PM_BIT      (1 << 5)
#define DS1307_ADDR 0x68

static uint8_t bcd2dec(uint8_t val)
{
    return (val/16) * 10 + (val %16);
}

static uint8_t dec2bcd(uint8_t val)
{
    return ((val / 10) << 4) + (val % 10);
}

void ds1307_set_time(const struct tm *time)
{
    uint8_t buf[7] = {
        dec2bcd(time->tm_sec),
        dec2bcd(time->tm_min),
        dec2bcd(time->tm_hour),
        dec2bcd(time->tm_wday + 1),
        dec2bcd(time->tm_mday),
        dec2bcd(time->tm_mon + 1),
        dec2bcd(time->tm_year - 100)
    };
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS1307_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0x00, true);
    i2c_master_write(cmd, buf, 7, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

}

void ds1307_get_time(struct tm *time)
{
    uint8_t buf[7];
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS1307_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0x00, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS1307_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, buf, 7, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    time->tm_sec = bcd2dec(buf[0] & SECONDS_MASK);
    time->tm_min = bcd2dec(buf[1]);
    if(buf[2] & HOUR12_BIT)
    {
        // RTC in 12-hour mode
        time->tm_hour = bcd2dec(buf[2] & HOUR12_MASK) - 1;
        if (buf[2] & PM_BIT)
            time->tm_hour += 12;
    }
    else
        time->tm_hour = bcd2dec(buf[2] & HOUR24_MASK);
    time->tm_wday = bcd2dec(buf[3]) - 1;
    time->tm_mday = bcd2dec(buf[4]);
    time->tm_mon  = bcd2dec(buf[5]) - 1;
    time->tm_year = bcd2dec(buf[6]) + 100;
}
const char* get_day_of_week(const struct tm *time)
{
    static const char* const days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    return days[time->tm_wday];
}

const char* get_date_string(const struct tm *time)
{
    static char date_str[20];
    strftime(date_str, sizeof(date_str), "%Y-%m-%d", time);
    return date_str;
}

const char* get_time_string(const struct tm *time)
{
    static char time_str[20];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", time);
    return time_str;
}