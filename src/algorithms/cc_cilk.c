#include <stdlib.h>
#include <stdint.h>
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
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

int cc_count_parallel_cilk(const CSCBinaryMatrix *matrix) {
    if (!matrix || matrix->nrows == 0) return 0;
    
    uint32_t *label = (uint32_t*)malloc(sizeof(uint32_t) * matrix->nrows);
    if (!label) return -1;
    
    // Parallel initialization
    cilk_for (uint32_t i = 0; i < matrix->nrows; i++) {
        label[i] = i;
    }
    
    int iteration = 0;
    const int MAX_ITERATIONS = 100;
    int changed = 1;
    
    while (changed && iteration < MAX_ITERATIONS) {
        changed = 0;
        
        // Process edges in parallel
        cilk_for (uint32_t col = 0; col < matrix->ncols; col++) {
            for (uint32_t idx = matrix->col_ptr[col]; 
                 idx < matrix->col_ptr[col + 1]; idx++) {
                uint32_t row = matrix->row_idx[idx];
                
                if (row < matrix->nrows && col < matrix->nrows) {
                    if (union_sets(label, row, col)) {
                        changed = 1;
                    }
                }
            }
        }
        
        iteration++;
    }
    
    // Final path compression pass
    cilk_for (uint32_t i = 0; i < matrix->nrows; i++) {
        find_root(label, i);
    }
    
    // Count unique roots using atomic flags
    uint32_t *root_flags = (uint32_t*)calloc(matrix->nrows, sizeof(uint32_t));
    if (!root_flags) {
        free(label);
        return -1;
    }
    
    cilk_for (uint32_t i = 0; i < matrix->nrows; i++) {
        uint32_t root = find_root(label, i);
        if (root < matrix->nrows) {
            __atomic_store_n(&root_flags[root], 1, __ATOMIC_RELAXED);
        }
    }
    
    // Count components
    uint32_t count = 0;
    for (uint32_t i = 0; i < matrix->nrows; i++) {
        if (root_flags[i] == 1) {
            count++;
        }
    }
    
    free(root_flags);
    free(label);
    
    return (int)count;
}

