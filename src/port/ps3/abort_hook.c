#include <stdio.h>
#include <stdlib.h>
#include <sys/process.h>

void _Assert(const char* test, const char* funname) {
    printf("\n=== \nASSERTION FAILED on PS3!\n");
    printf("TEST/LOC: %s\n", test ? test : "<null>");
    printf("FUNC: %s\n", funname ? funname : "<null>");
    printf("===\n\n");
    fflush(stdout);

    // Attempt explicit file write to emulator root or package root
    FILE* f = fopen("/app_home/fatal.log", "w");
    if (!f)
        f = fopen("/dev_hdd0/game/3SX00001/USRDIR/fatal.log", "w");
    if (f) {
        fprintf(f, "ASSERTION FAILED on PS3!\nTEST/LOC: %s\nFUNC: %s\n", test, funname);
        fclose(f);
    }

    sys_process_exit(1);
    while (1) {}
}

void _SCE_Assert(const char* test, const char* funname) {
    _Assert(test, funname);
}
