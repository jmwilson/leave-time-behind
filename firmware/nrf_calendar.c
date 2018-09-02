/**
 * Copyright (c) 2018 James Wilson
 * All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "nrf_calendar.h"
#include "nrfx_clock.h"
#include "nrfx_rtc.h"

static struct tm time_struct, m_tm_return_time;
static time_t m_time;
static uint32_t m_rtc_increment = 60;

static nrfx_rtc_t rtc_inst = NRFX_RTC_INSTANCE(2);

void cal_rtc_handler(nrfx_rtc_int_type_t int_type)
{
    if (int_type == NRFX_RTC_INT_COMPARE0) {
        nrfx_rtc_counter_clear(&rtc_inst);
        m_time += m_rtc_increment;
    }
}

void clock_handler(nrfx_clock_evt_type_t event)
{
}

void nrf_cal_init(void)
{
    uint32_t err_code;

    err_code = nrfx_clock_init(clock_handler);
    APP_ERROR_CHECK(err_code);
    nrfx_clock_lfclk_start();
    while (!nrfx_clock_lfclk_is_running());
    nrfx_clock_enable();

    nrfx_rtc_config_t config = NRFX_RTC_DEFAULT_CONFIG;
    config.prescaler = 4095;  // 8 Hz = 32 kHz / (prescaler + 1)
    config.interrupt_priority = 2;
    config.reliable = true;
    err_code = nrfx_rtc_init(&rtc_inst, &config, cal_rtc_handler);
    APP_ERROR_CHECK(err_code);
    nrfx_rtc_tick_disable(&rtc_inst);
    // Compare counter will rollover every 60 s
    err_code = nrfx_rtc_cc_set(&rtc_inst, 0, 60 * 8, true);

    APP_ERROR_CHECK(err_code);
    nrfx_rtc_enable(&rtc_inst);
}

void nrf_cal_set_time(uint32_t year, uint32_t month, uint32_t day, uint32_t hour, uint32_t minute, uint32_t second)
{
    static time_t newtime;
    time_struct.tm_year = year - 1900;
    time_struct.tm_mon = month - 1;
    time_struct.tm_mday = day;
    time_struct.tm_hour = hour;
    time_struct.tm_min = minute;
    time_struct.tm_sec = second;
    newtime = mktime(&time_struct);

    nrfx_rtc_counter_clear(&rtc_inst);
    m_time = newtime;
}

struct tm *nrf_cal_get_time(void)
{
    time_t return_time;
    return_time = m_time + nrfx_rtc_counter_get(&rtc_inst) / 8;
    m_tm_return_time = *localtime(&return_time);
    return &m_tm_return_time;
}

char *nrf_cal_get_time_string(void)
{
    static char cal_string[80];
    strftime(cal_string, 80, "%x - %H:%M:%S", nrf_cal_get_time());
    return cal_string;
}
