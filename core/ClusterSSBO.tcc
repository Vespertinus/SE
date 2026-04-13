#include <ClusterSSBO.h>
#include <Logging.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cfloat>

namespace SE {

// ClusterData size in std430: vec4(16) + vec4(16) + uint(4)*4 = 48 bytes
static constexpr size_t CLUSTER_DATA_SIZE = 48;
// 16-byte header: tileCountX, tileCountY, depthSliceCount, pad
static constexpr size_t HEADER_PREFIX = 16;

ClusterSSBO::~ClusterSSBO() noexcept {
        Destroy();
}

void ClusterSSBO::Init(const ClusterConfig & newCfg, uint32_t maxLightCount) {

        // Free old buffers if they exist (e.g. on resize)
        Destroy();

        cfg = newCfg;

        // ClusterHeaders size: 16-byte prefix + N clusters × 32 bytes
        headerBufferSize = HEADER_PREFIX + cfg.totalClusters * CLUSTER_DATA_SIZE;

        // LightIndexBuffer size: totalClusters × maxLightsPerCluster × 4 bytes
        indexBufferSize = static_cast<size_t>(cfg.totalClusters) * cfg.MAX_LIGHTS_PER_CLUSTER * 4;

        lightDataCapacity = maxLightCount;

        if (!glClusterHeaders) {
                glGenBuffers(1, &glClusterHeaders);
                glGenBuffers(1, &glLightIndex);
                glGenBuffers(1, &glLightData);
                glGenBuffers(1, &glDebugCounter);
                // Debug counter: single uint32, GPU-only access
                glBindBuffer(GL_SHADER_STORAGE_BUFFER, glDebugCounter);
                glBufferData(GL_SHADER_STORAGE_BUFFER, 4 * sizeof(uint32_t), nullptr, GL_DYNAMIC_COPY);
        }

        // ClusterHeaders — dynamic (compute writes counters)
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, glClusterHeaders);
        glBufferData(GL_SHADER_STORAGE_BUFFER, headerBufferSize, nullptr, GL_DYNAMIC_DRAW);

        // LightIndexBuffer — read-only in fragment, written by compute
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, glLightIndex);
        glBufferData(GL_SHADER_STORAGE_BUFFER, indexBufferSize, nullptr, GL_DYNAMIC_DRAW);

        // LightDataBuffer — CPU uploads each frame, GPU reads
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, glLightData);
        glBufferData(GL_SHADER_STORAGE_BUFFER,
                        static_cast<GLsizeiptr>(lightDataCapacity * sizeof(PointLight)),
                        nullptr, GL_DYNAMIC_DRAW);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        log_d("ClusterSSBO: headers={} KB, index={} KB, light_data_capacity={} lights",
                        headerBufferSize / 1024,
                        indexBufferSize / 1024,
                        lightDataCapacity);
}

void ClusterSSBO::Destroy() noexcept {

        if (glClusterHeaders) { glDeleteBuffers(1, &glClusterHeaders); glClusterHeaders = 0; }
        if (glLightIndex)     { glDeleteBuffers(1, &glLightIndex);     glLightIndex     = 0; }
        if (glLightData)      { glDeleteBuffers(1, &glLightData);      glLightData      = 0; }
        if (glDebugCounter)   { glDeleteBuffers(1, &glDebugCounter);   glDebugCounter   = 0; }
}

