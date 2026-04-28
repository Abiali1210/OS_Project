# Spring26Project-Assignments-adjusted-april-v2.pdf

## Page 1

### 1 Objective

In this project you are going to create your own remote shell that simulates some services of the OS including the current Linux shell features, processes, threads, communication, scheduling, etc. The project is divided in four related phases provided as graded assignments in order to build it progressively during the semester. A group of 2 students is required.

**Phase1- Prog. Assi1: Local CLI Shell**
(Due Date: 9th of April)

**Phase2- Prog. Assi2: Remote CLI Shell**
(Due Date: 19th of April)

**Phase3- Prog. Assi3: Remote Multitasking CLI Shell**
(Due Date: 29th of April)

**Final Report + Final Demo**
(Due Date: 11 - 14 May, last lab week)

## 2 Implementation

**1- Phase1- Local CLI Shell:** In this phase, you have to develop using C your own shell/CLI that replicates features from the linux one such as ls, ls- l, pwd, mkdir, rm, |, ... or a program to execute including its name. You should implement at least 15 commands and should include the composed commands including 1 pipe (...), 2 pipes (...), and 3 pipes (...), (...). You can use the techniques that you have learned about creating processes (e.g. fork, exec), interprocess communications (e.g. pipes), etc.

**2- Phase2- Remote CLI Shell:** In this phase, you have to upgrade Phase1 with remote access capability to your shell (implemented in the server) through Socket communication. In this context, the client takes inputs from the user and sends requests to the server. Each input from the client can be either a command (such as ls, ls- l, pwd, mkdir, rm - r, ...), composed commands or a program to execute including its name. The client will then receive the output/result from the server and print it on the screen.

**3- Phase3- Remote Multitasking CLI Shell:** In this phase, you have to upgrade Phase2 server with multitasking capability using threads. The server should be able to serve several clients simultaneously. Each thread should handle the request, communication and computation with one client.

The solutions require the use of process creation, inter-process communication using sockets and

## Page 2

pipes, multiprocessing, multithreading, scheduling and synchronization. In your project, you are supposed to create a fully functional remote shell including your own time-based scheduler. Don't be limited to any implementation techniques described above. You can implement the above requirements using your own techniques. Comment your code. Add a few test cases that demonstrate the functionalities of your project. Add a report including the usage of your program, test cases used and their results, and a detailed description of your implementation. Finally, be ready for a demo.

## 3 Grading



|Description|Score (/40)|
|-|-|
|Phase 1: 25 (15 points on the single commands and 10 points on the composed commands including pipes) - Phase 2: 25 (proper remote execution of the commands by one client) - Phase 3: 20 (proper multithreaded remote execution of the commands by multiple clients)|70|
|Proper error handling, documentation, and demos|20|
|Report|10|
|**Total**|**100**|



You should solve and work in group of 2 on this project. The deadline of each phase (Programming Assignment) is listed above in this document and on Blackboard.

You should directly submit your C source files and Makefile on Blackboard, followed by scheduled demos with the lab instructor. Submissions via email are unacceptable.

Note that your program should be implemented in C and must be runnable on the Linux operating system.

All the phases (programming assignments) are due at 11:59pm on the due date. For late submissions, (5%) will be deducted from the homework grade per late day. Late submissions will be accepted only up to 5 days late, afterwards you will receive zero points.

