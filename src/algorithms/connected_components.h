#ifndef CONNECTED_COMPONENTS_H
#define CONNECTED_COMPONENTS_H

#include "../core/matrix.h"

/**
 * @brief Count connected components using sequential label propagation
 * @param matrix Input sparse binary matrix in CSC format
 * @return Number of connected components, or -1 on error
 */
int cc_count_sequential(const CSCBinaryMatrix *matrix);

/**
 * @brief Count connected components using parallel label propagation
 * @param matrix Input sparse binary matrix in CSC format
 * @return Number of connected components, or -1 on error
 */
int cc_count_parallel_omp(const CSCBinaryMatrix *matrix);

int cc_count_parallel_cilk(const CSCBinaryMatrix *matrix);

int cc_count_parallel_pthreads(const CSCBinaryMatrix *matrix);

#endif
