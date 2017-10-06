
#ifndef __GLOBAL_H__
#define __GLOBAL_H__ 1

#include <math.h>
#include <algorithm>
#include <tuple>
#include <chrono>
#include <experimental/string_view>

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

namespace SE {

//TODO remove after switching on gcc7
typedef std::experimental::string_view string_view;

}
#endif
