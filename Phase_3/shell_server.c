#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <pthread.h>

#include "shell_common.h"

// Struct to store session information for each client
typedef struct {
    int client_socket;      // client socket
    int client_id;          // client id
    char session_directory[PATH_MAX];       // session directory
    int active;         // indicates if client is active
} Session;

// Struct to store server state
typedef struct {
    int active_clients;     // number of active clients
    int next_client_id;     // next available client id
    pthread_mutex_t state_mutex;    // protects access to active_clients
    pthread_mutex_t log_mutex;      // protects access to logs
} ServerState;

// Arguments passed to client thread routine
typedef struct {
    Session *session;       // client session
    ServerState *server_state;      // server state
} ThreadArg;    

/* -------------------- Networking -------------------- */

int setup_server_socket(void) {
    int server_socket;
    int opt = 1;    // for setsockopt
    struct sockaddr_in server_addr;

    // Create the server socket.
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("server: socket failed");
        return -1;
    }

    // Allow the port to be reused after restart.
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("server: setsockopt failed");
        close(server_socket);
        return -1;
    }

    // Fill the server address information.
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind the socket to the chosen port.
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("server: bind failed");
        close(server_socket);
        return -1;
    }

    // Start listening for clients.
    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("server: listen failed");
        close(server_socket);
        return -1;
    }

    printf("Server listening on port %d...\n", PORT);
    return server_socket;
}

int accept_client(int server_socket) {
    int client_socket;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    // Accept one incoming client connection
    client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
    if (client_socket < 0) {
        perror("server: accept failed");
        return -1;
    }

    return client_socket;
}

/* -------------------- Transport Helpers -------------------- */

int send_all(int socket_fd, const void *buffer, size_t length) {
    size_t total_sent = 0;
    const char *ptr = (const char *)buffer;

    // Keep sending until all bytes are sent.
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

    // Keep reading until the full message is received.
    while (total_received < length) {
        ssize_t bytes_received = recv(socket_fd, ptr + total_received, length - total_received, 0);
        if (bytes_received <= 0) {
            return -1;
        }
        total_received += (size_t)bytes_received;
    }

    return 0;
}

int receive_command(int client_socket, char *line) {
    // Read one full command from the client.
    if (recv_all(client_socket, line, MAX_LINE) < 0) {
        return -1;
    }

    // Make sure the received string ends safely.
    line[MAX_LINE - 1] = '\0';
    return 0;
}

int send_result(int client_socket, CommandResult *result) {
    // Send the full command result back to the client.
    return send_all(client_socket, result, sizeof(CommandResult));
}

/* -------------------- Session -------------------- */
// Initialize server state, counters and mutexes
void initialize_server_state(ServerState *state) {
    state->active_clients = 0;
    state->next_client_id = 1;
    pthread_mutex_init(&state->state_mutex, NULL);
    pthread_mutex_init(&state->log_mutex, NULL);
}

// Register a new client session and return its client id
int register_client(ServerState *state, Session *session) {
    int allowed = 0;

    // Lock the server state mutex, and check for space for a new client
    pthread_mutex_lock(&state->state_mutex);

    // If there is space, register the client
    if (state->active_clients < MAX_CLIENTS) {
        state->active_clients++;
        session->client_id = state->next_client_id++;
        allowed = 1;    // set the allowed flag
    }

    // Unlock the server state mutex
    pthread_mutex_unlock(&state->state_mutex);
    return allowed;
}

// Unregister a client session
void unregister_client(ServerState *state) {
    pthread_mutex_lock(&state->state_mutex);

    // Decrement the active client count
    if (state->active_clients > 0) {
        state->active_clients--;
    }

    pthread_mutex_unlock(&state->state_mutex);
}

// Log a message to the console
void safe_log(ServerState *state, const char *fmt, ...) {
    va_list args;   // variable argument list

    pthread_mutex_lock(&state->log_mutex);  // Lock the log mutex

    // Print the message to stdout
    va_start(args, fmt);   
    vprintf(fmt, args);
    va_end(args);
    fflush(stdout);     // Flush stdout

    pthread_mutex_unlock(&state->log_mutex);    // Unlock the log mutex
}

void initialize_session(Session *session, int client_socket) {
    // Save the client socket for this session
    session->client_socket = client_socket;
    session->client_id = -1;   
    session->active = 1;

    // Start the session in the server's current directory
    if (getcwd(session->session_directory, sizeof(session->session_directory)) == NULL) {
        strcpy(session->session_directory, "/");
    }
}

