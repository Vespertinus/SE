#ifndef __CLUSTER_CONFIG_H__
#define __CLUSTER_CONFIG_H__ 1

#include <glm/glm.hpp>
#include <cmath>
#include <cstdint>

namespace SE {

/**
 * Clustered shading configuration.
 * Divides the view frustum into a 3D grid of clusters:
 *   - Screen-space tiles: 16×16 pixels each
 *   - Depth slices: logarithmically distributed from near to far
 *
 * Each cluster is identified by (tileX, tileY, depthSlice).
 */
struct ClusterConfig {

        static constexpr uint32_t TILE_SIZE       = 16;
        static constexpr uint32_t DEFAULT_DEPTH_SLICES = 32;
        static constexpr uint32_t MAX_LIGHTS_PER_CLUSTER = 16;

        uint32_t  tileX;             // ceil(screenWidth  / TILE_SIZE)
        uint32_t  tileY;             // ceil(screenHeight / TILE_SIZE)
        uint32_t  depthSlices;
        uint32_t  totalClusters;     // tileX * tileY * depthSlices
        float     nearZ;
        float     farZ;
        float     logFactor;         // logarithmic depth partition curvature
        float     sliceZ[33];        // depth boundaries per slice (N+1 entries)

        ClusterConfig();
        void Recompute(uint32_t screenWidth, uint32_t screenHeight, float near, float far);

        // Depth boundaries for a given slice index
        float SliceMinZ(uint32_t slice) const;
        float SliceMaxZ(uint32_t slice) const;

        // Compute dispatch dimensions
        uint32_t DispatchGroupsX() const;  // ceil(tileX / 16)
        uint32_t DispatchGroupsY() const;  // ceil(tileY / 16)
};


inline ClusterConfig::ClusterConfig() :
        tileX(0), tileY(0), depthSlices(DEFAULT_DEPTH_SLICES),
        totalClusters(0), nearZ(0.1f), farZ(1000.f), logFactor(1.f) {
        for (auto & z : sliceZ) z = 0.f;
}

inline void ClusterConfig::Recompute(uint32_t screenWidth, uint32_t screenHeight, float near, float far) {

        nearZ = near;
        farZ  = far;
        tileX = (screenWidth  + TILE_SIZE - 1) / TILE_SIZE;
        tileY = (screenHeight + TILE_SIZE - 1) / TILE_SIZE;
        totalClusters = tileX * tileY * depthSlices;

        // Logarithmic depth partition:
        // Z(s) = near * (far/near)^(s / N)
        // This gives finer resolution near the camera.
        const float ratio = farZ / nearZ;
        logFactor = std::log(ratio) / static_cast<float>(depthSlices);

        for (uint32_t s = 0; s <= depthSlices; ++s) {
                sliceZ[s] = nearZ * std::exp(logFactor * static_cast<float>(s));
        }
}

inline float ClusterConfig::SliceMinZ(uint32_t slice) const {
        return slice < depthSlices ? sliceZ[slice] : farZ;
}

inline float ClusterConfig::SliceMaxZ(uint32_t slice) const {
        return slice < depthSlices ? sliceZ[slice + 1] : farZ;
}

inline uint32_t ClusterConfig::DispatchGroupsX() const {
        return (tileX + 15) / 16;
}

inline uint32_t ClusterConfig::DispatchGroupsY() const {
        return (tileY + 15) / 16;
}

} // namespace SE

#endif
