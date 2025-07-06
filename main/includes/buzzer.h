#ifndef __BUZZER_H__INCLUDED__
#define __BUZZER_H__INCLUDED__

#include "driver/gpio.h"
#include "driver/ledc.h"

#define BUZZER_TIMER LEDC_TIMER_0
#define BUZZER_MODE LEDC_LOW_SPEED_MODE
#define BUZZER_CHANNEL LEDC_CHANNEL_0
#define BUZZER_DUTY_RES LEDC_TIMER_13_BIT // Set duty resolution to 13 bits

void buzzer_init(int gpio_num, int frequency);
void buzzer_on(void);
void buzzer_off(void);
int get_buzzer_frequency(void);
void set_buzzer_frequency(int frequency);

#endif