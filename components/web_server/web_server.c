#include "web_server.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "esp_http_server.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include <sys/time.h>
#include <time.h>
#include <dirent.h>

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

static const char *TAG = "web-server";

/* Define file server root directory */
#define FILE_SYSTEM_BASE_PATH "/csv_logs"
#define FILE_SERVER_BASE_PATH "/"

/* Max size of a file path */
#define FILE_PATH_MAX 512

/* Scratch buffer size */
#define SCRATCH_BUFSIZE 8192

/* Declare the Queue */
static QueueHandle_t q;  // Declare q globally for use in multiple functions
typedef enum {
    CMD_START_SENSOR,
    CMD_STOP_SENSOR,
    CMD_SET_NAME,
    CMD_TOGGLE_SENSOR

} CommandType;


typedef struct {
    CommandType type;
    char payload[64];  
} CommandMessage;



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
    } else if (strstr(filepath, ".csv")) {
        type = "text/csv";
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

    // block to strip query strings
    char clean_uri[128];
    char *query = strchr(filename, '?');
    if (query) {
        size_t len = query - filename;
        if (len >= sizeof(clean_uri)) len = sizeof(clean_uri) - 1;
        strncpy(clean_uri, filename, len);
        clean_uri[len] = '\0';
        filename = clean_uri;
    }

    if (strcmp(filename, "/") == 0) {
        filename = "/index.html";
    }

    size_t base_len = strlen(server_data->base_path);
    if (base_len + strlen(filename) + 1 > FILE_PATH_MAX) {
        ESP_LOGE(TAG, "Path too long");
        return ESP_FAIL;
    }

    // Avoid double base path (e.g., /csv_logs/csv_logs)
    const char *relative_path = filename;
    if (strncmp(filename, server_data->base_path, strlen(server_data->base_path)) == 0) {
        relative_path = filename + strlen(server_data->base_path);
    }

    strcpy(filepath, server_data->base_path);
    strcat(filepath, relative_path);


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

    fclose(fd);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}



/* Initialize SPIFFS */
static esp_err_t init_spiffs(void)
{
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/csv_logs",
        .partition_label = "csv_logs",
        .max_files = 5,
        .format_if_mount_failed = true
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
    ret = esp_spiffs_info("csv_logs", &total, &used);
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
    ESP_LOGI(TAG, "Start sensor command sent.");
    return ESP_OK;
}

// static esp_err_t resume_sensor_handler(httpd_req_t *req) {
//     QueueHandle_t q = (QueueHandle_t) req->user_ctx;

//     CommandType cmd = CMD_RESUME_SENSOR;
//     xQueueSend(q, &cmd, portMAX_DELAY);
//     httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
//     ESP_LOGI(TAG, "Resume sensor command sent.");
//     return ESP_OK;
// }

// static esp_err_t toggle_sensor_handler(httpd_req_t *req) {
//     QueueHandle_t q = (QueueHandle_t) req->user_ctx;

//     CommandType cmd = CMD_TOGGLE_SENSOR;
//     if (xQueueSend(q, &cmd, portMAX_DELAY) != pdPASS) {
//         httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send toggle command");
//         return ESP_FAIL;
//     }

//     httpd_resp_send(req, "Toggle sensor command sent", HTTPD_RESP_USE_STRLEN);
//     ESP_LOGI(TAG, "Toggle sensor command sent.");
//     return ESP_OK;
// }

static esp_err_t toggle_sensor_handler(httpd_req_t *req) {
    QueueHandle_t q = (QueueHandle_t) req->user_ctx;

    CommandType cmd = CMD_TOGGLE_SENSOR;
    if (xQueueSend(q, &cmd, portMAX_DELAY) != pdPASS) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send toggle command");
        return ESP_FAIL;
    }

    httpd_resp_send(req, "Toggle sensor command sent", HTTPD_RESP_USE_STRLEN);
    ESP_LOGI(TAG, "Toggle sensor command sent.");
    return ESP_OK;
}



