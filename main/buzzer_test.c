/* Breathalyzer

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <stdio.h>

// GPIO Port Mapping
#define GPIO_LED          7
#define GPIO_BUTTON       10

// SD Card Reader
#define GPIO_SD_CS        1
#define GPIO_SD_MOSI      6
#define GPIO_SD_CLK       5
#define GPIO_SD_MISO      4

// // Alcohol Sensor
// #define GPIO_ALCOHOL_SCLK 3
// #define GPIO_ALCOHOL_DAT  2

// Buzzer Configuration
static void periodic_timer_callback(void *arg);

#define BUZZER_TIMER LEDC_TIMER_0
#define BUZZER_MODE LEDC_LOW_SPEED_MODE
#define BUZZER_OUTPUT (0) // Define the output GPIO
#define BUZZER_CHANNEL LEDC_CHANNEL_0
#define BUZZER_DUTY_RES LEDC_TIMER_13_BIT // Set duty resolution to 13 bits
// #define LEDC_DUTY               (4096) // Set duty to 50%. (2 ** 13) * 50% =
// 4096k
#define BUZZER_FREQUENCY (1000) // Frequency in Hertz. Set frequency at 4 kHz


typedef enum {
	OFF = 0,
	ON = 1000
} buzzer_duty_t;

static const buzzer_duty_t buzzer_duty_levels[] = {
	OFF, ON};

static void buzzer_init(void) {
	// Prepare and then apply the LEDC PWM timer configuration
	ledc_timer_config_t buzzer_timer = {
		.speed_mode = BUZZER_MODE,
		.duty_resolution = BUZZER_DUTY_RES,
		.timer_num = BUZZER_TIMER,
		.freq_hz = BUZZER_FREQUENCY, // Set output frequency at 1 kHz
		.clk_cfg = LEDC_AUTO_CLK};
	ESP_ERROR_CHECK(ledc_timer_config(&buzzer_timer));

	// Prepare and then apply the LEDC PWM channel configuration
	ledc_channel_config_t buzzer_channel = {.speed_mode = BUZZER_MODE,
										  .channel = BUZZER_CHANNEL,
										  .timer_sel = BUZZER_TIMER,
										  .gpio_num = BUZZER_OUTPUT,
										  .duty = 0, // Set duty to 0%
										  .hpoint = 0};
	ESP_ERROR_CHECK(ledc_channel_config(&buzzer_channel));
}

void buzzer_on(void) {
	// Set duty to 50%
	ESP_ERROR_CHECK(ledc_set_duty(BUZZER_MODE, BUZZER_CHANNEL, buzzer_duty_levels[1]));
	// Update duty to apply the new value
	ESP_ERROR_CHECK(ledc_update_duty(BUZZER_MODE, BUZZER_CHANNEL));
}

void buzzer_off(void) {
	// Set duty to 50%
	ESP_ERROR_CHECK(ledc_set_duty(BUZZER_MODE, BUZZER_CHANNEL, buzzer_duty_levels[0]));
	// Update duty to apply the new value
	ESP_ERROR_CHECK(ledc_update_duty(BUZZER_MODE, BUZZER_CHANNEL));
}

// LED Configuration

static void configure_led(void)
{
    // ESP_LOGI("LED", "Configured GPIO LED!");
    gpio_reset_pin(GPIO_LED);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(GPIO_LED, GPIO_MODE_OUTPUT);
}

// Button Configuration

static void configure_button(void)
{
    // ESP_LOGI("LED", "Configured GPIO LED!");
    gpio_reset_pin(GPIO_BUTTON);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(GPIO_BUTTON, GPIO_MODE_INPUT);
}

// Timer Configuration

static void esp_t_init(void) {

	const esp_timer_create_args_t periodic_timer_args = {
		.callback = &periodic_timer_callback,
		/* name is optional, but may help identify the timer when debugging */
		.name = "periodic"};

	esp_timer_handle_t periodic_timer;
	ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
	/* The timer has been created but is not running yet */
    /* Start the timers */
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 2000000));
}

static void periodic_timer_callback(void* arg)
{
    // int64_t time_since_boot = esp_timer_get_time();
    // ESP_LOGI(TAG, "Periodic timer called, time since boot: %lld us", time_since_boot);
     buzzer_off();
}


void app_main(void)
{
    static uint8_t s_led_state = 0;

    /* Configure the peripheral according to the LED type */
    esp_t_init();
		buzzer_init();
    configure_led();
    configure_button();
		// gpio_set_level(GPIO_LED, s_led_state);

    while (1) {
		gpio_set_level(GPIO_BUTTON, s_led_state);	
        // // ESP_LOGI(TAG, "Turning the LED %s!", s_led_state == true ? "ON" : "OFF");
        // gpio_set_level(GPIO_LED, s_led_state);
        // /* Toggle the LED state */
        // s_led_state = !s_led_state;
        // vTaskDelay(1000);
        
    }
}
