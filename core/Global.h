
#ifndef __GLOBAL_H__
#define __GLOBAL_H__ 1

#include <math.h>
#include <algorithm>
#include <tuple>
#include <chrono>

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>	
#include <GL/glu.h>
#include <GL/glext.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/ext.hpp>

#include <boost/filesystem.hpp>




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


#endif
