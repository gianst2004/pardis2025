/**
 * @file runner.c
 * @brief Unified benchmark runner
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

#include "args.h"
#include "error.h"
#include "json.h"

#define MAX_BUFFER 65536
#define MAX_RESULTS 4

const char *program_name = "runner";

typedef struct {
    char *name;
    char *binary_path;
    char *output;
    BenchmarkData data;
    int success;
} BenchmarkResult;

/**
 * @brief Executes a single benchmark binary and captures its output.
 */
static int
run_benchmark(const char *binary, const char *matrix_file,
              int threads, int trials, int algorithm_variant, char **output)
{
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        print_error(__func__, "pipe() failed", errno);
        return -1;
    }

    pid_t pid = fork();
    if (pid == -1) {
        print_error(__func__, "fork() failed", errno);
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return -1;
    }

    if (pid == 0) {
        // Child process
        close(pipe_fd[0]);
        dup2(pipe_fd[1], STDOUT_FILENO);
        dup2(pipe_fd[1], STDERR_FILENO);
        close(pipe_fd[1]);

        char threads_str[16], trials_str[16], variant_str[16];
        snprintf(threads_str, sizeof(threads_str), "%d", threads);
        snprintf(trials_str, sizeof(trials_str), "%d", trials);
        snprintf(variant_str, sizeof(variant_str), "%u", algorithm_variant);

        execl(binary, binary, "-t", threads_str, "-n", trials_str, "-v", variant_str, matrix_file, NULL);
        exit(1);
    }

    // Parent process
    close(pipe_fd[1]);

    *output = malloc(MAX_BUFFER);
    if (!*output) {
        print_error(__func__, "malloc() failed", errno);
        close(pipe_fd[0]);
        return -1;
    }

    size_t total = 0;
    ssize_t n;
    while ((n = read(pipe_fd[0], *output + total, MAX_BUFFER - total - 1)) > 0) {
        total += n;
        if (total >= MAX_BUFFER - 1) {
            fprintf(stderr, "[%s] Warning: output truncated (>%d bytes)\n",
                    binary, MAX_BUFFER);
            break;
        }
    }
    (*output)[total] = '\0';

    close(pipe_fd[0]);

    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        fprintf(stderr, "[%s] Terminated by signal %d (%s)\n",
                binary, WTERMSIG(status), strsignal(WTERMSIG(status)));
        return 128 + WTERMSIG(status);
    } else {
        fprintf(stderr, "[%s] Unknown termination cause\n", binary);
        return -1;
    }
}

/**
 * @brief Find sequential baseline time from results.
 */
static double
find_sequential_time(BenchmarkResult *results, int count)
{
    for (int i = 0; i < count; i++) {
        if (!results[i].success || !results[i].data.valid) continue;
        
        if (strcmp(results[i].data.result.algorithm, "Sequential") == 0) {
            return results[i].data.result.stats.mean_time_s;
        }
    }
    return -1.0;
}

/**
 * @brief Compute speedup and efficiency for all results.
 */
static void
compute_performance_metrics(BenchmarkResult *results, int count, int threads)
{
    double sequential_time = find_sequential_time(results, count);
    if (sequential_time <= 0) return;
    
    for (int i = 0; i < count; i++) {
        if (!results[i].success || !results[i].data.valid) continue;
        
        double mean_time = results[i].data.result.stats.mean_time_s;
        if (mean_time > 0) {
            results[i].data.result.speedup = sequential_time / mean_time;
            results[i].data.result.efficiency = results[i].data.result.speedup / threads;
            results[i].data.result.has_metrics = 1;
        }
    }
}

/**
 * @brief Prints combined benchmark results as JSON.
 */
static void
print_combined_results(BenchmarkResult *results, int count)
{
    // Find first valid result for metadata
    BenchmarkData *first = NULL;
    for (int i = 0; i < count; i++) {
        if (results[i].success && results[i].data.valid) {
            first = &results[i].data;
            break;
        }
    }
    
    if (!first) {
        print_error(__func__, "No valid benchmark results found", 0);
        return;
    }
    
    // Print combined JSON
    printf("{\n");
    print_sys_info(&first->sys_info, 2);
    printf(",\n");
    print_matrix_info(&first->matrix_info, 2);
    printf(",\n");
    print_benchmark_info(&first->benchmark_info, 2);
    printf(",\n");
    
    printf("  \"results\": [\n");
    
    int first_result = 1;
    for (int i = 0; i < count; i++) {
        if (!results[i].success || !results[i].data.valid) continue;
        
        if (!first_result) printf(",\n");
        print_result(&results[i].data.result, 4);
        first_result = 0;
    }
    
    printf("\n  ]\n");
    printf("}\n");
}

int
main(int argc, char *argv[])
{
    set_program_name(argv[0]);

    char *matrix_file = NULL;
    unsigned int threads;
    unsigned int trials;
    unsigned int algorithm_variant;

    int parse_status = parseargs(argc, argv, &threads, &trials, &algorithm_variant, &matrix_file);
    if (parse_status != 0) return parse_status == -1 ? 0 : 1;

    if (threads <= 0 || trials <= 0) {
        print_error(__func__, "threads and trials must be positive integers", 0);
        return 1;
    }

    if (access(matrix_file, R_OK) != 0) {
        char err[MAX_BUFFER];
        snprintf(err, sizeof(err), "Error: cannot access matrix file '%s': %s",
                matrix_file, strerror(errno));
        print_error(__func__, err, errno);
        return 1;
    }

    BenchmarkResult results[MAX_RESULTS] = {
        {.name = "Sequential", .binary_path = "bin/connected_components_sequential"},
        {.name = "OpenMP",     .binary_path = "bin/connected_components_openmp"},
        {.name = "Pthreads",   .binary_path = "bin/connected_components_pthreads"},
        {.name = "Cilk",       .binary_path = "bin/connected_components_cilk"}
    };

    fprintf(stderr, "Running benchmarks for: %s\n", matrix_file);
    fprintf(stderr, "Threads: %d, Trials: %d\n\n", threads, trials);

    for (int i = 0; i < MAX_RESULTS; i++) {
        if (access(results[i].binary_path, X_OK) != 0) {
            fprintf(stderr, "[%s] Binary not found or not executable: %s\n",
                    results[i].name, results[i].binary_path);
            results[i].success = 0;
            results[i].output = NULL;
            continue;
        }

        fprintf(stderr, "[%s] Running...\n", results[i].name);
        
        int ret = run_benchmark(results[i].binary_path, matrix_file,
                               threads, trials, algorithm_variant, &results[i].output);
        
        if (ret == 0) {
            // Parse the output
            if (parse_benchmark_data(results[i].output, &results[i].data)) {
                results[i].success = 1;
                fprintf(stderr, "[%s] Completed successfully\n", results[i].name);
            } else {
                results[i].success = 0;
                fprintf(stderr, "[%s] Failed to parse output\n", results[i].name);
            }
        } else {
            results[i].success = 0;
            fprintf(stderr, "[%s] Failed with exit code %d\n", results[i].name, ret);
            if (results[i].output && strlen(results[i].output) > 0) {
                fprintf(stderr, "[%s] Output:\n%s\n", results[i].name, results[i].output);
            }
        }
    }

    // Compute speedup and efficiency
    compute_performance_metrics(results, MAX_RESULTS, threads);

    fprintf(stderr, "\n");
    print_combined_results(results, MAX_RESULTS);

    for (int i = 0; i < MAX_RESULTS; i++) {
        if (results[i].output) free(results[i].output);
    }

    return 0;
}
