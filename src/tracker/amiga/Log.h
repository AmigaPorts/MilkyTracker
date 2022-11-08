#ifndef AMIGA_LOG_H
#   define AMIGA_LOG_H

#   define LOG_TRACE   4
#   define LOG_INFO    3
#   define LOG_WARNING 2
#   define LOG_ERROR   1
#   define LOG_FATAL   0

#   ifndef LOG_LEVEL
#       define LOG_LEVEL LOG_TRACE
#   endif

#   define LOG_FILENAME   (strrchr(__FILE__, '/') ? (char *)strrchr(__FILE__, '/') + 1 : __FILE__)
#   define LOG_LINE       __LINE__

#   define LOG_PRINT(pre, fmt, ...) do { \
    FILE * fh = fopen("milkytracker.log", "at"); \
    fprintf(fh, pre " t=%08ld %s@%ld " fmt "\n", PPGetTickCount(), LOG_FILENAME, LOG_LINE, __VA_ARGS__); \
    fclose(fh); \
} while(0);

#   define LOG(fmt, ...)   LOG_PRINT(">", fmt, __VA_ARGS__);

#   if LOG_LEVEL >= LOG_TRACE
#      define TRACE(fmt, ...) LOG_PRINT("T", fmt, __VA_ARGS__);
#   else
#      define TRACE(fmt, ...)
#   endif

#   if LOG_LEVEL >= LOG_INFO
#      define INFO(fmt, ...) LOG_PRINT("I", fmt, __VA_ARGS__);
#   else
#      define INFO(fmt, ...)
#   endif

#   if LOG_LEVEL >= LOG_WARNING
#      define WARN(fmt, ...) LOG_PRINT("W", fmt, __VA_ARGS__);
#   else
#      define WARN(fmt, ...)
#   endif

#   if LOG_LEVEL >= LOG_ERROR
#      define ERROR(fmt, ...) LOG_PRINT("E", fmt, __VA_ARGS__);
#   else
#      define ERROR(fmt, ...)
#   endif

#   if LOG_LEVEL >= LOG_FATAL
#      define FATAL(fmt, ...) LOG_PRINT("F", fmt, __VA_ARGS__);
#   else
#      define FATAL(fmt, ...)
#   endif

#endif /* AMIGA_LOG_H */