// Reset the result struct before handling a new command
void clear_result(CommandResult *result) {
    result->status = STATUS_OK;     // Reset the status to OK
    memset(result->output, 0, sizeof(result->output));  // Clear the output
}

// Send a rejection message to the client
void reject_client_full(int client_socket) {
    CommandResult result;

    clear_result(&result);
    result.status = STATUS_SERVER_FULL;     // Set the status to server full
    snprintf(result.output, sizeof(result.output),
             "server full, try again later\n");
    send_result(client_socket, &result);
}

/* -------------------- Parsing (from Phase 1) -------------------- */
// Split a line into multiple commands using the pipe symbol
int parse_line(char *line, char **commands) {
    int count = 0;
    char *token = strtok(line, "|");

    // Split the line into commands using the pipe symbol.
    while (token != NULL && count < MAX_CMDS) {
        commands[count++] = token;
        token = strtok(NULL, "|");
    }

    return count;
}

// Split a command into multiple arguments using spaces
int parse_command(char *command, char **args) {
    int count = 0;
    char *token = strtok(command, " \t\n");

    // Split one command into arguments.
    while (token != NULL && count < MAX_ARGS - 1) {
        args[count++] = token;
        token = strtok(NULL, " \t\n");
    }

    args[count] = NULL;
    return count;
}

/* -------------------- Utility -------------------- */
// Check if a line is blank 
int is_blank_line(const char *line) {
    // Check if the input only contains spaces or tabs.
    while (*line != '\0') {
        if (!isspace((unsigned char)*line)) { 
            return 0;
        }
        line++;
    }
    return 1;
}

// Read command output and store it in the output buffer
int store_output(int read_fd, char *output) {
    ssize_t bytes_read;
    size_t total = 0;
    char buffer[512];

    // Clear the output buffer before saving command output.
    memset(output, 0, MAX_OUTPUT);

    // Read command output in chunks.
    while ((bytes_read = read(read_fd, buffer, sizeof(buffer))) > 0) {
        // Stop safely if the output would become too large.
        if (total + (size_t)bytes_read >= MAX_OUTPUT - 1) {
            size_t space_left = MAX_OUTPUT - 1 - total;
            if (space_left > 0) {
                memcpy(output + total, buffer, space_left);
                total += space_left;
            }

            {
                const char *note = "\n[output truncated]\n";
                size_t note_len = strlen(note);

                if (total + note_len < MAX_OUTPUT - 1) {
                    memcpy(output + total, note, note_len);
                    total += note_len;
                }
            }
            break;
        }

        memcpy(output + total, buffer, (size_t)bytes_read);
        total += (size_t)bytes_read;
    }

    output[total] = '\0';

    // Return an error if reading from the pipe failed.
    if (bytes_read < 0) {
        return -1;
    }

    return 0;
}

/* -------------------- Built-ins -------------------- */
// Handle built-in commands (cd, exit, pwd)
int handle_builtin(char **args, Session *session, CommandResult *result) {
    char candidate[PATH_MAX];
    char resolved[PATH_MAX];
    struct stat path_stat;
    int written;

    // No command means nothing to handle.
    if (args[0] == NULL) {
        return 0;
    }

    // Handle the built-in cd command.
    if (strcmp(args[0], "cd") == 0) {
        if (args[1] == NULL) {
            result->status = STATUS_ERROR;
            snprintf(result->output, sizeof(result->output),
                     "shell: cd: no argument provided\n");
            return 1;
        }

        if (args[2] != NULL) {  
            result->status = STATUS_ERROR;
            snprintf(result->output, sizeof(result->output),
                     "shell: cd: too many arguments\n");
            return 1;
        }

        // Build the target path for cd.
        if (args[1][0] == '/') {
            written = snprintf(candidate, sizeof(candidate), "%s", args[1]);
        } else {
            written = snprintf(candidate, sizeof(candidate), "%s/%s",
                               session->session_directory, args[1]);
        }

        // Reject paths that do not fit in the buffer.
        if (written < 0 || (size_t)written >= sizeof(candidate)) {
            result->status = STATUS_ERROR;
            snprintf(result->output, sizeof(result->output),
                     "shell: cd: path too long\n");
            return 1;
        }

        // Convert the path into its real absolute form.
        if (realpath(candidate, resolved) == NULL) {
            result->status = STATUS_ERROR;
            snprintf(result->output, sizeof(result->output),
                     "shell: cd: %s\n", strerror(errno));
            return 1;
        }

        // Make sure the target is really a directory.
        if (stat(resolved, &path_stat) != 0 || !S_ISDIR(path_stat.st_mode)) {
            result->status = STATUS_ERROR;
            snprintf(result->output, sizeof(result->output),
                     "shell: cd: not a directory\n");
            return 1;
        }

        // Save the new working directory for this client session.
        snprintf(session->session_directory, sizeof(session->session_directory), "%s", resolved);
        result->status = STATUS_OK;
        snprintf(result->output, sizeof(result->output), "%s\n", session->session_directory);
        return 1;
    }

    // Handle the built-in clear command.
    if (strcmp(args[0], "clear") == 0) {
        result->status = STATUS_CLEAR;
        result->output[0] = '\0';
        return 1;
    }

    // Handle the built-in exit command.
    if (strcmp(args[0], "exit") == 0) {
        result->status = STATUS_EXIT;
        snprintf(result->output, sizeof(result->output), "Session closed.\n");
        session->active = 0;
        return 1;
    }

    return 0;
}