static esp_err_t stop_sensor_handler(httpd_req_t *req) {
    QueueHandle_t q = (QueueHandle_t) req->user_ctx;

    CommandType cmd = CMD_STOP_SENSOR;
    xQueueSend(q, &cmd, portMAX_DELAY);
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// static esp_err_t set_name_handler(httpd_req_t *req) {
//     QueueHandle_t q = (QueueHandle_t) req->user_ctx;

//     CommandType cmd = CMD_SET_NAME;
//     xQueueSend(q, &cmd, portMAX_DELAY);
//     httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
//     return ESP_OK;
// }

static esp_err_t set_name_handler(httpd_req_t *req) {
    QueueHandle_t q = (QueueHandle_t) req->user_ctx;

    char buf[100];
    int ret = httpd_req_recv(req, buf, MIN(req->content_len, sizeof(buf) - 1));
    if (ret <= 0) {
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    ESP_LOGI("SET_NAME", "Received: %s", buf);

    char *name_start = strstr(buf, "\"name\":");
    if (name_start) {
        name_start += 7;
        while (*name_start == ' ' || *name_start == '\"' || *name_start == ':') name_start++;
        char *name_end = strchr(name_start, '\"');
        if (name_end) {
            *name_end = '\0';

            ESP_LOGI("SET_NAME", "Extracted name: %s", name_start);

            CommandMessage msg = {
                .type = CMD_SET_NAME
            };
            strncpy(msg.payload, name_start, sizeof(msg.payload) - 1);
            msg.payload[sizeof(msg.payload) - 1] = '\0';

            ESP_LOGI("SET_NAME", "Sending to queue: %s", msg.payload);
            xQueueSend(q, &msg, portMAX_DELAY);

            httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
    }

    // If we reach here, parsing failed
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON format");
    return ESP_FAIL;
}


#define MAX_NAME_LEN 128 
static esp_err_t list_logs_handler(httpd_req_t *req) {
    struct dirent *entry;
    DIR *dir = opendir("/csv_logs");

    if (!dir) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr_chunk(req, "[");
    bool first = true;

    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".csv")) {
            if (!first) {
                httpd_resp_sendstr_chunk(req, ",");
            }
            first = false;

        char file_name[MAX_NAME_LEN + 1];
        strncpy(file_name, entry->d_name, MAX_NAME_LEN);
        file_name[MAX_NAME_LEN] = '\0';  // Null-terminate

        char json_entry[512];
        snprintf(json_entry, sizeof(json_entry),
            "{\"fileName\":\"%s\",\"id\":\"%s\"}",
            file_name, file_name);
            httpd_resp_sendstr_chunk(req, json_entry);
        }
    }

    httpd_resp_sendstr_chunk(req, "]");
    httpd_resp_sendstr_chunk(req, NULL);  // End response
    closedir(dir);
    return ESP_OK;
}

// Summarize log entries into an average of all scans, except for NPK
static esp_err_t get_log_summary_handler(httpd_req_t *req)
{
    char query[100] = {0};
    char file_id[64] = "Test_s_log.csv";  // default

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        httpd_query_key_value(query, "id", file_id, sizeof(file_id));
    }

    char filepath[FILE_PATH_MAX];
    snprintf(filepath, sizeof(filepath), "/csv_logs/%s", file_id);

    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        ESP_LOGE(TAG, "Failed to open CSV file: %s", filepath);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "CSV file not found");
        return ESP_FAIL;
    }

    char *line = malloc(512);
    if (!line) {
        fclose(fp);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
        return ESP_FAIL;
    }

    const char *col_names[8] = {"Moisture", "Humidity", "SoilTemp", "AirTemp", "pH", "Nitrogen", "Phosphorus", "Potassium"};
    int col_index[8];
    float sum[8] = {0};
    int count[8] = {0};
    memset(col_index, -1, sizeof(col_index));

    bool header_parsed = false;

    while (fgets(line, 512, fp)) {
        line[strcspn(line, "\r\n")] = 0;
        if (strlen(line) < 3) continue;

        if (!header_parsed) {
            int i = 0;
            char *token = strtok(line, ",");
            while (token && i < 32) {
                for (int j = 0; j < 8; j++) {
                    if (col_names[j] && strcmp(token, col_names[j]) == 0) {
                        col_index[j] = i;
                    }
                }
                token = strtok(NULL, ",");
                i++;
            }
            header_parsed = true;
            continue;
        }

        // Tokenize data line
        char *token_array[32] = {0};
        int col = 0;
        char *token = strtok(line, ",");
        while (token && col < 32) {
            token_array[col++] = token;
            token = strtok(NULL, ",");
        }

        for (int j = 0; j < 8; j++) {
            int idx = col_index[j];
            if (idx >= 0 && idx < col && token_array[idx]) {
                char *endptr;
                float val = strtof(token_array[idx], &endptr);
                if (endptr != token_array[idx]) {
                    sum[j] += val;
                    count[j]++;
                }
            }
        }
    }
    fclose(fp);
    free(line);

    char avg[8][16];
    for (int i = 0; i < 8; i++) {
        if (count[i]) {
            snprintf(avg[i], sizeof(avg[i]), "%.2f", sum[i] / count[i]);
        } else {
            strcpy(avg[i], "#");
        }
    }

    float fert_score = 0;
    int fert_count = 0;
    for (int i = 5; i <= 7; i++) {
        if (count[i]) {
            fert_score += sum[i] / count[i];
            fert_count++;
        }
    }
    char fert_str[16];
    if (fert_count) {
        snprintf(fert_str, sizeof(fert_str), "%.2f", fert_score / fert_count);
    } else {
        strcpy(fert_str, "#");
    }

    char response[384];
    snprintf(response, sizeof(response),
        "{\"Moisture\": \"%s\", \"Humidity\": \"%s\", \"SoilTemp\": \"%s\", \"AirTemp\": \"%s\", \"pH\": \"%s\", \"Fertility\": \"%s\"}",
        avg[0], avg[1], avg[2], avg[3], avg[4], fert_str);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}




