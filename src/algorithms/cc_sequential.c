/**
 * @file cc_sequential.c
 * @brief Optimized sequential algorithms for computing connected components.
 *
 * This module implements two sequential algorithms for finding connected
 * components in an undirected graph represented as a sparse binary matrix:
 *
 * - Label Propagation (variant 0): Iteratively propagates minimum labels
 *   until convergence with early termination optimization.
 *
 * - Union-Find (variant 1): Uses disjoint-set data structure with path
 *   halving optimization. Generally faster and more scalable.
 *
 * Both algorithms return the count of unique connected components.
 */

#include <stdlib.h>
#include <errno.h>
#include "connected_components.h"
#include "error.h"

/* ========================================================================== */
/*                           UNION-FIND ALGORITHM                             */
/* ========================================================================== */

/**
 * @brief Finds the root of a node with path halving optimization.
 *
 * Path halving is a one-pass variant of path compression that makes every
 * node point to its grandparent, effectively halving the path length on
 * each traversal. This provides nearly the same performance as full path
 * compression with less overhead.
 *
 * @param label Array where label[i] is the parent of node i
 * @param i Node to find root for
 * @return Root node (where label[root] == root)
 */
static inline uint32_t
find_root_halving(uint32_t *label, uint32_t i)
{
	while (label[i] != i) {
		label[i] = label[label[i]];  /* Path halving: skip one level */
		i = label[i];
	}
	return i;
}

/**
 * @brief Unites two nodes by attaching their roots.
 *
 * This performs union-by-index, where the root with the larger index is
 * always attached to the root with the smaller index. This maintains a
 * canonical form where component representatives are always the minimum
 * node index in each component.
 *
 * @param label Array of parent pointers
 * @param i First node
 * @param j Second node
 * @return 1 if union was performed, 0 if nodes already in same set
 */
static inline int
union_nodes_by_index(uint32_t *label, uint32_t i, uint32_t j)
{
	uint32_t root_i = find_root_halving(label, i);
	uint32_t root_j = find_root_halving(label, j);
	
	if (root_i == root_j)
		return 0;
	
	/* Attach larger index to smaller (maintains canonical form) */
	if (root_i < root_j) {
		label[root_j] = root_i;
	} else {
		label[root_i] = root_j;
	}
	return 1;
}

/**
 * @brief Computes connected components using union-find algorithm.
 *
 * Algorithm steps:
 * 1. Initialize each node as its own parent (singleton sets)
 * 2. For each edge (i,j), union the sets containing i and j
 * 3. Perform final path compression to flatten all trees
 * 4. Count nodes that are their own parent (roots = components)
 *
 * @param matrix Sparse binary matrix in CSC format representing graph
 * @return Number of connected components, or -1 on error
 */
static int
cc_union_find(const CSCBinaryMatrix *matrix)
{
	uint32_t *label = malloc(matrix->nrows * sizeof(uint32_t));
	if (!label) {
		print_error(__func__, "malloc() failed", errno);
		return -1;
	}
	
	/* Initialize: each node is its own parent */
	for (size_t i = 0; i < matrix->nrows; i++) {
		label[i] = i;
	}
	
	/* Process all edges: union connected nodes */
	for (size_t i = 0; i < matrix->ncols; i++) {
		for (uint32_t j = matrix->col_ptr[i]; j < matrix->col_ptr[i + 1]; j++) {
			union_nodes_by_index(label, i, matrix->row_idx[j]);
		}
	}
	
	/* Final compression pass: flatten all paths for accurate counting */
	for (size_t i = 0; i < matrix->nrows; i++) {
		find_root_halving(label, i);
	}
	
	/* Count roots (each root represents one component) */
	uint32_t unique_count = 0;
	for (size_t i = 0; i < matrix->nrows; i++) {
		if (label[i] == i) {
			unique_count++;
		}
	}
	
	free(label);
	return (int)unique_count;
}

/* ========================================================================== */
/*                       LABEL PROPAGATION ALGORITHM                          */
/* ========================================================================== */

/**
 * @brief Computes connected components using optimized label propagation.
 *
 * Algorithm steps:
 * 1. Initialize each node with its own index as label
 * 2. Iterate over all edges, propagating minimum labels
 * 3. Use cached column label to reduce redundant reads
 * 4. Repeat until no labels change (convergence)
 * 5. Count unique components using bitmap with hardware popcount
 *
 * Optimization: Cache the column label in the inner loop to avoid
 * redundant memory reads when processing multiple edges in the same column.
 *
 * @param matrix Sparse binary matrix in CSC format representing graph
 * @return Number of connected components, or -1 on error
 */
static int
cc_label_propagation(const CSCBinaryMatrix *matrix)
{
	uint32_t *label = malloc(sizeof(uint32_t) * matrix->nrows);
	if (!label) {
		return -1;
	}
	
	/* Initialize: each node labeled with its own index */
	for (size_t i = 0; i < matrix->nrows; i++) {
		label[i] = i;
	}
	
	/* Iterate until convergence */
	uint8_t finished;
	do {
		finished = 1;
		
		/* Process all edges, propagating minimum labels */
		for (size_t i = 0; i < matrix->ncols; i++) {
			uint32_t col_label = label[i];  /* Cache column label */
			
			for (uint32_t j = matrix->col_ptr[i]; j < matrix->col_ptr[i + 1]; j++) {
				uint32_t row = matrix->row_idx[j];
				uint32_t row_label = label[row];
				
				if (col_label != row_label) {
					uint32_t min_label = col_label < row_label ? col_label : row_label;
					
					/* Update column label if needed (and cache it) */
					if (col_label > min_label) {
						label[i] = col_label = min_label;
						finished = 0;
					}
					
					/* Update row label if needed */
					if (row_label > min_label) {
						label[row] = min_label;
						finished = 0;
					}
				}
			}
		}
	} while (!finished);
	
	/* Count unique components using a bitmap */
	size_t bitmap_size = (matrix->nrows + 63) / 64;
	uint64_t *bitmap = calloc(bitmap_size, sizeof(uint64_t));
	if (!bitmap) {
		free(label);
		return -1;
	}
	
	/* Bitmap construction: set bit for each unique label */
	for (uint32_t i = 0; i < matrix->nrows; i++) {
		uint32_t val = label[i];
		bitmap[val >> 6] |= (1ULL << (val & 63));
	}
	
	/* Count set bits using hardware popcount */
	uint32_t count = 0;
	for (size_t i = 0; i < bitmap_size; i++) {
		count += __builtin_popcountll(bitmap[i]);
	}
	
	free(label);
	free(bitmap);
	return (int)count;
}

/* ========================================================================== */
/*                              PUBLIC INTERFACE                              */
/* ========================================================================== */

/**
 * @brief Computes connected components using sequential algorithms.
 *
 * This is the main entry point for sequential connected components
 * computation. It dispatches to one of two algorithm implementations
 * based on the variant parameter.
 *
 * Supported variants:
 *   0: Label propagation
 *   1: Union-find
 *
 * @param matrix Sparse binary matrix in CSC format
 * @param n_threads Unused (for API compatibility with parallel versions)
 * @param algorithm_variant Algorithm selection (0 or 1)
 * @return Number of connected components, or -1 on error
 */
int
cc_sequential(const CSCBinaryMatrix *matrix,
              const unsigned int n_threads __attribute__((unused)),
              const unsigned int algorithm_variant)
{
	switch (algorithm_variant) {
	case 0:
		return cc_label_propagation(matrix);
	case 1:
		return cc_union_find(matrix);
	default:
		break;
	}
	return -1;
}
