#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
// #include <dirent.h>
#include <errno.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"
#include "cJSON.h"

static const char *TAG = "web_server";

// Replace with your network credentials
#define WIFI_SSID "drone"
#define WIFI_PASS "drone_peci"

#define MOUNT_POINT "/sdcard"

// Highscore related definitions
#define MAX_NAME_LENGTH 3
#define MAX_HIGHSCORES 10
#define SCORES_FILE "/sdcard/scores.txt"
#define MAX_CHAR_SIZE    64

typedef struct {
    char name[MAX_NAME_LENGTH];
    int score;
} highscore;

static highscore highscores[MAX_HIGHSCORES];

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "retry to connect to the AP");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    }
}

void init_wifi(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");
}

// void list_sd_files(void)
// {
//     ESP_LOGI(TAG, "Listing SD card contents:");
    
//     DIR *dir = opendir("/sdcard");
//     if (dir == NULL) {
//         ESP_LOGE(TAG, "Failed to open SD card directory");
//         return;
//     }

//     struct dirent *entry;
//     while ((entry = readdir(dir)) != NULL) {
//         ESP_LOGI(TAG, "Found file: %s", entry->d_name);
//     }
//     closedir(dir);
// }

void init_sd_card(void)
{
    esp_err_t ret;
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    sdmmc_card_t *card;
    const char mount_point[] = MOUNT_POINT;
    ESP_LOGI(TAG, "Initializing SD card");

    // Use SPI2 host (HSPI) for better compatibility
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST;
    
    // Configure SPI bus with proper settings for ESP32-C3
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = 6,
        .miso_io_num = 4,
        .sclk_io_num = 5,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    
    ESP_LOGI(TAG, "Initializing SPI bus with pins: MOSI=%d, MISO=%d, SCLK=%d, CS=%d", 
             bus_cfg.mosi_io_num, bus_cfg.miso_io_num, bus_cfg.sclk_io_num, 13);
    
    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = 1;
    slot_config.host_id = host.slot;

    ESP_LOGI(TAG, "Mounting SD card...");
    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. Format SD card in FAT32 and try again.");
        } else if (ret == ESP_ERR_TIMEOUT) {
            ESP_LOGE(TAG, "SD card timeout. Check wiring and ensure SD card is properly inserted.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). Check wiring and SD card.", esp_err_to_name(ret));
        }
        
        // Try to deinitialize the SPI bus if mount failed
        spi_bus_free(host.slot);
        return;
    }

    ESP_LOGI(TAG, "SD card mounted successfully");
    sdmmc_card_print_info(stdout, card);

    // list_sd_files();
}

static esp_err_t index_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Index handler called for URI: %s", req->uri);
    
    FILE *file = fopen("/sdcard/index.html", "r");
    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to open /sdcard/index.html - serving fallback content");
        ESP_LOGE(TAG, "Error opening file: %s", strerror(errno));
        
        // Serve fallback HTML content
        const char* fallback_html = 
            "<!DOCTYPE html>"
            "<html><head><title>ESP32 Breathalyzer</title></head>"
            "<body style='font-family: Arial, sans-serif; text-align: center; padding: 50px;'>"
            "<h1>ESP32 Breathalyzer Server</h1>"
            "<p>SD card file not found. Please check:</p>"
            "<ul style='text-align: left; max-width: 400px; margin: 0 auto;'>"
            "<li>SD card is properly inserted</li>"
            "<li>index.html file exists in the root directory</li>"
            "<li>SD card is formatted as FAT32</li>"
            "</ul>"
            "<p><a href='/api/status'>Check API Status</a></p>"
            "</body></html>";
        
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, fallback_html, strlen(fallback_html));
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Successfully opened index.html");
    httpd_resp_set_type(req, "text/html");

    char chunk[1024];
    size_t chunksize;
    do {
        chunksize = fread(chunk, 1, sizeof(chunk), file);
        if (chunksize > 0) {
            if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK) {
                fclose(file);
                return ESP_FAIL;
            }
        }
    } while (chunksize != 0);

    fclose(file);
    httpd_resp_send_chunk(req, NULL, 0);
    ESP_LOGI(TAG, "Index page served successfully");
    return ESP_OK;
}

