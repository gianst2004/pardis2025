/**
 * @file args.h
 * @brief Command-line argument parsing interface.
 *
 * This header declares the function used to parse command-line arguments
 * for configuring the program's execution parameters such as number of threads,
 * number of trials, and input file path.
 */

#ifndef ARGS_H
#define ARGS_H

/**
 * @brief Parses command-line arguments.
 *
 * Supported options:
 *   -t <threads>   Number of threads (default: 8)
 *   -n <trials>    Number of trials (default: 3)
 *   -h             Show usage and exit
 *
 * Arguments:
 * filepath Path to the input matrix file (Matlab Matrix format)
 *
 * It validates each argument and reports errors using `print_error()`.
 *
 * @param argc Argument count
 * @param argv Argument vector
 * @param n_threads Output: number of threads
 * @param n_trials Output: number of trials
 * @param filepath Output: path to matrix file
 * @return 0 on success, -1 if help requested, 1 on error
 */
int parseargs(int argc, char *argv[], int *n_threads, int *n_trials, char **filepath);

#endif /* ARGS_H */
