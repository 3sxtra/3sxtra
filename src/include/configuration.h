/**
 * @file configuration.h
 * @brief Standalone application configuration structs (no heavy dependencies).
 *
 * Separated from main.h so that files with Winsock/platform header conflicts
 * (e.g. discovery.c) can access Configuration without pulling in structs.h.
 * Mirrors upstream PR #174's configuration.h pattern.
 */
#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <stdbool.h>

typedef struct NetplayConfiguration {
    unsigned short port; /**< Game port (default 50000, set via --port). */
} NetplayConfiguration;

typedef struct TestRunnerConfiguration {
    bool enabled;
    const char* states_path;
    const char* inputs_path;
} TestRunnerConfiguration;

typedef struct Configuration {
    NetplayConfiguration netplay;
    TestRunnerConfiguration test;
} Configuration;

extern Configuration configuration;

#endif
