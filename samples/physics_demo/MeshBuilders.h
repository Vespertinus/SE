#ifndef __PHYSICS_MESH_BUILDERS_H__
#define __PHYSICS_MESH_BUILDERS_H__

#include <MeshBuilder.h>
#include <MeshGen.h>
#include <VertexLayout.h>
#include <glm/vec3.hpp>

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

} // namespace SE

#endif
