// Defining Commands for Modularity
#ifndef COMMAND_TYPES_H
#define COMMAND_TYPES_H

typedef enum {
    CMD_START_SENSOR,
    CMD_STOP_SENSOR,
    CMD_SET_NAME,
    // Add other commands as necessary
} CommandType;

typedef struct {
    CommandType type;
    char payload[64];  
} CommandMessage;

#endif // COMMAND_TYPES_H
