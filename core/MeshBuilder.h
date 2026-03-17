
#ifndef __MESH_BUILDER_H__
#define __MESH_BUILDER_H__ 1

#include <vector>
#include <cstdint>
#include <cstring>
#include <VertexLayout.h>
#include <ResourceHandle.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

namespace SE {

class Mesh;
typedef Mesh TMesh;

class MeshBuilder {

        const VertexLayout &    oLayout;
        std::vector<uint8_t>    vVerts;
        std::vector<uint32_t>   vIndices;
        uint8_t                 staging[256]; // one vertex being assembled
        uint32_t                vert_count  { 0 };
        glm::vec3               vBboxMin    {  1e30f,  1e30f,  1e30f };
        glm::vec3               vBboxMax    { -1e30f, -1e30f, -1e30f };

        void WriteAttr(VertexAttrib attrib, const float * data, uint32_t n);

public:
        explicit MeshBuilder(const VertexLayout & layout);

        // Vertex assembly (call attribute setters then EndVertex)
        MeshBuilder & Position(glm::vec3 p);
        MeshBuilder & Normal  (glm::vec3 n);
        MeshBuilder & Tangent (glm::vec3 t);
        MeshBuilder & UV      (glm::vec2 uv);
        MeshBuilder & Color   (glm::vec4 c);
        MeshBuilder & EndVertex();

        // Index assembly
        MeshBuilder & Triangle(uint32_t a, uint32_t b, uint32_t c);
        MeshBuilder & Quad    (uint32_t a, uint32_t b, uint32_t c, uint32_t d);

        // Bulk append (raw float vertex data + uint32 indices)
        MeshBuilder & AppendVertices(const void * data, uint32_t count);
        MeshBuilder & AppendIndices (const uint32_t * idx, uint32_t count);

        // Post-process
        MeshBuilder & ApplyTransform(const glm::mat4 & m);
        MeshBuilder & Merge         (const MeshBuilder & other);

        uint32_t CurrentVertexIndex() const;
        uint32_t IndexCount()         const;
        void     Clear();

        // Read-only access for MeshGen post-process utilities
        const VertexLayout & Layout()     const { return oLayout; }
        const uint8_t *      VertexData() const { return vVerts.data(); }
        const uint32_t *     IndexData()  const { return vIndices.data(); }
        uint8_t *            VertexDataMutable()  { return vVerts.data(); }

        // Finalise: build FlatBuffer, call CreateResource<TMesh>(name, fb_mesh)
        H<TMesh> Upload(const char * name);
};

} // namespace SE

#endif
