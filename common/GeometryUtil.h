#ifndef __GEOMETRY_UTIL_H__
#define __GEOMETRY_UTIL_H__

#include <vector>
#include <glm/vec3.hpp>

namespace SE {

static const uint8_t VERTEX_BASE_SIZE  = 3 + 2;
static const uint8_t VERTEX_SIZE       = VERTEX_BASE_SIZE + 3;

void CalcNormal(float normals[3], float v0[3], float v1[3], float v2[3]);


}

#ifdef SE_IMPL
#include <GeometryUtil.tcc>
#endif

#endif
