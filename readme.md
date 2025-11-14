<h1 align="center">Connected Components Parralel Implementations</h1>
<h3 align="center">Parallel and Distributed Systems</h3>

<p align="center">
  <em>Department of Electrical and Computer Engineering,</em><br>
  Aristotle University of Thessaloniki</em><br>
  <strong>Homework #1</strong>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/language-C-blue.svg" alt="Language">
  <img src="https://img.shields.io/badge/build-Makefile-success" alt="Build">
  <img src="https://img.shields.io/badge/platform-Linux%20%7C%20Unix-lightgrey" alt="Platform">
</p>

---

## Overview

This repository contains the source code for the **Connected Components Parralel Implementations**, developed as part of the *Parallel and Distributed Systems* course.

It provides **four implementations** of the connected components algorithm for sparse graphs/matrices, each employing a distinct parallel programming model.

| Implementation | Parallel Model | Compiler |
|----------------|----------------|-----------|
| Sequential     | None (baseline) | `gcc` |
| OpenMP         | Shared memory threads | `gcc` |
| Pthreads       | POSIX threads | `gcc` |
| Cilk           | Task parallelism | `clang` (OpenCilk) |

A dedicated **benchmark runner** automates testing, timing, and result collection across all versions.

---

## Dependencies

| Dependency | Purpose | Notes |
|-------------|----------|--------|
| `libmatio` | Reading `.mat` (Matrix Market) files | Required |
| `pthread` | POSIX threading | Required |
| `openmp` | OpenMP parallelism | Required |
| `opencilk` | Cilk parallel runtime | Ensure OpenCilk's `clang` is installed |
| `tree` | Optional (for project tree view) | Optional |

Check your setup using:

```bash
make check-deps
```

---

##  Build Instructions

Before building, ensure the `CILK_PATH` in the **Makefile** is correctly set for your OpenCilk installation.

To build all targets:

```bash
make
```

To build specific implementations:

```bash
make sequential   # Sequential version
make openmp       # OpenMP version
make pthreads     # Pthreads version
make cilk         # Cilk version
make runner       # Benchmark runner
```

To clean and rebuild:

```bash
make clean
make rebuild
```

---

## Usage

After building, executables are placed in the `bin/` directory.

Run all implementations:

```bash
make benchmark MATRIX=data/dictionary28.mat TRIALS=100 THREADS=16
```

This command:

* Runs all algorithms on the given `.mat` file.
* Performs 100 trials.
* Uses up to 16 threads where applicable.
* Prints JSON to `stdout`.

Alternatively you can use:

```bash
make benchmark-save MATRIX=data/dictionary28.mat TRIALS=100 THREADS=16
```
This command is identical with `make benchmark`, but
saves the results as a timestamped JSON in `benchmarks/`

Example output file:

```
benchmarks/benchmark-result-20251110_223015.json
```

---

## Benchmark Runner

You can also run the benchmark manually:

```bash
bin/benchmark_runner -t <threads> -n <trials> path/to/matrix.mat
```

Or every algorithm implementation accordingly:

```bash
bin/connected_components_pthreads -t <threads> -n <trials> path/to/matrix.mat
```

**Arguments:**

* `-t <threads>` — number of threads (default: 8)
* `-n <trials>` — number of trials (default: 3)

**Example:**

```bash
bin/benchmark_runner -t 16 -n 50 data/dictionary28.mat
```

---

## Project Structure

```
connected_components/
├── src/
│   ├── algorithms/      # Implementations: sequential, OpenMP, Pthreads, Cilk
│   ├── core/            # Core matrix data structures (CSC format, etc.)
│   ├── utils/           # Helpers: JSON, timing, parsing, logging
│   ├── main.c           # Entry point for algorithms
│   └── runner.c         # Benchmark runner
├── data/                # Input matrices (.mat)
├── benchmarks/          # Saved benchmark results
├── Makefile             # Build automation
└── README.md
```

View the tree interactively:

```bash
make tree
```

---

## Example Output

Sample benchmark JSON:

```json
{
  "sys_info": {
    "timestamp": "2025-11-11T13:36:55",
    "cpu_info": "11th Gen Intel(R) Core(TM) i7-11800H @ 2.30GHz",
    "ram_mb": 15687.50,
    "swap_mb": 16384.00
  },
  "matrix_info": {
    "path": "data/dictionary28.mat",
    "rows": 52652,
    "cols": 52652,
    "nnz": 178076
  },
  "benchmark_info": {
    "threads": 16,
    "trials": 100
  },
  "results": [
    {
      "algorithm": "Sequential",
      "connected_components": 17903,
      "statistics": {
        "mean_time_s": 0.005175,
        "std_dev_s": 0.000137,
        "median_time_s": 0.005185,
        "min_time_s": 0.004760,
        "max_time_s": 0.005613
      },
      "throughput_edges_per_sec": 34412754.75,
      "memory_peak_mb": 6.54,
      "speedup": 1.0000,
      "efficiency": 0.0625
    },
    {
      "algorithm": "OpenMP",
      "connected_components": 17903,
      "statistics": {
        "mean_time_s": 0.002833,
        "std_dev_s": 0.000605,
        "median_time_s": 0.002712,
        "min_time_s": 0.002451,
        "max_time_s": 0.006450
      },
      "throughput_edges_per_sec": 62849122.80,
      "memory_peak_mb": 6.82,
      "speedup": 1.8267,
      "efficiency": 0.1142
    },
    {
      "algorithm": "Pthreads",
      "connected_components": 17903,
      "statistics": {
        "mean_time_s": 0.002180,
        "std_dev_s": 0.000465,
        "median_time_s": 0.002165,
        "min_time_s": 0.001357,
        "max_time_s": 0.003255
      },
      "throughput_edges_per_sec": 81686649.21,
      "memory_peak_mb": 6.48,
      "speedup": 2.3739,
      "efficiency": 0.1484
    },
    {
      "algorithm": "OpenCilk",
      "connected_components": 17903,
      "statistics": {
        "mean_time_s": 0.000927,
        "std_dev_s": 0.000333,
        "median_time_s": 0.000808,
        "min_time_s": 0.000668,
        "max_time_s": 0.003167
      },
      "throughput_edges_per_sec": 192200265.07,
      "memory_peak_mb": 6.59,
      "speedup": 5.5825,
      "efficiency": 0.3489
    }
  ]
}
```

---

## Maintenance Commands

| Command              | Description                   |
| -------------------- | ----------------------------- |
| `make info`          | Display build configuration   |
| `make list-sources`  | List all `.c` source files    |
| `make list-binaries` | Show all compiled executables |
| `make clean`         | Remove build artifacts        |
| `make rebuild`       | Clean and rebuild everything  |

---

<p align="center">
  <sub>Noevember 2025, Aristotle University of Thessaloniki</sub><br>
</p>
