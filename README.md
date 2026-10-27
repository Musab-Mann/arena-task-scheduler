# Arena Task Scheduler ⚽

**A C++17 console application demonstrating operating-system concepts through a football tournament simulation.**

Arena Task Scheduler combines a tournament-management scenario with educational demonstrations of CPU scheduling, synchronization, resource allocation, deadlock handling, and memory management.

## 📌 Project Overview

The project maps familiar operating-system concepts to a tournament environment:

- **Tasks/processes:** match fixtures or scheduled activities
- **Shared resources:** stadiums and tournament resources
- **Scheduling:** choosing which task is handled next
- **Resource conflicts:** competing requests for shared resources

This is an educational simulation, not a production operating-system scheduler.

## ✨ Features

### CPU Scheduling
- First-Come, First-Served (FCFS)
- Priority Scheduling
- Round Robin
- Shortest Job First (SJF)
- Multi-Level Feedback Queue (MLFQ)

### Operating-System Concepts
- Threading and synchronization demonstrations
- Mutual exclusion and condition variables
- Semaphore-related synchronization
- Deadlock detection and recovery demonstrations
- Banker's Algorithm
- LRU page-replacement simulation
- Scheduling and performance reporting

### Tournament Management
- Team, player, stadium, and match data
- Tournament brackets and standings
- Sponsor and ticket-related data
- Match simulation and performance summaries
- Saving and loading tournament data
- Match prediction feature

> This repository is an educational simulation. Exact behavior and output depend on the current implementation in `main.cpp`; review and test individual algorithms before relying on their results.

## 🧰 Tech Stack

- **Language:** C++17
- **Concepts:** Operating systems, scheduling, synchronization, resource management, file I/O
- **Tools:** GCC/MinGW-w64, Git, GitHub

No third-party C++ libraries are required by the documented build setup.

## ⚙️ Requirements

- A C++17-compatible compiler
- Linux: GCC with thread support
- Windows: a recent MinGW-w64 GCC installation

## 🚀 Build and Run

The application source is `main.cpp`.

### Linux

```bash
g++ -std=c++17 -pthread main.cpp -o arena
./arena
```

### Windows (MinGW-w64)

```powershell
g++ -std=c++17 -pthread main.cpp -o arena.exe
.\arena.exe
```

Run these commands from the repository directory. If compilation fails, check that your compiler supports C++17 and threading.

## 📂 Repository Structure

```text
arena-task-scheduler/
├── README.md
└── main.cpp
```

## 🗃️ Runtime Data

The program may create or use local data and report files such as:

- `arena_db.txt`
- `arena_log.txt`
- `arena_report.txt`

Back up any data files you need before testing or changing the program.

## 🧭 Suggested Walkthrough

1. Clone or download the repository.
2. Compile the program with the command for your operating system.
3. Run the application and explore its menu options.
4. Compare the available scheduling algorithms and their reports.
5. Review `main.cpp` to understand the implementation and data handling.

## 🔧 Potential Next Improvements

- Add screenshots or a short terminal demonstration
- Include verified sample inputs and outputs for each scheduling algorithm
- Add automated tests and edge-case validation
- Split major components into separate source and header files
- Document algorithm assumptions and known limitations

## 👤 Author

**Musab Hafeez Mann**

- [GitHub Profile](https://github.com/Musab-Mann)
- [Other Projects](https://github.com/Musab-Mann?tab=repositories)
- [Savora Restaurant — Live Demo](https://musab-mann.github.io/Savora-Restaurant/)
