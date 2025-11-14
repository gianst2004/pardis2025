#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <string.h>
#include "connected_components.h"

// Simplified find with path halving (more concurrent-friendly)
static inline uint32_t find_root(uint32_t *label, uint32_t x) {
    while (label[x] != x) {
        uint32_t next = label[x];
        label[x] = label[next];  // Path halving
        x = next;
    }
    return x;
}

// Lock-free union with limited retries
static int union_sets(uint32_t *label, uint32_t x, uint32_t y) {
    int retries = 0;
    const int MAX_RETRIES = 10;
    
    while (retries < MAX_RETRIES) {
        x = find_root(label, x);
        y = find_root(label, y);
        
        if (x == y) return 0;
        
        // Always link smaller to larger
        if (x > y) {
            uint32_t temp = x;
            x = y;
            y = temp;
        }
        
        // Try atomic update
        uint32_t expected = y;
        if (__atomic_compare_exchange_n(&label[y], &expected, x, 
                                        0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
            return 1;
        }
        
        retries++;
    }
    
    // Fallback: optimistic write
    if (label[y] > label[x]) {
        label[y] = x;
        return 1;
    }
    
    return 0;
}

// Thread data structure
typedef struct {
    const CSCBinaryMatrix *matrix;
    uint32_t *label;
    uint32_t start_col;
    uint32_t end_col;
    int *local_changed;
    int thread_id;
} thread_data_t;

// Thread function for processing edges
static void* process_edges_thread(void *arg) {
    thread_data_t *data = (thread_data_t*)arg;
    
    for (uint32_t col = data->start_col; col < data->end_col; col++) {
        for (uint32_t idx = data->matrix->col_ptr[col]; 
             idx < data->matrix->col_ptr[col + 1]; idx++) {
            uint32_t row = data->matrix->row_idx[idx];
            
            if (row < data->matrix->nrows && col < data->matrix->nrows) {
                if (union_sets(data->label, row, col)) {
                    *data->local_changed = 1;
                }
            }
        }
    }
    
    return NULL;
}

// Thread function for initialization
typedef struct {
    uint32_t *label;
    uint32_t start;
    uint32_t end;
} init_data_t;

static void* init_labels_thread(void *arg) {
    init_data_t *data = (init_data_t*)arg;
    for (uint32_t i = data->start; i < data->end; i++) {
        data->label[i] = i;
    }
    return NULL;
}

// Thread function for path compression
static void* compress_paths_thread(void *arg) {
    init_data_t *data = (init_data_t*)arg;
    for (uint32_t i = data->start; i < data->end; i++) {
        find_root(data->label, i);
    }
    return NULL;
}

// Thread function for counting roots
typedef struct {
    uint32_t *label;
    uint32_t *root_flags;
    uint32_t start;
    uint32_t end;
    uint32_t nrows;
} count_data_t;

static void* count_roots_thread(void *arg) {
    count_data_t *data = (count_data_t*)arg;
    for (uint32_t i = data->start; i < data->end; i++) {
        uint32_t root = find_root(data->label, i);
        if (root < data->nrows) {
            __atomic_store_n(&data->root_flags[root], 1, __ATOMIC_RELAXED);
        }
    }
    return NULL;
}

int cc_pthreads(const CSCBinaryMatrix *matrix, const unsigned int n_threads __attribute__((unused)), const unsigned int algorithm_variant __attribute__((unused))) {
    if (!matrix || matrix->nrows == 0) return 0;
    
    // Get number of threads (default to 4, can be set via environment)
    int num_threads = 4;
    char *env_threads = getenv("OMP_NUM_THREADS");
    if (env_threads) {
        num_threads = atoi(env_threads);
        if (num_threads <= 0) num_threads = 4;
    }
    
    uint32_t *label = (uint32_t*)malloc(sizeof(uint32_t) * matrix->nrows);
    if (!label) return -1;
    
    pthread_t *threads = (pthread_t*)malloc(sizeof(pthread_t) * num_threads);
    if (!threads) {
        free(label);
        return -1;
    }
    
    // Parallel initialization
    init_data_t *init_data = (init_data_t*)malloc(sizeof(init_data_t) * num_threads);
    if (!init_data) {
        free(threads);
        free(label);
        return -1;
    }
    
    uint32_t chunk_size = (matrix->nrows + num_threads - 1) / num_threads;
    for (int t = 0; t < num_threads; t++) {
        init_data[t].label = label;
        init_data[t].start = t * chunk_size;
        init_data[t].end = (t + 1) * chunk_size;
        if (init_data[t].end > matrix->nrows) {
            init_data[t].end = matrix->nrows;
        }
        pthread_create(&threads[t], NULL, init_labels_thread, &init_data[t]);
    }
    
    for (int t = 0; t < num_threads; t++) {
        pthread_join(threads[t], NULL);
    }
    free(init_data);
    
    // Main iteration loop
    int iteration = 0;
    const int MAX_ITERATIONS = 100;
    int global_changed = 1;
    
    thread_data_t *thread_data = (thread_data_t*)malloc(sizeof(thread_data_t) * num_threads);
    int *local_changed = (int*)malloc(sizeof(int) * num_threads);
    if (!thread_data || !local_changed) {
        free(threads);
        free(label);
        if (thread_data) free(thread_data);
        if (local_changed) free(local_changed);
        return -1;
    }
    
    uint32_t cols_per_thread = (matrix->ncols + num_threads - 1) / num_threads;
    
    while (global_changed && iteration < MAX_ITERATIONS) {
        global_changed = 0;
        memset(local_changed, 0, sizeof(int) * num_threads);
        
        // Process edges in parallel
        for (int t = 0; t < num_threads; t++) {
            thread_data[t].matrix = matrix;
            thread_data[t].label = label;
            thread_data[t].start_col = t * cols_per_thread;
            thread_data[t].end_col = (t + 1) * cols_per_thread;
            if (thread_data[t].end_col > matrix->ncols) {
                thread_data[t].end_col = matrix->ncols;
            }
            thread_data[t].local_changed = &local_changed[t];
            thread_data[t].thread_id = t;
            
            pthread_create(&threads[t], NULL, process_edges_thread, &thread_data[t]);
        }
        
        for (int t = 0; t < num_threads; t++) {
            pthread_join(threads[t], NULL);
            if (local_changed[t]) {
                global_changed = 1;
            }
        }
        
        iteration++;
    }
    
    free(thread_data);
    free(local_changed);
    
    // Final path compression pass (parallel)
    init_data = (init_data_t*)malloc(sizeof(init_data_t) * num_threads);
    if (!init_data) {
        free(threads);
        free(label);
        return -1;
    }
    
    for (int t = 0; t < num_threads; t++) {
        init_data[t].label = label;
        init_data[t].start = t * chunk_size;
        init_data[t].end = (t + 1) * chunk_size;
        if (init_data[t].end > matrix->nrows) {
            init_data[t].end = matrix->nrows;
        }
        pthread_create(&threads[t], NULL, compress_paths_thread, &init_data[t]);
    }
    
    for (int t = 0; t < num_threads; t++) {
        pthread_join(threads[t], NULL);
    }
    free(init_data);
    
    // Count unique roots (parallel)
    uint32_t *root_flags = (uint32_t*)calloc(matrix->nrows, sizeof(uint32_t));
    if (!root_flags) {
        free(threads);
        free(label);
        return -1;
    }
    
    count_data_t *count_data = (count_data_t*)malloc(sizeof(count_data_t) * num_threads);
    if (!count_data) {
        free(root_flags);
        free(threads);
        free(label);
        return -1;
    }
    
    for (int t = 0; t < num_threads; t++) {
        count_data[t].label = label;
        count_data[t].root_flags = root_flags;
        count_data[t].start = t * chunk_size;
        count_data[t].end = (t + 1) * chunk_size;
        if (count_data[t].end > matrix->nrows) {
            count_data[t].end = matrix->nrows;
        }
        count_data[t].nrows = matrix->nrows;
        pthread_create(&threads[t], NULL, count_roots_thread, &count_data[t]);
    }
    
    for (int t = 0; t < num_threads; t++) {
        pthread_join(threads[t], NULL);
    }
    free(count_data);
    
    // Count components (sequential - this is fast)
    uint32_t count = 0;
    for (uint32_t i = 0; i < matrix->nrows; i++) {
        if (root_flags[i] == 1) {
            count++;
        }
    }
    
    free(root_flags);
    free(threads);
    free(label);
    
    return (int)count;
}