# Parallelizing the $\Delta$-Stepping Algorithm with POSIX Threads
### Master's Degree Project: High-Performance Computing

## Project Overview
This project was developed as part of my **Master's degree** curriculum. It focuses on the implementation and optimization of a high-performance, multi-threaded version of the **$\Delta$-stepping (Delta-stepping)** algorithm for solving the **Single-Source Shortest Path (SSSP)** problem. 

Originally proposed by Meyer and Sanders, $\Delta$-stepping bridges the gap between Dijkstra’s algorithm (sequential) and Bellman-Ford (highly parallelizable but inefficient) by grouping edges into "light" and "heavy" categories. This allows for increased parallelism during the relaxation phase without the massive overhead of Bellman-Ford.

The core objective was to transform a sequential C implementation into a scalable parallel system using the **POSIX Threads (Pthreads)** library.

## Key Features
* **Parallel Relaxation:** Light edges are processed in parallel by multiple worker threads using a synchronized work-list and a custom thread-pool approach.
* **Thread Safety:** Implements fine-grained locking and atomic operations to manage concurrent updates to tentative distances ($dist[v]$) and bucket assignments.
* **Dynamic Bucket Management:** Efficient handling of the bucket array $B$ to minimize contention between threads during vertex insertion.
* **Master's Level Rigor:** Includes comprehensive performance analysis, memory profiling, and automated consistency verification.

---

## Technical Specifications & Memory Safety
### Parallelization Strategy
1.  **Work Distribution:** The current bucket $B[i]$ is partitioned among available threads to maximize core utilization.
2.  **Synchronization:** * Used `pthread_mutex_t` for protecting shared bucket structures.
    * Utilized `pthread_barrier_wait` to ensure all threads finish the light-edge relaxation (fixpoint) before moving to heavy-edge processing.
3.  **Memory Integrity:** The implementation has been rigorously tested using **Valgrind**.
    * **Status:** 0 leaks / 0 errors detected.
    * **Report:** `definitely lost: 0 bytes`, `indirectly lost: 0 bytes`.



---

## Validation & Testing Tools
To ensure the parallel implementation maintains the exact correctness of the original serial code, two Python utility scripts were utilized:

### 1. `consistency_worker.py`
This script automates the execution of both the serial and parallel binaries across various thread counts and $\Delta$ values. It ensures the environment remains stable for performance benchmarking and checks for race conditions.
* **Usage:** `python3 consistency_worker.py --binary ./delta_parallel --graph data/sample.txt`

### 2. `discrepancies_comparison.py`
A verification tool that performs a line-by-line comparison of the output distances produced by the serial code vs. the parallel code. It identifies any floating-point mismatches or logic errors.
* **Usage:** `python3 discrepancies_comparison.py serial_out.txt parallel_out.txt`
* **Output:** Reports the number of mismatched vertices and the magnitude of any numerical deviation.

---

## Getting Started

### Prerequisites
* GCC Compiler
* POSIX Threads library (`-lpthread`)
* Python 3.x (for validation scripts)
* Valgrind (for memory profiling)

### Compilation
```
gcc -O3 -o delta_parallel main.c -lpthread -lm
```

### Execution
```
./delta_parallel <graph_file> <source_node> <delta_value>
```

# Memory validation
```
valgrind --leak-check=full --show-leak-kinds=all ./delta_parallel <graph_file> 0 10.0
```
