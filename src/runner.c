/**
 * @file benchmark_runner.c
 * @brief Unified benchmark runner for all connected components implementations.
 * 
 * This program executes all four algorithm implementations (sequential, OpenMP,
 * Pthreads, Cilk) and combines their results into a single JSON output.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

#include "error.h"
#include "args.h"

#define MAX_BUFFER 65536
#define MAX_RESULTS 4

const char *program_name = "runner";

typedef struct {
    char *name;
    char *binary_path;
    char *output;
    int success;
} BenchmarkResult;

/**
 * @brief Indents stdout by n characters.
 */
static void indent(int n) {
    for (int i = 0; i < n; i++) putchar(' ');
}

/**
 * @brief Executes a single benchmark binary and captures its output.
 */
static int
run_benchmark(const char *binary, const char *matrix_file,
              int threads, int trials, char **output)
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

        char threads_str[16], trials_str[16];
        snprintf(threads_str, sizeof(threads_str), "%d", threads);
        snprintf(trials_str, sizeof(trials_str), "%d", trials);

        execl(binary, binary, "-t", threads_str, "-n", trials_str, matrix_file, NULL);
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
        return 128 + WTERMSIG(status); // standard convention
    } else {
        fprintf(stderr, "[%s] Unknown termination cause\n", binary);
        return -1;
    }
}

/**
 * @brief Extracts a JSON object or array by finding matching braces/brackets.
 */
static char*
extract_json_block(const char *json, const char *key, char open, char close)
{
    char search[128];
    snprintf(search, sizeof(search), "\"%s\":", key);
    
    const char *start = strstr(json, search);
    if (!start) return NULL;

    start = strchr(start, open);
    if (!start) return NULL;

    int count = 1;
    const char *p = start + 1;
    
    while (*p && count > 0) {
        if (*p == open) count++;
        else if (*p == close) count--;
        p++;
    }

    if (count != 0) {
        print_error(__func__, "unbalanced braces in JSON block", 0);
        return NULL;
    }

    size_t len = p - start;
    char *result = malloc(len + 1);
    if (!result) {
        print_error(__func__, "malloc() failed", errno);
        return NULL;
    }

    memcpy(result, start, len);
    result[len] = '\0';
    return result;
}

/**
 * @brief Extracts a single result object from the results array.
 */
static char*
extract_single_result(const char *results_array)
{
    const char *start = strchr(results_array, '{');
    if (!start) return NULL;

    int brace_count = 1;
    const char *p = start + 1;
    
    while (*p && brace_count > 0) {
        if (*p == '{') brace_count++;
        else if (*p == '}') brace_count--;
        p++;
    }

    if (brace_count != 0) {
        print_error(__func__, "unbalanced braces in JSON block", 0);
        return NULL;
    }

    size_t len = p - start;
    char *result = malloc(len + 1);
    if (!result) {
        print_error(__func__, "malloc() failed", errno);
        return NULL;
    }

    memcpy(result, start, len);
    result[len] = '\0';
    return result;
}

/**
 * @brief Extracts the results array from individual benchmark JSON output.
 */
static char*
extract_results_array(const char *json_output)
{
    return extract_json_block(json_output, "results", '[', ']');
}

/**
 * @brief Prints combined benchmark results as JSON.
 */
static void
print_combined_results(BenchmarkResult *results, int count)
{
    char *sys_info = NULL;
    char *matrix_info = NULL;
    char *benchmark_info = NULL;

    for (int i = 0; i < count; i++) {
        if (!results[i].success || !results[i].output) continue;
        sys_info = extract_json_block(results[i].output, "sys_info", '{', '}');
        matrix_info = extract_json_block(results[i].output, "matrix_info", '{', '}');
        benchmark_info = extract_json_block(results[i].output, "benchmark_info", '{', '}');
        if (sys_info && matrix_info && benchmark_info)
            break;
    }

    if (!sys_info || !matrix_info || !benchmark_info) {
        print_error(__func__, "JSON file corrupted", 0);
        free(sys_info); free(matrix_info); free(benchmark_info);
        return;
    }

    indent(0); printf("{\n");
    indent(2); printf("\"sys_info\": %s,\n", sys_info);
    indent(2); printf("\"matrix_info\": %s,\n", matrix_info);
    indent(2); printf("\"benchmark_info\": %s,\n", benchmark_info);
    indent(2); printf("\"results\": [\n");

    int first = 1;
    for (int i = 0; i < count; i++) {
        if (!results[i].success) continue;

        char *results_array = extract_results_array(results[i].output);
        if (!results_array) continue;

        char *single_result = extract_single_result(results_array);
        free(results_array);
        if (!single_result) continue;
        
        if (!first) printf(",\n");
        indent(4); printf("%s", single_result);
        free(single_result);
        first = 0;
    }

    printf("\n");
    indent(2); printf("]\n");
    indent(0); printf("}\n");

    free(sys_info);
    free(matrix_info);
    free(benchmark_info);
}

int
main(int argc, char *argv[])
{
    set_program_name(argv[0]);

    char *matrix_file = NULL;
    int threads;
    int trials;

    int parse_status = parseargs(argc, argv, &threads, &trials, &matrix_file);
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
                               threads, trials, &results[i].output);
        
        if (ret == 0) {
            results[i].success = 1;
            fprintf(stderr, "[%s] Completed successfully\n", results[i].name);
        } else {
            results[i].success = 0;
            fprintf(stderr, "[%s] Failed with exit code %d\n", results[i].name, ret);
            if (results[i].output && strlen(results[i].output) > 0) {
                fprintf(stderr, "[%s] Output:\n%s\n", results[i].name, results[i].output);
            }
        }
    }

    fprintf(stderr, "\n");
    print_combined_results(results, MAX_RESULTS);

    for (int i = 0; i < MAX_RESULTS; i++) {
        if (results[i].output) free(results[i].output);
    }

    return 0;
}
