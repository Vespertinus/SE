
#ifndef APP_TERRAIN_QUADTREE_H
#define APP_TERRAIN_QUADTREE_H 1

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>

#include <Frustum.h>   // SFrustum promoted to core — shared with the renderer

namespace SE {

// Compact quadtree over a terrain tile grid (the terrain system's private
// LOD/culling structure — NOT the gameplay spatial index, NOT a renderer BVH).
// Nodes cover tile rects; leaves are single tiles. Build over the manifest grid
// extent; CollectVisible walks nodes with the frustum and emits candidate tile
// indices (row-major y * tiles_x + x). Sparse grids simply yield tiles the
// caller filters against its active set.
class TerrainQuadtree {
public:
        void Build(const glm::vec3 & vExtentMin, const glm::vec3 & vExtentMax,
                   const uint32_t tiles_x, const uint32_t tiles_y) {

                vNodes.clear();
                tiles_x_count = tiles_x;
                tiles_y_count = tiles_y;
                cell_x = (vExtentMax.x - vExtentMin.x) / float(tiles_x);
                cell_z = (vExtentMax.z - vExtentMin.z) / float(tiles_y);
                if (tiles_x == 0 || tiles_y == 0) { return; }

                SNode root;
                root.tx0 = 0; root.ty0 = 0;
                root.tx1 = tiles_x - 1; root.ty1 = tiles_y - 1;
                root.vMin = vExtentMin;
                root.vMax = vExtentMax;
                vNodes.push_back(root);
                Split(0);
        }

        void Clear() { vNodes.clear(); }

        void CollectVisible(const SFrustum & oFrustum, std::vector<uint32_t> & vOut) const {

                vOut.clear();
                if (vNodes.empty()) { return; }
                Collect(0, oFrustum, vOut);
        }

private:
        struct SNode {
                uint32_t  tx0 = 0, ty0 = 0, tx1 = 0, ty1 = 0;   // inclusive rect; leaf when 1x1
                glm::vec3 vMin{0.0f}, vMax{0.0f};
                uint32_t  children[4] = { 0, 0, 0, 0 };          // 0 = none
        };

        void MakeChild(const uint32_t parent_idx, const int q) {
                // copy the parent: push_back below may reallocate vNodes, and a
                // reference taken here would dangle — the child-link write then
                // lands in freed memory and the subtree silently becomes
                // unreachable (tiles drop out of the candidate set; the damage
                // pattern varies per run with allocator reallocation luck)
                const SNode n = vNodes[parent_idx];
                const uint32_t txm = (n.tx0 + n.tx1) / 2;
                const uint32_t tym = (n.ty0 + n.ty1) / 2;

                const uint32_t ax0 = (q & 1) ? txm + 1 : n.tx0;
                const uint32_t ax1 = (q & 1) ? n.tx1   : txm;
                const uint32_t ay0 = (q & 2) ? tym + 1 : n.ty0;
                const uint32_t ay1 = (q & 2) ? n.ty1   : tym;
                if (ax0 > ax1 || ay0 > ay1) { return; }                       // empty quadrant
                if (ax0 == n.tx0 && ay0 == n.ty0 && ax1 == n.tx1 && ay1 == n.ty1) { return; }   // no progress

                SNode child;
                child.tx0 = ax0; child.ty0 = ay0; child.tx1 = ax1; child.ty1 = ay1;
                // ax0/ay0 are global tile coords; the parent's world origin
                // already includes its own (tx0, ty0) offset — subtract it or the
                // offset is applied twice and every non-zero-origin subtree gets
                // its box displaced out of the grid (tiles vanish while visible)
                child.vMin = glm::vec3(n.vMin.x + (ax0 - n.tx0) * cell_x, n.vMin.y,
                                       n.vMin.z + (ay0 - n.ty0) * cell_z);
                child.vMax = glm::vec3(n.vMin.x + (ax1 + 1 - n.tx0) * cell_x, n.vMax.y,
                                       n.vMin.z + (ay1 + 1 - n.ty0) * cell_z);

                vNodes.push_back(child);
                vNodes[parent_idx].children[q] = uint32_t(vNodes.size() - 1);
        }

        void Split(const uint32_t node_idx) {

                MakeChild(node_idx, 0);
                MakeChild(node_idx, 1);
                MakeChild(node_idx, 2);
                MakeChild(node_idx, 3);

                // children indices are stable (MakeChild only appends); copy them out
                // because recursion appends and would invalidate the node reference
                const uint32_t aChildren[4] = {
                        vNodes[node_idx].children[0], vNodes[node_idx].children[1],
                        vNodes[node_idx].children[2], vNodes[node_idx].children[3]
                };
                for (const uint32_t c : aChildren) {
                        if (c != 0) { Split(c); }
                }
        }

        void Collect(const uint32_t node_idx, const SFrustum & oFrustum, std::vector<uint32_t> & vOut) const {

                const SNode & n = vNodes[node_idx];
                if (!oFrustum.IntersectsAABB(n.vMin, n.vMax)) { return; }

                if (n.tx0 == n.tx1 && n.ty0 == n.ty1) {
                        vOut.push_back(n.ty0 * tiles_x_count + n.tx0);
                        return;
                }
                for (const uint32_t c : n.children) {
                        if (c != 0) { Collect(c, oFrustum, vOut); }
                }
        }

        std::vector<SNode> vNodes;
        uint32_t           tiles_x_count = 0;
        uint32_t           tiles_y_count = 0;
        float              cell_x = 0.0f;
        float              cell_z = 0.0f;
};

} // namespace SE

#endif
