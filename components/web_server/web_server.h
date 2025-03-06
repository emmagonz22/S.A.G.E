#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <esp_err.h>

/**
 * @brief Initialize and start the web server
 * 
 * @return ESP_OK on success, or an error code
 */
esp_err_t start_web_server(void);

#endif // WEB_SERVER_H