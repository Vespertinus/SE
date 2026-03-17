#ifndef __PHYSICS_MESH_BUILDERS_H__
#define __PHYSICS_MESH_BUILDERS_H__

#include <MeshBuilder.h>
#include <MeshGen.h>
#include <VertexLayout.h>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <cstdint>

namespace SE {

inline H<TMesh> CreateBoxMesh(glm::vec3 half, const char * name) {
        MeshBuilder oBuilder(VertexLayout::PosNormTanUV());
        MeshGen::Box(oBuilder, half);
        return oBuilder.Upload(name);
}

inline H<TMesh> CreatePlaneMesh(float w, float d, const char * name) {
        MeshBuilder oBuilder(VertexLayout::PosNormTanUV());
        MeshGen::Quad(oBuilder, glm::vec2(w * 0.5f, d * 0.5f));
        return oBuilder.Upload(name);
}

// Build a staircase visual mesh and fill physics vertex/index arrays.
// n_steps steps ascending in +X/+Y, step_w wide, step_h tall, depth along Z.
// Local space: x=[0, n*step_w], y=[0, n*step_h], z=[-depth/2, +depth/2].
inline H<TMesh> CreateStaircaseMesh(
        int n_steps, float step_w, float step_h, float depth,
        std::vector<glm::vec3> & outPhysVerts,
        std::vector<uint32_t>  & outPhysIndices,
        const char * name)
{
        MeshBuilder oBuilder(VertexLayout::PosNormTanUV());

        outPhysVerts.clear();
        outPhysIndices.clear();

        for (int i = 0; i < n_steps; ++i) {
                // Visual: box for step i, centered at its geometric center
                float cx   = (i + 0.5f) * step_w;
                float cy   = (i + 1) * step_h * 0.5f;
                glm::vec3 half { step_w * 0.5f, (i + 1) * step_h * 0.5f, depth * 0.5f };

                MeshBuilder oStep(VertexLayout::PosNormTanUV());
                MeshGen::Box(oStep, half);
                oStep.ApplyTransform(glm::translate(glm::mat4(1.0f), glm::vec3(cx, cy, 0.0f)));
                oBuilder.Merge(oStep);

                // Physics: 8 corner vertices for this step's AABB
                float x0 = (float)i * step_w,         x1 = (float)(i + 1) * step_w;
                float y0 = 0.0f,                       y1 = (float)(i + 1) * step_h;
                float z0 = -depth * 0.5f,              z1 =  depth * 0.5f;

                uint32_t b = (uint32_t)outPhysVerts.size();
                outPhysVerts.push_back({x0, y0, z0}); // b+0
                outPhysVerts.push_back({x1, y0, z0}); // b+1
                outPhysVerts.push_back({x1, y1, z0}); // b+2
                outPhysVerts.push_back({x0, y1, z0}); // b+3
                outPhysVerts.push_back({x0, y0, z1}); // b+4
                outPhysVerts.push_back({x1, y0, z1}); // b+5
                outPhysVerts.push_back({x1, y1, z1}); // b+6
                outPhysVerts.push_back({x0, y1, z1}); // b+7

                // Top (+Y): (7,6,2,3)
                outPhysIndices.insert(outPhysIndices.end(), {b+7,b+6,b+2, b+7,b+2,b+3});
                // Bottom (-Y): (0,1,5,4)
                outPhysIndices.insert(outPhysIndices.end(), {b+0,b+1,b+5, b+0,b+5,b+4});
                // Front -X: (0,4,7,3)
                outPhysIndices.insert(outPhysIndices.end(), {b+0,b+4,b+7, b+0,b+7,b+3});
                // Back +X: (5,1,2,6)
                outPhysIndices.insert(outPhysIndices.end(), {b+5,b+1,b+2, b+5,b+2,b+6});
                // Left -Z: (0,3,2,1)
                outPhysIndices.insert(outPhysIndices.end(), {b+0,b+3,b+2, b+0,b+2,b+1});
                // Right +Z: (5,6,7,4)
                outPhysIndices.insert(outPhysIndices.end(), {b+5,b+6,b+7, b+5,b+7,b+4});
        }

        return oBuilder.Upload(name);
}

} // namespace SE

#endif
