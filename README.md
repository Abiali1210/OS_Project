# Remote Multitasking CLI Shell

A Linux-like shell project developed in three phases for an Operating Systems course. The project starts as a local shell, grows into a remote client-server shell, and finishes as a multithreaded remote shell server that can handle multiple clients at the same time.

## Overview

This project combines several core operating system concepts in one system:

- process creation using `fork()`
- program execution using `execvp()`
- inter-process communication using `pipe()`
- TCP socket communication
- multithreading using `pthread`
- synchronization using mutexes

The final implementation supports normal Linux commands, built-in commands, pipelines, remote execution, and concurrent client sessions.

## Project Phases

### Phase 1 - Local CLI Shell

Phase 1 implements a basic local shell that:

- reads commands from the user
- parses commands and arguments
- supports built-in commands such as `cd`, `clear`, and `exit`
- executes normal commands using child processes
- supports pipelines using multiple processes and pipes

### Phase 2 - Remote CLI Shell

Phase 2 upgrades the shell into a client-server system:

- the client reads user input
- the client sends the command to the server
- the server executes the command
- the server sends the result back to the client

This phase introduces socket-based remote execution and per-session directory handling.

### Phase 3 - Remote Multitasking CLI Shell

Phase 3 upgrades the server into a multithreaded system:

- the main thread accepts new clients
- each client is handled by a detached worker thread
- each client has its own independent session
- the server supports up to `MAX_CLIENTS = 10`
- extra clients are rejected with a clear message

This phase introduces multithreading, synchronization, client lifecycle handling, and concurrent remote execution.

## Final Features

The final build supports:

- built-in commands: `cd`, `clear`, `exit`
- normal command execution through `execvp()`
- piped commands with up to 5 commands in one pipeline
- remote client-server communication over TCP
- per-client session directories
- multiple concurrent clients
- clean client rejection when the server is full
- output capture and safe fixed-size result transfer

## Repository Structure

```text
OS_Project/
├── Phase_2/
│   ├── shell_client.c
│   ├── shell_common.h
│   └── shell_server.c
├── Phase_3/
│   ├── shell_client.c
│   ├── shell_common.h
│   ├── shell_server.c
│   ├── Makefile
│   ├── Report OS final.pdf
├── OS Phase 1 Report + Code.pdf
└── Spring26Project-Assignments-adjusted-april-v2.pdf
```

## Build Instructions

Go to the final implementation folder:

```bash
cd Phase_3
```

Build both programs:

```bash
make clean
make
```

This produces:

- `server`
- `client`

## Run Instructions

Start the server in one terminal:

```bash
./server
```

Start the client in another terminal:

```bash
./client
```

After connecting, the client shows:

```text
shell>
```

You can then run commands such as:

```text
pwd
ls
mkdir testing1
cd testing1
ls | wc -l
exit
```

## Example Test Cases

### Basic command execution

```text
pwd
ls
mkdir testing1
rm -r testing1
```

### Error handling

```text
cd
cd a b
cd fake_dir
fakecommand
```

### Pipelines

```text
ls | wc -l
ls | grep shell | wc -l
echo one two three four five | wc -w | cat | tail -n 1 | head -n 1
```

### Concurrent clients

Run multiple `./client` sessions while the server stays active.

### Capacity control

Connect more than 10 clients and verify that the extra client receives:

```text
server full, try again later
```

## Design Notes

- The communication protocol uses a fixed-size command buffer and a fixed-size `CommandResult` structure.
- `send_all()` and `recv_all()` are used so partial socket transfers do not break communication.
- Each client gets its own `Session`, which keeps client state isolated.
- Shared server state is protected using mutexes.
- `SIGPIPE` is ignored so a disconnected client does not crash the whole server.

## Report and Screenshots

The final report assets are stored in:

- `Phase_3/Report OS final.docx`
- `Phase_3/images/report.md`
- `Phase_3/images/S1.jpeg` to `S14.jpeg`

## Limitations

- The parser is intentionally simple and does not fully support advanced shell syntax such as quoted strings.
- Command and output buffers are fixed in size.
- The server supports a fixed maximum of 10 active clients.
- The optional time-based scheduler idea mentioned generally in the assignment document was not implemented.

## Summary

This project demonstrates a full progression from a local shell to a remote multithreaded shell server. The final system successfully integrates process management, pipes, networking, multithreading, and synchronization into one working Operating Systems project.
