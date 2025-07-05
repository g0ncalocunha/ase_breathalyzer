/* SD card and FAT filesystem example.
   This example uses SPI peripheral to communicate with SD card.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "sd_test_io.h"


#define SCORES_FILE "/sdcard/highscores.txt"
#define MAX_CHAR_SIZE    64
#define MAX_NAME_LENGTH 3
#define MAX_HIGHSCORES 10

static const char *TAG = "example";

typedef struct {
    char name[MAX_NAME_LENGTH];
    int score;
} highscore;

static highscore highscores[MAX_HIGHSCORES];

#define MOUNT_POINT "/sdcard"
#define PIN_NUM_MISO  4
#define PIN_NUM_MOSI  6
#define PIN_NUM_CLK   5
#define PIN_NUM_CS    1

static esp_err_t s_example_write_file(const char *path, char *data)
{
    ESP_LOGI(TAG, "Opening file %s", path);
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return ESP_FAIL;
    }
    fprintf(f, data);
    fclose(f);
    ESP_LOGI(TAG, "File written");

    return ESP_OK;
}

static esp_err_t s_example_read_file(const char *path)
{
    ESP_LOGI(TAG, "Reading file %s", path);
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return ESP_FAIL;
    }
    char line[MAX_CHAR_SIZE];
    fgets(line, sizeof(line), f);
    fclose(f);

    // strip newline
    char *pos = strchr(line, '\n');
    if (pos) {
        *pos = '\0';
    }
    ESP_LOGI(TAG, "Read from file: '%s'", line);

    return ESP_OK;
}

// Function to load highscores from the file
static esp_err_t load_highscores(const char *file) {
    FILE *f = fopen(file, "r");
    if (f == NULL) {
        ESP_LOGW(TAG, "Highscore file not found, initializing empty table.");
        memset(highscores, 0, sizeof(highscores));
        return ESP_OK;
    }

    for (int i = 0; i < MAX_HIGHSCORES; i++) {
        if (fscanf(f, "%3s 0.%d", highscores[i].name, &highscores[i].score) != 2) {
            break;
        }
    }

    fclose(f);
    ESP_LOGI(TAG, "Highscores loaded successfully.");
    return ESP_OK;
}

// Function to save highscores to the file
static esp_err_t save_highscores(const char *file) {
    FILE *f = fopen(file, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open highscore file for adding.");
        return ESP_FAIL;
    }

    for (int i = 0; i < MAX_HIGHSCORES; i++) {
        if (highscores[i].score > 0) {
            fprintf(f, "%3s 0.%d\n", highscores[i].name, highscores[i].score);
        }
    }

    fclose(f);
    ESP_LOGI(TAG, "Highscores saved successfully.");
    return ESP_OK;
}

// Function to add a new highscore
static void add_highscore(const char *file, const char *name, int score) {
    for (int i = 0; i < MAX_HIGHSCORES; i++) {
        if (score > highscores[i].score) {
            for (int j = MAX_HIGHSCORES - 1; j > i; j--) {
                highscores[j] = highscores[j - 1];
            }
            strncpy(highscores[i].name, name, MAX_NAME_LENGTH - 1);
            highscores[i].name[MAX_NAME_LENGTH - 1] = '\0';
            highscores[i].score = score;
            ESP_LOGI(TAG, "New highscore added: %3s 0.%d", name, score);
            save_highscores(file);
            return;
        }
    }
}

// Function to display the highscore table
static void display_highscores(void) {
    ESP_LOGI(TAG, "Highscore Table:");
    for (int i = 0; i < MAX_HIGHSCORES; i++) {
        if (highscores[i].score > 0) {
            ESP_LOGI(TAG, "%d. %s - %d", i + 1, highscores[i].name, highscores[i].score);
        }
    }
}

void app_main(void)
{
    esp_err_t ret;

    // Options for mounting the filesystem.
    // If format_if_mount_failed is set to true, SD card will be partitioned and
    // formatted in case when mounting fails.
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
#ifdef CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif // EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    sdmmc_card_t *card;
    const char mount_point[] = MOUNT_POINT;
    ESP_LOGI(TAG, "Initializing SD card");

    // Use settings defined above to initialize SD card and mount FAT filesystem.
    // Note: esp_vfs_fat_sdmmc/sdspi_mount is all-in-one convenience functions.
    // Please check its source code and implement error recovery when developing
    // production applications.
    ESP_LOGI(TAG, "Using SPI peripheral");

    // By default, SD card frequency is initialized to SDMMC_FREQ_DEFAULT (20MHz)
    // For setting a specific frequency, use host.max_freq_khz (range 400kHz - 20MHz for SDSPI)
    // Example: for fixed frequency of 10MHz, use host.max_freq_khz = 10000;
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
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                     "If you want the card to be formatted, set the CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return;
    }
    ESP_LOGI(TAG, "Filesystem mounted");

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);
    const char *file_scores = MOUNT_POINT"/scores.txt";

    // Check if destination file exists before renaming
    struct stat st;
    if (stat(file_scores, &st) != 0) {
        char data[MAX_CHAR_SIZE];
        ESP_LOGI(TAG, "Scores file does not exist, creating new file");
        snprintf(data, MAX_CHAR_SIZE, "%3s 0.%d\n", "GC", 12);
        ret = s_example_write_file(file_scores, data);
        if (ret != ESP_OK) {
            return;
        }
    }
    add_highscore(file_scores, "GC", 12);
    add_highscore(file_scores, "AB", 15);
    add_highscore(file_scores, "CD", 10);


    ret = s_example_read_file(file_scores);
    if (ret != ESP_OK) {
        return;
    }

    // All done, unmount partition and disable SPI peripheral
    esp_vfs_fat_sdcard_unmount(mount_point, card);
    ESP_LOGI(TAG, "Card unmounted");

    //deinitialize the bus after all devices are removed
    spi_bus_free(host.slot);
}
