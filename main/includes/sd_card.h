#ifndef __SDCARD_H__INCLUDED__
#define __SDCARD_H__INCLUDED__

#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "sd_test_io.h"
#include <time.h>
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"


#define SCORES_FILE "/sdcard/scores.txt"
#define LOG_FILE "/sdcard/log.txt"
#define MAX_CHAR_SIZE    64
#define MAX_HIGHSCORES 10
#define MAX_LOG_SIZE    100

extern char logs[MAX_LOG_SIZE][MAX_CHAR_SIZE]; // Buffer for storing logs
extern int log_size; // Current size of the log buffer

extern const char *TAGSD;

typedef struct {
    struct tm date; // Date structure for storing date/time
    float score;
} highscore_t;

extern highscore_t highscores[MAX_HIGHSCORES];

#define MOUNT_POINT "/sdcard"
#define PIN_NUM_MISO  4
#define PIN_NUM_MOSI  6
#define PIN_NUM_CLK   5
#define PIN_NUM_CS    1


esp_err_t s_write_file(const char *path, char *data);
esp_err_t s_read_file(const char *path);
// Function to load highscores from the file
esp_err_t load_highscores(const char *file);
// Function to save highscores to the file
esp_err_t save_highscores(const char *file);
// Function to add a new highscore
void add_highscore(const char *file, struct tm date, float score);
// Function to display the highscore table
void display_highscores(void);

void add_log(float ppm, float bac);
esp_err_t save_log(const char *file, char *msg);


void sync_clock();

struct tm get_date(void);

#endif