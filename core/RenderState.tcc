
namespace SE {

static StrID    oScreenSizeID("ScreenSize");
/*
using IndexType2Size = MP::Dict< -1,
      MP::Pair<GL_UNSIGNED_BYTE,        sizeof(uint8_t)>,
      MP::Pair<GL_UNSIGNED_SHORT,       sizeof(uint16_t)>,
      MP::Pair<GL_UNSIGNED_INT,         sizeof(uint32_t)>
              >;
*/
static std::array<VertexIndexType, 3> vVertexIndexTypes = {{
        { GL_UNSIGNED_BYTE,       sizeof(uint8_t) },
        { GL_UNSIGNED_SHORT,      sizeof(uint16_t) },
        { GL_UNSIGNED_INT,        sizeof(uint32_t) }
}};

RenderState::RenderState() :
        pModelViewProjection(nullptr),
        pTransformMat(nullptr),
        pShader(nullptr),
        cur_vao(0),
        screen_size(800, 600),
        frame_start_time(std::chrono::time_point_cast<micro>(clock::now())),//FIXME
        last_frame_time(1/60.) {
}

ret_code_t RenderState::SetTexture(const TextureUnit unit_index, TTexture * pTex) {

        if (!pShader) {
                log_w("shader not set, tex: '{}'", pTex->Name());
                return uLOGIC_ERROR;
        }

        uint32_t unit_num = static_cast<uint32_t>(unit_index);


        if (unit_num >= vTextureUnits.size()) {
                log_e("too big unit index = {}, max allowed = {}",
                                unit_num,
                                vTextureUnits.size());
                return uWRONG_INPUT_DATA;
        }

        if (vTextureUnits[unit_num] == pTex) {
                return uSUCCESS;
        }

        if (!pShader->OwnTextureUnit(unit_index)) {
                log_e("texture unit {} unused, shader program: '{}'",
                                unit_num,
                                pShader->Name());
                return uWRONG_INPUT_DATA;
        }

        if (active_tex_unit != unit_num) {

                glActiveTexture(GL_TEXTURE0 + unit_num);
                active_tex_unit = unit_num;
        }

        if (pTex) {
                if (vTextureUnits[unit_num] && vTextureUnits[unit_num]->Type() != pTex->Type() ) {
                        glBindTexture(vTextureUnits[unit_num]->Type(), 0);
                }
                glBindTexture(pTex->Type(), pTex->GetID());
                vTextureUnits[unit_num] = pTex;
        }
        else if (vTextureUnits[unit_num]) {
                glBindTexture(vTextureUnits[unit_num]->Type(), 0);
                vTextureUnits[unit_num] = pTex;
        }

        return uSUCCESS;
}

ret_code_t RenderState::SetTexture(const StrID name, TTexture * pTex) {

        if (!pShader) {
                log_w("shader not set, tex: '{}'", pTex->Name());
                return uLOGIC_ERROR;
        }

        auto oTexInfo = pShader->GetTextureInfo(name);

        if (!oTexInfo) {
                log_e("can't find texture with strid = '{}' in shader program: '{}'",
                                name,
                                pShader->Name());
                return uWRONG_INPUT_DATA;
        }

        uint32_t unit_num = static_cast<uint32_t>(oTexInfo->get().unit_index);

        if (vTextureUnits[unit_num] == pTex) {
                return uSUCCESS;
        }

        if (oTexInfo->get().type != pTex->Type()) {
                log_e("wrong type '{}', sampler '{}' expect {}, in shader program: '{}'",
                                pTex->Type(),
                                oTexInfo->get().sName,
                                oTexInfo->get().type,
                                pShader->Name());
                return uWRONG_INPUT_DATA;
        }

        if (active_tex_unit != unit_num) {

                glActiveTexture(GL_TEXTURE0 + unit_num );
                active_tex_unit = unit_num;
        }

        if (pTex) {
                if (vTextureUnits[unit_num] && vTextureUnits[unit_num]->Type() != pTex->Type() ) {
                        glBindTexture(vTextureUnits[unit_num]->Type(), 0);
                }
                glBindTexture(pTex->Type(), pTex->GetID());
                vTextureUnits[unit_num] = pTex;
        }
        else if (vTextureUnits[unit_num]) {
                glBindTexture(vTextureUnits[unit_num]->Type(), 0);
                vTextureUnits[unit_num] = pTex;
        }

        return uSUCCESS;
}

void RenderState::SetViewProjection(const glm::mat4 & oMat) {

        pModelViewProjection = &oMat;
        if (pShader && (pShader->UsedSystemVariables() & ShaderSystemVariables::MVPMatrix)) {
                pShader->SetVariable("MVPMatrix", oMat);
        }
}

void RenderState::SetTransform(const glm::mat4 & oMat) {

        pTransformMat = &oMat;
        if (pShader && (pShader->UsedSystemVariables() & ShaderSystemVariables::MVMatrix)) {
                pShader->SetVariable("MVMatrix", oMat);
        }
}


void RenderState::SetShaderProgram(ShaderProgram * pNewShader) {

        if (pShader == pNewShader) { return; }

        pShader = pNewShader;
        pShader->Use();

        if (pTransformMat && (pShader->UsedSystemVariables() & ShaderSystemVariables::MVMatrix)) {
                pShader->SetVariable("MVMatrix", *pTransformMat);
        }
        if (pModelViewProjection && (pShader->UsedSystemVariables() & ShaderSystemVariables::MVPMatrix)) {

                pShader->SetVariable("MVPMatrix", *pModelViewProjection);
        }
        if (pShader->UsedSystemVariables() & ShaderSystemVariables::ScreenSize) {
                pShader->SetVariable(oScreenSizeID, screen_size);
        }
}

void RenderState::FrameStart() {

        time_point <micro> cur_time = std::chrono::time_point_cast<micro>(clock::now());
        last_frame_time = std::chrono::duration<float>(cur_time - frame_start_time).count();
        frame_start_time = cur_time;

        pModelViewProjection = nullptr;
        pTransformMat        = nullptr;
        pShader              = nullptr;
        cur_vao              = 0;
        glUseProgram(0);
        glBindVertexArray(0);

        //TODO reset all texture unit bindings
}

//TODO later sort all draw objects |vao|shader|shader values| and apply only changes
void RenderState::Draw(
                const uint32_t vao_id,
                const uint32_t mode,
                const uint32_t index_type,
                const uint32_t start,
                const uint32_t count) {

        if (vao_id < 1) {
               return;
        }

        SetVao(vao_id);

        assert(index_type <= VertexIndexType::INT);

        glDrawElements(mode, count, vVertexIndexTypes[index_type].type, reinterpret_cast<void*>(start * vVertexIndexTypes[index_type].size));
}

void RenderState::DrawArrays(
                const uint32_t vao_id,
                const uint32_t mode,
                const uint32_t start,
                const uint32_t count) {

        if (vao_id < 1) {
               return;
        }

        SetVao(vao_id);

        glDrawArrays(mode, start, count);
}

void RenderState::SetScreenSize(const uint32_t width, const uint32_t height) {

        screen_size.x = width;
        screen_size.y = height;

        if (pShader && (pShader->UsedSystemVariables() & ShaderSystemVariables::ScreenSize)) {
                pShader->SetVariable(oScreenSizeID, screen_size);
        }
}

const glm::uvec2 & RenderState::GetScreenSize() const {
        return screen_size;
}

float RenderState::GetLastFrameTime() const {
        return last_frame_time;
}

void RenderState::SetVao(const uint32_t vao_id) {

        if (vao_id != cur_vao) {
                glBindVertexArray(vao_id);
                cur_vao = vao_id;
        }
}

} // namespace SE