/* -------------------- Single Command Execution -------------------- */

int execute_single_command(char **args, Session *session, CommandResult *result) {
    int capture_pipe[2];
    pid_t pid;
    int status;

    // An empty command does nothing.
    if (args[0] == NULL) {
        result->status = STATUS_OK;
        result->output[0] = '\0';
        return 0;
    }

    // Create a pipe to capture command output.
    if (pipe(capture_pipe) < 0) {
        result->status = STATUS_ERROR;
        snprintf(result->output, sizeof(result->output), "shell: pipe failed\n");
        return -1;
    }

    // Fork so the command runs in a child process.
    pid = fork();
    if (pid < 0) {
        close(capture_pipe[0]);
        close(capture_pipe[1]);
        result->status = STATUS_ERROR;
        snprintf(result->output, sizeof(result->output), "shell: fork failed\n");
        return -1;
    }

    if (pid == 0) {
        // Child writes output into the capture pipe.
        close(capture_pipe[0]);

        // Change the working directory to the session directory.
        if (chdir(session->session_directory) != 0) {
            dprintf(capture_pipe[1], "shell: chdir failed: %s\n", strerror(errno));
            close(capture_pipe[1]);
            _exit(1);
        }

        // Redirect stdout and stderr into the pipe.
        if (dup2(capture_pipe[1], STDOUT_FILENO) < 0 ||
            dup2(capture_pipe[1], STDERR_FILENO) < 0) {
            dprintf(capture_pipe[1], "shell: dup2 failed: %s\n", strerror(errno));
            close(capture_pipe[1]);
            _exit(1);
        }

        close(capture_pipe[1]);

        execvp(args[0], args);  // Execute the command

        dprintf(STDERR_FILENO, "shell: %s: %s\n", args[0], strerror(errno));
        _exit(1);
    }

    // Parent reads the command output from the pipe.
    close(capture_pipe[1]);

    if (store_output(capture_pipe[0], result->output) < 0) {
        close(capture_pipe[0]);
        waitpid(pid, NULL, 0);
        result->status = STATUS_ERROR;
        snprintf(result->output, sizeof(result->output),
                 "shell: failed to read command output\n");
        return -1;
    }

    close(capture_pipe[0]);

    waitpid(pid, &status, 0);

    // Set the result status based on the child exit code.
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        result->status = STATUS_OK;
    } else {
        result->status = STATUS_ERROR;
    }

    return 0;
}

/* -------------------- Pipeline Execution -------------------- */

