/* ==============================================================================
 * AGENCY-1: Automated Test Runner
 * Automated Verification Harness for OS Prototype (Zero Dependencies)
 * ==============================================================================
 * Compilation:
 *   Windows (MinGW):  gcc -O2 test_runner.c -o test_runner.exe
 *   Linux / macOS:    gcc -O2 test_runner.c -o test_runner
 * ==============================================================================
 */

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
#ifdef _WIN32
    return system("agency_sim.exe --test");
#else
    return system("./agency_sim --test");
#endif
}
