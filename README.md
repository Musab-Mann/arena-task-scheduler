# Arena Task Scheduler

**Sports Tournament-Based Operating System Simulation | C++17**

Arena Task Scheduler is a console-based C++ project that uses a football tournament scenario to demonstrate operating-system scheduling, synchronization, resource allocation, and memory-management concepts.

## Overview

The project maps operating-system ideas to a tournament environment:

- **Processes:** scheduled match fixtures or tasks
- **Resources:** stadiums and shared tournament resources
- **Scheduling:** deciding which task or match is handled next
- **Resource conflicts:** competing requests for shared resources

The simulation is intended for learning and demonstrating OS concepts. It is not a production operating-system scheduler.

## Features

### CPU scheduling
- First-Come, First-Served (FCFS)
- Priority Scheduling
- Round Robin
- Shortest Job First (SJF)
- Multi-Level Feedback Queue (MLFQ)

### Operating-system concepts
- Threading and synchronization demonstrations
- Mutual exclusion and condition variables
- Semaphore-related synchronization
- Deadlock detection and recovery demonstrations
- Banker's Algorithm
- LRU page-replacement simulation
- Scheduling and performance reporting

### Tournament simulation
- Team, player, stadium, and match management
- Tournament brackets and standings
- Sponsor and ticket-related data
- Match simulation and performance summaries
- Saving and loading tournament data
- AI-based match prediction feature

> Feature availability and behavior depend on the current implementation in the source file. This repository is an educational simulation; verify individual algorithms and outputs when using it for coursework or demonstrations.

## Technologies

- C++17
- Standard C++ library, including threading, containers, algorithms, and file I/O
- Git and GitHub

No third-party C++ libraries are required based on the current project setup.

## Requirements

- A C++17-compatible compiler
- On Linux, a compiler such as GCC and POSIX thread support

## Build and run

The main source file is main.cpp.

### Linux

    g++ -std=c++17 -pthread main.cpp -o arena
    ./arena

### Windows

Use a C++17-compatible compiler such as a recent MinGW-w64 GCC build. From the project directory, run:

    g++ -std=c++17 -pthread main.cpp -o arena.exe
    arena.exe

If your compiler does not support the threading flags shown above, check the compiler's threading configuration.

## Runtime data

The application may create or use local data and report files, including:

- arena_db.txt
- arena_log.txt
- arena_report.txt

Keep a backup of any data files you want to preserve before testing changes.

## Suggested project walkthrough

1. Clone or download this repository.
2. Compile the application using the instructions above.
3. Run it and explore the available menu options.
4. Compare the scheduling algorithms and their reported results.
5. Inspect the source code to understand how resources and data are managed.

## Repository structure

    arena-task-scheduler/
    ├── README.md
    └── main.cpp

## Future improvements

- Add screenshots or a short terminal demo
- Document sample inputs and outputs for each scheduling algorithm
- Add automated test cases and edge-case validation
- Separate major modules into smaller source and header files
- Document known limitations and algorithm assumptions

## Author

**Musab Hafeez Mann**

[GitHub profile](https://github.com/Musab-Mann)
