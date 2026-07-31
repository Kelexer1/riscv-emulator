#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>

/**
 * @brief Logs an error to stderr in the form "ERROR <msg>" with a new line
 */
#define LOG_ERROR(fmt, ...) fprintf(stderr, "ERROR: " fmt "\n", ##__VA_ARGS__)

/**
 * @brief Logs an error to stderr in the form "ERROR (<line>:<col>): <msg>" with
 * a new line
 */
#define LOG_ERROR_LOCATION(fmt, line, col, ...) fprintf(stderr, "ERROR (%d:%d): " fmt "\n", line, col, ##__VA_ARGS__)

/**
 * @brief Logs an error to stderr in the form "ERROR (<line>): <msg>" with a new
 * line
 */
#define LOG_ERROR_LINE(fmt, line, ...) fprintf(stderr, "ERROR (%d): " fmt "\n", line, ##__VA_ARGS__)

/**
 * @brief Logs an error to stderr in the form "ERROR (0x<addr>): <msg>" with a
 * new line
 */
#define LOG_ERROR_ADDRESS(fmt, addr, ...) fprintf(stderr, "ERROR (0x%08X): " fmt "\n", addr, ##__VA_ARGS__)

#endif