#include <stdio.h>
#include <time.h>
#include <sys/time.h>

#include "algorithms/connected_components.h"
#include "core/matrix.h"
#include "utils/error.h"

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

int main(int argc, char *argv[]) {

    CSCBinaryMatrix *matrix;

    set_program_name(argv[0]);

    if (argc != 2) {
        print_error(__func__, "invalid arguments", 0);
        return 1;
    }
    
    matrix = csc_load_matrix(argv[1], "Problem", "A");
    if (!matrix)
        return 1;

  
    clock_t start, end;
    struct timeval s, e;
    
    const int retries = 100;

    double time = 0.0;
    int cycles = 0;

    int num_components = 0;

    for (int i = 0; i < retries; i++) {

        start = clock();
        gettimeofday(&s, NULL);

        #if defined(USE_OPENMP)
        num_components = cc_count_parallel_omp(matrix);
        #elif defined(USE_PTHREADS)
        num_components = cc_count_parallel_pthreads(matrix);
        #elif defined(USE_CILK)
        num_components = cc_count_parallel_cilk(matrix);       
        #elif defined(USE_SEQUENTIAL)
        num_components = cc_count_sequential(matrix);
        #endif

        gettimeofday(&e, NULL);
        end = clock();

        time += (e.tv_sec - s.tv_sec) + 
                (e.tv_usec - s.tv_usec) / 1e6;
        cycles += ((double)(end-start));
    }
    printf("Number of connected components: %u, cycles: %d, average time needed %lf\n", num_components, cycles/retries,time/retries);
    csc_free_matrix(matrix);
    return 0;
}
