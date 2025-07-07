#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "includes/MQ303A.h"
#include "includes/buzzer.h"
#include "includes/sd_card.h"

#define HEATER_SEL_PIN 3

#define SAMPLE_COUNT 100
#define VREF_DEFAULT 2500 // Default reference voltage in mV

#define BUZZER_GPIO 0 // Define the output GPIO
#define BUZZER_FREQ 4096

#define GPIO_LED 7
#define GPIO_BUTTON 10

#define ADC_CHANNEL ADC_CHANNEL_2 // Define the ADC channel to use

static void timer_callback(void *arg);

static const char *TAG = "BREATHALYZER";
static bool heater_finished = false;
static bool stop_counting = false;

int melody[] = {
  NOTE_E5, NOTE_D5, NOTE_FS4, NOTE_GS4, 
  NOTE_CS5, NOTE_B4, NOTE_D4, NOTE_E4, 
  NOTE_B4, NOTE_A4, NOTE_CS4, NOTE_E4,
  NOTE_A4
};

int durations[] = {
  8, 8, 4, 4,
  8, 8, 4, 4,
  8, 8, 4, 4,
  2
};

// LED Configuration
static void configure_led(void)
{
    ESP_LOGI("LED", "Configured GPIO LED!");
    gpio_reset_pin(GPIO_LED);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(GPIO_LED, GPIO_MODE_OUTPUT);
}

// Button Configuration

static void configure_button(void)
{
    ESP_LOGI("LED", "Configured GPIO LED!");
    gpio_reset_pin(GPIO_BUTTON);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(GPIO_BUTTON, GPIO_MODE_INPUT);
}

static void timer_init(const char *timer_name, esp_timer_handle_t *timer_handle, esp_timer_cb_t callback)
{
    const esp_timer_create_args_t timer_arg = {
        .callback = callback,
        .arg = NULL,
        .name = timer_name,
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_arg, timer_handle));
}

static void timer_deinit(esp_timer_handle_t timer_handle)
{
    if (timer_handle != NULL)
    {
        ESP_ERROR_CHECK(esp_timer_delete(timer_handle));
        timer_handle = NULL;
    }
}

static void heatup_timer_callback(void *arg)
{
    // This function will be called when the timer expires
    mq303a_stop_heatup(HEATER_SEL_PIN); // Stop the heater
    heater_finished = true;             // Set the ready flag to true
}

static void timer_callback(void *arg)
{
    buzzer_off();                // Turn off the buzzer
    gpio_set_level(GPIO_LED, 0); // Turn off the LED
    stop_counting = true;        // Stop counting when the timer expires
}

// static void component_init(adc_oneshot_unit_handle_t adc_handle, adc_cali_handle_t adc_cali_handle,
//                           esp_timer_handle_t counting_timer, esp_timer_handle_t heatup_timer)
// {

