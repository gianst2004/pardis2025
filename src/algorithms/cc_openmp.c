/**
 * @file cc_openmp.c
 * @brief Optimized OpenMP implementations for computing connected components.
 *
 * This module implements two parallel algorithms for finding connected
 * components in an undirected graph using OpenMP:
 *
 * - Label Propagation (variant 0): Iteratively propagates minimum labels
 *   until convergence with persistent threads and relaxed atomics.
 *
 * - Union-Find with Rem's Algorithm (variant 1): Lock-free parallel
 *   union-find using compare-and-swap operations and path compression.
 *
 * Both algorithms return the count of unique connected components.
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <omp.h>

#include "connected_components.h"

/* ========================================================================== */
/*                           UNION-FIND UTILITIES                             */
/* ========================================================================== */

/**
 * @brief Finds the root of a node with path compression.
 *
 * Traverses parent pointers until reaching the root, and compresses the
 * path by making all nodes along the path point directly to the root.
 * The early-exit optimization avoids redundant writes if the path is
 * already compressed.
 *
 * @param label Array of parent pointers representing disjoint sets
 * @param x Node index to find the root for
 * @return Root of the set containing x
 */
static inline uint32_t
find_compress(uint32_t *label, uint32_t x)
{
	uint32_t root = x;
	
	/* Find the root */
	while (label[root] != root)
		root = label[root];
	
	/* Compress the path */
	while (x != root) {
		uint32_t next = label[x];
		if (label[x] == next)
			break;  /* Already compressed */
		label[x] = root;
		x = next;
	}
	
	return root;
}

/**
 * @brief Unites two disjoint sets using Rem's algorithm.
 *
 * Implements lock-free parallel union-find using compare-and-swap (CAS)
 * operations. Retries up to MAX_RETRIES times before falling back to a
 * simpler atomic store. Canonical ordering (smaller index as root) ensures
 * deterministic results in parallel execution.
 *
 * @param label Array of parent pointers representing disjoint sets
 * @param a First node
 * @param b Second node
 */
