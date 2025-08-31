#include <stdarg.h>
#include <stdio.h>
#include "def.h"

/**
 * @brief A simple cross-platform logging function.
 *
 * This function logs a formatted message to the appropriate system logger.
 * On Windows, it uses OutputDebugStringA, which is useful for debugging.
 * On Linux and macOS, it uses the standard printf to stdout for demonstration.
 *
 * @param format The format string, similar to printf.
 * @param ... The variable arguments to be formatted.
 */
inline void LOG(const char* format, ...) {
    const int buffer_size = 1024;
    char buffer[buffer_size];
    
    va_list args;
    va_start(args, format);

    vsnprintf(buffer, buffer_size, format, args);

    va_end(args);
#ifdef _WIN32
    OutputDebugStringA(buffer);
#else
    fprintf(stderr, "%s", buffer);
#endif
}
