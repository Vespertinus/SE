#ifndef __CLUSTER_SSBO_H__
#define __CLUSTER_SSBO_H__ 1

#include <GLUtil.h>
#include <ClusterConfig.h>
#include <Light.h>
#include <glm/glm.hpp>
#include <vector>

namespace SE {

/**
 * Manages the three SSBO buffers used by clustered shading:
 *   Binding 0: ClusterHeaders — per-cluster metadata + atomic light counter
 *   Binding 1: LightIndexBuffer — flat array of light indices per cluster
 *   Binding 2: LightDataBuffer  — all active point light parameters
 *
 * SSBO layout (std430):
 *
 * ClusterHeaders:
 *   uint  tileCountX
 *   uint  tileCountY
 *   uint  depthSliceCount
 *   uint  _pad0
 *   ClusterData[tileX * tileY * depthSlices]
 *
 * ClusterData (48 bytes, std430):
 *   vec4  aabbMin    — view-space AABB minimum (xyz), w unused
 *   vec4  aabbMax    — view-space AABB maximum (xyz), w unused
 *   uint  lightStart — pre-computed offset into LightIndexBuffer
 *   uint  lightCount — written by compute shader (atomic counter)
 *   uint  _pad0, _pad1
 *
 * LightIndexBuffer:
 *   uint[totalClusters * MAX_LIGHTS_PER_CLUSTER]
 *
 * LightDataBuffer:
 *   Light[N] — each light is 32 bytes (vec3 pos, float r, vec3 col, float int)
 */
class ClusterSSBO {

        uint32_t  glClusterHeaders { 0 };
        uint32_t  glLightIndex   { 0 };
        uint32_t  glLightData    { 0 };
        uint32_t  glDebugCounter { 0 };  // debug: total lights assigned by compute

        size_t    headerBufferSize   { 0 };   // bytes
        size_t    indexBufferSize    { 0 };   // bytes
        size_t    lightDataCapacity  { 0 };   // max lights

        ClusterConfig cfg;

        public:

        ClusterSSBO()  = default;
        ~ClusterSSBO() noexcept;

        /** Allocate / resize all three SSBOs for the given config and max light count */
        void Init(const ClusterConfig & newCfg, uint32_t maxLightCount);

        /** Free all GL buffers */
        void Destroy() noexcept;

        /** Upload point light data to binding 2 (LightDataBuffer) */
        void UploadLightData(const std::vector<PointLight> & lights);

        /** Upload cluster header data: tile counts + per-cluster view-space AABBs + lightStart offsets.
         *  projMatrix is the camera projection matrix; projMatrix[0][0] and projMatrix[1][1] give
         *  1/tan(fovX/2) and 1/tan(fovY/2) used to compute view-space AABB corners. */
        void UploadClusterHeaders(const ClusterConfig & newCfg, const glm::mat4 & projMatrix);

        /** Bind all three SSBOs for compute shader (binding 0, 1, 2) */
        void BindForCompute() const;

        /** Bind all three SSBOs for fragment shader (binding 0, 1, 2) */
        void BindForFragment() const;

        /** Get debug counter buffer GL ID (for atomic counter readback) */
        uint32_t GetDebugCounterBuffer() const { return glDebugCounter; }

        /** Get individual SSBO GL IDs (for explicit re-binding) */
        uint32_t GetClusterHeadersBuffer() const { return glClusterHeaders; }
        uint32_t GetLightIndexBuffer()   const { return glLightIndex; }
        uint32_t GetLightDataBuffer()    const { return glLightData; }

        /** Unbind all three SSBO bindings */
        static void Unbind();
};


} // namespace SE

#endif
