/**
 * @file main.c
 * @brief Entry point for the connected components benchmark program.
 *
 * This implementation was developed for the purposes of the class:
 * Parallel and Distributed Systems,
 * Department of Electrical and Computer Engineering,
 * Aristotle University of Thessaloniki.
 *
 * Loads a sparse binary matrix in CSC format, based on the selected
 * connected components implementation (sequential or parallel),
 * runs a benchmark, and prints the statistics. The implementations
 * are selected through a series of definitions (through compiler flags).
 *
 * Supported implementations:
 * - USE_SEQUENTIAL
 * - USE_OPENMP
 * - USE_PTHREADS
 * - USE_CILK
 *
 * Usage: ./connected_components [-t n_threads] [-n n_trials] ./data_filepath
 */

#include "connected_components.h"
#include "matrix.h"
#include "error.h"
#include "benchmark.h"
#include "args.h"

#if defined(USE_OPENMP)
    #define IMPLEMENTATION_NAME "OpenMP"
#elif defined(USE_PTHREADS)
    #define IMPLEMENTATION_NAME "Pthreads"
#elif defined(USE_CILK)
    #define IMPLEMENTATION_NAME "OpenCilk"
#elif defined(USE_SEQUENTIAL)
    #define IMPLEMENTATION_NAME "Sequential"
#else
    #error "No implementation selected! Define USE_SEQUENTIAL, USE_OPENMP, USE_PTHREADS, or USE_CILK"
#endif

const char *program_name = "connected_components";

int
main(int argc, char *argv[])
{
    CSCBinaryMatrix *matrix;
    Benchmark *benchmark = NULL;
    char *filepath;
    unsigned int n_trials;
    unsigned int n_threads;
    unsigned int algorithm_variant;
    int ret = 0;
    int (*cc_func)(const CSCBinaryMatrix*, const unsigned int, const unsigned int);

    /* Initialize program name for error reporting */
    set_program_name(argv[0]);

    /* Parse command line arguments */
    if (parseargs(argc, argv, &n_threads, &n_trials, &algorithm_variant, &filepath)) {
        return 1;
    }
    
    /* Load the sparse matrix */
    matrix = csc_load_matrix(filepath, "Problem", "A");
    if (!matrix)
        return 1;

    /* Initialize benchmarking structure */
    benchmark = benchmark_init(IMPLEMENTATION_NAME, filepath, n_trials, n_threads, algorithm_variant, matrix);
    if (!benchmark) {
        csc_free_matrix(matrix);
        return 1;
    }

    /* Implementation is selected by the preproccesor.
     * (definitions made through compiler flags)
     */
    #if defined(USE_OPENMP)
    cc_func = cc_openmp;
    #elif defined(USE_PTHREADS)
    cc_func = cc_pthreads;
    #elif defined(USE_CILK)
    cc_func = cc_cilk;
    #elif defined(USE_SEQUENTIAL)
    cc_func = cc_sequential;
    #endif

    /* Actually run the benchmark */
    ret = benchmark_cc(cc_func, matrix, benchmark);

    benchmark_print(benchmark);

    /* Cleanup */
    benchmark_free(benchmark);
    csc_free_matrix(matrix);
    
    return ret;
}
