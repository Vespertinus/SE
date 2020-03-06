
namespace SE {

inline uint32_t ConvertColor(const glm::vec4 & vColor) {

        union {
                uint32_t res;
                std::array<uint8_t, 4> color;
        };

        color[0] = glm::packUnorm1x8(vColor.x);
        color[1] = glm::packUnorm1x8(vColor.y);
        color[2] = glm::packUnorm1x8(vColor.z);
        color[3] = glm::packUnorm1x8(vColor.w);

        return res;
}

inline uint32_t ConvertColor(const float r, const float g, const float b, const float a) {
        return ConvertColor(glm::vec4(r, g, b, a));
}

DebugRenderer::DebugRenderer() :
        oGeom(0, 0, 0, GL_LINES),
        red  (ConvertColor(1.0f, 0.0f, 0.0f, 1.0f)),
        green(ConvertColor(0.0f, 1.0f, 0.0f, 1.0f)),
        blue (ConvertColor(0.0f, 0.0f, 1.0f, 1.0f)) {

        pMaterial = CreateResource<Material>(GetSystem<Config>().sResourceDir + "material/default_color.semt");

        glGenVertexArrays(1, &debug_vao);
        glBindVertexArray(debug_vao);
        glBindBuffer(GL_ARRAY_BUFFER, oBuffer.ID());

        auto itLocation = mAttributeLocation.find("Position");
        if (itLocation == mAttributeLocation.end()) {
                glDeleteVertexArrays(1, &debug_vao);
                throw(std::runtime_error("unknown vertex attribute name: 'Position'"));
        }

        glVertexAttribPointer(itLocation->second,
                        3,
                        GL_FLOAT,
                        false,
                        3 * sizeof(float) + sizeof(uint32_t),
                        (const void *)0);
        glEnableVertexAttribArray(itLocation->second);

        itLocation = mAttributeLocation.find("Color");
        if (itLocation == mAttributeLocation.end()) {
                glDeleteVertexArrays(1, &debug_vao);
                throw(std::runtime_error("unknown vertex attribute name: 'Color'"));
        }

        glVertexAttribPointer(itLocation->second,
                        4,
                        GL_UNSIGNED_BYTE,
                        true,
                        3 * sizeof(float) + sizeof(uint32_t),
                        (const void *)(3 * sizeof(float)));
        glEnableVertexAttribArray(itLocation->second);

        glBindVertexArray(0);

        oGeom.SetVAO(debug_vao);

        GetSystem<EventManager>().AddListener<EPostUpdate, &DebugRenderer::Update>(this);
        GetSystem<EventManager>().AddListener<EPostRenderUpdate, &DebugRenderer::Clean>(this);
}

DebugRenderer::~DebugRenderer() noexcept {

        glDeleteVertexArrays(1, &debug_vao);
        GetSystem<EventManager>().RemoveListener<EPostUpdate, &DebugRenderer::Update>(this);
        GetSystem<EventManager>().RemoveListener<EPostRenderUpdate, &DebugRenderer::Clean>(this);
}

void DebugRenderer::DrawLine(const glm::vec3 & vStart, const glm::vec3 & vEnd, const glm::vec4 & vColor) {

        uint32_t color = ConvertColor(vColor);

        DrawLine(vStart, vEnd, color);
}

void DebugRenderer::DrawLine(const glm::vec3 & vStart, const glm::vec3 & vEnd, const uint32_t color) {

        oBuffer.Append(&vStart, sizeof(vStart));
        oBuffer.Append(&color,  sizeof(color));

        oBuffer.Append(&vEnd,   sizeof(vEnd));
        oBuffer.Append(&color,  sizeof(color));
}

void DebugRenderer::DrawTriangle(
                const glm::vec3 & v1,
                const glm::vec3 & v2,
                const glm::vec3 & v3,
                const glm::vec4 & vColor) {

        uint32_t color = ConvertColor(vColor);
        DrawTriangle(v1, v2, v3, color);
}

void DebugRenderer::DrawTriangle(
                const glm::vec3 & v1,
                const glm::vec3 & v2,
                const glm::vec3 & v3,
                const uint32_t color) {

        DrawLine(v1, v2, color);
        DrawLine(v1, v3, color);
        DrawLine(v2, v3, color);
}

void DebugRenderer::DrawPolygon(
                const glm::vec3 & v1,
                const glm::vec3 & v2,
                const glm::vec3 & v3,
                const glm::vec3 & v4,
                const glm::vec4 & vColor) {

        uint32_t color = ConvertColor(vColor);
        DrawPolygon(v1, v2, v3, v4, color);
}
void DebugRenderer::DrawPolygon(
                const glm::vec3 & v1,
                const glm::vec3 & v2,
                const glm::vec3 & v3,
                const glm::vec3 & v4,
                const uint32_t color) {

        DrawTriangle(v1, v2, v3, color);
        DrawTriangle(v3, v4, v1, color);
}

void DebugRenderer::DrawBBox(const BoundingBox & oBBox, const Transform & oTransform) {

        auto            & mWorld = oTransform.GetWorld();
        const glm::vec3 & vMin   = oBBox.Min();
        const glm::vec3 & vMax   = oBBox.Max();

        std::array<glm::vec3, 8> vPoints = {
                mWorld * glm::vec4(vMax.x, vMin.y, vMin.z, 1.0f),
                mWorld * glm::vec4(vMin.x, vMin.y, vMin.z, 1.0f),
                mWorld * glm::vec4(vMin.x, vMax.y, vMin.z, 1.0f),
                mWorld * glm::vec4(vMax.x, vMax.y, vMin.z, 1.0f),
                mWorld * glm::vec4(vMax.x, vMin.y, vMax.z, 1.0f),
                mWorld * glm::vec4(vMin.x, vMin.y, vMax.z, 1.0f),
                mWorld * glm::vec4(vMin.x, vMax.y, vMax.z, 1.0f),
                mWorld * glm::vec4(vMax.x, vMax.y, vMax.z, 1.0f)
        };

        DrawLine(vPoints[0], vPoints[1], red);
        DrawLine(vPoints[2], vPoints[3], red);
        DrawLine(vPoints[4], vPoints[5], red);
        DrawLine(vPoints[6], vPoints[7], red);

        DrawLine(vPoints[0], vPoints[3], green);
        DrawLine(vPoints[1], vPoints[2], green);
        DrawLine(vPoints[4], vPoints[7], green);
        DrawLine(vPoints[5], vPoints[6], green);

        DrawLine(vPoints[0], vPoints[4], blue);
        DrawLine(vPoints[1], vPoints[5], blue);
        DrawLine(vPoints[2], vPoints[6], blue);
        DrawLine(vPoints[3], vPoints[7], blue);
}

void DebugRenderer::DrawBBox(const BoundingBox & oBBox, const Transform & oTransform, const glm::vec4 & vColor) {

        auto            & mWorld = oTransform.GetWorld();
        const glm::vec3 & vMin   = oBBox.Min();
        const glm::vec3 & vMax   = oBBox.Max();
        uint32_t          color  = ConvertColor(vColor);

        std::array<glm::vec3, 8> vPoints = {
                mWorld * glm::vec4(vMax.x, vMin.y, vMin.z, 1.0f),
                mWorld * glm::vec4(vMin.x, vMin.y, vMin.z, 1.0f),
                mWorld * glm::vec4(vMin.x, vMax.y, vMin.z, 1.0f),
                mWorld * glm::vec4(vMax.x, vMax.y, vMin.z, 1.0f),
                mWorld * glm::vec4(vMax.x, vMin.y, vMax.z, 1.0f),
                mWorld * glm::vec4(vMin.x, vMin.y, vMax.z, 1.0f),
                mWorld * glm::vec4(vMin.x, vMax.y, vMax.z, 1.0f),
                mWorld * glm::vec4(vMax.x, vMax.y, vMax.z, 1.0f)
        };

        DrawLine(vPoints[0], vPoints[1], color);
        DrawLine(vPoints[2], vPoints[3], color);
        DrawLine(vPoints[4], vPoints[5], color);
        DrawLine(vPoints[6], vPoints[7], color);

        DrawLine(vPoints[0], vPoints[3], color);
        DrawLine(vPoints[1], vPoints[2], color);
        DrawLine(vPoints[4], vPoints[7], color);
        DrawLine(vPoints[5], vPoints[6], color);

        DrawLine(vPoints[0], vPoints[4], color);
        DrawLine(vPoints[1], vPoints[5], color);
        DrawLine(vPoints[2], vPoints[6], color);
        DrawLine(vPoints[3], vPoints[7], color);
}

void DebugRenderer::DrawLocalAxes(const Transform & oTransform) {

        auto & mWorld = oTransform.GetWorld();

        glm::vec3 zero(0);
        glm::vec3 x(1, 0, 0);
        glm::vec3 y(0, 1, 0);
        glm::vec3 z(0, 0, 1);

        zero    = mWorld * glm::vec4(zero, 1.0f);
        x       = mWorld * glm::vec4(x, 1.0f);
        y       = mWorld * glm::vec4(y, 1.0f);
        z       = mWorld * glm::vec4(z, 1.0f);

        DrawLine(zero, x, red);
        DrawLine(zero, y, green);
        DrawLine(zero, z, blue);
}

void DebugRenderer::DrawRenderObj(const RenderCommand && oCmd) {

        vRenderCommands.emplace_back(oCmd);
}

void DebugRenderer::Update(const Event & oEvent [[maybe_unused]]) {

        if (oBuffer.Size()) {
                oBuffer.UploadToGPU();
                static const uint32_t stride = 3 * sizeof(float) + sizeof(uint32_t);
                oGeom.SetRange(oBuffer.Size() / stride, 0);
                vRenderCommands.emplace_back(&oGeom, pMaterial, oRootTransform);
                //log_d("item cnt: {}", oBuffer.Size() / stride);
        }

        if (vRenderCommands.size() == 0) { return; }

        //log_d("cmd count: {}", vRenderCommands.size());

        auto & oRenderer = GetSystem<TRenderer>();

        for (auto & oItem : vRenderCommands) {

                oRenderer.AddRenderCmd(&oItem);
        }

}


void DebugRenderer::Clean(const Event & oEvent [[maybe_unused]]) {

        vRenderCommands.clear();
        oBuffer.Clear();

}

void DebugRenderer::DrawGrid(const Transform & oTransform, const float size, const float step) {

        auto & mWorld = oTransform.GetWorld();

        glm::vec3 zero(0);
        glm::vec3 x(size, 0, 0);
        glm::vec3 y(0, size, 0);
        glm::vec3 z(0, 0, size);

        zero    = mWorld * glm::vec4(zero, 1.0f);
        x       = mWorld * glm::vec4(x, 1.0f);
        y       = mWorld * glm::vec4(y, 1.0f);
        z       = mWorld * glm::vec4(z, 1.0f);

        DrawLine(zero, x, red);
        DrawLine(zero, y, green);
        DrawLine(zero, z, blue);

        glm::vec3 start;
        glm::vec3 end;

        for (float i = step; i <= size; i += step) {
                start = mWorld * glm::vec4(0, i, 0, 1.0f);
                end   = mWorld * glm::vec4(size, i, 0, 1.0f);
                DrawLine(start, end, red);
        }

        for (float i = step; i <= size; i += step) {
                start = mWorld * glm::vec4(0, 0, i, 1.0f);
                end   = mWorld * glm::vec4(0, size, i, 1.0f);
                DrawLine(start, end, green);
        }

        for (float i = step; i <= size; i += step) {
                start = mWorld * glm::vec4(i, 0, 0, 1.0f);
                end   = mWorld * glm::vec4(i, 0, size, 1.0f);
                DrawLine(start, end, blue);
        }
}


};