int execute_pipe_commands(char **command_strings, int command_count,
                          Session *session, CommandResult *result) {
    int pipes[MAX_CMDS - 1][2];     // Pipes between commands as a 2D array
    int capture_pipe[2];        // Pipe to capture final output
    pid_t pids[MAX_CMDS];       // PIDs of child processes
    int overall_status = 0;

    // No commands means there is nothing to run.
    if (command_count <= 0) {
        result->status = STATUS_OK;
        result->output[0] = '\0';
        return 0;
    }

    // Create a pipe to collect the final output.
    if (pipe(capture_pipe) < 0) {
        result->status = STATUS_ERROR;
        snprintf(result->output, sizeof(result->output),
                 "shell: capture pipe failed\n");
        return -1;
    }

    // Create the pipes needed between pipeline commands.
    for (int i = 0; i < command_count - 1; i++) {
        if (pipe(pipes[i]) < 0) {
            for (int j = 0; j < i; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            close(capture_pipe[0]);
            close(capture_pipe[1]);

            result->status = STATUS_ERROR;
            snprintf(result->output, sizeof(result->output),
                     "shell: pipe failed\n");
            return -1;
        }
    }

    // Fork one child process for each command in the pipeline.
    for (int i = 0; i < command_count; i++) {
        char *args[MAX_ARGS];
        parse_command(command_strings[i], args);

        if (args[0] == NULL) {
            pids[i] = -1;
            continue;
        }

        pids[i] = fork();
        if (pids[i] < 0) {
            for (int j = 0; j < command_count - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            close(capture_pipe[0]);
            close(capture_pipe[1]);

            result->status = STATUS_ERROR;
            snprintf(result->output, sizeof(result->output),
                     "shell: fork failed\n");
            return -1;
        }

        if (pids[i] == 0) {
            if (chdir(session->session_directory) != 0) {
                dprintf(capture_pipe[1], "shell: chdir failed: %s\n", strerror(errno));
                _exit(1);
            }

            // Connect stdin to the previous pipe when needed.
            if (i > 0) {
                if (dup2(pipes[i - 1][0], STDIN_FILENO) < 0) {
                    dprintf(capture_pipe[1], "shell: dup2 failed: %s\n", strerror(errno));
                    _exit(1);
                }
            }

            // Connect stdout to the next pipe or final output pipe.
            if (i < command_count - 1) {
                if (dup2(pipes[i][1], STDOUT_FILENO) < 0) {
                    dprintf(capture_pipe[1], "shell: dup2 failed: %s\n", strerror(errno));
                    _exit(1);
                }
                if (dup2(capture_pipe[1], STDERR_FILENO) < 0) {
                    dprintf(capture_pipe[1], "shell: dup2 failed: %s\n", strerror(errno));
                    _exit(1);
                }
            } else {
                if (dup2(capture_pipe[1], STDOUT_FILENO) < 0 ||
                    dup2(capture_pipe[1], STDERR_FILENO) < 0) {
                    dprintf(capture_pipe[1], "shell: dup2 failed: %s\n", strerror(errno));
                    _exit(1);
                }
            }

            for (int j = 0; j < command_count - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            close(capture_pipe[0]);
            close(capture_pipe[1]);

            execvp(args[0], args);
            dprintf(STDERR_FILENO, "shell: %s: %s\n", args[0], strerror(errno));
            _exit(1);
        }
    }

    // Parent closes pipe ends it no longer needs.
    for (int i = 0; i < command_count - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
    close(capture_pipe[1]);

    // Read the pipeline output into the result buffer.
    if (store_output(capture_pipe[0], result->output) < 0) {
        close(capture_pipe[0]);

        for (int i = 0; i < command_count; i++) {
            if (pids[i] > 0) {
                waitpid(pids[i], NULL, 0);
            }
        }

        result->status = STATUS_ERROR;
        snprintf(result->output, sizeof(result->output),
                 "shell: failed to read pipeline output\n");
        return -1;
    }

    close(capture_pipe[0]);

    // Wait for all children and check if any command failed.
    for (int i = 0; i < command_count; i++) {
        if (pids[i] > 0) {
            int status;
            waitpid(pids[i], &status, 0);

            if (!(WIFEXITED(status) && WEXITSTATUS(status) == 0)) {
                overall_status = 1;
            }
        }
    }

    // Set the result status 
    result->status = overall_status ? STATUS_ERROR : STATUS_OK;
    return 0;
}

// Process a single command
int process_command(char *line, Session *session, CommandResult *result) {
    char *command_strings[MAX_CMDS];
    int command_count;

    // Start with a clean result structure.
    clear_result(result);

    // Reject invalid input.
    if (line == NULL) {
        result->status = STATUS_ERROR;
        snprintf(result->output, sizeof(result->output), "shell: invalid input\n");
        return -1;
    }

    // Ignore blank lines.
    if (is_blank_line(line)) {
        result->status = STATUS_OK;
        result->output[0] = '\0';
        return 0;
    }

    // Split the input line into pipe-separated commands.
    command_count = parse_line(line, command_strings);

    if (command_count <= 0) {
        result->status = STATUS_OK;
        result->output[0] = '\0';
        return 0;
    }

    // If there is only one command, handle it directly.
    if (command_count == 1) {
        char *args[MAX_ARGS];

        parse_command(command_strings[0], args);

        if (args[0] == NULL) {
            result->status = STATUS_OK;
            result->output[0] = '\0';
            return 0;
        }

        // Run built-in commands without forking.
        if (handle_builtin(args, session, result)) {
            return 0;
        }

        return execute_single_command(args, session, result);
    }

    // Check that every command in the pipeline is valid.
    for (int i = 0; i < command_count; i++) {
        char temp_command[MAX_LINE];
        char *args[MAX_ARGS];

        memset(temp_command, 0, sizeof(temp_command));
        strncpy(temp_command, command_strings[i], MAX_LINE - 1);
        temp_command[MAX_LINE - 1] = '\0';

        parse_command(temp_command, args);

        if (args[0] == NULL) {
            result->status = STATUS_ERROR;
            snprintf(result->output, sizeof(result->output),
                     "shell: invalid null command in pipeline\n");
            return 0;
        }
    }

    return execute_pipe_commands(command_strings, command_count, session, result);  
}

/* -------------------- Session Loop -------------------- */

void handle_client_session(Session *session) {
    char line[MAX_LINE];    // Buffer to store received command
    CommandResult result;

    // Keep serving commands until the client exits
    while (session->active) {
        memset(line, 0, sizeof(line));

        // Stop if the client disconnects unexpectedly
        if (receive_command(session->client_socket, line) < 0) {
            break;
        }

        // Process the received command.
        if (process_command(line, session, &result) < 0) {
            // If error, send error message
            clear_result(&result);
            result.status = STATUS_ERROR;
            snprintf(result.output, sizeof(result.output),
                     "shell: internal server error\n");
        }

        // Send the result back to the client.
        if (send_result(session->client_socket, &result) < 0) {
            break;
        }

        // Stop the session after the exit command.
        if (result.status == STATUS_EXIT) {
            break;
        }
    }
}

void *client_thread_routine(void *arg) {
    ThreadArg *thread_arg = (ThreadArg *)arg;
    Session *session = thread_arg->session;
    ServerState *server_state = thread_arg->server_state;

    handle_client_session(session);
    safe_log(server_state, "Client %d disconnected\n", session->client_id);
    unregister_client(server_state);
    close(session->client_socket);
    free(session);
    free(thread_arg);
    return NULL;
}

/* -------------------- Main -------------------- */

int main(void) {
    int server_socket;
    ServerState server_state;
    pthread_attr_t attr;

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror("server: signal failed");
        return 1;
    }

    initialize_server_state(&server_state);

    if (pthread_attr_init(&attr) != 0) {
        perror("server: pthread_attr_init failed");
        return 1;
    }

    if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0) {
        perror("server: pthread_attr_setdetachstate failed");
        pthread_attr_destroy(&attr);
        return 1;
    }

    // Start the server socket.
    server_socket = setup_server_socket();
    if (server_socket < 0) {
        pthread_attr_destroy(&attr);
        return 1;
    }

    // Keep accepting clients and serve each one in a thread.
    while (1) {
        int client_socket = accept_client(server_socket);
        Session *session;
        ThreadArg *thread_arg;
        CommandResult startup_result;
        pthread_t thread_id;

        if (client_socket < 0) {
            continue;
        }

        session = (Session *)malloc(sizeof(Session));
        thread_arg = (ThreadArg *)malloc(sizeof(ThreadArg));
        if (session == NULL || thread_arg == NULL) {
            perror("server: malloc failed");
            free(session);
            free(thread_arg);
            close(client_socket);
            continue;
        }

        initialize_session(session, client_socket);

        if (!register_client(&server_state, session)) {
            safe_log(&server_state, "Server full, rejected client\n");
            reject_client_full(client_socket);
            close(client_socket);
            free(thread_arg);
            free(session);
            continue;
        }

        clear_result(&startup_result);
        if (send_result(session->client_socket, &startup_result) < 0) {
            unregister_client(&server_state);
            close(session->client_socket);
            free(thread_arg);
            free(session);
            continue;
        }

        thread_arg->session = session;
        thread_arg->server_state = &server_state;

        if (pthread_create(&thread_id, &attr, client_thread_routine, thread_arg) != 0) {
            perror("server: pthread_create failed");
            unregister_client(&server_state);
            close(session->client_socket);
            free(thread_arg);
            free(session);
            continue;
        }

        safe_log(&server_state, "Client %d connected\n", session->client_id);
    }

    close(server_socket);
    pthread_attr_destroy(&attr);
    return 0;
}