static inline void
union_rem(uint32_t *label, uint32_t a, uint32_t b)
{
	const int MAX_RETRIES = 10;
	
	/* Retry loop with CAS operations */
	for (int retry = 0; retry < MAX_RETRIES; retry++) {
		a = find_compress(label, a);
		b = find_compress(label, b);
		
		if (a == b)
			return;
		
		/* Canonical ordering: smaller index as root */
		if (a > b) {
			uint32_t temp = a;
			a = b;
			b = temp;
		}
		
		uint32_t expected = b;
		if (__atomic_compare_exchange_n(&label[b], &expected, a,
		                                0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {
			return;
		}
		
		b = expected;
	}
	
	/* Fallback after maximum retries */
	a = find_compress(label, a);
	b = find_compress(label, b);
	if (a != b) {
		if (a > b) {
			uint32_t temp = a;
			a = b;
			b = temp;
		}
		__atomic_store_n(&label[b], a, __ATOMIC_RELEASE);
	}
}

/* ========================================================================== */
/*                         UNION-FIND ALGORITHM                               */
/* ========================================================================== */

/**
 * @brief Computes connected components using parallel union-find.
 *
 * Algorithm phases:
 * 1. Initialize each node as its own root (parallel)
 * 2. Perform parallel union operations on edges using dynamic scheduling
 * 3. Flatten all paths to roots for accurate counting (parallel)
 * 4. Count roots in parallel using reduction
 *
 * @param matrix Sparse CSC binary matrix representing graph
 * @param n_threads Number of OpenMP threads to use
 * @return Number of connected components, or -1 on error
 */
static int
cc_union_find(const CSCBinaryMatrix *matrix, const unsigned int n_threads)
{
	if (!matrix || matrix->nrows == 0)
		return 0;
	
	const uint32_t n = (uint32_t)matrix->nrows;
	uint32_t *label = malloc(n * sizeof(uint32_t));
	if (!label)
		return -1;
	
	/* Initialize: each node as its own parent */
	#pragma omp parallel for num_threads(n_threads) schedule(static)
	for (uint32_t i = 0; i < n; i++)
		label[i] = i;
	
	/* Process all edges: union connected nodes */
	#pragma omp parallel num_threads(n_threads)
	{
		#pragma omp for schedule(dynamic, 128) nowait
		for (uint32_t col = 0; col < matrix->ncols; col++) {
			uint32_t start = matrix->col_ptr[col];
			uint32_t end = matrix->col_ptr[col + 1];
			
			for (uint32_t j = start; j < end; j++) {
				uint32_t row = matrix->row_idx[j];
				if (row < n)
					union_rem(label, row, col);
			}
		}
	}
	
	/* Final compression pass: flatten all paths */
	#pragma omp parallel for num_threads(n_threads) schedule(static, 2048)
	for (uint32_t i = 0; i < n; i++)
		find_compress(label, i);
	
	/* Count roots (each root represents one component) */
	uint32_t count = 0;
	#pragma omp parallel for reduction(+:count) num_threads(n_threads) schedule(static, 2048)
	for (uint32_t i = 0; i < n; i++)
		if (label[i] == i)
			count++;
	
	free(label);
	return (int)count;
}

/* ========================================================================== */
/*                       LABEL PROPAGATION ALGORITHM                          */
/* ========================================================================== */

/**
 * @brief Computes connected components using parallel label propagation.
 *
 * Algorithm steps:
 * 1. Initialize each node with its own index as label
 * 2. Iterate over all edges in parallel, propagating minimum labels
 * 3. Use atomic operations to safely update labels
 * 4. Repeat until no labels change (convergence)
 * 5. Count unique components using bitmap with hardware popcount
 *
 * Key optimization: Persistent parallel region and dynamic scheduling
 * minimize synchronization overhead. Atomic writes ensure correctness.
 *
 * @param matrix Sparse CSC binary matrix representing graph
 * @param n_threads Number of OpenMP threads to use
 * @return Number of connected components, or -1 on error
 */
static int
cc_label_propagation(const CSCBinaryMatrix *matrix, const int n_threads)
{
	uint32_t *label = malloc(sizeof(uint32_t) * matrix->nrows);
	if (!label)
		return -1;
	
	/* Initialize: each node labeled with its own index */
	for (size_t i = 0; i < matrix->nrows; i++) {
		label[i] = i;
	}
	
	/* Iterate until convergence */
	uint8_t finished;
	do {
		finished = 1;
		
		#pragma omp parallel num_threads(n_threads)
		{
			uint8_t local_changed = 0;
			
			/* Process edges with dynamic scheduling */
			#pragma omp for schedule(dynamic, 4096) nowait
			for (size_t col = 0; col < matrix->ncols; col++) {
				for (uint32_t j = matrix->col_ptr[col]; j < matrix->col_ptr[col + 1]; j++) {
					uint32_t row = matrix->row_idx[j];
					
					/* Read current labels */
					uint32_t label_col = label[col];
					uint32_t label_row = label[row];
					
					/* Propagate minimum label using atomic writes */
					if (label_col != label_row) {
						local_changed = 1;
						uint32_t min_label = label_col < label_row ? label_col : label_row;
						
						if (label_col != min_label) {
							#pragma omp atomic write
							label[col] = min_label;
						} else {
							#pragma omp atomic write
							label[row] = min_label;
						}
					}
				}
			}
			
			/* Update global finished flag if any thread saw changes */
			if (local_changed) {
				#pragma omp atomic write
				finished = 0;
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
	for (size_t i = 0; i < matrix->nrows; i++) {
		uint32_t val = label[i];
		size_t word = val >> 6;            /* Divide by 64 */
		uint64_t bit = 1ULL << (val & 63); /* Modulo 64 */
		bitmap[word] |= bit;
	}
	
	/* Count set bits using hardware popcount */
	uint32_t count = 0;
	for (size_t i = 0; i < bitmap_size; i++) {
		count += __builtin_popcountll(bitmap[i]);
	}
	
	free(bitmap);
	free(label);
	return (int)count;
}

/* ========================================================================== */
/*                              PUBLIC INTERFACE                              */
/* ========================================================================== */

/**
 * @brief Computes connected components using OpenMP parallel algorithms.
 *
 * This is the main entry point for OpenMP connected components computation.
 * It dispatches to one of two algorithm implementations based on the variant
 * parameter.
 *
 * Supported variants:
 *   0: Label propagation
 *   1: Union-find with Rem's algorithm
 *
 * @param matrix Sparse binary matrix in CSC format
 * @param n_threads Number of OpenMP threads to use
 * @param algorithm_variant Algorithm selection (0 or 1)
 * @return Number of connected components, or -1 on error
 */
int
cc_openmp(const CSCBinaryMatrix *matrix,
          const unsigned int n_threads,
          const unsigned int algorithm_variant)
{
	switch (algorithm_variant) {
	case 0:
		return cc_label_propagation(matrix, (int)n_threads);
	case 1:
		return cc_union_find(matrix, n_threads);
	default:
		break;
	}
	return -1;
}
