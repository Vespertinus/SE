
#ifndef DEBUG_RENDERER_MOCK
#define DEBUG_RENDERER_MOCK

#include<BoundingBox.h>
#include<Transform.h>

namespace SE {

class RenderCommand {};

class DebugRendererMock {

        public:

        MOCK_METHOD0(DrawDebug, void());
        MOCK_METHOD0(Enable, void());
        MOCK_METHOD0(Disable, void());

        MOCK_METHOD3(DrawLine, void(const glm::vec3 & vStart, const glm::vec3 & vEnd, const glm::vec4 & vColor));
        MOCK_METHOD3(DrawLine, void(const glm::vec3 & vStart, const glm::vec3 & vEnd, const uint32_t color));
        MOCK_METHOD4(DrawTriangle, void(
                        const glm::vec3 & v1,
                        const glm::vec3 & v2,
                        const glm::vec3 & v3,
                        const glm::vec4 & vColor));

        MOCK_METHOD4(DrawTriangle, void(
                        const glm::vec3 & v1,
                        const glm::vec3 & v2,
                        const glm::vec3 & v3,
                        const uint32_t color));

        MOCK_METHOD5(DrawPolygon, void(
                        const glm::vec3 & v1,
                        const glm::vec3 & v2,
                        const glm::vec3 & v3,
                        const glm::vec3 & v4,
                        const glm::vec4 & vColor));

        MOCK_METHOD5(DrawPolygon, void(
                        const glm::vec3 & v1,
                        const glm::vec3 & v2,
                        const glm::vec3 & v3,
                        const glm::vec3 & v4,
                        const uint32_t color));

        MOCK_METHOD2(DrawBBox, void(const BoundingBox & oBBox, const Transform & oTransform));
        MOCK_METHOD3(DrawBBox, void(const BoundingBox & oBBox, const Transform & oTransform, const glm::vec4 & vColor));
        MOCK_METHOD1(DrawLocalAxes, void(const Transform & oTransform));
        MOCK_METHOD3(DrawGrid, void(const Transform & oTransform, const float size, const float step));
        MOCK_METHOD1(DrawRenderObj, void(const RenderCommand && oCmd));
};

}

#ifdef SE_IMPL
#include <BoundingBox.tcc>
#include <Transform.tcc>
#endif

#endif
