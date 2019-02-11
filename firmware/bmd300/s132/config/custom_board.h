#ifndef CUSTOM_BOARD_H
#define CUSTOM_BOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "nrf_gpio.h"

#define LEDS_NUMBER    1

#define LED_START      7
#define LED_1          7
#define LED_STOP       7

#define LEDS_ACTIVE_STATE 0

#define LEDS_INV_MASK  LEDS_MASK

#define LEDS_LIST { LED_1 }

#define BSP_LED_0      LED_1

#define BUTTONS_NUMBER 1

#define BUTTON_START   6
#define BUTTON_1       6
#define BUTTON_STOP    6
#define BUTTON_PULL    NRF_GPIO_PIN_PULLUP

#define BUTTONS_ACTIVE_STATE 0

#define BUTTONS_LIST { BUTTON_1 }

#define BSP_BUTTON_0   BUTTON_1

#define CHG_AL_N       4
#define CHG_AL_N_PULL  NRF_GPIO_PIN_PULLUP
#define CHG_DET        5
#define CHG_DET_PULL   NRF_GPIO_PIN_NOPULL
#define TIME_PWM       15
#define PWR_STAT       16
#define PWR_STAT_PULL  NRF_GPIO_PIN_PULLUP
#define PWR_EN         17


#ifdef __cplusplus
}
#endif

#endif // CUSTOM_BOARD_H
