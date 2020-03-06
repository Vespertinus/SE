
#ifndef DEBUG_RENDERER
#define DEBUG_RENDERER

#include <VertexBuffer.h>
#include <GeometryEntity.h>

namespace SE {

class RenderCommand;
class VertexBuffer;

class DebugRenderer {

        std::vector<RenderCommand>      vRenderCommands;
        VertexBuffer                    oBuffer;
        /**
         data layout:
         |pos:3x float|color: 4x ubyte|
         */
        uint32_t                        debug_vao;
        Material                      * pMaterial;
        GeometryEntity                  oGeom;
        Transform                       oRootTransform;
        uint32_t                        red;
        uint32_t                        green;
        uint32_t                        blue;

        void            Update(const Event & oEvent);
        void            Clean(const Event & oEvent);

        public:
        DebugRenderer();
        ~DebugRenderer() noexcept;

        void            DrawLine(const glm::vec3 & vStart, const glm::vec3 & vEnd, const glm::vec4 & vColor);
        void            DrawLine(const glm::vec3 & vStart, const glm::vec3 & vEnd, const uint32_t color);
        void            DrawTriangle(
                        const glm::vec3 & v1,
                        const glm::vec3 & v2,
                        const glm::vec3 & v3,
                        const glm::vec4 & vColor);

        void            DrawTriangle(
                        const glm::vec3 & v1,
                        const glm::vec3 & v2,
                        const glm::vec3 & v3,
                        const uint32_t color);

        void            DrawPolygon(
                        const glm::vec3 & v1,
                        const glm::vec3 & v2,
                        const glm::vec3 & v3,
                        const glm::vec3 & v4,
                        const glm::vec4 & vColor);

        void            DrawPolygon(
                        const glm::vec3 & v1,
                        const glm::vec3 & v2,
                        const glm::vec3 & v3,
                        const glm::vec3 & v4,
                        const uint32_t color);

        void            DrawBBox(const BoundingBox & oBBox, const Transform & oTransform);
        void            DrawBBox(const BoundingBox & oBBox, const Transform & oTransform, const glm::vec4 & vColor);
        void            DrawLocalAxes(const Transform & oTransform);
        void            DrawGrid(const Transform & oTransform, const float size = 10.0f, const float step = 1.0f);
        /*TODO
         quad
         cross
         circle,
         sphere,
         cylinder
         grid

         solid option

         Enable \ Disable listeners
         */
        void            DrawRenderObj(const RenderCommand && oCmd);

};

}

#endif
