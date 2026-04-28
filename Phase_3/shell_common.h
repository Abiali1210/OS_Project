#ifndef SHELL_COMMON_H
#define SHELL_COMMON_H

#define PORT 8080              // Port number used by client and server
#define MAX_LINE 1024          // Maximum size of one command line
#define MAX_CMDS 5             // Maximum number of piped commands
#define MAX_ARGS 20            // Maximum number of arguments per command
#define MAX_OUTPUT 8192        // Maximum size of command output
#define MAX_CLIENTS 10         // Maximum number of connected clients

#define STATUS_OK 0            // Normal command result
#define STATUS_ERROR 1         // Command failed
#define STATUS_CLEAR 2         // Clear screen command
#define STATUS_EXIT 3          // Exit command
#define STATUS_SERVER_FULL 4   // Server reached client limit

typedef struct {
    int status;                 // Tells the client what happened
    char output[MAX_OUTPUT];    // Stores the text sent back
} CommandResult;

#endif
