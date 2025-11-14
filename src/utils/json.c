/**
 * @file json.c
 * @brief Minimal JSON parser for benchmark output
 * 
 * This file implements a lightweight JSON parser designed specifically
 * for the benchmark output format produced by connected components
 * performance tests. It avoids external dependencies and focuses on
 * parsing known structured data efficiently.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "json.h"

/* ------------------------------------------------------------------------- */
/*                             Parser Utilities                              */
/* ------------------------------------------------------------------------- */

/**
 * @brief Skip over whitespace characters in the JSON string.
 * @param p Pointer to current JSON position (updated in place)
 */
static void
skip_whitespace(const char **p)
{
    while (**p && (**p == ' ' || **p == '\n' || **p == '\r' || **p == '\t'))
        (*p)++;
}

/**
 * @brief Expect and consume a specific character from the JSON stream.
 * @param p Pointer to current JSON position
 * @param c Expected character
 * @return 1 if the character was found and consumed, 0 otherwise
 */
static int
expect_char(const char **p, char c)
{
    skip_whitespace(p);
    if (**p == c) {
        (*p)++;
        return 1;
    }
    return 0;
}

/**
 * @brief Parse a quoted JSON string.
 * 
 * @param p Pointer to JSON stream
 * @param dest Output buffer for parsed string
 * @param max_len Maximum buffer length
 * @return 1 on success, 0 on parse error
 */
static int
parse_string(const char **p, char *dest, size_t max_len)
{
    skip_whitespace(p);
    if (**p != '"') return 0;
    (*p)++;
    
    size_t i = 0;
    while (**p && **p != '"' && i < max_len - 1) {
        if (**p == '\\' && (*p)[1]) (*p)++;  /**< Handle escape character */
        dest[i++] = **p;
        (*p)++;
    }
    dest[i] = '\0';
    
    if (**p == '"') {
        (*p)++;
        return 1;
    }
    return 0;
}

/**
 * @brief Parse a JSON floating-point number.
 * @param p Pointer to JSON stream
 * @param value Output parsed value
 * @return 1 on success, 0 on parse error
 */
static int
parse_double(const char **p, double *value)
{
    skip_whitespace(p);
    char *end;
    *value = strtod(*p, &end);
    if (end == *p) return 0;
    *p = end;
    return 1;
}

/**
 * @brief Parse a JSON unsigned integer value.
 * @param p Pointer to JSON stream
 * @param value Output parsed integer
 * @return 1 on success, 0 on parse error
 */
static int
parse_uint(const char **p, unsigned int *value)
{
    skip_whitespace(p);
    char *end;
    long val = strtoul(*p, &end, 10);
    if (end == *p) return 0;
    *value = (unsigned int)val;
    *p = end;
    return 1;
}

/**
 * @brief Locate a JSON key and position the pointer after the colon.
 * 
 * @param p Pointer to JSON stream
 * @param key JSON key name to search for
 * @return 1 if key found and colon consumed, 0 otherwise
 */
