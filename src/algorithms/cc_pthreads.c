/**
 * @file cc_pthreads.c
 * @brief Optimized parallel algorithms for computing connected components using Pthreads.
 *
 * This module implements two parallel algorithms for finding connected
 * components in an undirected graph using Pthreads:
 *
 * - Label Propagation (variant 0): Iterative parallel label propagation
 *   with optimized atomic updates and bitmap-based counting.
 *
 * - Union-Find with Rem's Algorithm (variant 1): Lock-free parallel
 *   union-find using compare-and-swap (CAS) operations and path compression.
 *
 * Key optimizations:
 * - Label propagation: Conditional atomics to reduce contention
 * - Union-find: Dynamic work scheduling with atomic column counter
 * - Both: Large chunks to reduce scheduling overhead
 */

#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <stdatomic.h>

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
/*                       UNION-FIND WORKER THREAD                             */
/* ========================================================================== */

/**
 * @struct union_find_args_t
 * @brief Arguments for the union-find worker thread.
 *
 * Each thread uses these arguments to perform unions on a dynamically
 * scheduled chunk of columns from the sparse matrix.
 */
typedef struct {
	const CSCBinaryMatrix *matrix; /* Input CSC binary matrix */
	uint32_t *label;               /* Label array representing disjoint sets */
	atomic_uint *next_col;         /* Atomic counter for dynamic column scheduling */
	uint32_t num_cols;             /* Total number of columns in the matrix */
} union_find_args_t;

/**
 * @brief Worker function for parallel union-find.
 *
 * Each thread grabs a chunk of columns from the global atomic counter
 * and performs union operations on all edges in those columns using
 * lock-free CAS operations.
 *
 * @param arg Pointer to union_find_args_t structure containing arguments
 * @return NULL
 */
static void *
union_find_worker(void *arg)
{
	union_find_args_t *args = arg;
	const uint32_t CHUNK_SIZE = 4096;
	
	while (1) {
		/* Grab next chunk of columns */
		uint32_t col = atomic_fetch_add(args->next_col, CHUNK_SIZE);
		if (col >= args->num_cols)
			break;
		
		uint32_t end_col = col + CHUNK_SIZE;
		if (end_col > args->num_cols)
			end_col = args->num_cols;
		
		/* Process all edges in this chunk */
		for (uint32_t c = col; c < end_col; c++) {
			uint32_t start = args->matrix->col_ptr[c];
			uint32_t end = args->matrix->col_ptr[c + 1];
			
			for (uint32_t j = start; j < end; j++) {
				uint32_t row = args->matrix->row_idx[j];
				union_rem(args->label, row, c);
			}
		}
	}
	
	return NULL;
}

/* ========================================================================== */
/*                       COUNT ROOTS WORKER THREAD                            */
/* ========================================================================== */

/**
 * @struct count_roots_args_t
 * @brief Arguments for counting roots in a subset of the label array.
 *
 * Each thread counts the number of roots in its assigned range.
 */
typedef struct {
	uint32_t *label;    /* Label array representing disjoint sets */
	uint32_t begin;     /* Start index of the range */
	uint32_t end;       /* End index of the range (exclusive) */
	uint32_t local;     /* Thread-local count of roots */
} count_roots_args_t;

/**
 * @brief Worker function to count roots in a given range.
 *
 * Each thread iterates over its assigned slice of the label array and
 * counts how many elements are roots (label[i] == i).
 *
 * @param arg Pointer to count_roots_args_t
 * @return NULL
 */
static void *
count_roots_worker(void *arg)
{
	count_roots_args_t *args = arg;
	uint32_t count = 0;
	
	for (uint32_t i = args->begin; i < args->end; i++)
		if (args->label[i] == i)
			count++;
	
	args->local = count;
	return NULL;
}

/* ========================================================================== */
/*                         UNION-FIND ALGORITHM                               */
/* ========================================================================== */

/**
 * @brief Computes connected components using parallel union-find.
 *
 * Algorithm phases:
 * 1. Initialize each node as its own root
 * 2. Perform parallel union operations on edges using multiple threads
 * 3. Flatten all paths to roots for accurate counting
 * 4. Count roots in parallel using thread-local accumulation
 *
 * @param matrix Sparse CSC binary matrix representing graph
 * @param n_threads Number of Pthreads to use
 * @return Number of connected components, or -1 on error
 */
