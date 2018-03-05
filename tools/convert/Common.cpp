
#include <math.h>
#include "Common.h"
#include <Logging.h>



namespace SE {
namespace TOOLS {


void CalcNormal(float normals[3], float v0[3], float v1[3], float v2[3]) {
        float v10[3];
        v10[0] = v1[0] - v0[0];
        v10[1] = v1[1] - v0[1];
        v10[2] = v1[2] - v0[2];

        float v20[3];
        v20[0] = v2[0] - v0[0];
        v20[1] = v2[1] - v0[1];
        v20[2] = v2[2] - v0[2];

        normals[0] = v20[1] * v10[2] - v20[2] * v10[1];
        normals[1] = v20[2] * v10[0] - v20[0] * v10[2];
        normals[2] = v20[0] * v10[1] - v20[1] * v10[0];

        float len2 = normals[0] * normals[0] + normals[1] * normals[1] + normals[2] * normals[2];
        if (len2 > 0.0f) {
                float len = sqrtf(len2);

                normals[0] /= len;
                normals[1] /= len;
        }
}

void CalcBasicBBox(
                std::vector<float> & vVertices,
                const uint8_t        elem_size,
                glm::vec3 & min,
                glm::vec3 & max) {

        min = glm::vec3(std::numeric_limits<float>::max());
        max = glm::vec3(std::numeric_limits<float>::lowest());

        for (uint32_t i = 0; i < vVertices.size(); i += elem_size) {
                min.x = std::min(vVertices[i    ], min.x);
                min.y = std::min(vVertices[i + 1], min.y);
                min.z = std::min(vVertices[i + 2], min.z);

                max.x = std::max(vVertices[i    ], max.x);
                max.y = std::max(vVertices[i + 1], max.y);
                max.z = std::max(vVertices[i + 2], max.z);
        }
}

void ImportCtx::FixPath(std::string & sPath) {

        if (sCutPath.empty()) { return; }

        if (auto pos = sPath.rfind(sCutPath); pos != std::string::npos) {
                sPath = sReplace + sPath.substr(pos + sCutPath.size());
        }
}


}
}
