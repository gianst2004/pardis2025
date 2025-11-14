// cc_hybrid.c
#include <stdlib.h>
#include <stdint.h>
#include <omp.h>
#include "connected_components.h"

/* omp new */

// Simple sequential find with path compression
static inline uint32_t find_root(uint32_t *label, uint32_t x) {
    while (label[x] != x) {
        label[x] = label[label[x]];
        x = label[x];
    }
    return x;
}

// Union by label (non-atomic, used safely with local partitioning)
static inline void union_sets(uint32_t *label, uint32_t a, uint32_t b) {
    uint32_t ra = find_root(label, a);
    uint32_t rb = find_root(label, b);
    if (ra == rb) return;
    if (ra < rb)
        label[rb] = ra;
    else
        label[ra] = rb;
}

int cc_new(const CSCBinaryMatrix *matrix, const unsigned int n_threads) {
    if (!matrix || matrix->nrows == 0) return 0;

    const uint32_t n = matrix->nrows;
    uint32_t *label = malloc(n * sizeof(uint32_t));
    if (!label) return -1;

    for (uint32_t i = 0; i < n; i++)
        label[i] = i;

    const uint64_t threshold = 1000000ULL; // Sequential region threshold

    // Parallel phase: process columns in chunks
    #pragma omp parallel num_threads(n_threads)
    {
        int tid = omp_get_thread_num();
        int nthreads = omp_get_num_threads();

        uint64_t start = (uint64_t)tid * matrix->ncols / nthreads;
        uint64_t end   = (uint64_t)(tid + 1) * matrix->ncols / nthreads;

        for (uint64_t col = start; col < end; col++) {
            uint64_t degree = matrix->col_ptr[col + 1] - matrix->col_ptr[col];
            if (degree == 0) continue;

            // Sequential processing for small neighborhoods
            if (degree < threshold / n) {
                for (uint64_t j = matrix->col_ptr[col];
                     j < matrix->col_ptr[col + 1]; j++) {
                    uint32_t row = matrix->row_idx[j];
                    if (row < n)
                        union_sets(label, row, col);
                }
            } else {
                // Parallel-safe region: only use local updates
                for (uint64_t j = matrix->col_ptr[col];
                     j < matrix->col_ptr[col + 1]; j++) {
                    uint32_t row = matrix->row_idx[j];
                    if (row < n) {
                        uint32_t ra = find_root(label, row);
                        uint32_t rb = find_root(label, col);
                        if (ra != rb) {
                            uint32_t min = ra < rb ? ra : rb;
                            uint32_t max = ra ^ rb ^ min;
                            #pragma omp atomic write
                            label[max] = min;
                        }
                    }
                }
            }
        }
    }

    // Path compression pass
    #pragma omp parallel for num_threads(n_threads) schedule(static)
    for (uint32_t i = 0; i < n; i++)
        label[i] = find_root(label, i);

    // Count unique roots
    uint32_t *flags = calloc(n, sizeof(uint32_t));
    if (!flags) {
        free(label);
        return -1;
    }

    #pragma omp parallel for num_threads(n_threads) schedule(static)
    for (uint32_t i = 0; i < n; i++) {
        uint32_t root = label[i];
        #pragma omp atomic write
        flags[root] = 1;
    }

    uint32_t count = 0;
    for (uint32_t i = 0; i < n; i++)
        if (flags[i]) count++;

    free(flags);
    free(label);
    return (int)count;
}

#include <stdlib.h>
#include <omp.h>

#include "connected_components.h"


static int swap_min(uint32_t *label, uint32_t i, uint32_t j) {
    if (label[i] == label[j])
        return 0;
    
    if (label[i] < label[j]) {
        label[j] = label[i];
    } else {
        label[i] = label[j];
    }
    return 1;
}

static int cmp_uint32(const void *a, const void *b) {
    uint32_t x = *(uint32_t*)a;
    uint32_t y = *(uint32_t*)b;
    if (x < y) return -1;
    if (x > y) return 1;
    return 0;
}

int cc_old(const CSCBinaryMatrix *matrix, const int n_threads __attribute__((unused))) {

    uint32_t *label=malloc(sizeof(uint32_t)*matrix->nrows);
    for(size_t i=0;i<matrix->nrows;i++){
        label[i]=i;
    }

    uint8_t finished;
    do {

        
        finished = 1;
        
        #pragma  omp parallel 
        {
            uint8_t changed = 0;
            
            #pragma omp for
            for (size_t i = 0; i < matrix->ncols; i++) {
                for(uint32_t j = matrix->col_ptr[i]; j < matrix->col_ptr[i+1]; j++) {
                    uint32_t r, c;

                    c = i;
                    r = matrix->row_idx[j];

                    if (swap_min(label, c, r)) {
                        changed = 1;
                    }
                }
            }

            if (changed) {
                    #pragma omp atomic write
                    finished = 0;
            }
        }
    } while (!finished);

    qsort(label, matrix->nrows, sizeof(uint32_t), cmp_uint32);

    uint32_t uniqueCount = 0;
    for (size_t i = 0; i < matrix->nrows; i++) {
        if (i == 0 || label[i] != label[i-1])
            uniqueCount++;
    }

    free(label);

    return (int) uniqueCount;
}

int cc_openmp(const CSCBinaryMatrix *matrix, const unsigned int n_threads, const unsigned int algorithm_variant __attribute__((unused))) {
    switch (algorithm_variant) {
    case 0:
        return cc_old(matrix, n_threads);

    case 1:
        return cc_new(matrix, n_threads);
    
    default:
        break;
    }

    return -1;
}