esp_err_t get_all_logs_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");

    DIR *dir = opendir("/csv_logs");
    if (!dir) {
        ESP_LOGE("web_server", "Failed to open dir");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    struct dirent *entry;
    struct stat file_stat;

    char json_buffer[1024];
    size_t offset = 0;

    offset += snprintf(json_buffer + offset, sizeof(json_buffer) - offset,
                       "{ \"logs\": [");

    bool first = true;
    while ((entry = readdir(dir)) != NULL) {
        // Only process .csv files
        const char *ext = strrchr(entry->d_name, '.');
        if (!ext || strcmp(ext, ".csv") != 0) {
            continue; // skip non-.csv files
        }

        // Build full path
        char filepath[300];
        snprintf(filepath, sizeof(filepath),
                 "/csv_logs/%s", entry->d_name);

        if (stat(filepath, &file_stat) == 0 && S_ISREG(file_stat.st_mode)) {
            if (!first) {
                offset += snprintf(json_buffer + offset,
                                   sizeof(json_buffer) - offset,
                                   ",");
            }
            first = false;

            // Format time
            struct tm tm_info;
            gmtime_r(&file_stat.st_mtime, &tm_info);
            char time_str[64];
            strftime(time_str, sizeof(time_str),
                     "%Y-%m-%d %H:%M:%S", &tm_info);

            // Add to JSON
            offset += snprintf(json_buffer + offset,
                               sizeof(json_buffer) - offset,
                "{\"id\":%ld,\"name\":\"%s\",\"date\":\"%s\"}",
                (long)file_stat.st_mtime,
                entry->d_name,
                time_str);
        }
    }

    closedir(dir);

    offset += snprintf(json_buffer + offset, sizeof(json_buffer) - offset,
                       "] }");

    httpd_resp_send(req, json_buffer, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}


esp_err_t get_log_id_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    
    // Extract the ID from the URI
    char id_str[64] = {0};
    const char *uri = req->uri;
    const char *id_start = strrchr(uri, '/');
    
    if (!id_start || strlen(id_start) <= 1) {
        ESP_LOGE("web_server", "Invalid URI format or missing ID");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing ID parameter");
        return ESP_FAIL;
    }
    
    // Skip the '/' character
    id_start++;
    
    // Copy the ID (safely)
    strncpy(id_str, id_start, sizeof(id_str) - 1);
    id_str[sizeof(id_str) - 1] = '\0';
    
    ESP_LOGI("web_server", "Requested log ID: %s", id_str);
    
    // Build the full path to the CSV file
    char filepath[300];
    snprintf(filepath, sizeof(filepath), "/csv_logs/%s.csv", id_str);
    
    // Check if file exists
    struct stat file_stat;
    if (stat(filepath, &file_stat) != 0) {
        ESP_LOGE("web_server", "File not found: %s", filepath);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Log file not found");
        return ESP_FAIL;
    }
    
    // Open the CSV file
    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        ESP_LOGE("web_server", "Failed to open CSV file: %s", filepath);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read file");
        return ESP_FAIL;
    }
    
    // Parse the CSV and convert to JSON
    char line[512];
    char *headers[32] = {0};
    int num_headers = 0;
    bool header_parsed = false;
    
    // Begin the JSON response with metadata and start the data array
    struct tm tm_info;
    gmtime_r(&file_stat.st_mtime, &tm_info);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm_info);
    
    // Start response with metadata and begin data array
    char json_start[256];
    snprintf(json_start, sizeof(json_start),
             "{\"id\":%ld,\"name\":\"%s.csv\",\"date\":\"%s\",\"data\":[",
             (long)file_stat.st_mtime,
             id_str,
             time_str);
    
    httpd_resp_send_chunk(req, json_start, strlen(json_start));
    
    bool first_row = true;
    
    // Process the CSV file line by line
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\r\n")] = 0;
        if (strlen(line) < 1) continue;
        
        if (!header_parsed) {
            // Parse header row
            char *token = strtok(line, ",");
            while (token != NULL && num_headers < 32) {
                headers[num_headers++] = strdup(token);  // duplicate header strings
                token = strtok(NULL, ",");
            }
            header_parsed = true;
            continue;
        }
        
        // Parse a data row
        char *values[32] = {0};
        int val_count = 0;
        char *token = strtok(line, ",");
        while (token != NULL && val_count < num_headers) {
            values[val_count++] = token;
            token = strtok(NULL, ",");
        }
        
        // If there's at least one value and we have headers
        if (val_count > 0 && num_headers > 0) {
            // Add comma between JSON objects
            if (!first_row) {
                httpd_resp_send_chunk(req, ",", 1);
            }
            first_row = false;
            
            // Build JSON object for this row
            httpd_resp_send_chunk(req, "{", 1);
            for (int i = 0; i < val_count; i++) {
                char field_json[128];
                snprintf(field_json, sizeof(field_json),
                         "\"%s\":\"%s\"%s", 
                         headers[i], 
                         values[i],
                         (i < val_count - 1) ? "," : "");
                httpd_resp_send_chunk(req, field_json, strlen(field_json));
            }
            httpd_resp_send_chunk(req, "}", 1);
        }
    }
    
    // Close the data array and JSON object
    httpd_resp_send_chunk(req, "]}", 2);
    httpd_resp_send_chunk(req, NULL, 0);  // End response
    
    fclose(fp);
    
    // Free allocated headers
    for (int i = 0; i < num_headers; i++) {
        free(headers[i]);
    }
    
    return ESP_OK;
}

