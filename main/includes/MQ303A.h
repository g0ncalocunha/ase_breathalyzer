#ifndef __MQ303A_H__INCLUDED__
#define __MQ303A_H__INCLUDED__

#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"

#include "esp_timer.h"
#include "esp_log.h"
#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define ADC_WIDTH ADC_BITWIDTH_12
#define ADC_ATTEN ADC_ATTEN_DB_12

void mq303a_init(adc_channel_t channel, adc_oneshot_unit_handle_t* adc_handle,
                  adc_cali_handle_t* adc_cali_handle);
bool mq303a_start_heatup(int heater_gpio);
bool mq303a_stop_heatup(int heater_gpio);
float mq303a_get_rs_air(adc_channel_t channel, adc_oneshot_unit_handle_t* adc_handle,
                  adc_cali_handle_t* adc_cali_handle, int samples);
float mq303a_get_rs_gas(adc_channel_t channel, adc_oneshot_unit_handle_t* adc_handle,
                  adc_cali_handle_t* adc_cali_handle);

#endif