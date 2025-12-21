/* HTTP Restful API Server

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <fcntl.h>
#include "esp_http_server.h"
#include "esp_chip_info.h"
#include "esp_random.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "cJSON.h"
#include <nvs.h>
#include "TicTac.h"

static const char* JSON_HOURS_TAG = "HOURS";
static const char* JSON_MINUTES_TAG = "MINUTES";

static const char *REST_TAG = "esp-rest";

#define REST_CHECK(a, str, goto_tag, ...)                                              \
    do                                                                                 \
    {                                                                                  \
        if (!(a))                                                                      \
        {                                                                              \
            ESP_LOGE(REST_TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
            goto goto_tag;                                                             \
        }                                                                              \
    } while (0)

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 128)
#define SCRATCH_BUFSIZE (10240)

// Cette structure qui vient de l'example RESTfull API ne sert plus maintenant
typedef struct rest_server_context {
    char base_path[ESP_VFS_PATH_MAX + 1];
    char scratch[SCRATCH_BUFSIZE];
} rest_server_context_t;

#define CHECK_FILE_EXTENSION(filename, ext) (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)

// NVS Aspects : Non Volatile Storage
void saveTime(int hours, int minutes) {
    clockTimeReference.hours = hours;
    clockTimeReference.minutes = minutes;
    nvs_handle_t my_handle;
    nvs_open("storage", NVS_READWRITE, &my_handle);
    nvs_set_i32(my_handle, JSON_HOURS_TAG, hours);
    nvs_set_i32(my_handle, JSON_MINUTES_TAG, minutes);
    nvs_commit(my_handle);
    nvs_close(my_handle);
}

ClockTime_t readTimeFromNvs() {
    nvs_handle_t my_handle;
    int32_t hours, minutes;
    nvs_open("storage", NVS_READONLY, &my_handle);
    nvs_get_i32(my_handle, JSON_HOURS_TAG, &hours);
    nvs_get_i32(my_handle, JSON_MINUTES_TAG, &minutes);
    nvs_close(my_handle);
    clockTimeReference.hours = (int)hours;
    clockTimeReference.minutes = (int)minutes;
    ESP_LOGI(REST_TAG, "Read Time from NVS : %d:%d ", clockTimeReference.hours, clockTimeReference.minutes); 
    return clockTimeReference;
}

/* Simple handler for light brightness control */
static esp_err_t time_post_handler(httpd_req_t *req)
{
    int total_len = req->content_len;
    int cur_len = 0;
    char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
    int received = 0;
    if (total_len >= SCRATCH_BUFSIZE) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0) {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    saveTime(cJSON_GetObjectItem(root, JSON_HOURS_TAG)->valueint,    
             cJSON_GetObjectItem(root, JSON_MINUTES_TAG)->valueint);

    // Once time has be set, the number of tic must be reset to zero
    ticTacNumbers = 0;
    ESP_LOGI(REST_TAG, "Time: hours = %d, minutes = %d", clockTimeReference.hours, clockTimeReference.minutes);
    cJSON_Delete(root);
    httpd_resp_sendstr(req, "Post control value successfully");
    return ESP_OK;
}

/* Simple handler for getting system handler */
static esp_err_t time_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    cJSON_AddNumberToObject(root, "hours", clockTimeReference.hours);
    cJSON_AddNumberToObject(root, "minutes", clockTimeReference.minutes);
    const char *timeInfo = cJSON_Print(root);
    httpd_resp_sendstr(req, timeInfo);
    free((void *)timeInfo);
    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t start_rest_server(const char *base_path)
{
    // Initialize internal clockTimeReference from storage
    readTimeFromNvs();
    REST_CHECK(base_path, "wrong base path", err);
    rest_server_context_t *rest_context = calloc(1, sizeof(rest_server_context_t));
    REST_CHECK(rest_context, "No memory for rest context", err);
    strlcpy(rest_context->base_path, base_path, sizeof(rest_context->base_path));

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(REST_TAG, "Starting HTTP Server");
    REST_CHECK(httpd_start(&server, &config) == ESP_OK, "Start server failed", err_start);

    /* URI handler for fetching system info */
    httpd_uri_t time_get_uri = {
        .uri = "/api/v1/time",
        .method = HTTP_GET,
        .handler = time_get_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &time_get_uri);

    /* URI handler for light brightness control */
    httpd_uri_t time_post_uri = {
        .uri = "/api/v1/time",
        .method = HTTP_POST,
        .handler = time_post_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &time_post_uri);


    return ESP_OK;
err_start:
    free(rest_context);
err:
    return ESP_FAIL;
}