// }

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Breathalyzer Application");
    
    // Create breathalyzer task
    adc_oneshot_unit_handle_t adc_handle = NULL;
    adc_cali_handle_t adc_cali_handle = NULL;
    esp_timer_handle_t counting_timer = NULL;
    esp_timer_handle_t heatup_timer = NULL;

    buzzer_init(BUZZER_GPIO, BUZZER_FREQ);                            // Initialize the buzzer
    configure_button();                                               // Configure the button
    configure_led();                                                  // Configure the LED
    mq303a_init(ADC_CHANNEL, &adc_handle, &adc_cali_handle);          // Initialize the MQ303A sensor
    // component_init(adc_handle, adc_cali_handle, counting_timer, heatup_timer); // Initialize components
    
    // Initialize the SD card

    esp_err_t ret;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    sdmmc_card_t *card;
    const char mount_point[] = MOUNT_POINT;
    ESP_LOGI(TAG, "Initializing SD card");

    ESP_LOGI(TAG, "Using SPI peripheral");

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        return;
    }

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    ESP_LOGI(TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. ");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return;
    }
    ESP_LOGI(TAG, "Filesystem mounted");
    sync_clock(); // Synchronize the system clock with NTP server

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);
    const char *file_scores = MOUNT_POINT"/scores.txt";
    load_highscores(file_scores); // Load highscores from the file

    // End of SD card initialization
    while (1)
    {
        timer_init("Heatup Timer", &heatup_timer, heatup_timer_callback); // Initialize the heatup timer
        timer_init("Counting Timer", &counting_timer, timer_callback);    // Initialize the counting timer
        while (gpio_get_level(GPIO_BUTTON) == 0)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        ESP_ERROR_CHECK(esp_timer_start_once(heatup_timer, 10000000)); // Start timer for 10 seconds
        mq303a_start_heatup(HEATER_SEL_PIN);                           // Start the heater
        // Main loop
        ESP_LOGI(TAG, "Waiting for heater to be ready...");
        int size = sizeof(durations) / sizeof(int);

        for (int note = 0; note < size; note++)
        {
            // to calculate the note duration, take one second divided by the note type.
            // e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
            int duration = 1000 / durations[note];
            // tone(BUZZER_GPIO, melody[note]);
            buzzer_on(melody[note]); // Play the note on the buzzer
            gpio_set_level(GPIO_LED, 1); // Turn on the LED
            vTaskDelay(pdMS_TO_TICKS(duration)); // Play the note for the calculated duration
            // buzzer_off(); // Turn off the buzzer
            
            // to distinguish the notes, set a minimum time between them.
            // the note's duration + 30% seems to work well:
            int pauseBetweenNotes = duration * 1.30;
            // delay(pauseBetweenNotes);~
            vTaskDelay(pdMS_TO_TICKS(pauseBetweenNotes)); // Wait for the pause duration
            
            // stop the tone playing:
            // noTone(BUZZER_GPIO);
            buzzer_off(); // Turn off the buzzer
            gpio_set_level(GPIO_LED, 0); // Turn off the LED
        }
        // while (!heater_finished) {
        //     buzzer_on();
        //     gpio_set_level(GPIO_LED, 1); // Turn on the LED
        //     vTaskDelay(pdMS_TO_TICKS(100)); // Wait for the heater to be ready
        //     buzzer_off();
        //     gpio_set_level(GPIO_LED, 0); // Turn off the LED
        //     vTaskDelay(pdMS_TO_TICKS(100)); // Delay for 1 second
        // }

        float RS_air = mq303a_get_rs_air(ADC_CHANNEL, &adc_handle, &adc_cali_handle, SAMPLE_COUNT); // Get RS_air value
        ESP_LOGI(TAG, "RS_air: %.3f", RS_air);

        ESP_ERROR_CHECK(esp_timer_start_once(counting_timer, 5000000));

        float ppm = 0;
        float bac = 0;
        int count = 0;
        while (count < 50)
        {
            // buzzer_on(BUZZER_FREQ);
            gpio_set_level(GPIO_LED, 1);                                                  // Turn on the LED
            float RS_gas = mq303a_get_rs_gas(ADC_CHANNEL, &adc_handle, &adc_cali_handle); // Get RS_gas value

            float ratio = RS_gas / RS_air;                  // Calculate the ratio of RS values
            ESP_LOGI(TAG, "RS_gas: %.3f, Ratio: %.3f", RS_gas, ratio);
            ESP_LOGI(TAG, "PPM: %.2f", pow(10, (log10(ratio) - 0.328) / -0.55));
            ESP_LOGI(TAG, "BAC: %.2f", pow(10, (log10(ratio) - 0.328) / -0.55) / 2600);
            if (pow(10, (log10(ratio) - 0.328) / -0.55) > ppm) // Check if the current PPM is greater than the previous one
            {
                ppm = pow(10, (log10(ratio) - 0.328) / -0.55); // Update PPM value
            }
            vTaskDelay(pdMS_TO_TICKS(100)); // Delay for 0.1 second
            count++;
        }
        count = 0; // Reset the stop counting flag
        bac = ppm / 2600; // Convert PPM to BAC
        add_log(ppm, bac); // Add a log entry
        save_log(LOG_FILE, *logs); // Save the log to the file
        add_highscore(file_scores, get_date(), bac); // Add a highscore
        save_highscores(file_scores); // Save highscores to the file
        display_highscores(); // Display the highscore table
        // ésp_timer_stop(counting_timer); // Stop the counting timer
        esp_timer_delete(counting_timer); // Delete the counting timer
        // esp_timer_stop(heatup_timer); // Stop the heatup timer
        esp_timer_delete(heatup_timer); // Delete the heatup timer
    }

    ESP_LOGI(TAG, "Breathalyzer initialized successfully");
}
