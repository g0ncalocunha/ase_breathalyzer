#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "includes/MQ303A.h"
#include "includes/buzzer.h"

#define HEATER_SEL_PIN  3

#define SAMPLE_COUNT 100
#define VREF_DEFAULT 2500 // Default reference voltage in mV

#define BUZZER_GPIO 0 // Define the output GPIO
#define BUZZER_FREQ 81

#define ADC_CHANNEL ADC_CHANNEL_2 // Define the ADC channel to use

static void timer_callback(void *arg);

static const char *TAG = "BREATHALYZER";
static bool heater_finished = false;


void breathalyzer_task(void *pvParameters)
{
    adc_oneshot_unit_handle_t adc_handle = NULL;
    adc_cali_handle_t adc_cali_handle = NULL;

    mq303a_init(ADC_CHANNEL, &adc_handle, &adc_cali_handle); // Initialize the MQ303A sensor
    mq303a_start_heatup(HEATER_SEL_PIN); // Start the heater
    // Main loop
    ESP_LOGI(TAG, "Waiting for heater to be ready...");
    while (!heater_finished) {
        vTaskDelay(pdMS_TO_TICKS(100)); // Wait for the heater to be ready
    }

    float RS_air = mq303a_get_rs_air(ADC_CHANNEL, &adc_handle, &adc_cali_handle, SAMPLE_COUNT); // Get RS_air value
    ESP_LOGI(TAG, "RS_air: %.3f", RS_air);

    while (1) {
        float RS_gas = mq303a_get_rs_gas(ADC_CHANNEL, &adc_handle, &adc_cali_handle); // Get RS_gas value

        float ratio = RS_gas / RS_air; // Calculate the ratio of RS values
        float PPM = pow(10, (log10(ratio) - 0.328) / -0.55); // Calculate PPM using the formula
        ESP_LOGI(TAG, "RS_gas: %.3f, Ratio: %.3f", RS_gas, ratio);
        ESP_LOGI(TAG, "PPM: %.2f", PPM);

        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second
    }

}

static void timer_callback(void *arg)
{
    // This function will be called when the timer expires
    mq303a_stop_heatup(HEATER_SEL_PIN); // Stop the heater
    heater_finished = true; // Set the ready flag to true
    
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Breathalyzer Application");

    buzzer_init(BUZZER_GPIO, BUZZER_FREQ); // Initialize the buzzer

    const esp_timer_create_args_t timer_arg = {
        .callback = &timer_callback,
        .arg = NULL, 
        .name = "heatup_timer",
    };
    esp_timer_handle_t heatup_timer;
    ESP_ERROR_CHECK(esp_timer_create(&timer_arg, &heatup_timer));
    ESP_ERROR_CHECK(esp_timer_start_once(heatup_timer, 10000000)); // Start timer for 10 seconds

    // Create breathalyzer task
    xTaskCreate(breathalyzer_task, "breathalyzer_task", 4096, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "Breathalyzer initialized successfully");
}
