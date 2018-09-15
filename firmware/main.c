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

#include <stdint.h>
#include <string.h>
#include "nrf.h"
#include "app_error.h"
#include "app_scheduler.h"
#include "app_timer.h"
#include "ble.h"
#include "ble_cts_c.h"
#include "ble_db_discovery.h"
#include "ble_hci.h"
#include "ble_srv_common.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#include "bsp.h"
#include "peer_manager.h"
#include "nordic_common.h"
#include "nrf_sdh.h"
#include "nrf_sdh_soc.h"
#include "nrf_sdh_ble.h"
#include "nrfx_pwm.h"
#include "ble_conn_state.h"
#include "nrf_ble_gatt.h"
#include "nrf_ble_qwr.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "nrf_calendar.h"


#define DEVICE_NAME                     "Nixie Clock"

#define APP_BLE_OBSERVER_PRIO           3
#define APP_BLE_CONN_CFG_TAG            1  // A tag identifying the SoftDevice BLE configuration

#define APP_ADV_FAST_INTERVAL           0x0028  // Fast advertising interval (in units of 0.625 ms) = 25 ms
#define APP_ADV_SLOW_INTERVAL           0x0C80  // Slow advertising interval (in units of 0.625 ms) = 2s

#define APP_ADV_FAST_DURATION           3000   // Fast Advertising duration in units of 10 milliseconds
#define APP_ADV_SLOW_DURATION           18000  // Slow advertising duration in units 10 milliseconds


#define MIN_CONN_INTERVAL               MSEC_TO_UNITS(500, UNIT_1_25_MS)   // Minimum acceptable connection interval (0.5 seconds)
#define MAX_CONN_INTERVAL               MSEC_TO_UNITS(1000, UNIT_1_25_MS)  // Maximum acceptable connection interval (1 second)
#define SLAVE_LATENCY                   0
#define CONN_SUP_TIMEOUT                MSEC_TO_UNITS(4000, UNIT_10_MS)  // Connection supervisory time-out (4 seconds)
#define FIRST_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(5000)  // Time from connect to first sd_ble_gap_conn_param_update
#define NEXT_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(30000)  // Time between subsequent calls to sd_ble_gap_conn_param_update
#define MAX_CONN_PARAMS_UPDATE_COUNT    3  // Number of attempts before giving up the connection parameter negotiation

// Parameters for Just Works security and no IO capabiltiies
#define SEC_PARAM_BOND                  0
#define SEC_PARAM_MITM                  0
#define SEC_PARAM_LESC                  0
#define SEC_PARAM_KEYPRESS              0
#define SEC_PARAM_IO_CAPABILITIES       BLE_GAP_IO_CAPS_NONE
#define SEC_PARAM_OOB                   0
#define SEC_PARAM_MIN_KEY_SIZE          7
#define SEC_PARAM_MAX_KEY_SIZE          16

#define SCHED_MAX_EVENT_DATA_SIZE       APP_TIMER_SCHED_EVENT_DATA_SIZE
#ifdef SVCALL_AS_NORMAL_FUNCTION
// Maximum number of events in the scheduler queue.
#define SCHED_QUEUE_SIZE                20
#else
#define SCHED_QUEUE_SIZE                10
#endif

#define DEAD_BEEF                       0xDEADBEEF


BLE_CTS_C_DEF(m_cts_c);  // Current Time service instance
NRF_BLE_GATT_DEF(m_gatt);  // GATT module instance
NRF_BLE_QWR_DEF(m_qwr);  // Context for the Queued Write module
BLE_ADVERTISING_DEF(m_advertising);  // Advertising module instance
BLE_DB_DISCOVERY_DEF(m_ble_db_discovery);  // DB discovery module instance

static uint16_t     m_cur_conn_handle = BLE_CONN_HANDLE_INVALID;  // Handle of the current connection

#define PWM_OUTPUT_PIN 15
#define PWM_INVERT_OUTPUT ((uint16_t)0x8000)
#define PWM_TOP_VALUE 1600  // At 16 MHz, this sets a PWM frequency of 10 kHz
static nrfx_pwm_t pwm_inst = NRFX_PWM_INSTANCE(0);
static nrf_pwm_values_common_t pwm_values[] = {PWM_INVERT_OUTPUT | 0};

