#ifndef PORT_CLI_PARSER_H
#define PORT_CLI_PARSER_H

#include "types.h"
#include <stdbool.h>

void ParseCLI(int argc, char* argv[]);

/** @brief When true, RmlUi handles all overlay menus (set via --ui rmlui CLI). */
extern bool g_ui_mode_rmlui;

#endif
