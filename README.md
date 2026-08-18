 Arena Task Scheduler

Sports Tournament-Based Operating System Simulation

Arena Task Scheduler is a C++17 software simulation that connects Operating System concepts with football tournament management.

The project demonstrates OS concepts through a sports tournament environment:

- Processes → Match Fixtures
- CPUs → Stadium Venues
- Schedulers → Tournament Scheduling
- Resource Contention → Stadium Scheduling Conflicts
- Deadlocks → Resource and Scheduling Conflicts

 Project Objectives

The main objectives of this project are:

- Demonstrate FCFS scheduling
- Demonstrate Priority Scheduling
- Demonstrate Round-Robin scheduling
- Demonstrate Shortest Job First (SJF)
- Demonstrate Multi-Level Feedback Queue (MLFQ)
- Simulate resource allocation and deadlock detection
- Demonstrate the Banker's Algorithm
- Simulate memory management using LRU page replacement
- Demonstrate thread synchronization
- Provide tournament management with persistent data storage
- Generate scheduling performance metrics
- Provide an interactive console-based application

 Features

 CPU Scheduling

The project implements five scheduling algorithms:

1. First-Come First-Served (FCFS)
2. Priority Scheduling
3. Round-Robin (RR)
4. Shortest Job First (SJF)
5. Multi-Level Feedback Queue (MLFQ)

 Operating System Concepts

The system demonstrates:

- Process scheduling
- Multithreading
- Mutual exclusion
- Condition variables
- Semaphores
- Deadlock detection
- Deadlock recovery
- Banker's Algorithm
- Memory management
- LRU page replacement
- Shared-state communication

 Tournament Management

The application provides:

- Team management
- Player management
- Stadium management
- Match management
- Sponsor management
- Ticket management
- Tournament bracket
- Match simulation
- Standings
- Performance metrics
- Analytics
- AI match prediction

 Persistence

The system supports saving and loading tournament data.

Runtime files include:

- arena_db.txt
- arena_log.txt
- arena_report.txt

## Technologies Used

The project is implemented using standard C++17.

Main technologies and libraries include:

- C++17
- std::thread
- std::mutex
- std::condition_variable
- std::atomic
- std::chrono
- std::random
- std::map
- std::vector
- std::queue
- std::deque
- std::algorithm
- std::fstream
- std::sstream

No external libraries are required.

 Requirements

- C++17 compatible compiler
- g++ 7 or later
- Linux or Linux-compatible environment
- POSIX thread support

## Compilation

Compile the project using:

bash
g++ -std=c++17 -pthread main.cpp -o arena