static ble_uuid_t m_adv_uuids[] = {{BLE_UUID_CURRENT_TIME_SERVICE, BLE_UUID_TYPE_BLE}};

APP_TIMER_DEF(ble_disconnect_timer);


/**
 * Callback function for asserts in the SoftDevice.
 *
 * @param[in] line_num  Line number of the failing ASSERT call.
 * @param[in] file_name File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
}


static void advertising_start(void)
{
    ret_code_t ret;

    ret = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
    if (ret != NRF_ERROR_INVALID_STATE) {
        APP_ERROR_CHECK(ret);
    }
}


static void pm_evt_handler(pm_evt_t const * p_evt)
{
    ret_code_t err_code;

    switch (p_evt->evt_id) {
        case PM_EVT_CONN_SEC_SUCCEEDED:
            NRF_LOG_INFO("Connection secured: role: %d, conn_handle: 0x%x, procedure: %d.",
                         ble_conn_state_role(p_evt->conn_handle),
                         p_evt->conn_handle,
                         p_evt->params.conn_sec_succeeded.procedure);

            // Discover peer's services.
            err_code  = ble_db_discovery_start(&m_ble_db_discovery, p_evt->conn_handle);
            APP_ERROR_CHECK(err_code);
            break;

        case PM_EVT_CONN_SEC_FAILED:
            break;

        case PM_EVT_ERROR_UNEXPECTED:
            APP_ERROR_CHECK(p_evt->params.error_unexpected.error);
            break;

        default:
            break;
    }
}


/**
 * Handler for Current Time Service errors.
 *
 * @param[in]  nrf_error  Error code containing information about what went wrong.
 */
static void current_time_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}


static void ble_disconnect_timer_handler(void *p_context)
{
    ble_cts_c_t *cts = (ble_cts_c_t *)p_context;
    (void)sd_ble_gap_disconnect(cts->conn_handle,
                                BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
}


static void timers_init(void)
{
    ret_code_t err_code;

    err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);

    err_code = app_timer_create(&ble_disconnect_timer,
        APP_TIMER_MODE_SINGLE_SHOT, ble_disconnect_timer_handler);
}


/**
 * Handler for Current Time Service client events.
 *
 * @param[in] p_evt Event received from the Current Time Service client.
 */
static void on_cts_c_evt(ble_cts_c_t * p_cts, ble_cts_c_evt_t * p_evt)
{
    ret_code_t err_code;

    switch (p_evt->evt_type) {
        case BLE_CTS_C_EVT_DISCOVERY_COMPLETE:
            NRF_LOG_INFO("Current Time Service discovered on server.");
            err_code = ble_cts_c_handles_assign(p_cts,
                                                p_evt->conn_handle,
                                                &p_evt->params.char_handles);
            APP_ERROR_CHECK(err_code);
            err_code = ble_cts_c_current_time_read(p_cts);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_CTS_C_EVT_DISCOVERY_FAILED:
            NRF_LOG_INFO("Current Time Service not found on server. ");
            if (p_evt->conn_handle != BLE_CONN_HANDLE_INVALID)
            {
                err_code = sd_ble_gap_disconnect(p_evt->conn_handle,
                                                 BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
                APP_ERROR_CHECK(err_code);
            }
            break;

        case BLE_CTS_C_EVT_DISCONN_COMPLETE:
            NRF_LOG_INFO("Disconnect Complete.");
            break;

        case BLE_CTS_C_EVT_CURRENT_TIME:
            NRF_LOG_INFO("Current Time received.");
            err_code = bsp_indication_set(BSP_INDICATE_RCV_OK);
            APP_ERROR_CHECK(err_code);
            nrf_cal_set_time(
                p_evt->params.current_time.exact_time_256.day_date_time.date_time.year,
                p_evt->params.current_time.exact_time_256.day_date_time.date_time.month,
                p_evt->params.current_time.exact_time_256.day_date_time.date_time.day,
                p_evt->params.current_time.exact_time_256.day_date_time.date_time.hours,
                p_evt->params.current_time.exact_time_256.day_date_time.date_time.minutes,
                p_evt->params.current_time.exact_time_256.day_date_time.date_time.seconds +
                    (p_evt->params.current_time.exact_time_256.fractions256 >= 128 ? 1 : 0)
            );
            (void)app_timer_start(ble_disconnect_timer, APP_TIMER_TICKS(5000), (void *)p_cts);
            break;

        case BLE_CTS_C_EVT_INVALID_TIME:
            NRF_LOG_INFO("Invalid Time received.");
            err_code = bsp_indication_set(BSP_INDICATE_RCV_ERROR);
            APP_ERROR_CHECK(err_code);
            break;

        default:
            break;
    }
}


static void gap_params_init(void)
{
    ret_code_t              err_code;
    ble_gap_conn_params_t   gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *)DEVICE_NAME,
                                          strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err_code);
    err_code = sd_ble_gap_appearance_set(BLE_APPEARANCE_GENERIC_CLOCK);
    APP_ERROR_CHECK(err_code);
    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);
}


