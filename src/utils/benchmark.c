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

    double *sorted = malloc(b->n_trials * sizeof(double));
    if (!sorted) {
        print_error(__func__, "malloc() allocation failed", errno);
        return 1;
    }

    memcpy(sorted, b->times, b->n_trials * sizeof(double));
    qsort(sorted, b->n_trials, sizeof(double), cmp_double);

    b->time_min = sorted[0];
    b->time_max = sorted[b->n_trials - 1];
    b->time_median = (b->n_trials % 2)
        ? sorted[b->n_trials / 2]
        : (sorted[b->n_trials / 2] + sorted[b->n_trials / 2 - 1]) / 2.0;

    double sum = 0.0, sum_sq = 0.0;
    for (uint16_t i = 0; i < b->n_trials; i++) {
        sum += sorted[i];
        sum_sq += sorted[i] * sorted[i];
    }

    b->time_avg = sum / b->n_trials;
    b->time_stddev = (b->n_trials > 1)
        ? sqrt((sum_sq - b->n_trials * b->time_avg * b->time_avg) / (b->n_trials - 1))
        : 0.0;

    free(sorted);
    return 0;
}

/**
 * @brief Retrieves system memory information in MB.
 */
static void
get_memory_info(double *ram_mb, double *swap_mb)
{
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        *ram_mb  = info.totalram  / 1024.0 / 1024.0 * info.mem_unit;
        *swap_mb = info.totalswap / 1024.0 / 1024.0 * info.mem_unit;
    } else {
        *ram_mb = *swap_mb = 0.0;
    }
}

/**
 * @brief Returns the peak resident set size (RSS) in MB.
 */
static double
get_peak_rss_mb(void)
{
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_maxrss / 1024.0; // ru_maxrss in KB on Linux
}

/**
 * @brief Retrieves CPU model information from /proc/cpuinfo.
 */
static void
get_cpu_info(char *buf, size_t size)
{
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (!f) {
        snprintf(buf, size, "unknown");
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "model name", 10) == 0) {
            char *p = strchr(line, ':');
            if (p) {
                snprintf(buf, size, "%s", p + 2); // skip ": "
                buf[strcspn(buf, "\n")] = 0;      // remove newline
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
get_iso_timestamp(char *buf, size_t size)
{
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(buf, size, "%Y-%m-%dT%H:%M:%S", &tm);
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

    b->n_threads = n_threads;
    b->n_trials  = n_trials;
    b->matrix_rows = mat->nrows;
    b->matrix_cols = mat->ncols;
    b->matrix_nnz  = mat->nnz;
    b->algorithm_name = NULL;
    b->dataset_filepath = NULL;
    b->times = NULL;

    b->algorithm_name = strdup(name);
    if (!b->algorithm_name) {
        print_error(__func__, "strdup() failed", errno);
        benchmark_free(b);
        return NULL;
    }

    b->dataset_filepath = strdup(filepath);
    if (!b->dataset_filepath) {
        print_error(__func__, "strdup() failed", errno);
        benchmark_free(b);
        return NULL;
    }

    b->times = malloc(b->n_trials * sizeof(double));
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
    free(b->algorithm_name);
    free(b->dataset_filepath);
    free(b->times);
    free(b);
}

/**
 * @copydoc benchmark_cc()
 */
int
benchmark_cc(int (*cc_func)(const CSCBinaryMatrix*, const int),
                 const CSCBinaryMatrix *m,
                 Benchmark *b)
{
    b->connected_components = cc_func(m, b->n_threads); /* warm-up run */

    for (int i = 0; i < b->n_trials; i++) {
        double start_time = now_sec();
        int result = cc_func(m, b->n_threads);
        b->times[i] = now_sec() - start_time;

        if (result < 0)
            return 1;

        if (result != b->connected_components) {
            printf("[%s] Components between retries don't match\n", b->algorithm_name);
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

    char timestamp[32], cpuinfo[128];
    double ram_mb, swap_mb, throughput, mem_peak;

    calculate_time_statistics(b);
    get_iso_timestamp(timestamp, sizeof(timestamp));
    get_cpu_info(cpuinfo, sizeof(cpuinfo));
    get_memory_info(&ram_mb, &swap_mb);
    throughput = b->matrix_nnz / b->time_avg;
    mem_peak = get_peak_rss_mb();

    printf("{\n");
    printf("  \"sys_info\": {\n");
    printf("    \"timestamp\": \"%s\",\n", timestamp);
    printf("    \"cpu_info\": \"%s\",\n", cpuinfo);
    printf("    \"ram_mb\": %.2f,\n", ram_mb);
    printf("    \"swap_mb\": %.2f\n", swap_mb);
    printf("  },\n");
    printf("  \"matrix_info\": {\n");
    printf("    \"path\": \"%s\",\n", b->dataset_filepath);
    printf("    \"rows\": %u,\n", b->matrix_rows);
    printf("    \"cols\": %u,\n", b->matrix_cols);
    printf("    \"nnz\": %u\n", b->matrix_nnz);
    printf("  },\n");
    printf("  \"benchmark_info\": {\n");
    printf("    \"threads\": %u,\n", b->n_threads);
    printf("    \"trials\": %u\n", b->n_trials);
    printf("  },\n");
    printf("  \"results\": [\n");
    printf("    {\n");
    printf("      \"algorithm\": \"%s\",\n", b->algorithm_name);
    printf("      \"connected_components\": %d,\n", b->connected_components);
    printf("      \"statistics\": {\n");
    printf("        \"mean_time_s\": %.6f,\n", b->time_avg);
    printf("        \"std_dev_s\": %.6f,\n", b->time_stddev);
    printf("        \"median_time_s\": %.6f,\n", b->time_median);
    printf("        \"min_time_s\": %.6f,\n", b->time_min);
    printf("        \"max_time_s\": %.6f\n", b->time_max);
    printf("      },\n");
    printf("      \"throughput_edges_per_sec\": %.2f,\n", throughput);
    printf("      \"memory_peak_mb\": %.2f\n", mem_peak);
    printf("    }\n");
    printf("  ]\n");
    printf("}\n");
}
