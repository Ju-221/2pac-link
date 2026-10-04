#include "terminal.h"

#include <cstdlib>
#include <iostream>

#if defined(_WIN32)
#include <io.h>
#elif defined(__APPLE__) || defined(__linux__)
#include <unistd.h>
#endif

void clearScreen() {
#if defined(_WIN32)
    if (_isatty(_fileno(stdout)) != 0) {
        std::system("cls");
    }
#elif defined(__APPLE__) || defined(__linux__)
    if (isatty(STDOUT_FILENO) != 0) {
        std::cout << "\033[2J\033[H" << std::flush;
    }
#endif
}