esp_err_t set_time_handler(httpd_req_t *req) {
    char query[100];
    size_t query_len = httpd_req_get_url_query_len(req) + 1;

    if (query_len > 1 && httpd_req_get_url_query_str(req, query, query_len) == ESP_OK) {
        char ts_str[32];
        if (httpd_query_key_value(query, "ts", ts_str, sizeof(ts_str)) == ESP_OK) {
            time_t end_time = (time_t)atoll(ts_str);  // Assume UNIX timestamp in seconds

            // Step 1: Locate latest CSV in /csv_logs/
            char filepath[272];
            char latest_file[272] = {0};

            DIR *dir = opendir("/csv_logs");
            if (!dir) {
                ESP_LOGE("TIME", "Failed to open /csv_logs");
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open directory");
                return ESP_FAIL;
            }

            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_type == DT_REG && strstr(entry->d_name, ".csv")) {
                    snprintf(filepath, sizeof(filepath), "/csv_logs/%s", entry->d_name);
                    ESP_LOGI("TIME", "Found file: %s", filepath);
                    // Replace latest_file with current filepath (alphabetically last)
                    if (strcmp(filepath, latest_file) > 0) {
                        strcpy(latest_file, filepath);
                    }
                }
            }
            closedir(dir);

            if (strlen(latest_file) == 0) {
                ESP_LOGE("TIME", "No CSV files found to annotate");
                httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "No CSV found");
                return ESP_FAIL;
            }

            ESP_LOGI("TIME", "Using latest file: %s", latest_file);

            // Step 2: Reopen latest file and get last line
            FILE *f = fopen(latest_file, "r");
            if (!f) {
                ESP_LOGE("TIME", "Failed to open latest CSV file");
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "CSV open error");
                return ESP_FAIL;
            }

            char line[256];
            char last_line[256] = {0};
            while (fgets(line, sizeof(line), f)) {
                if (strlen(line) > 5) strcpy(last_line, line);
            }
            fclose(f);

            // Step 3: Extract last timestamp
            char *token = strrchr(last_line, ',');  // last value assumed to be timestamp
            if (!token) {
                ESP_LOGE("TIME", "Malformed CSV: no timestamp found");
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Bad CSV format");
                return ESP_FAIL;
            }

            float last_timestamp = atof(token + 1);  // skip the comma
            time_t start_time = end_time - (time_t)(last_timestamp / 1000);  // assuming ms timestamp

            // Step 4: Append time info as new row
            f = fopen(latest_file, "a");
            if (!f) {
                ESP_LOGE("TIME", "Failed to reopen CSV for appending");
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "CSV append error");
                return ESP_FAIL;
            }

            struct tm tm_start, tm_end;
            localtime_r(&start_time, &tm_start);
            localtime_r(&end_time, &tm_end);
            char start_buf[32], end_buf[32];
            strftime(start_buf, sizeof(start_buf), "%Y-%m-%d %H:%M:%S", &tm_start);
            strftime(end_buf, sizeof(end_buf), "%Y-%m-%d %H:%M:%S", &tm_end);

            fprintf(f, "\nSTART_TIME,END_TIME\n%s,%s\n", start_buf, end_buf);
            fclose(f);

            ESP_LOGI("TIME", "Start: %s | End: %s written to %s", start_buf, end_buf, latest_file);
            httpd_resp_sendstr(req, "Time range written to CSV");
            return ESP_OK;
        }
    }

    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid timestamp");
    return ESP_FAIL;
}

