
#include <math.h>

namespace SE {

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


}
