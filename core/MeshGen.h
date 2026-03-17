
#ifndef __MESH_GEN_H__
#define __MESH_GEN_H__ 1

#include <MeshBuilder.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace SE {
namespace MeshGen {

// Primitives (append into an existing builder at its current vertex cursor)
void Box          (MeshBuilder &, glm::vec3 halfExtents);
void Sphere       (MeshBuilder &, float radius, int latDiv, int lonDiv);
void Cylinder     (MeshBuilder &, float radius, float height, int div);
void Capsule      (MeshBuilder &, float radius, float halfHeight, int div);
void Cone         (MeshBuilder &, float radius, float height, int div);
void Torus        (MeshBuilder &, float major, float minor, int majDiv, int minDiv);
void Quad         (MeshBuilder &, glm::vec2 halfSize);
void FullscreenTri(MeshBuilder &);

// Post-process utilities (operate on committed vertex/index data in the builder)
void ComputeNormals (MeshBuilder &); // flat normals per tri from current index+pos data
void ComputeTangents(MeshBuilder &); // basic tangents from uv+pos per tri

} // namespace MeshGen
} // namespace SE

#endif
