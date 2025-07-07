#include "../includes/buzzer.h"

static int global_DC = 4096; // Default frequency in Hz

void buzzer_init(int gpio_num, int frequency) {

    global_DC = frequency; // Store the frequency for later use

	// Prepare and then apply the LEDC PWM timer configuration
	ledc_timer_config_t buzzer_timer = {
		.speed_mode = BUZZER_MODE,
		.duty_resolution = BUZZER_DUTY_RES,
		.timer_num = BUZZER_TIMER,
		.freq_hz = 1000, // Set output frequency at 1 kHz
		.clk_cfg = LEDC_AUTO_CLK};
	ESP_ERROR_CHECK(ledc_timer_config(&buzzer_timer));

	// Prepare and then apply the LEDC PWM channel configuration
	ledc_channel_config_t buzzer_channel = {.speed_mode = BUZZER_MODE,
										  .channel = BUZZER_CHANNEL,
										  .timer_sel = BUZZER_TIMER,
										  .gpio_num = gpio_num,
										  .duty = 0, // Set duty to 0%
										  .hpoint = 0};
	ESP_ERROR_CHECK(ledc_channel_config(&buzzer_channel));
}

void buzzer_on(int freq) {
	if (freq == 0) {
		buzzer_off();
		return; // If frequency is 0, turn off the buzzer
	}
	ESP_ERROR_CHECK(ledc_set_freq(BUZZER_MODE, BUZZER_TIMER, freq)); // Set the frequency for the buzzer
    ESP_ERROR_CHECK(ledc_set_duty(BUZZER_MODE, BUZZER_CHANNEL, global_DC));
    ESP_ERROR_CHECK(ledc_update_duty(BUZZER_MODE, BUZZER_CHANNEL));
}

void buzzer_off(void) {
    // Set duty to 0%
    ESP_ERROR_CHECK(ledc_set_duty(BUZZER_MODE, BUZZER_CHANNEL, 0));
    ESP_ERROR_CHECK(ledc_update_duty(BUZZER_MODE, BUZZER_CHANNEL));
}

int get_buzzer_frequency(void) {
    return global_DC; // Return the stored frequency
}

void set_buzzer_frequency(int frequency) {
    global_DC = frequency; // Update the stored frequency
    ESP_ERROR_CHECK(ledc_set_freq(BUZZER_MODE, BUZZER_TIMER, frequency));
    ESP_ERROR_CHECK(ledc_update_duty(BUZZER_MODE, BUZZER_CHANNEL)); // Update duty to apply the new frequency
}