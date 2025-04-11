#include "web_server.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "esp_http_server.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"


static const char *TAG = "web-server";

/* Define file server root directory */
#define FILE_SYSTEM_BASE_PATH "/storage"
#define FILE_SERVER_BASE_PATH "/"

/* Max size of a file path */
#define FILE_PATH_MAX 512

/* Scratch buffer size */
#define SCRATCH_BUFSIZE 8192

/* Declare the Queue */
static QueueHandle_t q;  // Declare q globally for use in multiple functions
typedef enum {
    CMD_START_SENSOR,
    CMD_STOP_SENSOR
} CommandType;


struct file_server_data {
    char base_path[ESP_VFS_PATH_MAX + 1];
    char scratch[SCRATCH_BUFSIZE];
};

/* Handler to redirect root requests to index.html */
static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "307 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", "/index.html");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

/* Function to get the content type based on file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filepath)
{
    const char *type = "text/plain";
    
    if (strstr(filepath, ".html")) {
        type = "text/html";
    } else if (strstr(filepath, ".js")) {
        type = "application/javascript";
    } else if (strstr(filepath, ".css")) {
        type = "text/css";
    } else if (strstr(filepath, ".png")) {
        type = "image/png";
    } else if (strstr(filepath, ".jpg") || strstr(filepath, ".jpeg")) {
        type = "image/jpeg";
    } else if (strstr(filepath, ".ico")) {
        type = "image/x-icon";
    } else if (strstr(filepath, ".svg")) {
        type = "image/svg+xml";
    } else if (strstr(filepath, ".json")) {
        type = "application/json";
    }
    
    return httpd_resp_set_type(req, type);
}

/* Handler to serve static files from SPIFFS */
static esp_err_t file_get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    FILE *fd = NULL;
    struct stat file_stat;
    struct file_server_data *server_data = req->user_ctx;

    const char *filename = req->uri;
    if (strcmp(filename, "/") == 0) {
        filename = "/index.html";
    }

    // Base lenght of the path
    size_t base_len = strlen(server_data->base_path);
    // If the base lenght of the path plus the filename is biggeer than the file path return an error 
    if (base_len + strlen(filename) + 1 > FILE_PATH_MAX) {
        ESP_LOGE(TAG, "Path too long");
        return ESP_FAIL;
    }
    // Concatenate the base path with the filename
    strcpy(filepath, server_data->base_path);
    strcat(filepath, filename);


    if (stat(filepath, &file_stat) == -1) {
        ESP_LOGE(TAG, "Failed to stat file : %s", filepath);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
        return ESP_FAIL;
    }

    fd = fopen(filepath, "r");
    if (!fd) {
        ESP_LOGE(TAG, "Failed to read file : %s", filepath);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read file");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Sending file : %s (%ld bytes)", filepath, file_stat.st_size);
    set_content_type_from_file(req, filepath);

    /* Read file and send in chunks */
    size_t chunksize;
    do {
        chunksize = fread(server_data->scratch, 1, SCRATCH_BUFSIZE, fd);
        if (chunksize > 0) {
            if (httpd_resp_send_chunk(req, server_data->scratch, chunksize) != ESP_OK) {
                fclose(fd);
                ESP_LOGE(TAG, "File sending failed!");
                httpd_resp_sendstr_chunk(req, NULL);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                return ESP_FAIL;
            }
        }
    } while (chunksize != 0);

    /* Close file after sending complete */
    fclose(fd);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/* Initialize SPIFFS */
static esp_err_t init_spiffs(void)
{
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = FILE_SYSTEM_BASE_PATH,
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = false
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info("storage", &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
        return ESP_FAIL;
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
    
    return ESP_OK;
}

static esp_err_t start_sensor_handler(httpd_req_t *req) {
    QueueHandle_t q = (QueueHandle_t) req->user_ctx;

    CommandType cmd = CMD_START_SENSOR;
    xQueueSend(q, &cmd, portMAX_DELAY);
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    ESP_LOGI(TAG, "Running");
    return ESP_OK;
}

static esp_err_t stop_sensor_handler(httpd_req_t *req) {
    QueueHandle_t q = (QueueHandle_t) req->user_ctx;

    CommandType cmd = CMD_STOP_SENSOR;
    xQueueSend(q, &cmd, portMAX_DELAY);
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t start_web_server(QueueHandle_t cmd_queue)
{
    // Initialize queue
    q = cmd_queue;

    /* Initialize file storage */
    ESP_LOGI(TAG, "Starting web server");
    
    if (init_spiffs() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPIFFS");
        return ESP_FAIL;
    }

    static struct file_server_data *server_data = NULL;
    
    /* Allocate memory for server data */
    server_data = calloc(1, sizeof(struct file_server_data));
    if (!server_data) {
        ESP_LOGE(TAG, "Failed to allocate memory for server data");
        return ESP_ERR_NO_MEM;
    }
    strlcpy(server_data->base_path, FILE_SYSTEM_BASE_PATH, sizeof(server_data->base_path));

    /* Start the httpd server */
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG(); // Port and other configuration can be modified here
    config.max_uri_handlers = 8;
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(TAG, "Starting HTTP server on port: %d", config.server_port);
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        free(server_data);
        return ESP_FAIL;
    }

    /* URI handlers */
    httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_get_handler,
        .user_ctx = server_data
    };
    httpd_register_uri_handler(server, &root);

    /*URI Handler for Toggle Buttons*/
    httpd_uri_t start_uri = {
        .uri       = "/start_sensor",
        .method    = HTTP_GET,
        .handler   = start_sensor_handler,
        .user_ctx  = (void*) q
};
httpd_register_uri_handler(server, &start_uri);

    httpd_uri_t end_uri = {
        .uri       = "/end_sensor",
        .method    = HTTP_GET,
        .handler   = stop_sensor_handler,
        .user_ctx  = (void*) q
};
httpd_register_uri_handler(server, &end_uri);


    /* URI handler for static files */
    httpd_uri_t file_download = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = file_get_handler,
        .user_ctx = server_data
    };
    httpd_register_uri_handler(server, &file_download);

    return ESP_OK;


}   

