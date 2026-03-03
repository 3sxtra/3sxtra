#ifndef MAIN_H
#define MAIN_H

#include "structs.h"
#include "types.h"

typedef struct TestRunnerConfiguration {
    bool enabled;
    const char* states_path;
    const char* inputs_path;
} TestRunnerConfiguration;

typedef struct Configuration {
    TestRunnerConfiguration test;
} Configuration;

typedef enum TaskID {
    TASK_INIT = 0,
    TASK_ENTRY = 1,
    TASK_RESET = 2,
    TASK_MENU = 3,
    TASK_PAUSE = 4,
    TASK_GAME = 5,
    TASK_SAVER = 6,
    TASK_DEBUG = 9,
} TaskID;

extern MPP mpp_w;
extern s32 system_init_level;
extern Configuration configuration;

void cpInitTask();
void cpReadyTask(TaskID num, void* func_adrs);
void cpExitTask(TaskID num);
s32 mppGetFavoritePlayerNumber();

#endif
