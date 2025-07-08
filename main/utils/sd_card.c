
#include "../includes/sd_card.h"

// Global variable definitions
char logs[MAX_LOG_SIZE][MAX_CHAR_SIZE]; // Buffer for storing logs
int log_size = 0;                       // Current size of the log buffer
const char *TAGSD = "sd_card";
highscore_t highscores[MAX_HIGHSCORES];

esp_err_t s_write_file(const char *path, char *data)
{
    ESP_LOGI(TAGSD, "Opening file %s", path);
    FILE *f = fopen(path, "w");
    if (f == NULL)
    {
        ESP_LOGE(TAGSD, "Failed to open file for writing");
        return ESP_FAIL;
    }
    fprintf(f, data);
    fclose(f);
    ESP_LOGI(TAGSD, "File written");

    return ESP_OK;
}

esp_err_t s_read_file(const char *path)
{
    ESP_LOGI(TAGSD, "Reading file %s", path);
    FILE *f = fopen(path, "r");
    if (f == NULL)
    {
        ESP_LOGE(TAGSD, "Failed to open file for reading");
        return ESP_FAIL;
    }
    char line[MAX_CHAR_SIZE];
    fgets(line, sizeof(line), f);
    fclose(f);

    // strip newline
    char *pos = strchr(line, '\n');
    if (pos)
    {
        *pos = '\0';
    }
    ESP_LOGI(TAGSD, "Read from file: '%s'", line);

    return ESP_OK;
}

// Function to load highscores from the file
esp_err_t load_highscores(const char *file)
{
    for (int i = 0; i < MAX_HIGHSCORES; i++)
    {
        highscores[i].score = -1.0f;
        memset(&highscores[i].date, 0, sizeof(struct tm));
    }

    FILE *f = fopen(file, "r");
    if (f == NULL)
    {
        ESP_LOGW(TAGSD, "Highscore file not found, initializing empty table.");
        memset(highscores, 0, sizeof(highscores));
        return ESP_OK;
    }

    for (int i = 0; i < MAX_HIGHSCORES; i++)
    {
        int day, month, year;
        if (fscanf(f, "%d/%d/%d %f", &day, &month, &year, &highscores[i].score) != 4)
        {
            break;
        }
        // Set the date fields
        highscores[i].date.tm_mday = day;
        highscores[i].date.tm_mon = month - 1;    // tm_mon is 0-based
        highscores[i].date.tm_year = year - 1900; // tm_year is years since 1900
        highscores[i].date.tm_hour = 0;
        highscores[i].date.tm_min = 0;
        highscores[i].date.tm_sec = 0;
    }

    fclose(f);
    ESP_LOGI(TAGSD, "Highscores loaded successfully.");
    return ESP_OK;
}

// Function to save highscores to the file
esp_err_t save_highscores(const char *file)
{
    FILE *f = fopen(file, "w");
    if (f == NULL)
    {
        ESP_LOGE(TAGSD, "Failed to open highscore file for adding.");
        return ESP_FAIL;
    }

    for (int i = 0; i < MAX_HIGHSCORES; i++)
    {
        if (highscores[i].score > 0)
        {
            fprintf(f, "%02d/%02d/%04d %.2f\n",
                    highscores[i].date.tm_mday,
                    highscores[i].date.tm_mon + 1,
                    highscores[i].date.tm_year + 1900,
                    highscores[i].score);
        }
    }

    fclose(f);
    ESP_LOGI(TAGSD, "Highscores saved successfully.");
    return ESP_OK;
}

// Function to add a new highscore
void add_highscore(const char *file, struct tm date, float score)
{
    for (int i = 0; i < MAX_HIGHSCORES; i++)
    {
        if (score > highscores[i].score)
        {
            for (int j = MAX_HIGHSCORES - 1; j > i; j--)
            {
                highscores[j] = highscores[j - 1];
            }
            highscores[i].date = date;
            highscores[i].score = score;
            ESP_LOGI(TAGSD, "New highscore added: %02d/%02d/%04d - %.2f",
                     date.tm_mday, date.tm_mon + 1, date.tm_year + 1900, score);
            return;
        }
    }
}

// Function to display the highscore table
void display_highscores(void)
{
    ESP_LOGI(TAGSD, "Highscore Table:");
    for (int i = 0; i < MAX_HIGHSCORES; i++)
    {
        if (highscores[i].score > 0)
        {
            ESP_LOGI(TAGSD, "%d. %02d/%02d/%04d - %.2f", i + 1,
                     highscores[i].date.tm_mday,
                     highscores[i].date.tm_mon + 1,
                     highscores[i].date.tm_year + 1900,
                     highscores[i].score);
        }
    }
}

void add_log(float ppm, float bac)
{
    struct tm tm_info = get_date();
    if (log_size >= MAX_LOG_SIZE)
    {
        ESP_LOGW(TAGSD, "Log buffer is full, saving log.");
        // TODO: Implement saving all logs to file
    }
    sprintf(logs[log_size], "%02d/%02d/%04d %02d:%02d:%02d - PPM: %.2f, BAC: %.3f\n",
            tm_info.tm_mday, tm_info.tm_mon + 1, tm_info.tm_year + 1900,
            tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec, ppm, bac);
    log_size++;
}

esp_err_t save_log(const char *file, char *msg)
{
    FILE *f = fopen(file, "a");
    if (f == NULL)
    {
        ESP_LOGE(TAGSD, "Failed to open log file for adding.");
        return ESP_FAIL;
    }

    fprintf(f, "%s", msg);

    fclose(f);
    ESP_LOGI(TAGSD, "Log saved successfully.");

    log_size = 0; // Reset log size after saving
    msg= '\0'; // Clear the log buffer

    return ESP_OK;
}

void sync_clock()
{
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_netif_sntp_init(&config);
    if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10000)) != ESP_OK) {
        ESP_LOGW(TAGSD, "Failed to update system time within 10s, timestamps will be wrong!");
    }
    ESP_LOGI(TAGSD, "Current timestamp: %lld", time(NULL));
}

struct tm get_date(void)
{
    time_t now = time(NULL);
    struct tm tm_info;
    if (now == (time_t)-1 || localtime_r(&now, &tm_info) == NULL)
    {
        // handle error, set to a default date
        memset(&tm_info, 0, sizeof(tm_info));
        tm_info.tm_year = 70;  // 1970
        tm_info.tm_mon = 0;    // January
        tm_info.tm_mday = 1;   // 1st day
    }
    return tm_info;
}