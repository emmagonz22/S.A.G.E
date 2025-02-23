idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES web_server spiffs_handler)