void ClusterSSBO::UploadLightData(const std::vector<PointLight> & lights) {

        if (lights.size() > lightDataCapacity) {
                log_w("ClusterSSBO: {} lights exceed capacity {}, resizing",
                                lights.size(), lightDataCapacity);
                lightDataCapacity = lights.size();
                glBindBuffer(GL_SHADER_STORAGE_BUFFER, glLightData);
                glBufferData(GL_SHADER_STORAGE_BUFFER,
                                static_cast<GLsizeiptr>(lightDataCapacity * sizeof(PointLight)),
                                nullptr, GL_DYNAMIC_DRAW);
        }

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, glLightData);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                        static_cast<GLsizeiptr>(lights.size() * sizeof(PointLight)),
                        lights.data());
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void ClusterSSBO::UploadClusterHeaders(const ClusterConfig & newCfg, const glm::mat4 & projMatrix) {

        cfg = newCfg;

        // Build header data on CPU
        std::vector<uint32_t> headerData;
        const size_t totalUint32 = (HEADER_PREFIX + cfg.totalClusters * CLUSTER_DATA_SIZE) / 4;
        headerData.resize(totalUint32, 0);

        // Prefix: tileCountX, tileCountY, depthSliceCount, pad
        headerData[0] = cfg.tileX;
        headerData[1] = cfg.tileY;
        headerData[2] = cfg.depthSlices;
        headerData[3] = 0;

        // Per-cluster data (48 bytes = 12 uint32 per cluster)
        // Layout: vec4 aabbMin, vec4 aabbMax, uint lightStart, uint lightCount, uint pad0, uint pad1
        const uint32_t clusterOffset = HEADER_PREFIX / 4;  // 4 uint32
        const uint32_t stride = CLUSTER_DATA_SIZE / 4;     // 12 uint32

        // projMatrix[col][row] in GLM (column-major):
        //   [0][0] = P_00 = 1/tan(fovX/2) = invFovX
        //   [1][1] = P_11 = 1/tan(fovY/2) = invFovY
        const float invFovX = projMatrix[0][0];
        const float invFovY = projMatrix[1][1];

        uint32_t lightStartAccum = 0;
        for (uint32_t z = 0; z < cfg.depthSlices; ++z) {
                const float dNear = cfg.SliceMinZ(z);   // positive depth (distance from camera)
                const float dFar  = cfg.SliceMaxZ(z);

                for (uint32_t ty = 0; ty < cfg.tileY; ++ty) {
                        for (uint32_t tx = 0; tx < cfg.tileX; ++tx) {
                                const uint32_t clusterIdx = z * cfg.tileX * cfg.tileY + ty * cfg.tileX + tx;
                                const uint32_t base = clusterOffset + clusterIdx * stride;

                                // NDC corners of this tile.
                                // UV convention: V=0 at bottom (NDC y=-1), V=1 at top (NDC y=+1).
                                // ComputeClusterCoord maps tileUV.y = screenUV.y * tileCountY,
                                // so tile ty=0 is at the bottom (NDC y=-1).
                                const float nxMin = static_cast<float>(tx)     / cfg.tileX * 2.f - 1.f;
                                const float nxMax = static_cast<float>(tx + 1) / cfg.tileX * 2.f - 1.f;
                                const float nyMin = static_cast<float>(ty)     / cfg.tileY * 2.f - 1.f;
                                const float nyMax = static_cast<float>(ty + 1) / cfg.tileY * 2.f - 1.f;

                                // Compute view-space AABB from 8 frustum corners.
                                // For NDC coord (nx, ny) at positive camera distance d:
                                //   x_view = d * nx / invFovX
                                //   y_view = d * ny / invFovY
                                //   z_view = -d   (OpenGL: camera looks toward -Z)
                                const float nx[2] = { nxMin, nxMax };
                                const float ny[2] = { nyMin, nyMax };
                                const float d[2]  = { dNear, dFar  };

                                glm::vec3 aabbMin( FLT_MAX,  FLT_MAX,  FLT_MAX);
                                glm::vec3 aabbMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);

                                for (int xi = 0; xi < 2; ++xi)
                                for (int yi = 0; yi < 2; ++yi)
                                for (int zi = 0; zi < 2; ++zi) {
                                        const float di = d[zi];
                                        const glm::vec3 corner(di * nx[xi] / invFovX,
                                                               di * ny[yi] / invFovY,
                                                               -di);
                                        aabbMin = glm::min(aabbMin, corner);
                                        aabbMax = glm::max(aabbMax, corner);
                                }

                                // Store vec4 aabbMin (xyz, w=0) and vec4 aabbMax (xyz, w=0)
                                uint32_t * pBase = &headerData[base];
                                float aabbData[8] = {
                                        aabbMin.x, aabbMin.y, aabbMin.z, 0.f,
                                        aabbMax.x, aabbMax.y, aabbMax.z, 0.f
                                };
                                std::memcpy(pBase, aabbData, sizeof(aabbData));

                                // lightStart, lightCount(=0), pad0, pad1
                                pBase[8]  = lightStartAccum;
                                pBase[9]  = 0;
                                pBase[10] = 0;
                                pBase[11] = 0;

                                lightStartAccum += cfg.MAX_LIGHTS_PER_CLUSTER;
                        }
                }
        }

        // Upload to GL
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, glClusterHeaders);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                        static_cast<GLsizeiptr>(headerData.size() * sizeof(uint32_t)),
                        headerData.data());
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void ClusterSSBO::BindForCompute() const {

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, glClusterHeaders);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, glLightIndex);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, glLightData);
}

void ClusterSSBO::BindForFragment() const {

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, glClusterHeaders);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, glLightIndex);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, glLightData);
}

void ClusterSSBO::Unbind() {

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, 0);
}

} // namespace SE
