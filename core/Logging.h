#ifndef __LOGGING_H__
#define __LOGGING_H__ 1

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
#include <spdlog/sinks/stdout_sinks.h>

extern std::shared_ptr<spdlog::logger> gLogger;

#define LOG_BASENAME (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__)


#define log_d(format, ...) \
        do { \
                gLogger->debug("{}: " format " ({}:{})", __func__, ##__VA_ARGS__, LOG_BASENAME, __LINE__ ); \
        } while (0)

#define log_i(format, ...) \
        do { \
                gLogger->info("{}: " format " ({}:{})", __func__, ##__VA_ARGS__, LOG_BASENAME, __LINE__ ); \
        } while (0)

#define log_w(format, ...) \
        do { \
                gLogger->warn("{}: " format " ({}:{})", __func__, ##__VA_ARGS__, LOG_BASENAME, __LINE__ ); \
        } while (0)

#define log_e(format, ...) \
        do { \
                gLogger->error("{}: " format " ({}:{})", __func__, ##__VA_ARGS__, LOG_BASENAME, __LINE__ ); \
        } while (0)

#define log_e_clean(format, ...) \
        do { \
                gLogger->error("{}: " format, __func__, ##__VA_ARGS__); \
        } while (0)

#define log_d_clean(format, ...) \
        do { \
                gLogger->debug(format, ##__VA_ARGS__); \
        } while (0)

#define se_assert(cond, ...) \
        do { \
                if (!(cond)) { \
                        log_e_clean("("#cond") failed ({}:{})",  ##__VA_ARGS__, LOG_BASENAME, __LINE__); \
                        abort(); \
                } \
        } while (0)
#endif
