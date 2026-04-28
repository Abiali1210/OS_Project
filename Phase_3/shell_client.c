#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "shell_common.h"

/* -------------------- Networking -------------------- */

int send_all(int socket_fd, const void *buffer, size_t length) {
    size_t total_sent = 0;
    const char *ptr = (const char *)buffer;

    // Keep sending until the whole message is sent
    while (total_sent < length) {
        ssize_t bytes_sent = send(socket_fd, ptr + total_sent, length - total_sent, 0);
        if (bytes_sent <= 0) {
            return -1;
        }
        total_sent += (size_t)bytes_sent;
    }

    return 0;
}

int recv_all(int socket_fd, void *buffer, size_t length) {
    size_t total_received = 0;
    char *ptr = (char *)buffer;

    // Keep receiving until the full result arrives
    while (total_received < length) {
        ssize_t bytes_received = recv(socket_fd, ptr + total_received, length - total_received, 0);
        if (bytes_received <= 0) {
            return -1;
        }
        total_received += (size_t)bytes_received;
    }

    return 0;
}

int receive_result(int socket_fd, CommandResult *result) {
    return recv_all(socket_fd, result, sizeof(CommandResult));
}

int connect_to_server(void) {
    int socket_fd;
    struct sockaddr_in server_addr;
    CommandResult startup_result;

    // Create the client socket.
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        perror("client: socket failed");
        return -1;
    }

    // Fill the server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    // Convert the IP address to binary form
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        perror("client: invalid server address");
        close(socket_fd);
        return -1;
    }

    // Connect the client socket to the server
    if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("client: connect failed");
        close(socket_fd);
        return -1;
    }

    // Read the server handshake before starting the shell loop
    if (receive_result(socket_fd, &startup_result) < 0) {
        printf("client: failed to receive server status\n");
        close(socket_fd);
        return -1;
    }

    if (startup_result.status == STATUS_SERVER_FULL) {
        if (startup_result.output[0] != '\0') {
            printf("%s", startup_result.output);
        }
        close(socket_fd);
        return -1;
    }

    if (startup_result.status != STATUS_OK) {
        if (startup_result.output[0] != '\0') {
            printf("%s", startup_result.output);
        }
        close(socket_fd);
        return -1;
    }

    printf("Connected to server on port %d.\n", PORT);
    return socket_fd;
}

/* -------------------- Transport Helpers -------------------- */

int send_command(int socket_fd, char *line) {
    char buffer[MAX_LINE];

    // Clear the buffer before copying the command
    memset(buffer, 0, sizeof(buffer));
    // Copy the input safely into a fixed-size buffer
    strncpy(buffer, line, MAX_LINE - 1);
    buffer[MAX_LINE - 1] = '\0';

    return send_all(socket_fd, buffer, MAX_LINE);
}

/* -------------------- Client UI -------------------- */

int read_user_input(char *line) {
    // Show the shell prompt to the user
    printf("shell> ");
    fflush(stdout);

    // Read one command line from stdin
    if (fgets(line, MAX_LINE, stdin) == NULL) {
        return 0;
    }

    return 1;
}

void clear_screen(void) {
    // ANSI escape sequence to clear the terminal
    printf("\033[H\033[J");
    fflush(stdout);
}

void client_loop(int socket_fd) {
    char line[MAX_LINE];
    CommandResult result;

    // Keep running until the user exits or connection fails
    while (1) {
        memset(line, 0, sizeof(line));

        if (!read_user_input(line)) {
            printf("\n");
            break;
        }

        // Send the typed command to the server
        if (send_command(socket_fd, line) < 0) {
            printf("client: failed to send command\n");
            break;
        }

        // Wait for the server's response
        if (receive_result(socket_fd, &result) < 0) {
            printf("client: connection closed by server\n");
            break;
        }

        // Handle special commands based on the returned status
        if (result.status == STATUS_CLEAR) {
            clear_screen();
        } else if (result.status == STATUS_EXIT) {
            if (result.output[0] != '\0') {
                printf("%s", result.output);
            }
            break;
        } else {
            if (result.output[0] != '\0') {
                printf("%s", result.output);
            }
        }
    }
}

/* -------------------- Main -------------------- */

int main(void) {
    // Start the connection with the server
    int socket_fd = connect_to_server();
    if (socket_fd < 0) {
        return 1;
    }

    // Run the main client loop
    client_loop(socket_fd);

    close(socket_fd);
    return 0;
}