static int
find_key(const char **p, const char *key)
{
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    
    const char *found = strstr(*p, search);
    if (!found) return 0;
    
    *p = found + strlen(search);
    skip_whitespace(p);
    if (**p == ':') {
        (*p)++;
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------------- */
/*                           Section Parsers                                 */
/* ------------------------------------------------------------------------- */

/**
 * @brief Parse the "sys_info" JSON object.
 * @param json Input JSON string
 * @param info Output structure to populate
 * @return 1 on success, 0 on failure
 */
static int
parse_sys_info(const char *json, SystemInfo *info)
{
    const char *p = json;
    if (!find_key(&p, "sys_info")) return 0;
    if (!expect_char(&p, '{')) return 0;
    
    if (find_key(&p, "timestamp") && !parse_string(&p, info->timestamp, sizeof(info->timestamp)))
        return 0;
    if (find_key(&p, "cpu_info") && !parse_string(&p, info->cpu_info, sizeof(info->cpu_info)))
        return 0;
    if (find_key(&p, "ram_mb") && !parse_double(&p, &info->ram_mb))
        return 0;
    if (find_key(&p, "swap_mb") && !parse_double(&p, &info->swap_mb))
        return 0;
    
    return 1;
}

/**
 * @brief Parse the "matrix_info" JSON object.
 * @param json Input JSON string
 * @param info Output structure to populate
 * @return 1 on success, 0 on failure
 */
static int
parse_matrix_info(const char *json, MatrixInfo *info)
{
    const char *p = json;
    if (!find_key(&p, "matrix_info")) return 0;
    if (!expect_char(&p, '{')) return 0;
    
    if (find_key(&p, "path") && !parse_string(&p, info->path, sizeof(info->path)))
        return 0;
    if (find_key(&p, "rows") && !parse_uint(&p, &info->rows))
        return 0;
    if (find_key(&p, "cols") && !parse_uint(&p, &info->cols))
        return 0;
    if (find_key(&p, "nnz") && !parse_uint(&p, &info->nnz))
        return 0;
    
    return 1;
}

/**
 * @brief Parse the "benchmark_info" JSON object.
 * @param json Input JSON string
 * @param info Output structure to populate
 * @return 1 on success, 0 on failure
 */
static int
parse_benchmark_info(const char *json, BenchmarkInfo *info)
{
    const char *p = json;
    if (!find_key(&p, "benchmark_info")) return 0;
    if (!expect_char(&p, '{')) return 0;
    
    if (find_key(&p, "threads") && !parse_uint(&p, &info->threads))
        return 0;
    if (find_key(&p, "trials") && !parse_uint(&p, &info->trials))
        return 0;
    
    return 1;
}

/**
 * @brief Parse the "statistics" JSON object.
 * @param p Pointer to JSON stream
 * @param stats Output statistics structure
 * @return 1 on success, 0 on failure
 */
static int
parse_statistics(const char **p, Statistics *stats)
{
    if (!find_key(p, "statistics")) return 0;
    if (!expect_char(p, '{')) return 0;
    
    if (find_key(p, "mean_time_s") && !parse_double(p, &stats->mean_time_s))
        return 0;
    if (find_key(p, "std_dev_s") && !parse_double(p, &stats->std_dev_s))
        return 0;
    if (find_key(p, "median_time_s") && !parse_double(p, &stats->median_time_s))
        return 0;
    if (find_key(p, "min_time_s") && !parse_double(p, &stats->min_time_s))
        return 0;
    if (find_key(p, "max_time_s") && !parse_double(p, &stats->max_time_s))
        return 0;
    
    return 1;
}

/**
 * @brief Parse a single algorithm result object.
 * @param json Input JSON string
 * @param result Output result structure
 * @return 1 on success, 0 on failure
 */
static int
parse_result(const char *json, Result *result)
{
    const char *p = json;
    if (!find_key(&p, "results")) return 0;
    if (!expect_char(&p, '[')) return 0;
    if (!expect_char(&p, '{')) return 0;
    
    result->has_metrics = 0;
    
    if (find_key(&p, "algorithm") && !parse_string(&p, result->algorithm, sizeof(result->algorithm)))
        return 0;
    if (find_key(&p, "algorithm_variant") && !parse_uint(&p, &result->algorithm_variant))
        return 0;
    if (find_key(&p, "connected_components") && !parse_uint(&p, &result->connected_components))
        return 0;
    if (!parse_statistics(&p, &result->stats))
        return 0;
    if (find_key(&p, "throughput_edges_per_sec") && !parse_double(&p, &result->throughput_edges_per_sec))
        return 0;
    if (find_key(&p, "memory_peak_mb") && !parse_double(&p, &result->memory_peak_mb))
        return 0;
    
    return 1;
}

/* ------------------------------------------------------------------------- */
/*                             Public API                                    */
/* ------------------------------------------------------------------------- */

/**
 * @brief Parse benchmark JSON data into a structured BenchmarkData object.
 * 
 * This function coordinates parsing of all top-level sections:
 * `sys_info`, `matrix_info`, `benchmark_info`, and `results`.
 * 
 * @param json Input JSON string
 * @param data Output structure to populate
 * @return 1 on success, 0 on failure
 */
int
parse_benchmark_data(const char *json, BenchmarkData *data)
{
    data->valid = 0;
    
    if (!parse_sys_info(json, &data->sys_info)) return 0;
    if (!parse_matrix_info(json, &data->matrix_info)) return 0;
    if (!parse_benchmark_info(json, &data->benchmark_info)) return 0;
    if (!parse_result(json, &data->result)) return 0;
    
    data->valid = 1;
    return 1;
}

/* ------------------------------------------------------------------------- */
/*                           JSON Print Helpers                              */
/* ------------------------------------------------------------------------- */

/**
 * @brief Print system information as formatted JSON.
 */
void
print_sys_info(const SystemInfo *info, int indent_level)
{
    printf("%*s\"sys_info\": {\n", indent_level, "");
    printf("%*s\"timestamp\": \"%s\",\n", indent_level + 2, "", info->timestamp);
    printf("%*s\"cpu_info\": \"%s\",\n", indent_level + 2, "", info->cpu_info);
    printf("%*s\"ram_mb\": %.2f,\n", indent_level + 2, "", info->ram_mb);
    printf("%*s\"swap_mb\": %.2f\n", indent_level + 2, "", info->swap_mb);
    printf("%*s}", indent_level, "");
}

/**
 * @brief Print matrix information as formatted JSON.
 */
void
print_matrix_info(const MatrixInfo *info, int indent_level)
{
    printf("%*s\"matrix_info\": {\n", indent_level, "");
    printf("%*s\"path\": \"%s\",\n", indent_level + 2, "", info->path);
    printf("%*s\"rows\": %u,\n", indent_level + 2, "", info->rows);
    printf("%*s\"cols\": %u,\n", indent_level + 2, "", info->cols);
    printf("%*s\"nnz\": %u\n", indent_level + 2, "", info->nnz);
    printf("%*s}", indent_level, "");
}

/**
 * @brief Print benchmark parameters as formatted JSON.
 */
void
print_benchmark_info(const BenchmarkInfo *info, int indent_level)
{
    printf("%*s\"benchmark_info\": {\n", indent_level, "");
    printf("%*s\"threads\": %u,\n", indent_level + 2, "", info->threads);
    printf("%*s\"trials\": %u\n", indent_level + 2, "", info->trials);
    printf("%*s}", indent_level, "");
}

/**
 * @brief Print algorithm result as formatted JSON.
 */
void
print_result(const Result *result, int indent_level)
{
    printf("%*s{\n", indent_level, "");
    printf("%*s\"algorithm\": \"%s\",\n", indent_level + 2, "", result->algorithm);
    printf("%*s\"algorithm_variant\": %u,\n", indent_level + 2, "", result->algorithm_variant);
    printf("%*s\"connected_components\": %u,\n", indent_level + 2, "", result->connected_components);
    printf("%*s\"statistics\": {\n", indent_level + 2, "");
    printf("%*s\"mean_time_s\": %.6f,\n", indent_level + 4, "", result->stats.mean_time_s);
    printf("%*s\"std_dev_s\": %.6f,\n", indent_level + 4, "", result->stats.std_dev_s);
    printf("%*s\"median_time_s\": %.6f,\n", indent_level + 4, "", result->stats.median_time_s);
    printf("%*s\"min_time_s\": %.6f,\n", indent_level + 4, "", result->stats.min_time_s);
    printf("%*s\"max_time_s\": %.6f\n", indent_level + 4, "", result->stats.max_time_s);
    printf("%*s},\n", indent_level + 2, "");
    printf("%*s\"throughput_edges_per_sec\": %.2f,\n", indent_level + 2, "", result->throughput_edges_per_sec);
    printf("%*s\"memory_peak_mb\": %.2f", indent_level + 2, "", result->memory_peak_mb);
    
    if (result->has_metrics) {
        printf(",\n");
        printf("%*s\"speedup\": %.4f,\n", indent_level + 2, "", result->speedup);
        printf("%*s\"efficiency\": %.4f\n", indent_level + 2, "", result->efficiency);
    } else {
        printf("\n");
    }
    
    printf("%*s}", indent_level, "");
}
