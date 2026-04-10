#ifndef PS3_APP_H
#define PS3_APP_H

#include <stdbool.h>
#include <stdint.h>

int PS3App_PreInit(void);
int PS3App_FullInit(void);
void PS3App_Quit(void);
bool PS3App_PollEvents(void);
void PS3App_BeginFrame(void);
void PS3App_EndFrame(void);
void PS3App_Exit(void);

bool PS3App_IsFrameRateUncapped(void);
bool PS3App_IsVSyncEnabled(void);
uint64_t PS3App_GetTargetFrameTimeNS(void);

struct CellSpurs;
struct CellSpurs* PS3App_GetSpurs(void);

#endif
