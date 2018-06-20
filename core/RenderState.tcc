
namespace SE {

static StrID    oScreenSizeID("ScreenSize");

RenderState::RenderState() :
        pModelViewProjection(nullptr),
        pTransformMat(nullptr),
        pShader(nullptr),
        cur_vao(0),
        screen_size(800, 600) {
}


ret_code_t RenderState::SetTexture(const TextureUnit unit_index, const TTexture * pTex) {

        if (pShader) {
                return pShader->SetTexture(unit_index, pTex);
        }

        log_w("shader not set, tex: '{}'", pTex->Name());
        return uLOGIC_ERROR;
}

ret_code_t RenderState::SetTexture(const StrID name, const TTexture * pTex) {

        if (pShader) {
                return pShader->SetTexture(name, pTex);
        }

        log_w("shader not set, tex: '{}'", pTex->Name());
        return uLOGIC_ERROR;
}

void RenderState::SetViewProjection(const glm::mat4 & oMat) {

        pModelViewProjection = &oMat;
        if (pShader) { pShader->SetVariable("MVPMatrix", oMat); }
}

void RenderState::SetTransform(const glm::mat4 & oMat) {

        pTransformMat = &oMat;
        if (pShader) { pShader->SetVariable("MVMatrix", oMat); }
}


void RenderState::SetShaderProgram(ShaderProgram * pNewShader) {

        if (pShader == pNewShader) { return; }

        pShader = pNewShader;
        pShader->Use();

        if (pTransformMat) {
                pShader->SetVariable("MVMatrix", *pTransformMat);
        }
        if (pModelViewProjection) {

                pShader->SetVariable("MVPMatrix", *pModelViewProjection);
        }
        if (pShader->UsedSystemVariables() & ShaderSystemVariables::ScreenSize) {
                pShader->SetVariable(oScreenSizeID, screen_size);
        }
}

void RenderState::Reset() {

        pModelViewProjection = nullptr;
        pTransformMat        = nullptr;
        pShader              = nullptr;
        glUseProgram(0);
        glBindVertexArray(0);
}

//TODO later sort all draw objects |vao|shader|shader values| and apply only changes
void RenderState::Draw(
                const uint32_t vao_id,
                const uint32_t triangles_cnt,
                const uint32_t gl_index_type) {

        if (vao_id < 1) {
               return;
        }

        if (vao_id != cur_vao) {
                glBindVertexArray(vao_id);
                cur_vao = vao_id;
        }
        glDrawElements(GL_TRIANGLES, triangles_cnt * 3, gl_index_type, 0);
}

void RenderState::DrawArrays(
                const uint32_t vao_id,
                const uint32_t mode,
                const uint32_t first,
                const uint32_t count) {

        if (vao_id < 1) {
               return;
        }

        if (vao_id != cur_vao) {
                glBindVertexArray(vao_id);
                cur_vao = vao_id;
        }

        glDrawArrays(mode, first, count);
}

void RenderState::SetScreenSize(const uint32_t width, const uint32_t height) {

        screen_size.x = width;
        screen_size.y = height;

        if (pShader && (pShader->UsedSystemVariables() & ShaderSystemVariables::ScreenSize)) {
                pShader->SetVariable(oScreenSizeID, screen_size);
        }
}

} // namespace SE