static int
cc_union_find(const CSCBinaryMatrix *matrix, unsigned int n_threads)
{
	if (!matrix || matrix->nrows == 0)
		return 0;
	
	const uint32_t n = matrix->nrows;
	
	uint32_t *label = malloc(n * sizeof(uint32_t));
	if (!label)
		return -1;
	
	/* Initialize: each node as its own parent */
	for (uint32_t i = 0; i < n; i++)
		label[i] = i;
	
	/* Process all edges: union connected nodes */
	atomic_uint next_col;
	atomic_store(&next_col, 0);
	
	pthread_t threads[n_threads];
	union_find_args_t args = {
		.matrix = matrix,
		.label = label,
		.next_col = &next_col,
		.num_cols = matrix->ncols
	};
	
	for (unsigned i = 0; i < n_threads; i++)
		pthread_create(&threads[i], NULL, union_find_worker, &args);
	for (unsigned i = 0; i < n_threads; i++)
		pthread_join(threads[i], NULL);
	
	/* Final compression pass: flatten all paths */
	for (uint32_t i = 0; i < n; i++)
		find_compress(label, i);
	
	/* Count roots (each root represents one component) */
	uint32_t total = 0;
	uint32_t chunk = (n + n_threads - 1) / n_threads;
	count_roots_args_t count_args[n_threads];
	
	for (unsigned i = 0; i < n_threads; i++) {
		count_args[i].label = label;
		count_args[i].begin = i * chunk;
		count_args[i].end = (count_args[i].begin + chunk > n ? n : count_args[i].begin + chunk);
		pthread_create(&threads[i], NULL, count_roots_worker, &count_args[i]);
	}
	for (unsigned i = 0; i < n_threads; i++) {
		pthread_join(threads[i], NULL);
		total += count_args[i].local;
	}
	
	free(label);
	return (int)total;
}

/* ========================================================================== */
/*                   LABEL PROPAGATION WORKER THREAD                          */
/* ========================================================================== */

/**
 * @struct label_propagation_args_t
 * @brief Arguments for the label propagation worker thread.
 *
 * Each thread processes a subset of columns and updates labels atomically.
 */
typedef struct {
	const CSCBinaryMatrix *matrix; /* Input CSC binary matrix */
	uint32_t *label;               /* Label array */
	atomic_uint *next_col;         /* Atomic column counter for dynamic scheduling */
	atomic_uint *global_change;    /* Atomic flag indicating if any label changed */
} label_propagation_args_t;

/**
 * @brief Worker function for optimized parallel label propagation.
 *
 * Each thread grabs a chunk of columns dynamically, then iterates over all
 * edges in the chunk, updating the labels of connected nodes to the minimum
 * value using conditional atomic stores. Sets a global flag if any label
 * changed.
 *
 * Key optimization: Only performs atomic stores when the value actually
 * changes, dramatically reducing atomic operation overhead and contention.
 *
 * @param arg Pointer to label_propagation_args_t structure
 * @return NULL
 */
static void *
label_propagation_worker(void *arg)
{
	label_propagation_args_t *args = arg;
	const uint32_t CHUNK_SIZE = 4096;  /* Larger chunks for less overhead */
	
	while (1) {
		/* Grab next chunk of columns */
		uint32_t col = atomic_fetch_add(args->next_col, CHUNK_SIZE);
		if (col >= args->matrix->ncols)
			break;
		
		uint32_t end_col = col + CHUNK_SIZE;
		if (end_col > args->matrix->ncols)
			end_col = args->matrix->ncols;
		
		uint8_t changed = 0;
		
		/* Process all edges in this chunk */
		for (uint32_t c = col; c < end_col; c++) {
			for (uint32_t j = args->matrix->col_ptr[c]; j < args->matrix->col_ptr[c + 1]; j++) {
				uint32_t row = args->matrix->row_idx[j];
				uint32_t label_col = args->label[c];
				uint32_t label_row = args->label[row];
				
				if (label_col != label_row) {
					uint32_t min_label = label_col < label_row ? label_col : label_row;
					
					/* Conditional atomic stores: only update if value changes */
					if (label_col > min_label) {
						__atomic_store_n(&args->label[c], min_label, __ATOMIC_RELAXED);
						changed = 1;
					}
					if (label_row > min_label) {
						__atomic_store_n(&args->label[row], min_label, __ATOMIC_RELAXED);
						changed = 1;
					}
				}
			}
		}
		
		/* Update global change flag if any local changes occurred */
		if (changed)
			atomic_store(args->global_change, 1);
	}
	
	return NULL;
}

