#include "port/paths.h"

static const char* mock_pref_path = "./test_config_dir/";

const char* Paths_GetPrefPath() {
    return mock_pref_path;
}

const char* Paths_GetBasePath() {
    return "./";
}