// Treat CSV as a JSON for the App
static esp_err_t get_csv_as_json_handler(httpd_req_t *req)
{
    char query[100];
    char file_id[64] = "Test_s_log.csv";  // default fallback

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        httpd_query_key_value(query, "id", file_id, sizeof(file_id));
    }

    char filepath[FILE_PATH_MAX];
    snprintf(filepath, sizeof(filepath), "/csv_logs/%s", file_id);

    // Check if the file exists in the directory first
    struct dirent *entry;
    DIR *dir = opendir("/csv_logs");
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open directory: /csv_logs");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to access log directory");
        return ESP_FAIL;
    }

    bool file_found = false;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, file_id) == 0) {
            file_found = true;
            break;
        }
    }
    closedir(dir);

    if (!file_found) {
        ESP_LOGE(TAG, "JSON Handler: CSV file not found: %s", filepath);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "CSV file not found");
        return ESP_FAIL;
    }

    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        ESP_LOGE(TAG, "Failed to open CSV file: %s", filepath);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "CSV file not found");
        return ESP_FAIL;
    }

    char line[512];
    char *headers[32] = {0};
    int num_headers = 0;
    bool header_parsed = false;
    bool first_object = true;

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send_chunk(req, "[", 1);  // Start of JSON array

    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\r\n")] = 0;
        if (strlen(line) < 1) continue;

        if (!header_parsed) {
            // Parse header
            char *token = strtok(line, ",");
            while (token != NULL && num_headers < 32) {
                headers[num_headers++] = strdup(token);  // duplicate header strings
                token = strtok(NULL, ",");
            }
            header_parsed = true;
            continue;
        }

        // Parse a data row
        char *values[32] = {0};
        int val_count = 0;
        char *token = strtok(line, ",");
        while (token != NULL && val_count < num_headers) {
            values[val_count++] = token;
            token = strtok(NULL, ",");
        }

        // Estimate size needed for the JSON object (safely oversize)
        size_t json_size = num_headers * 64 + 32;
        char *json_line = malloc(json_size);
        if (!json_line) {
            fclose(fp);
            for (int i = 0; i < num_headers; i++) free(headers[i]);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
            return ESP_FAIL;
        }

        json_line[0] = '\0';
        strcat(json_line, "{");
        for (int i = 0; i < val_count; i++) {
            strcat(json_line, "\"");
            strcat(json_line, headers[i]);
            strcat(json_line, "\":\"");
            strcat(json_line, values[i]);
            strcat(json_line, "\"");
            if (i < val_count - 1) strcat(json_line, ",");
        }
        strcat(json_line, "}");

        if (!first_object) httpd_resp_send_chunk(req, ",", 1);
        first_object = false;
        httpd_resp_send_chunk(req, json_line, strlen(json_line));
        free(json_line);
    }

    httpd_resp_send_chunk(req, "]", 1);  // End of JSON array
    httpd_resp_send_chunk(req, NULL, 0);  // Signal end of response
    fclose(fp);

    for (int i = 0; i < num_headers; i++) {
        free(headers[i]);
    }

    return ESP_OK;
}


