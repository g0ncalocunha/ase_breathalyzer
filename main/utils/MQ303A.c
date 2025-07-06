#include "../includes/MQ303A.h"

static const char *TAG = "MQ303A";

static float calculate_current(float voltage) {
    float sensor_volt = voltage / 1000.0; // Convert to volts
    float real_volt = sensor_volt * (5.0 / 2.5); // Convert to the true sensor output
    ESP_LOGI(TAG, "Sensor Voltage: %.3f V", real_volt);
    return real_volt / (5.0 - real_volt); //
}

void mq303a_init(adc_channel_t channel, adc_oneshot_unit_handle_t *adc_handle,
                 adc_cali_handle_t *adc_cali_handle)
{
    // Initialize ADC
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, adc_handle));

    // ADC config
    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN,    // Set attenuation to 12dB
        .bitwidth = ADC_WIDTH, // Set bitwidth to 12 bits
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(*adc_handle, channel, &config));

    // Initialize ADC calibration
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN,
        .bitwidth = ADC_WIDTH,
    };
    esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_config, adc_cali_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "ADC calibration scheme not supported, using raw values");
        adc_cali_handle = NULL;
    }
    else
    {
        ESP_LOGI(TAG, "ADC calibration initialized successfully");
    }
}

bool mq303a_start_heatup(int heater_gpio)
{
    gpio_set_direction(heater_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(heater_gpio, 1); // Set heater pin HIGH initially
    ESP_LOGI(TAG, "GPIO setup complete: HEATER_SEL_PIN set to output");

    return true;
}

bool mq303a_stop_heatup(int heater_gpio)
{
    // Turn off the heater
    gpio_set_level(heater_gpio, 0); // Set heater pin LOW to turn off
    ESP_LOGI(TAG, "Heater turned off");

    return true;
}

float mq303a_get_rs_air(adc_channel_t channel, adc_oneshot_unit_handle_t *adc_handle,
                        adc_cali_handle_t *adc_cali_handle, int samples)
{
    int adc_raw = 0;
    int voltage = 0;    
    int total_voltage = 0;

    for (int i = 0;  i < samples; i++) {
        // Read ADC value
        ESP_ERROR_CHECK(adc_oneshot_read(*adc_handle, channel, &adc_raw));
        ESP_LOGI(TAG, "ADC Raw Value: %d", adc_raw);

        // Convert raw value to voltage
        if (adc_cali_handle) {
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(*adc_cali_handle, adc_raw, &voltage));
            ESP_LOGI(TAG, "Calibrated Voltage: %d mV", voltage);
            total_voltage += voltage;
        } 
    }

    total_voltage /= samples; // Average the voltage over the samples
    float RS_air = calculate_current(total_voltage); // Calculate RS_air

    return RS_air;
}

float mq303a_get_rs_gas(adc_channel_t channel, adc_oneshot_unit_handle_t *adc_handle,
                        adc_cali_handle_t *adc_cali_handle)
{
    int adc_raw = 0;
    int voltage = 0;    
    // Read ADC value
    ESP_ERROR_CHECK(adc_oneshot_read(*adc_handle, channel, &adc_raw));

    // Convert raw value to voltage
    if (adc_cali_handle) {
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(*adc_cali_handle, adc_raw, &voltage));
    } 

    float RS_gas = calculate_current(voltage); // Calculate RS_gas

    return RS_gas;
}



