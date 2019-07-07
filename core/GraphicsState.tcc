
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

GraphicsState::GraphicsState() :
        pModelViewProjection(nullptr),
        pTransformMat(nullptr),
        pShader(nullptr),
        cur_vao(0),
        //screen_size(800, 600),
        frame_start_time(std::chrono::time_point_cast<micro>(clock::now()))//FIXME
        /*last_frame_time(1/60.)*/ {

        //oFrame.frame_start_time= std::chrono::time_point_cast<micro>(clock::now());
        glClearColor(vClearColor.r, vClearColor.g, vClearColor.b, vClearColor.a);
        glClearDepth(clear_depth);
        SetDepthTest(true);
}

ret_code_t GraphicsState::SetTexture(const TextureUnit unit_index, TTexture * pTex) {

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

ret_code_t GraphicsState::SetTexture(const StrID name, TTexture * pTex) {

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

void GraphicsState::SetViewProjection(const glm::mat4 & oMat) {

        pModelViewProjection = &oMat;
        if (pShader && (pShader->UsedSystemVariables() & ShaderSystemVariables::MVPMatrix)) {
                pShader->SetVariable("MVPMatrix", oMat);
        }
}

void GraphicsState::SetTransform(const glm::mat4 & oMat) {

        pTransformMat = &oMat;
        if (pShader && (pShader->UsedSystemVariables() & ShaderSystemVariables::MVMatrix)) {
                pShader->SetVariable("MVMatrix", oMat);
        }
}


void GraphicsState::SetShaderProgram(ShaderProgram * pNewShader) {

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
                pShader->SetVariable(oScreenSizeID, oFrame.screen_size);
        }
}

void GraphicsState::FrameStart() {

        time_point <micro> cur_time = std::chrono::time_point_cast<micro>(clock::now());
        //last_frame_time = std::chrono::duration<float>(cur_time - frame_start_time).count();
        oFrame.last_frame_time  = std::chrono::duration<float>(cur_time - frame_start_time).count();
        frame_start_time = cur_time;
        //oFrame.frame_start_time = cur_time;
        ++oFrame.frame_num;

        pModelViewProjection = nullptr;
        pTransformMat        = nullptr;
        pShader              = nullptr;
        cur_vao              = 0;
        glUseProgram(0);
        glBindVertexArray(0);

        //TODO reset all texture unit bindings
        //TODO reset all uniform unit bindings

        for (size_t i = 0; i < vUniformUnits.size(); ++i ) {
                //vUniformUnits[i] = nullptr;
                vUniformUnits[i]  = -1;
                vUniformRanges[i] = -1;
        }
}

//TODO later sort all draw objects |vao|shader|shader values| and apply only changes
void GraphicsState::Draw(
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

void GraphicsState::DrawArrays(
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

void GraphicsState::SetScreenSize(const glm::uvec2 new_screen_size) {

        oFrame.screen_size = new_screen_size;

        if (pShader && (pShader->UsedSystemVariables() & ShaderSystemVariables::ScreenSize)) {
                pShader->SetVariable(oScreenSizeID, oFrame.screen_size);
        }

        //THINK move glViewport here?
}
void GraphicsState::SetScreenSize(const uint32_t width, const uint32_t height) {

        oFrame.screen_size.x = width;
        oFrame.screen_size.y = height;

        if (pShader && (pShader->UsedSystemVariables() & ShaderSystemVariables::ScreenSize)) {
                pShader->SetVariable(oScreenSizeID, oFrame.screen_size);
        }

        //THINK move glViewport here?
}

const glm::uvec2 & GraphicsState::GetScreenSize() const {
        return oFrame.screen_size;
}

float GraphicsState::GetLastFrameTime() const {
        return oFrame.last_frame_time;
}

void GraphicsState::SetVao(const uint32_t vao_id) {

        if (vao_id != cur_vao) {
                glBindVertexArray(vao_id);
                cur_vao = vao_id;
        }
}

void GraphicsState::UploadUniformBufferData(
                const uint32_t buf_id,
                const uint32_t buf_size,
                const void * pData) {

        if (active_ubo != buf_id) {
                active_ubo = buf_id;
                glBindBuffer(GL_UNIFORM_BUFFER, active_ubo);
        }

        glBufferData(GL_UNIFORM_BUFFER, buf_size, pData, GL_DYNAMIC_DRAW);
}

void GraphicsState::UploadUniformBufferSubData(
                const uint32_t buf_id,
                const uint32_t buf_offset,
                const uint32_t block_size,
                const void * pData) {

        if (active_ubo != buf_id) {
                active_ubo = buf_id;
                glBindBuffer(GL_UNIFORM_BUFFER, active_ubo);
        }

        glBufferSubData(GL_UNIFORM_BUFFER, buf_offset, block_size, pData);
}

void GraphicsState::BindUniformBufferRange(
                const uint32_t buf_id,
                const UniformUnitInfo::Type unit_id,
                const uint32_t buf_offset,
                const uint32_t block_size) {

        uint8_t unit = static_cast<uint8_t>(unit_id);

        if (vUniformUnits[unit] != buf_id) {
                vUniformUnits[unit] = buf_id;
                //bind
        }
        else if (vUniformRanges[unit] == buf_offset) {
                //already binded
                return;
        }
        vUniformRanges[unit] = buf_offset;

        glBindBufferRange(GL_UNIFORM_BUFFER, unit, buf_id, buf_offset, block_size);
}

std::shared_ptr<UniformBuffer> GraphicsState::GetUniformBuffer(
                const UniformUnitInfo::Type unit_id,
                const uint16_t block_size) {

        uint32_t key = (static_cast<uint8_t>(unit_id) << 16) | block_size;

        auto it = mUniformBuffers.find(key);

        if (it == mUniformBuffers.end() || it->second.expired()) {
                auto pItem = std::make_shared<UniformBuffer>(block_size, GetUniformUnitInfo(unit_id).initial_block_cnt);
                mUniformBuffers.insert_or_assign(key, pItem);
                log_d("create buffer: key: {}, unit: {}, block_size: {}",
                                key,
                                GetUniformUnitInfo(unit_id).sName,
                                block_size);
                return pItem;
        }

        if (auto pItem = it->second.lock()) {
                return pItem;
        }
        log_e("failed to lock ptr for buffer key: {}, block_size: {} ", key, block_size);
        return nullptr;
}

const UniformUnitInfo & GraphicsState::GetUniformUnitInfo(const UniformUnitInfo::Type unit_id) const {

        se_assert(unit_id <= UniformUnitInfo::Type::MAX);
        return vUniformUnitInfo[static_cast<uint8_t>(unit_id)];
}

void GraphicsState::SetClearColor(const glm::vec4 & vColor) {

        if (glm::all(glm::equal(vColor, vClearColor))) { return; }

        vClearColor = vColor;
        glClearColor(vClearColor.r, vClearColor.g, vClearColor.b, vClearColor.a);
}

void GraphicsState::SetClearColor(const float r, const float g, const float b, const float a) {

        glm::vec4 vColor(r, g, b, a);
        SetClearColor(vColor);
}

void GraphicsState::SetClearDepth(const float value) {

        if (value == clear_depth) { return; }

        clear_depth = value;
        glClearDepth(clear_depth);
}

void GraphicsState::SetDepthFunc(const DepthFunc value) {

        static const uint32_t vDepthMapping[] = {
                GL_ALWAYS,
                GL_EQUAL,
                GL_NOTEQUAL,
                GL_LESS,
                GL_LEQUAL,
                GL_GREATER,
                GL_GEQUAL
        };

        if (value == depth_func) { return; }

        depth_func = value;
        glDepthFunc(vDepthMapping[static_cast<uint32_t>(depth_func)]);
}

void GraphicsState::SetDepthTest(const bool enable) {

        if (enable == depth_test) { return; }

        if (enable) {
                glEnable(GL_DEPTH_TEST);
        }
        else {
                glDisable(GL_DEPTH_TEST);
        }
        depth_test = enable;
}

void GraphicsState::SetDepthMask(const bool enable) {

        if (enable == depth_write) { return; }
        depth_write = enable;
        glDepthMask(depth_write);
}

void GraphicsState::SetColorMask(const bool enable) {

        if (enable == color_write) { return; }
        color_write = enable;
        if (color_write) {
                glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        }
        else {
                glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        }
}

const FrameState & GraphicsState::GetFrameState() const {
        return oFrame;
}


} // namespace SE