static void gatt_init(void)
{
    ret_code_t err_code = nrf_ble_gatt_init(&m_gatt, NULL);
    APP_ERROR_CHECK(err_code);
}

/**
 * Handler for Queued Write Module errors.
 *
 * @param[in]   nrf_error   Error code containing information about what went wrong.
 */
static void nrf_qwr_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}


static void services_init(void)
{
    ret_code_t         err_code;
    ble_cts_c_init_t   cts_init = {0};
    nrf_ble_qwr_init_t qwr_init = {0};

    // Initialize Queued Write Module.
    qwr_init.error_handler = nrf_qwr_error_handler;
    err_code = nrf_ble_qwr_init(&m_qwr, &qwr_init);
    APP_ERROR_CHECK(err_code);

    // Initialize CTS.
    cts_init.evt_handler   = on_cts_c_evt;
    cts_init.error_handler = current_time_error_handler;
    err_code = ble_cts_c_init(&m_cts_c, &cts_init);
    APP_ERROR_CHECK(err_code);
}


/**
 * Handler for Connection Parameters module errors.
 *
 * @param[in]   nrf_error   Error code containing information about what went wrong.
 */
static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}


static void conn_params_init(void)
{
    ret_code_t             err_code;
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail             = true;
    cp_init.error_handler                  = conn_params_error_handler;

    err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);
}


static void db_disc_handler(ble_db_discovery_evt_t * p_evt)
{
    ble_cts_c_on_db_disc_evt(&m_cts_c, p_evt);
}


/**
 * Handler for BLE advertising events.
 *
 * @param[in] ble_adv_evt  Advertising event.
 */
static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    ret_code_t err_code;

    switch (ble_adv_evt) {
        case BLE_ADV_EVT_FAST:
            NRF_LOG_INFO("Fast advertising");
            err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_ADV_EVT_SLOW:
            NRF_LOG_INFO("Slow advertising");
            err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING_SLOW);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_ADV_EVT_IDLE:
            NRF_LOG_INFO("Stopping advertising");
            err_code = bsp_indication_set(BSP_INDICATE_IDLE);
            APP_ERROR_CHECK(err_code);
            break;

        default:
            break;
    }
}


