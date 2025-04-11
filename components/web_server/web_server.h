#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <esp_err.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
/**
 * @brief Initialize and start the web server
 * 
 * @return ESP_OK on success, or an error code
 */
esp_err_t start_web_server(QueueHandle_t cmd_queue);

#endif // WEB_SERVER_H