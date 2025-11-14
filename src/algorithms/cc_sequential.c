#include <stdlib.h>
#include <errno.h>
#include "connected_components.h"
#include "error.h"

/* Union find */

// Find with path compression
static inline uint32_t
find_root(uint32_t *label, uint32_t i)
{
    uint32_t root = i;
    // Find root
    while (label[root] != root) {
        root = label[root];
    }
    // Path compression
    uint32_t curr = i;
    while (curr != root) {
        uint32_t next = label[curr];
        label[curr] = root;
        curr = next;
    }
    return root;
}

// Union operation - always attach smaller id to larger to reduce tree depth
static inline int
union_nodes(uint32_t *label, uint32_t i, uint32_t j)
{
    uint32_t root_i = find_root(label, i);
    uint32_t root_j = find_root(label, j);
    
    if (root_i == root_j)
        return 0;
    
    // Attach higher index to lower index for canonical form
    if (root_i < root_j) {
        label[root_j] = root_i;
    } else {
        label[root_i] = root_j;
    }
    return 1;
}

int
cc_unionfind(const CSCBinaryMatrix *matrix)
{
    uint32_t *label = NULL;
    
    label = malloc(matrix->nrows * sizeof(uint32_t));
    if (!label) {
        print_error(__func__, "malloc() failed", errno);
        return -1;
    }
    
    // Initialize: each node is its own parent
    for (size_t i = 0; i < matrix->nrows; i++) {
        label[i] = i;
    }
    
    // Process all edges with union-find (single pass)
    for (size_t i = 0; i < matrix->ncols; i++) {
        for (uint32_t j = matrix->col_ptr[i]; j < matrix->col_ptr[i+1]; j++) {
            union_nodes(label, i, matrix->row_idx[j]);
        }
    }
    
    // Count unique components by finding all roots
    uint32_t uniqueCount = 0;
    for (size_t i = 0; i < matrix->nrows; i++) {
        if (find_root(label, i) == i) {
            uniqueCount++;
        }
    }
    
    free(label);
    return (int)uniqueCount;
}

/* Label propegation */

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

int cc_label_propegation(const CSCBinaryMatrix *matrix) {

    uint32_t *label=malloc(sizeof(uint32_t)*matrix->nrows);
    for(size_t i=0;i<matrix->nrows;i++){
        label[i]=i;
    }

    uint8_t finished;
    do {
        finished = 1;
        for (size_t i = 0; i < matrix->ncols; i++) {
            for(uint32_t j = matrix->col_ptr[i]; j < matrix->col_ptr[i+1]; j++) {
                uint32_t r, c;

                c = i;
                r = matrix->row_idx[j];

                if (swap_min(label, c, r)) {
                    finished = 0;
                }
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

int
cc_sequential(const CSCBinaryMatrix *matrix,
              const unsigned int n_threads __attribute__((unused)),
              const unsigned int algorithm_variant)
{
    switch (algorithm_variant) {
    case 0:
        return cc_label_propegation(matrix);

    case 1:
        return cc_unionfind(matrix);

    default:
        break;
    }

    return -1;
}