static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context)
{
    ret_code_t err_code;
    pm_conn_sec_status_t status;

    switch (p_ble_evt->header.evt_id) {
        case BLE_GAP_EVT_CONNECTED:
            NRF_LOG_INFO("Connected.");
            err_code = bsp_indication_set(BSP_INDICATE_CONNECTED);
            APP_ERROR_CHECK(err_code);
            m_cur_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            err_code = nrf_ble_qwr_conn_handle_assign(&m_qwr, m_cur_conn_handle);
            APP_ERROR_CHECK(err_code);

            err_code = pm_conn_sec_status_get(m_cur_conn_handle, &status);
            APP_ERROR_CHECK(err_code);
            if (!status.encrypted) {
                err_code = pm_conn_secure(m_cur_conn_handle, false);
                APP_ERROR_CHECK(err_code);
            }
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            NRF_LOG_INFO("Disconnected.");
            err_code = bsp_indication_set(BSP_INDICATE_IDLE);
            APP_ERROR_CHECK(err_code);
            m_cur_conn_handle = BLE_CONN_HANDLE_INVALID;
            if (p_ble_evt->evt.gap_evt.conn_handle == m_cts_c.conn_handle) {
                m_cts_c.conn_handle = BLE_CONN_HANDLE_INVALID;
            }
            break;

        case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
            NRF_LOG_DEBUG("PHY update request.");
            ble_gap_phys_t const phys =
            {
                .rx_phys = BLE_GAP_PHY_AUTO,
                .tx_phys = BLE_GAP_PHY_AUTO,
            };
            err_code = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTC_EVT_TIMEOUT:
            // Disconnect on GATT Client timeout event.
            NRF_LOG_DEBUG("GATT Client Timeout.");
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTS_EVT_TIMEOUT:
            // Disconnect on GATT Server timeout event.
            NRF_LOG_DEBUG("GATT Server Timeout.");
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;

        default:
            break;
    }
}


void bsp_event_handler(bsp_event_t event)
{
    switch (event) {
        case BSP_EVENT_KEY_0:
            NRF_LOG_INFO("%s", nrf_log_push(nrf_cal_get_time_string()));
            break;

        case BSP_EVENT_ADVERTISING_START:
            if (m_cur_conn_handle == BLE_CONN_HANDLE_INVALID) {
                advertising_start();
            }
            break;

        default:
            break;
    }
}


static void ble_stack_init(void)
{
    ret_code_t err_code;

    err_code = nrf_sdh_enable_request();
    APP_ERROR_CHECK(err_code);

    // Configure the BLE stack using the default settings.
    // Fetch the start address of the application RAM.
    uint32_t ram_start = 0;
    err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
    APP_ERROR_CHECK(err_code);

    // Enable BLE stack.
    err_code = nrf_sdh_ble_enable(&ram_start);
    APP_ERROR_CHECK(err_code);

    // Register a handler for BLE events.
    NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);
}


static void scheduler_init(void)
{
    APP_SCHED_INIT(SCHED_MAX_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE);
}


static void buttons_leds_init(void)
{
    ret_code_t err_code;

    err_code = bsp_init(BSP_INIT_LEDS | BSP_INIT_BUTTONS, bsp_event_handler);
    APP_ERROR_CHECK(err_code);

    err_code = bsp_event_to_button_action_assign(
        0, BSP_BUTTON_ACTION_LONG_PUSH, BSP_EVENT_ADVERTISING_START);
    APP_ERROR_CHECK(err_code);
}


static void peer_manager_init(void)
{
    ble_gap_sec_params_t sec_param;
    ret_code_t           err_code;

    err_code = pm_init();
    APP_ERROR_CHECK(err_code);

    memset(&sec_param, 0, sizeof(ble_gap_sec_params_t));

    // Security parameters for Just Works pairing without bonding
    sec_param.bond           = SEC_PARAM_BOND;
    sec_param.mitm           = SEC_PARAM_MITM;
    sec_param.lesc           = SEC_PARAM_LESC;
    sec_param.keypress       = SEC_PARAM_KEYPRESS;
    sec_param.io_caps        = SEC_PARAM_IO_CAPABILITIES;
    sec_param.oob            = SEC_PARAM_OOB;
    sec_param.min_key_size   = SEC_PARAM_MIN_KEY_SIZE;
    sec_param.max_key_size   = SEC_PARAM_MAX_KEY_SIZE;
    sec_param.kdist_own.enc  = 0;
    sec_param.kdist_own.id   = 0;
    sec_param.kdist_peer.enc = 0;
    sec_param.kdist_peer.id  = 0;

    err_code = pm_sec_params_set(&sec_param);
    APP_ERROR_CHECK(err_code);

    err_code = pm_register(pm_evt_handler);
    APP_ERROR_CHECK(err_code);
}