static esp_err_t static_handler(httpd_req_t *req)
{
    char filepath[520];
    snprintf(filepath, sizeof(filepath), "/sdcard%s", req->uri);
    
    FILE *file = fopen(filepath, "r");
    if (file == NULL) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    // Set content type based on file extension
    const char *ext = strrchr(req->uri, '.');
    if (ext) {
        if (strcmp(ext, ".js") == 0) {
            httpd_resp_set_type(req, "application/javascript");
        } else if (strcmp(ext, ".css") == 0) {
            httpd_resp_set_type(req, "text/css");
        } else if (strcmp(ext, ".html") == 0) {
            httpd_resp_set_type(req, "text/html");
        }
    }

    char chunk[1024];
    size_t chunksize;
    do {
        chunksize = fread(chunk, 1, sizeof(chunk), file);
        if (chunksize > 0) {
            if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK) {
                fclose(file);
                return ESP_FAIL;
            }
        }
    } while (chunksize != 0);

    fclose(file);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

// Function to save highscores to the file
static esp_err_t save_highscores(const char *file) {
    FILE *f = fopen(file, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open highscore file for writing.");
        return ESP_FAIL;
    }

    for (int i = 0; i < MAX_HIGHSCORES; i++) {
        if (highscores[i].score > 0) {
            fprintf(f, "%3s 0.%d\n", highscores[i].date, highscores[i].score);
        }
    }

    fclose(f);
    ESP_LOGI(TAG, "Highscores saved successfully.");
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

static esp_err_t status_handler(httpd_req_t *req)
{
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(netif, &ip_info);
    
    char response[256];
    snprintf(response, sizeof(response), 
        "{"
        "\"ip\":\"%d.%d.%d.%d\","
        "\"status\":\"connected\","
        "\"ssid\":\"%s\""
        "}", 
        IP2STR(&ip_info.ip), WIFI_SSID);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}

/* Handler for getting alcohol highscores */
static esp_err_t highscores_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    
    // Load highscores from the file
    load_highscores(SCORES_FILE);
    cJSON *highscores_array = cJSON_CreateArray();
    for (int i = 0; i < MAX_HIGHSCORES; i++) {
        if (highscores[i].score > 0) {
            cJSON *highscore_item = cJSON_CreateObject();
            cJSON_AddStringToObject(highscore_item, "date", highscores[i].date);
            cJSON_AddNumberToObject(highscore_item, "score", highscores[i].score);
            cJSON_AddItemToArray(highscores_array, highscore_item);
        }
    }

    const char *json_response = cJSON_Print(highscores_array);
    httpd_resp_send(req, json_response, strlen(json_response));
    free((void *)json_response);
    cJSON_Delete(highscores_array);
    return ESP_OK;
}

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    if (httpd_start(&server, &config) == ESP_OK) {
        // Register specific handlers first
        httpd_uri_t status_uri = {
            .uri       = "/api/status",
            .method    = HTTP_GET,
            .handler   = status_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &status_uri);

        httpd_uri_t highscores_uri = {
            .uri       = "/api/v1/highscores",
            .method    = HTTP_GET,
            .handler   = highscores_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &highscores_uri);

        httpd_uri_t index_uri = {
            .uri       = "/",
            .method    = HTTP_GET,
            .handler   = index_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &index_uri);

        // Register wildcard handler last
        httpd_uri_t static_uri = {
            .uri       = "/*",
            .method    = HTTP_GET,
            .handler   = static_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &static_uri);

        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

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

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_LOGI(TAG, "NVS Flash initialized");

    init_wifi();
    ESP_LOGI(TAG, "WiFi initialization complete");
    
    init_sd_card();
    ESP_LOGI(TAG, "SD card initialization complete");
    
    esp_err_t ret;

    // Initialize highscores data
    struct stat st;
    if (stat(SCORES_FILE, &st) != 0) {
        char data[MAX_CHAR_SIZE];
        ESP_LOGI(TAG, "Scores file does not exist, creating new file");
        snprintf(data, MAX_CHAR_SIZE, "%3s 0.%d\n", "GC", 12);
        ret = s_example_write_file(SCORES_FILE, data);
        if (ret != ESP_OK) {
            return;
        }
    }
    add_highscore(SCORES_FILE, "GC", 12);
    add_highscore(SCORES_FILE, "AB", 15);
    add_highscore(SCORES_FILE, "CD", 10);
    
    httpd_handle_t server = start_webserver();
    if (server) {
        ESP_LOGI(TAG, "Web server started successfully - ready to serve requests");
    } else {
        ESP_LOGE(TAG, "Failed to start web server");
    }
}