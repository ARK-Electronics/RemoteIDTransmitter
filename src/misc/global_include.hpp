#pragma once

#include <chrono>

#define NORMAL_TEXT "\033[0m" // Restore normal console colour
#define CYAN_TEXT "\u001b[36m" // Turn text on console blue
#define RED_TEXT "\x1B[31m" // Turn text on console blue
#define GREEN_TEXT "\u001b[32;1m" // Turn text on console green

#define LOG(...) do { printf(__VA_ARGS__); puts(""); } while (0)

#define millis() std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count()
