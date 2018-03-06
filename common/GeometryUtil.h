#ifndef __GEOMETRY_UTIL_H__
#define __GEOMETRY_UTIL_H__

#include <vector>
#include <glm/vec3.hpp>

namespace SE {

static const uint8_t VERTEX_BASE_SIZE  = 3 + 2;
static const uint8_t VERTEX_SIZE       = VERTEX_BASE_SIZE + 3;

void CalcNormal(float normals[3], float v0[3], float v1[3], float v2[3]);

void CalcBasicBBox(
                std::vector<float> & vVertices,
                const uint8_t        elem_size,
                glm::vec3          & min,
                glm::vec3          & max);

template <class T> void CalcCompositeBBox(
                std::vector<T>     & vItems,
                glm::vec3          & min,
                glm::vec3          & max) {

                min = glm::vec3(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
                max = glm::vec3(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest());

                for (auto & oItem : vItems) {
                        min.x = std::min(oItem.min.x, min.x);
                        min.y = std::min(oItem.min.y, min.y);
                        min.z = std::min(oItem.min.z, min.z);

                        max.x = std::max(oItem.max.x, max.x);
                        max.y = std::max(oItem.max.y, max.y);
                        max.z = std::max(oItem.max.z, max.z);
                }
}


}

#ifdef SE_IMPL
#include <GeometryUtil.tcc>
#endif

#endif
