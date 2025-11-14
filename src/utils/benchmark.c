/**
 * @file benchmark.c
 * @brief Implementation of benchmarking framework for parallel algorithms.
 */

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/sysinfo.h>
#include <sys/resource.h>

#include "error.h"
#include "benchmark.h"
#include "json.h"

/* ------------------------------------------------------------------------- */
/*                            Static Helper Functions                        */
/* ------------------------------------------------------------------------- */

/**
 * @brief Returns current monotonic time in seconds.
 */
static double
now_sec(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}

/**
 * @brief Comparison function for sorting doubles.
 */
static int
cmp_double(const void *a, const void *b)
{
    double da = *(const double *)a;
    double db = *(const double *)b;
    return (da > db) - (da < db);
}

/**
 * @brief Calculates timing statistics for a benchmark.
 *
 * Populates fields in the Benchmark structure, including min, max,
 * mean, median, and standard deviation.
 *
 * @param b Pointer to the Benchmark structure.
 * @return 0 on success, 1 on error.
 */
static int
calculate_time_statistics(Benchmark *b)
{
    if (!b)
        return 1;

    size_t n_trials = b->benchmark_info.trials;

    double *sorted = malloc(n_trials * sizeof(double));
    if (!sorted) {
        print_error(__func__, "malloc() allocation failed", errno);
        return 1;
    }

    memcpy(sorted, b->times, n_trials * sizeof(double));
    qsort(sorted, n_trials, sizeof(double), cmp_double);

    b->result.stats.min_time_s = sorted[0];
    b->result.stats.max_time_s = sorted[n_trials - 1];
    b->result.stats.median_time_s = (n_trials % 2)
        ? sorted[n_trials / 2]
        : (sorted[n_trials / 2] + sorted[n_trials / 2 - 1]) / 2.0;

    double sum = 0.0, sum_sq = 0.0;
    for (uint16_t i = 0; i < n_trials; i++) {
        sum += sorted[i];
        sum_sq += sorted[i] * sorted[i];
    }

    double time_avg = sum / n_trials;
    b->result.stats.mean_time_s = time_avg;
    b->result.stats.std_dev_s = (n_trials > 1)
        ? sqrt((sum_sq - n_trials * time_avg * time_avg) / (n_trials - 1))
        : 0.0;

    free(sorted);
    return 0;
}

/**
 * @brief Retrieves system memory information in MB.
 */
static void
get_memory_info(Benchmark *b)
{
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        b->sys_info.ram_mb  = info.totalram  / 1024.0 / 1024.0 * info.mem_unit;
        b->sys_info.swap_mb = info.totalswap / 1024.0 / 1024.0 * info.mem_unit;
    } else {
        b->sys_info.ram_mb = b->sys_info.swap_mb = 0.0;
    }
}

/**
 * @brief Returns the peak resident set size (RSS) in MB.
 */
static void
get_peak_rss_mb(Benchmark *b)
{
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    b->result.memory_peak_mb = usage.ru_maxrss / 1024.0;
}

/**
 * @brief Retrieves CPU model information from /proc/cpuinfo.
 */
static void
get_cpu_info(Benchmark *b)
{
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (!f) {
        snprintf(b->sys_info.cpu_info, sizeof(b->sys_info.cpu_info), "unknown");
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "model name", 10) == 0) {
            char *p = strchr(line, ':');
            if (p) {
                snprintf(b->sys_info.cpu_info, sizeof(b->sys_info.cpu_info), "%s", p + 2); // skip ": "
                b->sys_info.cpu_info[strcspn(b->sys_info.cpu_info, "\n")] = 0;      // remove newline
            }
            break;
        }
    }
    fclose(f);
}

/**
 * @brief Generates an ISO-8601 formatted timestamp.
 */
static void
get_iso_timestamp(Benchmark *b)
{
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(b->sys_info.timestamp, sizeof(b->sys_info.timestamp), "%Y-%m-%dT%H:%M:%S", &tm);
}

/* ------------------------------------------------------------------------- */
/*                            Public API Implementation                      */
/* ------------------------------------------------------------------------- */

/**
 * @copydoc benchmark_init()
 */
Benchmark*
benchmark_init(const char *name,
               const char *filepath,
               const unsigned int n_trials,
               const unsigned int n_threads,
               const unsigned int algorithm_variant,
               const CSCBinaryMatrix *mat)
{
    if (!n_trials) {
        print_error(__func__, "invalid number of trials", 0);
        return NULL;
    }

    Benchmark *b = malloc(sizeof(Benchmark));
    if (!b) {
        print_error(__func__, "malloc() allocation failed", errno);
        return NULL;
    }

    // Add matrix info
    b->matrix_info.cols = mat->ncols;
    b->matrix_info.rows = mat->nrows;
    b->matrix_info.nnz = mat->nnz;
    strncpy(b->matrix_info.path, filepath, sizeof(b->matrix_info.path));
    b->matrix_info.path[sizeof(b->matrix_info.path) - 1] = '\0';

    // Add benchmark info
    b->benchmark_info.threads = n_threads;
    b->benchmark_info.trials  = n_trials;

    // Add result
    b->result.has_metrics = 0;
    b->result.algorithm_variant = algorithm_variant;
    strncpy(b->result.algorithm, name, sizeof(b->result.algorithm));
    b->result.algorithm[sizeof(b->result.algorithm) - 1] = '\0';

    b->times = NULL;

    b->times = malloc(n_trials * sizeof(double));
    if (!b->times) {
        print_error(__func__, "malloc() failed", errno);
        benchmark_free(b);
        return NULL;
    }

    return b;
}

/**
 * @copydoc benchmark_free()
 */
void
benchmark_free(Benchmark *b)
{
    if (!b) return;
    if (b->times) free(b->times);
    free(b);
}

/**
 * @copydoc benchmark_cc()
 */
int
benchmark_cc(int (*cc_func)(const CSCBinaryMatrix*, const unsigned int, const unsigned int),
             const CSCBinaryMatrix *m,
             Benchmark *b)
{
    long result;

    result = cc_func(m, b->benchmark_info.threads, b->result.algorithm_variant); /* warm-up run */

    if (result < 0)
        return 1;

    b->result.connected_components = result;

    for (unsigned int i = 0; i < b->benchmark_info.trials; i++) {
        double start_time = now_sec();
        result = cc_func(m, b->benchmark_info.threads, b->result.algorithm_variant);
        b->times[i] = now_sec() - start_time;

        if (result < 0)
            return 1;

        if (result != b->result.connected_components) {
            printf("[%s] Components between retries don't match\n", b->result.algorithm);
            return 2;
        }
    }

    return 0;
}

/**
 * @copydoc benchmark_print()
 */
void
benchmark_print(Benchmark *b)
{
    if (!b) return;

    calculate_time_statistics(b);
    
    get_iso_timestamp(b);
    get_cpu_info(b);
    get_memory_info(b);
    b->result.throughput_edges_per_sec = b->matrix_info.nnz / b->result.stats.mean_time_s;
    get_peak_rss_mb(b);

    printf("{\n");
    print_sys_info(&(b->sys_info), 2);
    printf(",\n");
    print_matrix_info(&(b->matrix_info), 2);
    printf(",\n");
    print_benchmark_info(&(b->benchmark_info), 2);
    printf(",\n");
    printf("  \"results\": [\n");
    print_result(&(b->result), 4);
    printf("\n  ]\n");
    printf("}\n");
}
