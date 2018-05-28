
namespace SE {

RenderState::RenderState() :
        pModelViewProjection(nullptr),
        pTransformMat(nullptr),
        pShader(nullptr) {
}


template <class T> ret_code_t RenderState::SetVariable(const StrID name, const T & val) {

        if (pShader) {
                return pShader->SetVariable(name, val);
        }

        log_w("shader not set, var: '{}'", name);
        return uLOGIC_ERROR;
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

        glBindVertexArray(vao_id);
        glDrawElements(GL_TRIANGLES, triangles_cnt * 3, gl_index_type, 0);
}

} // namespace SE