static esp_err_t check_csv_exists_handler(httpd_req_t *req)
{
    char query[100];
    char id[64] = ""; 

    // Try to get the query string
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
        ESP_LOGW(TAG, "No query string provided");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing query string");
        return ESP_FAIL;
    }

    // Try to extract 'id' key from query
    if (httpd_query_key_value(query, "id", id, sizeof(id)) != ESP_OK) {
        ESP_LOGW(TAG, "Missing 'id' in query string");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing 'id' parameter");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Received ID: %s", id);

    // Construct the full path to the CSV file
    char filepath[128];
    snprintf(filepath, sizeof(filepath), "/csv_logs/%s", id);

    // Try to open the file
    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        ESP_LOGE(TAG, "CSV file not found: %s", filepath);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "CSV file not found");
        return ESP_FAIL;
    }

    fclose(fp);  // File found and successfully opened
    ESP_LOGI(TAG, "CSV file exists: %s", filepath);

    // Return a simple JSON response
    const char *response = "{\"status\": \"found\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
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
    config.max_uri_handlers = 12;
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

    httpd_uri_t toggle_uri = {
        .uri       = "/toggle_sensor",
        .method    = HTTP_GET,
        .handler   = toggle_sensor_handler,
        .user_ctx  = (void*) q
    };
    httpd_register_uri_handler(server, &toggle_uri);

    httpd_uri_t end_uri = {
        .uri       = "/end_sensor",
        .method    = HTTP_GET,
        .handler   = stop_sensor_handler,
        .user_ctx  = (void*) q
    };
    httpd_register_uri_handler(server, &end_uri);

    httpd_uri_t name_uri = {
        .uri       = "/set_name",
        .method    = HTTP_POST,
        .handler   = set_name_handler,
        .user_ctx  = (void*) q
    };
    httpd_register_uri_handler(server, &name_uri);

    httpd_uri_t list_logs_uri = {
        .uri       = "/logs/list",
        .method    = HTTP_GET,
        .handler   = list_logs_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &list_logs_uri);

    httpd_uri_t csv_json_uri = {
        .uri       = "/get_csv_json",
        .method    = HTTP_GET,
        .handler   = get_csv_as_json_handler,
        .user_ctx  = server_data
    };
    httpd_register_uri_handler(server, &csv_json_uri);

    httpd_uri_t log_summary_uri = {
        .uri       = "/api/log_summary",
        .method    = HTTP_GET,
        .handler   = get_log_summary_handler,
        .user_ctx  = server_data
    };
    httpd_register_uri_handler(server, &log_summary_uri);


    httpd_uri_t get_all_logs_uri = {
        .uri       = "/getAllLogs",
        .method    = HTTP_GET,
        .handler   = get_all_logs_handler,
        .user_ctx  = NULL
    };

    httpd_register_uri_handler(server, &get_all_logs_uri);

    httpd_uri_t get_log_id_uri = {
        .uri       = "/getLogId/*",
        .method    = HTTP_GET,
        .handler   = get_log_id_handler,
        .user_ctx  = NULL
    };

    httpd_register_uri_handler(server, &get_log_id_uri);

    httpd_uri_t time_uri = {
        .uri       = "/set_time",
        .method    = HTTP_GET,
        .handler   = set_time_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &time_uri);

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