/* ========================================================================== */
/*                       LABEL PROPAGATION ALGORITHM                          */
/* ========================================================================== */

/**
 * @brief Computes connected components using optimized parallel label propagation.
 *
 * Algorithm steps:
 * 1. Initialize each node with its own label
 * 2. Iterate until convergence:
 *    - Each thread updates labels of connected nodes with conditional atomics
 *    - A global atomic flag indicates whether any changes occurred
 * 3. Construct a bitmap of unique labels to count components efficiently
 *
 * Key optimization: Only perform atomic stores when values actually change,
 * which dramatically reduces atomic operation overhead.
 *
 * @param matrix Sparse CSC binary matrix representing graph
 * @param n_threads Number of Pthreads to use
 * @return Number of connected components, or -1 on error
 */
static int
cc_label_propagation(const CSCBinaryMatrix *matrix, unsigned int n_threads)
{
	const uint32_t n = matrix->nrows;
	uint32_t *label = malloc(n * sizeof(uint32_t));
	if (!label)
		return -1;
	
	/* Initialize: each node labeled with its own index */
	for (uint32_t i = 0; i < n; i++)
		label[i] = i;
	
	/* Iterate until convergence */
	atomic_uint global_change;
	
	do {
		atomic_store(&global_change, 0);
		atomic_uint next_col;
		atomic_store(&next_col, 0);
		
		pthread_t threads[n_threads];
		label_propagation_args_t args = {
			.matrix = matrix,
			.label = label,
			.next_col = &next_col,
			.global_change = &global_change
		};
		
		for (unsigned i = 0; i < n_threads; i++)
			pthread_create(&threads[i], NULL, label_propagation_worker, &args);
		for (unsigned i = 0; i < n_threads; i++)
			pthread_join(threads[i], NULL);
		
	} while (atomic_load(&global_change));
	
	/* Count unique components using a bitmap */
	size_t bitmap_size = (n + 63) / 64;
	uint64_t *bitmap = calloc(bitmap_size, sizeof(uint64_t));
	if (!bitmap) {
		free(label);
		return -1;
	}
	
	/* Bitmap construction: set bit for each unique label */
	for (uint32_t i = 0; i < n; i++) {
		uint32_t val = label[i];
		bitmap[val >> 6] |= (1ULL << (val & 63));
	}
	
	/* Count set bits using hardware popcount */
	uint32_t count = 0;
	for (size_t i = 0; i < bitmap_size; i++)
		count += __builtin_popcountll(bitmap[i]);
	
	free(bitmap);
	free(label);
	return count;
}

/* ========================================================================== */
/*                              PUBLIC INTERFACE                              */
/* ========================================================================== */

/**
 * @brief Computes connected components using Pthreads parallel algorithms.
 *
 * This is the main entry point for Pthreads connected components computation.
 * It dispatches to one of two algorithm implementations based on the variant
 * parameter.
 *
 * Supported variants:
 *   0: Label propagation
 *   1: Union-find with Rem's algorithm
 *
 * @param matrix Sparse binary matrix in CSC format
 * @param n_threads Number of threads to use
 * @param algorithm_variant Algorithm selection (0 or 1)
 * @return Number of connected components, or -1 on error
 */
int
cc_pthreads(const CSCBinaryMatrix *matrix,
            unsigned int n_threads,
            unsigned int algorithm_variant)
{
	switch (algorithm_variant) {
	case 0:
		return cc_label_propagation(matrix, n_threads);
	case 1:
		return cc_union_find(matrix, n_threads);
	default:
		break;
	}
	return -1;
}