static void advertising_init()
{
    ret_code_t             err_code;
    ble_advertising_init_t init;

    memset(&init, 0, sizeof(init));

    init.advdata.name_type                = BLE_ADVDATA_FULL_NAME;
    init.advdata.include_appearance       = true;
    init.advdata.flags                    = BLE_GAP_ADV_FLAGS_LE_ONLY_LIMITED_DISC_MODE;
    init.advdata.uuids_solicited.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
    init.advdata.uuids_solicited.p_uuids  = m_adv_uuids;

    init.config.ble_adv_on_disconnect_disabled = true;
    init.config.ble_adv_fast_enabled      = true;
    init.config.ble_adv_fast_interval     = APP_ADV_FAST_INTERVAL;
    init.config.ble_adv_fast_timeout      = APP_ADV_FAST_DURATION;
    init.config.ble_adv_slow_enabled      = true;
    init.config.ble_adv_slow_interval     = APP_ADV_SLOW_INTERVAL;
    init.config.ble_adv_slow_timeout      = APP_ADV_SLOW_DURATION;
    init.evt_handler = on_adv_evt;

    err_code = ble_advertising_init(&m_advertising, &init);
    APP_ERROR_CHECK(err_code);

    ble_advertising_conn_cfg_tag_set(&m_advertising, APP_BLE_CONN_CFG_TAG);
}


static void db_discovery_init(void)
{
    ret_code_t err_code = ble_db_discovery_init(db_disc_handler);
    APP_ERROR_CHECK(err_code);
}


static void log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_DEFAULT_BACKENDS_INIT();
}


static void idle_state_handle(void)
{
    ret_code_t err_code;

    app_sched_execute();

    if (NRF_LOG_PROCESS() == false) {
        err_code = sd_app_evt_wait();
        APP_ERROR_CHECK(err_code);
    }
}

static void pwm_handler(nrfx_pwm_evt_type_t event_type)
{
    if (event_type == NRFX_PWM_EVT_FINISHED) {
        struct tm *current_time = nrf_cal_get_time();
        // tm_hour / 24 * PWM_TOP_VALUE = tm_hour * 600 / 9
        // tm_min / (24 * 60) * PWM_TOP_VALUE = tm_min * 10 / 9
        // Set high bit to invert PWM:
        // https://devzone.nordicsemi.com/f/nordic-q-a/28263/invert-pwm-behaviour
        pwm_values[0] = PWM_INVERT_OUTPUT |
                        ((600 * current_time->tm_hour +
                          10 * current_time->tm_min) /
                         9);
    }
}


static void pwm_init(void)
{
    ret_code_t err_code;

    nrfx_pwm_config_t config = {
        .output_pins = {
            PWM_OUTPUT_PIN,
            NRFX_PWM_PIN_NOT_USED,
            NRFX_PWM_PIN_NOT_USED,
            NRFX_PWM_PIN_NOT_USED
        },
        .irq_priority = APP_IRQ_PRIORITY_LOWEST,
        .base_clock = NRF_PWM_CLK_16MHz,
        .count_mode = NRF_PWM_MODE_UP,
        .top_value = PWM_TOP_VALUE,
        .load_mode = NRF_PWM_LOAD_COMMON,
        .step_mode = NRF_PWM_STEP_AUTO
    };
    err_code = nrfx_pwm_init(&pwm_inst, &config, pwm_handler);
    APP_ERROR_CHECK(err_code);

    nrf_pwm_sequence_t const seq = {
        .values.p_common = pwm_values,
        .length = NRF_PWM_VALUES_LENGTH(pwm_values),
        .repeats = 0,
        .end_delay = 0
    };

    err_code = nrfx_pwm_simple_playback(&pwm_inst, &seq, 10000, NRFX_PWM_FLAG_LOOP);
    APP_ERROR_CHECK(err_code);
}


int main(void)
{
    log_init();
    timers_init();
    buttons_leds_init();
    scheduler_init();
    ble_stack_init();
    gap_params_init();
    gatt_init();
    db_discovery_init();
    advertising_init();
    peer_manager_init();
    services_init();
    conn_params_init();
    nrf_cal_init();
    pwm_init();

    for (;;) {
        idle_state_handle();
    }
}
